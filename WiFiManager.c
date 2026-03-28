#include "WiFiManager.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "sys/param.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "netinet/in.h"

static const char *TAG = "[WiFi]";

/* ===================== INTERNAL: DNS SERVER ========================= */

/** DNS packet header (network byte order). Packed to avoid compiler padding. */
typedef struct __attribute__((packed))
{
    uint16_t id;      /**< Transaction ID, copied from query to response. */
    uint16_t flags;   /**< QR | Opcode | AA | TC | RD | RA | Z | RCODE. */
    uint16_t qdcount; /**< Number of questions. */
    uint16_t ancount; /**< Number of answers. */
    uint16_t nscount; /**< Number of authority records (unused). */
    uint16_t arcount; /**< Number of additional records (unused). */
} dns_hdr_t;

/** Internal state of the DNS server instance. */
typedef struct
{
    esp_ip4_addr_t ip; /**< IPv4 address returned for every A query. */
    TaskHandle_t task; /**< FreeRTOS task handle, used to delete on stop. */
    int sock;          /**< UDP socket fd, -1 if not yet opened. */
} dns_server_t;

/** FreeRTOS task that listens on UDP port 53 and answers every A query with the configured IP. */
static void dns_task(void *arg)
{
    dns_server_t *srv = (dns_server_t *)arg;
    uint8_t buf[512];

    srv->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (srv->sock < 0)
    {
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = INADDR_ANY,
    };
    bind(srv->sock, (struct sockaddr *)&addr, sizeof(addr));
    ESP_LOGI(TAG, "[DNS] Started → " IPSTR, IP2STR(&srv->ip));

    while (1)
    {
        struct sockaddr_in client;
        socklen_t clen = sizeof(client);
        int len = recvfrom(srv->sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&client, &clen);

        if (len < (int)sizeof(dns_hdr_t))
            continue;

        dns_hdr_t *hdr = (dns_hdr_t *)buf;
        if (hdr->flags & htons(0x8000)) /* QR=1 → response, ignore */
            continue;
        if (ntohs(hdr->qdcount) == 0)
            continue;

        /* Build response in-place: QR=1, AA=1, RCODE=0 */
        hdr->flags = htons(0x8400);
        hdr->ancount = htons(1);

        /* Append answer record after the question section */
        uint8_t *ans = buf + len;
        *ans++ = 0xC0;
        *ans++ = 0x0C; /**< NAME: pointer to QNAME at offset 12. */
        *ans++ = 0x00;
        *ans++ = 0x01; /**< TYPE: A (IPv4). */
        *ans++ = 0x00;
        *ans++ = 0x01; /**< CLASS: IN. */
        *ans++ = 0x00;
        *ans++ = 0x00;
        *ans++ = 0x00;
        *ans++ = 0x3C; /**< TTL: 60 seconds. */
        *ans++ = 0x00;
        *ans++ = 0x04; /**< RDLENGTH: 4 bytes. */
        memcpy(ans, &srv->ip.addr, 4);
        ans += 4;

        sendto(srv->sock, buf, (int)(ans - buf), 0,
               (struct sockaddr *)&client, clen);
    }
    close(srv->sock);
    vTaskDelete(NULL);
}

/** Allocate and start the DNS server. Returns NULL on allocation failure. */
static dns_server_t *dns_start(esp_ip4_addr_t ip)
{
    dns_server_t *srv = calloc(1, sizeof(*srv));
    if (!srv)
        return NULL;
    srv->ip = ip;
    srv->sock = -1;
    xTaskCreate(dns_task, "dns_srv", 4096, srv, 5, &srv->task);
    return srv;
}

/** Stop the DNS server and release all resources. */
static void dns_stop(dns_server_t *srv)
{
    if (!srv)
        return;
    if (srv->sock >= 0)
        close(srv->sock);
    if (srv->task)
        vTaskDelete(srv->task);
    free(srv);
}

/* ===================== INTERNAL: STRING BUILDER ===================== */

