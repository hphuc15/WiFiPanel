#include "WiFiPanel_Priv.h"
#include "WiFiPanel_Defs.h"

#include "wifi_home.h"   /* HOME_HTML  */
#include "wifi_scan.h"   /* SCAN_HTML  */
#include "wifi_config.h" /*  */

#include "lwip/inet.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================
 * String builder  (private, used only for /config dynamic fields)
 * ================================================================ */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} StrBuf;

static int sb_init(StrBuf *sb, size_t cap)
{
    sb->buf = malloc(cap);
    if (!sb->buf) return -1;
    sb->buf[0] = '\0';
    sb->len = 0;
    sb->cap = cap;
    return 0;
}

static int sb_append(StrBuf *sb, const char *s)
{
    if (!sb->buf || !s) return -1;
    size_t slen = strlen(s);
    size_t need = sb->len + slen + 1;
    if (need > sb->cap) {
        size_t new_cap = sb->cap * 2 < need ? need : sb->cap * 2;
        char *tmp = realloc(sb->buf, new_cap);
        if (!tmp) return -1;
        sb->buf = tmp;
        sb->cap = new_cap;
    }
    memcpy(sb->buf + sb->len, s, slen + 1);
    sb->len += slen;
    return 0;
}

static char *sb_release(StrBuf *sb)
{
    char *r = sb->buf;
    sb->buf = NULL;
    sb->len = sb->cap = 0;
    return r;
}

/* ================================================================
 * HTML escape  (used for dynamic /config fields)
 * ================================================================ */

static char *_html_escape(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len * 6 + 1);
    if (!out) return NULL;
    char *p = out;
    for (; *s; s++) {
        switch (*s) {
        case '&':  p += sprintf(p, "&amp;");  break;
        case '<':  p += sprintf(p, "&lt;");   break;
        case '>':  p += sprintf(p, "&gt;");   break;
        case '"':  p += sprintf(p, "&quot;"); break;
        case '\'': p += sprintf(p, "&#39;");  break;
        default:   *p++ = *s;                 break;
        }
    }
    *p = '\0';
    return out;
}

/* ================================================================
 * JSON escape  (used for inject scripts)
 * ================================================================ */

static void _json_escape(char *dst, const char *src, size_t dst_size)
{
    char *end = dst + dst_size - 1;
    while (*src && dst < end) {
        if (*src == '"' || *src == '\\') {
            if (dst + 1 >= end) break;
            *dst++ = '\\';
        }
        *dst++ = *src++;
    }
    *dst = '\0';
}

/* ================================================================
 * Public: param API
 * ================================================================ */

void WiFiPanelPage_Init(WiFiPanel *wp)
{
    memset(&wp->page, 0, sizeof(wp->page));
}

WiFiPanel_Status WiFiPanelPage_AddParam(WiFiPanel *wp,
    const char *id, const char *label, const char *placeholder,
    const char *value, const char *type, bool required)
{
    WiFiPanelPage_t *page = &wp->page;
    if (page->count >= WP_MAX_PARAMS) return WP_ERR_NOMEM;

    WiFiPanelParam_t *p = &page->params[page->count++];
    memset(p, 0, sizeof(*p));
    strncpy(p->id,          id          ? id          : "",     WP_FIELD_LEN - 1);
    strncpy(p->label,       label       ? label       : "",     WP_FIELD_LEN - 1);
    strncpy(p->placeholder, placeholder ? placeholder : "",     WP_FIELD_LEN - 1);
    strncpy(p->value,       value       ? value       : "",     WP_FIELD_LEN - 1);
    strncpy(p->type,        type        ? type        : "text", sizeof(p->type) - 1);
    p->required = required;
    return WP_OK;
}

const char *WiFiPanelPage_GetParam(const WiFiPanel *wp, const char *id)
{
    const WiFiPanelPage_t *page = &wp->page;
    for (size_t i = 0; i < page->count; i++) {
        if (strcmp(page->params[i].id, id) == 0){
            return page->params[i].value;
        }
    }
    return NULL;
}

/* ================================================================
 * Public: /config page builders  (used by portal handlers)
 * ================================================================ */

