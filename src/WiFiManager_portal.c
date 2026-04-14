#include "WiFiManager_private.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "sys/param.h"
#include "esp_log.h"
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

        char *html = WiFiManagerPage_Build(&wm->page);
        if (!html)
        {
            ESP_LOGE(TAG, "[CP] Failed to generate config page.");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

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

        char content[WM_PORTAL_BODY_SIZE];
        size_t recv_size = MIN(req->content_len, sizeof(content) - 1);
        int ret = httpd_req_recv(req, content, recv_size);
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
                httpd_resp_send_408(req);
            return ESP_FAIL;
        }
        content[ret] = '\0';
        WiFiManager_ParseUrlencoded(wm, content);

        ESP_LOGI(TAG, "[CP] Form body: %s", content);
        const char *resp_str = "Data received";
        httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(TAG, "[CP] Configuration received");

        if (wm->portal_waiting_task)
            xTaskNotifyGive(wm->portal_waiting_task);

        return ESP_OK;
    }

    default:
        return ESP_OK;
    }
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

httpd_handle_t WiFiManager_StartWebServer(WiFiManager_t *wm)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 6;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 180;

    ESP_LOGI(TAG, "[Captive Portal] - Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
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

void WiFiManager_StopWebServer(WiFiManager_t *wm)
{
    if (wm->server)
    {
        httpd_stop(wm->server);
        wm->server = NULL;
    }
}

void *WiFiManager_StartDNS(esp_ip4_addr_t ip)
{
    return (void *)dns_start(ip);
}

void WiFiManager_StopDNS(void *handle)
{
    dns_stop((dns_server_t *)handle);
}

void WiFiManager_SetCaptivePortalURI(WiFiManager_t *wm)
{
    esp_err_t ret;

    esp_netif_t *netif = wm->netif;
    if (!netif)
    {
        ESP_LOGE(TAG, "[DHCP] netif is null");
        return;
    }

    esp_netif_ip_info_t ip_info;
    ret = esp_netif_get_ip_info(netif, &ip_info);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[DHCP] Failed to get IP info, error: %s", esp_err_to_name(ret));
        return;
    }

    char ip_addr[INET_ADDRSTRLEN];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, INET_ADDRSTRLEN);
    ESP_LOGI(TAG, "[AP Mode] - Set up softAP with IP: %s", ip_addr);

    char captiveportal_uri[32];
    snprintf(captiveportal_uri, sizeof(captiveportal_uri), "http://%s", ip_addr);

    ret = esp_netif_dhcps_stop(netif);
    if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
    {
        ESP_LOGW(TAG, "DHCP stop warning: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI,
                                 captiveportal_uri, strlen(captiveportal_uri));
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "[DHCP] - Failed to set DHCP option: %s", esp_err_to_name(ret));
    else
        ESP_LOGI(TAG, "[DHCP] Captive Portal URI set: %s", captiveportal_uri);

    ret = esp_netif_dhcps_start(netif);
    if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)
        ESP_LOGW(TAG, "Failed to start DHCP: %s", esp_err_to_name(ret));
    else
        ESP_LOGI(TAG, "[DHCP] Server started succesfully");
}