/** Growable heap string buffer. Resizes automatically via realloc. */
typedef struct
{
    char *buf;  /**< Heap-allocated character buffer. */
    size_t len; /**< Current string length (excluding null terminator). */
    size_t cap; /**< Current allocated capacity in bytes. */
} StrBuf;

/** Initialize a growable string buffer with initial capacity. */
static int sb_init(StrBuf *sb, size_t initial_cap)
{
    sb->buf = (char *)malloc(initial_cap);
    if (!sb->buf)
        return -1;
    sb->buf[0] = '\0';
    sb->len = 0;
    sb->cap = initial_cap;
    return 0;
}

/** Append string s to the buffer, reallocating if needed. */
static int sb_append(StrBuf *sb, const char *s)
{
    size_t slen = strlen(s);
    size_t need = sb->len + slen + 1;

    if (need > sb->cap)
    {
        size_t new_cap = sb->cap * 2;
        if (new_cap < need)
            new_cap = need;

        char *tmp = (char *)realloc(sb->buf, new_cap);
        if (!tmp)
            return -1;

        sb->buf = tmp;
        sb->cap = new_cap;
    }

    memcpy(sb->buf + sb->len, s, slen + 1);
    sb->len += slen;
    return 0;
}

/** Release the buffer and return the internal string. Caller must free(). */
static char *sb_release(StrBuf *sb)
{
    char *result = sb->buf;
    sb->buf = NULL;
    sb->len = sb->cap = 0;
    return result;
}

/* ===================== INTERNAL: HTML PAGE BUILDER ================== */

/** Escape HTML special chars to prevent XSS. Caller must free(). */
static char *escape_html(const char *s)
{
    size_t len = strlen(s);
    char *out = (char *)malloc(len * 6 + 1);
    if (!out)
        return NULL;

    char *p = out;
    for (; *s; s++)
    {
        switch (*s)
        {
        case '&':
            p += sprintf(p, "&amp;");
            break;
        case '<':
            p += sprintf(p, "&lt;");
            break;
        case '>':
            p += sprintf(p, "&gt;");
            break;
        case '"':
            p += sprintf(p, "&quot;");
            break;
        case '\'':
            p += sprintf(p, "&#39;");
            break;
        default:
            *p++ = *s;
            break;
        }
    }
    *p = '\0';
    return out;
}

/** Append CSS styles to the page buffer. */
static int append_css(StrBuf *sb)
{
    return sb_append(sb,
                     "<style>"
                     "*{margin:0;padding:0;box-sizing:border-box;font-family:'Segoe UI',sans-serif}"
                     "body{background:#f5f5f5;display:flex;justify-content:center;"
                     "align-items:center;min-height:100vh;padding:20px}"
                     ".container{background:#fff;border-radius:2px;width:100%;"
                     "max-width:450px;padding:30px}"
                     "h1{color:#2c3e50;font-size:28px;margin-bottom:20px;text-align:center}"
                     ".form-group{margin-bottom:20px}"
                     "label{display:block;color:#34495e;font-weight:600;margin-bottom:8px}"
                     ".label-info{font-size:12px;color:#7f8c8d;margin-top:5px}"
                     "input{width:100%;padding:12px 15px;border:1px solid #ddd;"
                     "border-radius:2px;font-size:16px}"
                     "input:focus{outline:none;border-color:#3498db}"
                     ".password-wrapper{position:relative}"
                     ".toggle-password{position:absolute;right:12px;top:50%;"
                     "transform:translateY(-50%);cursor:pointer;color:#7f8c8d}"
                     ".submit-btn{width:100%;background:#3498db;color:#fff;border:none;"
                     "border-radius:2px;padding:14px;font-size:16px;"
                     "font-weight:600;cursor:pointer;margin-top:4px}"
                     ".submit-btn:hover{background:#2980b9}"
                     ".status{margin-top:20px;padding:12px;border-radius:2px;"
                     "text-align:center;display:none}"
                     ".status.success{background:#d4edda;color:#155724;display:block}"
                     ".status.error{background:#f8d7da;color:#721c24;display:block}"
                     "</style>");
}