char *WiFiPanelPage_BuildFields(const WiFiPanel *wp)
{
    StrBuf sb;
    if (sb_init(&sb, 512) < 0) return NULL;

    const WiFiPanelPage_t *page = &wp->page;
    for (size_t i = 0; i < page->count; i++) {
        const WiFiPanelParam_t *p = &page->params[i];

        char *eid  = _html_escape(p->id);
        char *elbl = _html_escape(p->label);
        char *eph  = _html_escape(p->placeholder);
        char *eval = _html_escape(p->value);
        char *etyp = _html_escape(p->type);
        if (!eid || !elbl || !eph || !eval || !etyp) {
            free(eid); free(elbl); free(eph); free(eval); free(etyp);
            free(sb.buf);
            return NULL;
        }

        const char *req = p->required ? " required" : "";

        sb_append(&sb, "<div class=\"form-group\">");
        sb_append(&sb, "<label for=\""); sb_append(&sb, eid); sb_append(&sb, "\">");
        sb_append(&sb, elbl); sb_append(&sb, "</label>");

        if (strcmp(p->type, "password") == 0) {
            sb_append(&sb, "<div class=\"password-wrapper\">"
                           "<input type=\"password\" id=\"");
            sb_append(&sb, eid); sb_append(&sb, "\" name=\""); sb_append(&sb, eid);
            sb_append(&sb, "\" placeholder=\""); sb_append(&sb, eph);
            sb_append(&sb, "\" value=\"");       sb_append(&sb, eval);
            sb_append(&sb, "\""); sb_append(&sb, req); sb_append(&sb, ">");
            sb_append(&sb, "<span class=\"toggle-password\" onclick=\"toggleField('");
            sb_append(&sb, eid);
            sb_append(&sb, "')\">&#128065;</span></div>");
        } else {
            sb_append(&sb, "<input type=\""); sb_append(&sb, etyp);
            sb_append(&sb, "\" id=\"");       sb_append(&sb, eid);
            sb_append(&sb, "\" name=\"");     sb_append(&sb, eid);
            sb_append(&sb, "\" placeholder=\""); sb_append(&sb, eph);
            sb_append(&sb, "\" value=\"");    sb_append(&sb, eval);
            sb_append(&sb, "\""); sb_append(&sb, req); sb_append(&sb, ">");
        }

        if (p->placeholder[0] != '\0') {
            sb_append(&sb, "<div class=\"label-info\">");
            sb_append(&sb, eph);
            sb_append(&sb, "</div>");
        }
        sb_append(&sb, "</div>");

        free(eid); free(elbl); free(eph); free(eval); free(etyp);
    }
    return sb_release(&sb);
}

/* ================================================================
 * Public: /home inject script builder
 *
 * Returns a heap-allocated "<script>window.wifistatus={...};</script>"
 * string to be chunked before HOME_HTML.  Caller must free().
 * ================================================================ */

char *WiFiPanelPage_BuildHome(const WiFiPanel *wp)
{
    bool        connected = WiFiPanel_IsConnected(wp);
    char        ip_str[INET_ADDRSTRLEN]  = "192.168.4.1";
    char        ssid_str[33]             = "--";
    char        host_str[64]             = "sharp-edge.local";
    const char *mode_str                 = "AP";

    esp_netif_t *netif = wp->priv->netif;
    if (netif) {
        esp_netif_ip_info_t ip_info = {0};
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
            inet_ntoa_r(ip_info.ip.addr, ip_str, sizeof(ip_str));
        const char *h = NULL;
        if (esp_netif_get_hostname(netif, &h) == ESP_OK && h)
            snprintf(host_str, sizeof(host_str), "%s", h);
    }

    if (connected) {
        wifi_ap_record_t ap = {0};
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
            snprintf(ssid_str, sizeof(ssid_str), "%s", (char *)ap.ssid);
        mode_str = "STA";
    }

    /* HOME_HTML có 5 placeholder, mỗi cái dài ~12 chars, value tối đa ~64 chars
       → +256 bytes dư thoải mái */
    size_t len = strlen(HOME_HTML);
    char *page = malloc(len + 256);
    if (!page) return NULL;
    memcpy(page, HOME_HTML, len + 1);

    char scan_buf[4];
    snprintf(scan_buf, sizeof(scan_buf), "0");   /* scan count không có ở home */

    #define PH(needle, val) do {                                        \
        char *_p = strstr(page, needle);                                \
        if (_p) {                                                       \
            size_t _nl = strlen(needle), _vl = strlen(val);            \
            memmove(_p + _vl, _p + _nl, strlen(_p + _nl) + 1);        \
            memcpy(_p, val, _vl);                                       \
        }                                                               \
    } while(0)

    PH("%HOSTNAME%",  host_str);
    PH("%IP%",        ip_str);
    PH("%SSID%",      ssid_str);
    PH("%MODE%",      mode_str);
    PH("%CONNECTED%", connected ? "true" : "false");
    PH("%SCANCOUNT%", scan_buf);

    #undef PH
    return page;
}

/* ================================================================
 * Public: /scan inject script builder
 *
 * Runs a blocking WiFi scan and returns a heap-allocated
 * "<script>window.wifiscandata=[...];</script>" string.
 * Caller must free().
 * ================================================================ */

char *WiFiPanelPage_BuildScanInject(const WiFiPanel *wp)
{
    uint16_t max = wp->scan_max_count ? wp->scan_max_count : WP_SCAN_DEFAULT_MAX;
    wifi_ap_record_t *ap_buf = calloc(max, sizeof(wifi_ap_record_t));
    uint16_t fetched = 0;

    if (ap_buf) {
        wifi_scan_config_t cfg = {
            .ssid        = NULL,
            .bssid       = NULL,
            .channel     = 0,
            .show_hidden = false,
            .scan_type   = WIFI_SCAN_TYPE_ACTIVE,
        };
        if (esp_wifi_scan_start(&cfg, true) == ESP_OK) {
            uint16_t found = 0;
            esp_wifi_scan_get_ap_num(&found);
            fetched = found < max ? found : max;
            if (esp_wifi_scan_get_ap_records(&fetched, ap_buf) != ESP_OK)
                fetched = 0;
        }
    }

    /* each entry: {"ssid":"<64>","rssi":-100,"enc":false},  ~90 bytes */
    size_t sz = 32 + (size_t)fetched * 90 + 4;
    char *out = calloc(1, sz);
    if (!out) { free(ap_buf); return NULL; }

    char *p   = out;
    char *end = out + sz - 1;
    p += snprintf(p, end - p, "window.wifiscandata=[");

    for (uint16_t i = 0; i < fetched; i++) {
        if (ap_buf[i].ssid[0] == '\0') continue;
        char safe[65] = {0};
        _json_escape(safe, (char *)ap_buf[i].ssid, sizeof(safe));
        bool enc = (ap_buf[i].authmode != WIFI_AUTH_OPEN);
        p += snprintf(p, end - p, "%s{\"ssid\":\"%s\",\"rssi\":%d,\"enc\":%s}",
                      i > 0 ? "," : "",
                      safe, ap_buf[i].rssi, enc ? "true" : "false");
    }
    snprintf(p, end - p, "];");

    free(ap_buf);
    return out;
}

char *WiFiPanelPage_BuildConfig(const WiFiPanel *wp)
{
    const WiFiPanelPage_t *page = &wp->page;

    /* ── Build window.wifiparam_schema JSON array ── */
    StrBuf sb;
    if (sb_init(&sb, 1024) < 0) return NULL;

    sb_append(&sb, "<script>");
    sb_append(&sb, "window.wifiparam_schema=[");

    for (size_t i = 0; i < page->count; i++) {
        const WiFiPanelParam_t *p = &page->params[i];

        /* json-escape các string fields */
        char eid[WP_FIELD_LEN * 2], elbl[WP_FIELD_LEN * 2];
        char eph[WP_FIELD_LEN * 2], eval[WP_FIELD_LEN * 2];
        char etyp[16];
        _json_escape(eid,  p->id,          sizeof(eid));
        _json_escape(elbl, p->label,       sizeof(elbl));
        _json_escape(eph,  p->placeholder, sizeof(eph));
        _json_escape(eval, p->value,       sizeof(eval));

        /* map HTML input type → badge type cho UI */
        const char *badge_type;
        if      (strcmp(p->type, "password") == 0) badge_type = "pass";
        else if (strcmp(p->type, "number")   == 0) badge_type = "int";
        else if (strcmp(p->type, "checkbox") == 0) badge_type = "bool";
        else if (strcmp(p->type, "select")   == 0) badge_type = "sel";
        else                                       badge_type = "str";
        _json_escape(etyp, badge_type, sizeof(etyp));

        char entry[1150];
        snprintf(entry, sizeof(entry),
            "%s{\"key\":\"%s\",\"type\":\"%s\",\"label\":\"%s\","
            "\"default\":\"%s\",\"desc\":\"%s\"}",
            i > 0 ? "," : "",
            eid, etyp, elbl, eval, eph   /* desc reuse placeholder */
        );
        if (sb_append(&sb, entry) < 0) { free(sb.buf); return NULL; }
    }

    sb_append(&sb, "];");

    /* ── window.wifiparam_values — current saved values ── */
    sb_append(&sb, "window.wifiparam_values={");
    for (size_t i = 0; i < page->count; i++) {
        const WiFiPanelParam_t *p = &page->params[i];
        char eid[WP_FIELD_LEN * 2], eval[WP_FIELD_LEN * 2];
        _json_escape(eid,  p->id,    sizeof(eid));
        _json_escape(eval, p->value, sizeof(eval));

        char entry[WP_FIELD_LEN * 4 + 8];
        snprintf(entry, sizeof(entry), "%s\"%s\":\"%s\"",
                 i > 0 ? "," : "", eid, eval);
        if (sb_append(&sb, entry) < 0) { free(sb.buf); return NULL; }
    }
    sb_append(&sb, "};");
    sb_append(&sb, "</script>");

    char *inject = sb_release(&sb);
    if (!inject) return NULL;

    /* ── Concat: inject + CONFIG_HTML ── */
    size_t inject_len = strlen(inject);
    size_t html_len   = strlen(CONFIG_HTML);
    char  *out        = malloc(inject_len + html_len + 1);
    if (!out) { free(inject); return NULL; }

    memcpy(out,              inject,    inject_len);
    memcpy(out + inject_len, CONFIG_HTML, html_len + 1);
    free(inject);
    return out;
}