/** Append extra dynamic input fields to the page buffer. */
static int append_extra_fields(StrBuf *sb, const WiFiManagerPage_t *page)
{
    for (size_t i = 0; i < page->count; i++)
    {
        const WiFiManagerParam_t *p = &page->params[i];

        char *eid = escape_html(p->id);
        char *elbl = escape_html(p->label);
        char *eph = escape_html(p->placeholder);
        char *eval = escape_html(p->value);
        char *etyp = escape_html(p->type);
        if (!eid || !elbl || !eph || !eval || !etyp)
        {
            free(eid);
            free(elbl);
            free(eph);
            free(eval);
            free(etyp);
            return -1;
        }

        const char *req = p->required ? " required" : "";

        sb_append(sb, "<div class=\"form-group\">");
        sb_append(sb, "<label for=\"");
        sb_append(sb, eid);
        sb_append(sb, "\">");
        sb_append(sb, elbl);
        sb_append(sb, "</label>");

        if (strcmp(p->type, "password") == 0)
        {
            /* Password: Add toggle show/hide button */
            sb_append(sb, "<div class=\"password-wrapper\">"
                          "<input type=\"password\" id=\"");
            sb_append(sb, eid);
            sb_append(sb, "\" name=\"");
            sb_append(sb, eid);
            sb_append(sb, "\" placeholder=\"");
            sb_append(sb, eph);
            sb_append(sb, "\" value=\"");
            sb_append(sb, eval);
            sb_append(sb, "\"");
            sb_append(sb, req);
            sb_append(sb, ">");
            sb_append(sb, "<span class=\"toggle-password\""
                          " onclick=\"toggleField('");
            sb_append(sb, eid);
            sb_append(sb, "')\">&#128065;</span></div>");
        }
        else
        {
            sb_append(sb, "<input type=\"");
            sb_append(sb, etyp);
            sb_append(sb, "\" id=\"");
            sb_append(sb, eid);
            sb_append(sb, "\" name=\"");
            sb_append(sb, eid);
            sb_append(sb, "\" placeholder=\"");
            sb_append(sb, eph);
            sb_append(sb, "\" value=\"");
            sb_append(sb, eval);
            sb_append(sb, "\"");
            sb_append(sb, req);
            sb_append(sb, ">");
        }

        if (p->placeholder[0] != '\0')
        {
            sb_append(sb, "<div class=\"label-info\">");
            sb_append(sb, eph);
            sb_append(sb, "</div>");
        }
        sb_append(sb, "</div>");

        free(eid);
        free(elbl);
        free(eph);
        free(eval);
        free(etyp);
    }
    return 0;
}

/** Append form submit JavaScript to the page buffer. */
static int append_script(StrBuf *sb, const WiFiManagerPage_t *page)
{
    /* Build JS arrays: required ids and all ids */
    StrBuf required_ids, all_ids;
    if (sb_init(&required_ids, 128) < 0)
        return -1;
    if (sb_init(&all_ids, 128) < 0)
    {
        free(required_ids.buf);
        return -1;
    }

    for (size_t i = 0; i < page->count; i++)
    {
        const WiFiManagerParam_t *p = &page->params[i];
        sb_append(&all_ids, "'");
        sb_append(&all_ids, p->id);
        sb_append(&all_ids, "',");
        if (p->required)
        {
            sb_append(&required_ids, "'");
            sb_append(&required_ids, p->id);
            sb_append(&required_ids, "',");
        }
    }

    sb_append(sb, "<script>"
                  "function toggleField(id){"
                  "var el=document.getElementById(id);"
                  "el.type=el.type==='password'?'text':'password';}"
                  "document.getElementById('configForm').addEventListener('submit',function(e){"
                  "e.preventDefault();"
                  "var ssid=document.getElementById('ssid').value.trim();"
                  "var status=document.getElementById('statusMessage');"
                  "if(!ssid){status.textContent='SSID is required';"
                  "status.className='status error';return;}"
                  "var required=[");
    sb_append(sb, required_ids.buf);
    sb_append(sb, "];"
                  "for(var i=0;i<required.length;i++){"
                  "var el=document.getElementById(required[i]);"
                  "if(el&&!el.value.trim()){status.textContent=required[i]+' is required';"
                  "status.className='status error';return;}}"
                  "var body=new URLSearchParams();"
                  "body.append('ssid',ssid);"
                  "body.append('password',document.getElementById('password').value);"
                  "var extras=[");
    sb_append(sb, all_ids.buf);
    sb_append(sb, "];"
                  "for(var j=0;j<extras.length;j++){"
                  "var el=document.getElementById(extras[j]);"
                  "if(el)body.append(extras[j],el.value);}"
                  "fetch('/',{method:'POST',"
                  "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
                  "body:body.toString()})"
                  ".then(function(res){"
                  "if(!res.ok)throw new Error('Save failed');"
                  "status.textContent='Saved. Restarting...';"
                  "status.className='status success';})"
                  ".catch(function(err){"
                  "status.textContent='Error: '+err.message;"
                  "status.className='status error';});"
                  "});"
                  "</script>");

    free(required_ids.buf);
    free(all_ids.buf);
    return 0;
}

/* ===================== INTERNAL: HTTP PORTAL ======================== */

/** Decode a URL-encoded string into dst. */
static void url_decode(char *dst, const char *src, size_t dst_size)
{
    char *end = dst + dst_size - 1;
    while (*src && dst < end)
    {
        if (*src == '+')
        {
            *dst++ = ' ';
            src++;
        }
        else if (*src == '%' && src[1] && src[2])
        {
            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/** Parse a URL-encoded form body and store field values into wm. */
static esp_err_t WiFiManager_ParseUrlencoded(WiFiManager_t *wm, char *body)
{
    char *sp_pair, *sp_kv;
    char *pair = strtok_r(body, "&", &sp_pair);

    while (pair)
    {
        char *key = strtok_r(pair, "=", &sp_kv);
        char *val = strtok_r(NULL, "=", &sp_kv);

        if (key)
        {
            char dkey[WM_FIELD_LEN], dval[WM_FIELD_LEN];
            url_decode(dkey, key, sizeof(dkey));
            url_decode(dval, val ? val : "", sizeof(dval));

            if (strcmp(dkey, "ssid") == 0)
            {
                strncpy((char *)wm->config.sta.ssid, dval,
                        sizeof(wm->config.sta.ssid) - 1);
            }
            else if (strcmp(dkey, "password") == 0)
            {
                strncpy((char *)wm->config.sta.password, dval,
                        sizeof(wm->config.sta.password) - 1);
                wm->config.sta.threshold.authmode =
                    strlen(dval) ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN;
            }
            else
            {
                for (size_t i = 0; i < wm->page.count; i++)
                {
                    WiFiManagerParam_t *p = &wm->page.params[i];
                    if (strcmp(p->id, dkey) == 0)
                    {
                        strncpy(p->value, dval, WM_FIELD_LEN - 1);
                        break;
                    }
                }
            }
        }
        pair = strtok_r(NULL, "&", &sp_pair);
    }
    return ESP_OK;
}

/** Handle requests on the captive portal root URI ("/"). */
static esp_err_t WiFiManager_PortalRequestHandler(httpd_req_t *req)
{
    switch (req->method)
    {
    case HTTP_GET:
    {
        WiFiManager_t *wm = (WiFiManager_t *)req->user_ctx;
        if (!wm)
        {
            ESP_LOGE(TAG, "[CP] user context is null.");
            return ESP_ERR_INVALID_ARG;
        }

        /* Generate config page */
        char *html = WiFiManagerPage_Build(&wm->page);
        if (!html)
        {
            ESP_LOGE(TAG, "[CP] Failed to generate config page.");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        /* Response */
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
        free(html);
        return ESP_OK;
    }

    case HTTP_POST:
    {
        WiFiManager_t *wm = (WiFiManager_t *)req->user_ctx;
        if (!wm)
        {
            ESP_LOGE(TAG, "[CP] user context is null.");
            return ESP_ERR_INVALID_ARG;
        }

        /* Handle request */
        char content[WM_PORTAL_BODY_SIZE];
        // Truncate if content length larger than the buffer
        size_t recv_size = MIN(req->content_len, sizeof(content) - 1);
        // Read HTTP content data (body) from the HTTP request into provided buffer
        int ret = httpd_req_recv(req, content, recv_size);
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                httpd_resp_send_408(req);
            }
            return ESP_FAIL;
        }
        content[ret] = '\0';
        WiFiManager_ParseUrlencoded(wm, content);

        ESP_LOGI(TAG, "[CP] Form body: %s", content);
        const char *resp_str = "Data received";
        httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(TAG, "[CP] Configuration received");

        if (wm->portal_waiting_task)
        {
            xTaskNotifyGive(wm->portal_waiting_task);
        }
        return ESP_OK;
    }

    default:
    {
        return ESP_OK;
    }
    };
}

/** Redirects all requests to the root page. */
static esp_err_t WiFiManager_Portal404Handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 - Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "[Captive Portal] - Redirecting to root");
    return ESP_OK;
}

/** URI descriptors for the captive portal root endpoint. */
static httpd_uri_t root_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = WiFiManager_PortalRequestHandler,
    .user_ctx = NULL};
static httpd_uri_t root_post = {
    .uri = "/",
    .method = HTTP_POST,
    .handler = WiFiManager_PortalRequestHandler,
    .user_ctx = NULL};

/** Start the HTTP server and register portal URI handlers. */
static httpd_handle_t WiFiManager_StartWebServer(WiFiManager_t *wm)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 6;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 180; // Timeout for waiting client data: 3 minutes

    // Start the httpd server
    ESP_LOGI(TAG, "[Captive Portal] - Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "[Captive Portal] - Registering URI handlers");

        root_post.user_ctx = wm;
        root_get.user_ctx = wm;

        httpd_register_uri_handler(server, &root_get);
        httpd_register_uri_handler(server, &root_post);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, WiFiManager_Portal404Handler);

        wm->server = server;
    }
    return server;
}

/** Stop the HTTP server */
static void WiFiManager_StopWebServer(WiFiManager_t *wm)
{
    if (wm->server)
    {
        httpd_stop(wm->server);
        wm->server = NULL;
    }
}

/* ===================== INTERNAL: WIFI CORE ========================== */

/** Dispatch WiFi and IP events to update event group bits and invoke callbacks. */
static void WiFiManager_EventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    WiFiManager_t *wm = (WiFiManager_t *)arg;
    esp_err_t err;

    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_AP_START:
        {
            /* setup status */
            xEventGroupSetBits(wm->event.group, WM_EVENT_BIT_APSTART);

            /* get ip info */
            esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
            esp_netif_ip_info_t ip_info;
            err = esp_netif_get_ip_info(ap_netif, &ip_info);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "[AP] Failed to get IP: %s", esp_err_to_name(err));
                return;
            }

            char *ap_ssid = (char *)wm->config.ap.ssid;
            ESP_LOGI(TAG, "[AP] SoftAP Started. SSID: %s, IP: " IPSTR, ap_ssid, IP2STR(&ip_info.ip));
            break;
        }

        case WIFI_EVENT_AP_STACONNECTED:
        {
            /* joined station data */
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;

            ESP_LOGI(TAG, "[AP] Station " MACSTR " join, AID: %d", MAC2STR(event->mac), event->aid);
            break;
        }

        case WIFI_EVENT_AP_STOP:
        {

            /* clear status */
            xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_APSTART);

            ESP_LOGI(TAG, "[AP] AP Stopped");
            break;
        }

        case WIFI_EVENT_STA_START:
        {
            /* Setup status */
            xEventGroupSetBits(wm->event.group, WM_EVENT_BIT_STASTART);
            /* Connect to AP */
            err = esp_wifi_connect();
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "[STA] Failed to connect to the AP: %s", esp_err_to_name(err));
                return;
            }
            break;
        }

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            /* setup status */
            xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_STACONNECTED);
            xEventGroupSetBits(wm->event.group, WM_EVENT_BIT_STADISCONNECTED);
            /* sta_disconnected info */
            wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;

            ESP_LOGE(TAG, "[STA] Failed to connect to the AP, reason: %d", event->reason);

            /* Callback */
            if (wm->DisconnectedAP_Cb)
            {
                wm->DisconnectedAP_Cb();
            }
            break;
        }
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        /* setup status */
        xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_STADISCONNECTED);
        xEventGroupSetBits(wm->event.group, WM_EVENT_BIT_STACONNECTED);
        /* sta_got_ip info */
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        /* log */
        ESP_LOGI(TAG, "[STA] Connected to the AP, ip: " IPSTR, IP2STR(&event->ip_info.ip));

        /* Callback */
        if (wm->ConnectedAP_Cb)
        {
            wm->ConnectedAP_Cb();
        }
    }
}

/** Set DHCP option 114 to redirect connecting clients to the captive portal. */
static void WiFiManager_SetCaptivePortalURI(WiFiManager_t *wm)
{
    esp_err_t ret;

    // get a handle to configure DHCP with
    esp_netif_t *netif = wm->netif;
    if (!netif)
    {
        ESP_LOGE(TAG, "[DHCP] netif is null");
        return;
    }

    // get the IP of the access point to redirect to
    esp_netif_ip_info_t ip_info;
    ret = esp_netif_get_ip_info(netif, &ip_info); // Get AP IP
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[DHCP] Failed to get IP info, error: %s", esp_err_to_name(ret));
        return;
    }

    char ip_addr[INET_ADDRSTRLEN];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, INET_ADDRSTRLEN); // Convert IPv4 (uint32_t) to string ("a.b.c.d") format
    ESP_LOGI(TAG, "[AP Mode] - Set up softAP with IP: %s", ip_addr);

    // turn the IP into a URI
    char captiveportal_uri[32];
    snprintf(captiveportal_uri, sizeof(captiveportal_uri), "http://%s", ip_addr);

    // set the DHCP option 114
    ret = esp_netif_dhcps_stop(netif);
    if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
    {
        ESP_LOGW(TAG, "DHCP stop warning: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, captiveportal_uri, strlen(captiveportal_uri));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[DHCP] - Failed to set DHCP option: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "[DHCP] Captive Portal URI set: %s", captiveportal_uri);
    }

    ret = esp_netif_dhcps_start(netif);
    if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)
    {
        ESP_LOGW(TAG, "Failed to start DHCP: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "[DHCP] Server started succesfully");
    }
}

/* ===================== PUBLIC: WIFI LIFECYCLE ======================= */

void WiFiManager_Init(WiFiManager_t *wm)
{
    esp_err_t ret;

    ret = esp_netif_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[Init] Failed to initialize network interface (netif)!");
        return;
    }
    if (wm->event.group == NULL)
    {
        wm->event.group = xEventGroupCreate();
    }
    ret = esp_event_loop_create_default();
    if (ret == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGW(TAG, "Default event loop has already been created");
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create event loop, error: %s", esp_err_to_name(ret));
        return;
    }

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_mode_t mode;
    ret = esp_wifi_get_mode(&mode);
    if (ret != ESP_ERR_WIFI_NOT_INIT)
    {
        WiFiManager_Stop(wm);
    }

    // WiFi driver config
    wifi_init_config_t wifi_drv_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_drv_cfg));
}

void WiFiManager_StartSTA(WiFiManager_t *wm)
{
    if (wm->netif == NULL)
    {
        wm->netif = esp_netif_create_default_wifi_sta();
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START, WiFiManager_EventHandler, wm, &wm->event.sta_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, WiFiManager_EventHandler, wm, &wm->event.sta_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, WiFiManager_EventHandler, wm, &wm->event.ip_handle));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (strlen((char *)wm->config.sta.ssid) > 0)
    {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wm->config));
    }
    else
    {
        ESP_LOGI(TAG, "[STA] Attempt to load saved credentials.");
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(pdMS_TO_TICKS(100));
}

void WiFiManager_StartAP(WiFiManager_t *wm)
{
    if (!wm->netif)
    {
        wm->netif = esp_netif_create_default_wifi_ap();
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, WiFiManager_EventHandler, wm, &wm->event.ap_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, WiFiManager_EventHandler, wm, &wm->event.ap_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_START, WiFiManager_EventHandler, wm, &wm->event.ap_handle));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wm->config));
    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(pdMS_TO_TICKS(100));
}

void WiFiManager_Stop(WiFiManager_t *wm)
{
    if (!wm)
    {
        ESP_LOGE(TAG, "[WiFi Stop] - WiFi manager is NULL");
        return;
    }

    esp_err_t err;
    wifi_mode_t mode;

    err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "[WiFi Stop] - Failed to get WiFi mode: %s", esp_err_to_name(err));
        mode = WIFI_MODE_NULL;
    }

    ESP_LOGI(TAG, "[WiFi Stop] - Stopping WiFi (mode: %d)", mode);

    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA)
    {
        err = esp_wifi_disconnect();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
        {
            ESP_LOGW(TAG, "[WiFi Stop] - Disconnect failed: %s", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(TAG, "[WiFi Stop] - STA disconnected");
        }
    }

    err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
    {
        ESP_LOGE(TAG, "[WiFi Stop] WiFi stop failed: %s", esp_err_to_name(err));
        return;
    }

    if (wm->event.ap_handle)
    {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wm->event.ap_handle);
        wm->event.ap_handle = NULL;
    }
    if (wm->event.sta_handle)
    {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wm->event.sta_handle);
        wm->event.sta_handle = NULL;
    }
    if (wm->event.ip_handle)
    {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wm->event.ip_handle);
        wm->event.ip_handle = NULL;
    }

    xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_APSTART);
    xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_STASTART);
    xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_STACONNECTED);
    xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_STADISCONNECTED);

    if (wm->netif)
    {
        esp_netif_destroy_default_wifi(wm->netif);
        wm->netif = NULL;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG, "[WiFi Stop] - WiFi stopped.");
}

void WiFiManager_Deinit(WiFiManager_t *wm)
{
    esp_err_t err;
    WiFiManager_Stop(wm);

    err = esp_wifi_deinit();
    if (err == ESP_ERR_WIFI_NOT_INIT)
    {
        ESP_LOGE(TAG, "[WiFi Deinit] WiFi driver was not installed by esp_wifi_init");
        return;
    }

    err = nvs_flash_deinit();
    if (err == ESP_ERR_NVS_NOT_INITIALIZED)
    {
        ESP_LOGE(TAG, "[WiFi Deinit] The storage driver is not initialized");
        return;
    }

    err = esp_event_loop_delete_default();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "[WiFi Deinit] error: %s", esp_err_to_name(err));
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG, "[WiFi Deinit] - WiFi deinitialized");
}

void WiFiManager_ConfigViaAP(WiFiManager_t *wm)
{
    WiFiManager_StartAP(wm);
    xEventGroupWaitBits(wm->event.group, WM_EVENT_BIT_APSTART, pdFALSE, pdFALSE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(200));

    WiFiManager_SetCaptivePortalURI(wm);
    WiFiManager_StartWebServer(wm);

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(wm->netif, &ip_info);
    dns_server_t *dns = dns_start(ip_info.ip);

    /* Wait to received data */
    wm->portal_waiting_task = xTaskGetCurrentTaskHandle();
    BaseType_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WM_PORTAL_TIMOUT_MS));
    wm->portal_waiting_task = NULL;

    if (!notified)
    {
        ESP_LOGW(TAG, "[ConfigViaAP] Timeout - no credentials received");
        dns_stop(dns);
        WiFiManager_StopWebServer(wm);
        WiFiManager_Stop(wm);
        return;
    }

    /* Stop server */
    dns_stop(dns);
    WiFiManager_StopWebServer(wm);
    WiFiManager_Stop(wm);

    /* Set config */
    char ssid[32], password[64];
    wifi_auth_mode_t authmode = wm->config.sta.threshold.authmode;
    memcpy(ssid, wm->config.sta.ssid, sizeof(ssid));
    memcpy(password, wm->config.sta.password, sizeof(password));

    memset(&wm->config, 0, sizeof(wm->config));
    memcpy(wm->config.sta.ssid, ssid, sizeof(ssid));
    memcpy(wm->config.sta.password, password, sizeof(password));
    wm->config.sta.threshold.authmode = authmode;
    wm->config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wm->config.sta.failure_retry_cnt = wm->sta_retry_num;

    ESP_LOGI(TAG, "[STA_ConfigViaAP] Switch to STA Mode");
    WiFiManager_StartSTA(wm);
}

wifi_mode_t WiFiManager_GetMode(void)
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK)
    {
        mode = WIFI_MODE_NULL;
    }
    return mode;
}

/* ===================== PUBLIC: PAGE APIs ============================ */

void WiFiManagerPage_Init(WiFiManagerPage_t *page)
{
    memset(page, 0, sizeof(*page));
}

int WiFiManagerPage_AddParam(WiFiManagerPage_t *page,
                             const char *id,
                             const char *label,
                             const char *placeholder,
                             const char *value,
                             const char *type,
                             bool required)
{
    if (page->count >= WM_MAX_PARAMS)
        return -1;

    WiFiManagerParam_t *p = &page->params[page->count++];
    memset(p, 0, sizeof(*p));

    strncpy(p->id, id ? id : "", WM_FIELD_LEN - 1);
    strncpy(p->label, label ? label : "", WM_FIELD_LEN - 1);
    strncpy(p->placeholder, placeholder ? placeholder : "", WM_FIELD_LEN - 1);
    strncpy(p->value, value ? value : "", WM_FIELD_LEN - 1);
    strncpy(p->type, type ? type : "text", sizeof(p->type) - 1);
    p->required = required;

    return 0;
}

const char *WiFiManagerPage_GetParam(const WiFiManagerPage_t *page, const char *id)
{
    for (size_t i = 0; i < page->count; i++)
    {
        if (strcmp(page->params[i].id, id) == 0)
        {
            return page->params[i].value;
        }
    }
    return NULL;
}

/** Assemble the full HTML configuration page. Caller must free(). */
char *WiFiManagerPage_Build(const WiFiManagerPage_t *page)
{
    StrBuf sb;
    if (sb_init(&sb, 5120) < 0)
        return NULL;

    /* <head> */
    sb_append(&sb, "<!DOCTYPE html><html lang=\"en\"><head>"
                   "<meta charset=\"UTF-8\">"
                   "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">"
                   "<title>WiFi Configuration</title>");
    if (append_css(&sb) < 0)
        goto fail;
    sb_append(&sb, "</head><body><div class=\"container\">"
                   "<header><h1>WiFi Configuration</h1></header>"
                   "<form class=\"config-form\" id=\"configForm\">");

    /* Fixed: SSID */
    sb_append(&sb, "<div class=\"form-group\">"
                   "<label for=\"ssid\">SSID</label>"
                   "<input type=\"text\" id=\"ssid\" name=\"ssid\" required>"
                   "<div class=\"label-info\">WiFi name</div>"
                   "</div>");

    /* Fixed: Password */
    sb_append(&sb, "<div class=\"form-group\">"
                   "<label for=\"password\">Password</label>"
                   "<div class=\"password-wrapper\">"
                   "<input type=\"password\" id=\"password\" name=\"password\">"
                   "<span class=\"toggle-password\""
                   " onclick=\"toggleField('password')\">&#128065;</span>"
                   "</div>"
                   "<div class=\"label-info\">WiFi password</div>"
                   "</div>");

    /* Dynamic fields */
    if (append_extra_fields(&sb, page) < 0)
        goto fail;

    sb_append(&sb, "<button type=\"submit\" class=\"submit-btn\">Save</button>"
                   "<div id=\"statusMessage\" class=\"status\"></div>"
                   "</form></div>");

    if (append_script(&sb, page) < 0)
        goto fail;
    sb_append(&sb, "</body></html>");

    return sb_release(&sb);

fail:
    free(sb.buf);
    return NULL;
}