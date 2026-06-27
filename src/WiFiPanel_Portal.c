#include "WiFiPanel_Priv.h"
#include "WiFiPanel_Defs.h"

#include "wifi_home.h"
#include "wifi_scan.h"
#include "wifi_ota.h"

#include "esp_ota_ops.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include "sys/param.h"
#include "netinet/in.h"
#include "esp_log.h"
#include "esp_event.h"

#include <string.h>
#include <errno.h>

static const char *TAG      = "[WP][PORTAL]";
static const char *TAG_DNS  = "[WP][DNS]";
static const char *TAG_DHCP = "[WP][DHCP]";

/* OTA definitions */
#define WP_OTA_RECV_BUF_SIZE    2048
#define WP_OTA_TAIL_MARGIN      64

/* ================================================================
 * Forward declarations (WiFiPanel_Page.c)
 * ================================================================ */

char       *WiFiPanelPage_BuildHome(const WiFiPanel *wp);
char       *WiFiPanelPage_BuildScanInject(const WiFiPanel *wp);
char       *WiFiPanelPage_BuildConfig(const WiFiPanel *wp);
char       *WiFiPanelPage_BuildFields(const WiFiPanel *wp);
const char *WiFiPanelPage_GetHead(void);
const char *WiFiPanelPage_GetTail(void);

/* ================================================================
 * DNS - Constants
 * ================================================================ */

#define WP_DNS_PORT             53
#define WP_DNS_MAX_PACKET_SIZE  512     /* RFC 1035 §2.3.4 UDP limit        */
#define WP_DNS_LABEL_MAX_LEN    63      /* max length of a single label     */
#define WP_DNS_NAME_MAX_LEN     255     /* max total QNAME length           */
#define WP_DNS_TTL_DEFAULT      60      /* seconds - low for captive portal */
#define WP_DNS_HEADER_SIZE      12      /* fixed header size in bytes       */

/* Record types */
#define WP_DNS_TYPE_A           1u
#define WP_DNS_TYPE_AAAA        28u
#define WP_DNS_TYPE_ANY         255u
#define WP_DNS_CLASS_IN         1u

/* Flags masks */
#define WP_DNS_HEADER_FLAG_QR_MASK      (0x8000u)   /* bit 15       */
#define WP_DNS_HEADER_FLAG_OPCODE_MASK  (0x7800u)   /* bits [14:11] */
#define WP_DNS_HEADER_FLAG_AA_MASK      (0x0400u)   /* bit 10       */
#define WP_DNS_HEADER_FLAG_TC_MASK      (0x0200u)   /* bit 9        */
#define WP_DNS_HEADER_FLAG_RD_MASK      (0x0100u)   /* bit 8        */
#define WP_DNS_HEADER_FLAG_RA_MASK      (0x0080u)   /* bit 7        */
#define WP_DNS_HEADER_FLAG_ZERO_MASK    (0x0070u)   /* bits [6:4]   */
#define WP_DNS_HEADER_FLAG_RCODE_MASK   (0x000Fu)   /* bits [3:0]   */

/* Flag helpers */
#define WP_DNS_FLAG_GET_QR(f)       (((f) & WP_DNS_HEADER_FLAG_QR_MASK) >> 15)
#define WP_DNS_FLAG_GET_OPCODE(f)   (((f) & WP_DNS_HEADER_FLAG_OPCODE_MASK) >> 11)
#define WP_DNS_FLAG_GET_AA(f)       (((f) & WP_DNS_HEADER_FLAG_AA_MASK) >> 10)
#define WP_DNS_FLAG_GET_RD(f)       (((f) & WP_DNS_HEADER_FLAG_RD_MASK) >> 8)
#define WP_DNS_FLAG_GET_RCODE(f)    ((f) & WP_DNS_HEADER_FLAG_RCODE_MASK)

#define WP_DNS_FLAG_SET_QR(f)       ((f) | WP_DNS_HEADER_FLAG_QR_MASK)
#define WP_DNS_FLAG_SET_AA(f)       ((f) | WP_DNS_HEADER_FLAG_AA_MASK)
#define WP_DNS_FLAG_CLR_RCODE(f)    ((f) & ~WP_DNS_HEADER_FLAG_RCODE_MASK)
#define WP_DNS_FLAG_SET_RCODE(f, r) (WP_DNS_FLAG_CLR_RCODE(f) | ((r) & WP_DNS_HEADER_FLAG_RCODE_MASK))

/* RCODE values */
#define WP_DNS_RCODE_NOERROR    0u
#define WP_DNS_RCODE_FORMERR    1u
#define WP_DNS_RCODE_SERVFAIL   2u
#define WP_DNS_RCODE_NXDOMAIN   3u
#define WP_DNS_RCODE_REFUSED    5u

/* Pointer compression: 2 high bits == 0b11, offset = WP_DNS_HEADER_SIZE */
#define WP_DNS_PTR_OFFSET       WP_DNS_HEADER_SIZE
#define WP_DNS_PTR_COMPRESSION  (0xC000u | WP_DNS_PTR_OFFSET)
#define WP_DNS_IS_PTR(byte)     (((byte) & 0xC0) == 0xC0)

/* ================================================================
 * DNS - Types
 * ================================================================ */

typedef enum {
    WP_DNS_OK              =  0,
    WP_DNS_ERR_SOCKET      = -1,    /**< socket() failed                      */
    WP_DNS_ERR_BIND        = -2,    /**< bind() failed                        */
    WP_DNS_ERR_RECV        = -3,    /**< recvfrom() failed                    */
    WP_DNS_ERR_SEND        = -4,    /**< sendto() failed                      */
    WP_DNS_ERR_TRUNCATED   = -5,    /**< packet too short to parse            */
    WP_DNS_ERR_NOT_QUERY   = -6,    /**< QR bit is set - received a response  */
    WP_DNS_ERR_UNSUPPORTED = -7,    /**< OPCODE != 0 (not a standard query)   */
    WP_DNS_ERR_BUILD       = -8,    /**< failed to build response packet      */
    WP_DNS_ERR_INVALID_IP  = -9,    /**< inet_pton() failed on redirect_ip    */
} WiFiPanel_DNS_Status;

typedef struct __attribute__((packed)) {
    uint16_t id;            /**< Transaction ID - must be copied to response    */
    uint16_t flags;         /**< | QR[15] | Opcode[14:11] | AA[10] | TC[9] | RD[8] | RA[7] | Zero[6:4] | RCODE[3:0] | */
    uint16_t qd_count;      /**< Number of entries in the Question section      */
    uint16_t an_count;      /**< Number of resource records in the Answer section */
    uint16_t ns_count;      /**< Number of name server records in Authority     */
    uint16_t ar_count;      /**< Number of records in the Additional section    */
} WiFiPanel_DNS_Header;

typedef struct __attribute__((packed)) {
    /* QNAME is variable-length and precedes this struct in the raw buffer  */
    uint16_t qtype;         /**< Query type  - WP_DNS_TYPE_A, WP_DNS_TYPE_ANY, etc. */
    uint16_t qclass;        /**< Query class - WP_DNS_CLASS_IN                      */
} WiFiPanel_DNS_Question;

typedef struct __attribute__((packed)) {
    /* NAME is variable-length and precedes this struct in the raw buffer   */
    uint16_t type;          /**< Record type  - WP_DNS_TYPE_A                  */
    uint16_t rr_class;      /**< Record class - WP_DNS_CLASS_IN                */
    uint32_t ttl;           /**< Time to live in seconds (network byte order)  */
    uint16_t rdlength;      /**< Length of RDATA in bytes (4 for IPv4)         */
    /* RDATA follows immediately after this struct in the raw buffer        */
} WiFiPanel_DNS_ResourceRecord;

typedef struct __attribute__((packed)) {
    uint16_t name_ptr;      /**< Pointer compression - always WP_DNS_PTR_COMPRESSION */
    uint16_t type;
    uint16_t rr_class;
    uint32_t ttl;
    uint16_t rdlength;
    uint32_t rdata;         /**< IPv4 address in network byte order */
} WiFiPanel_DNS_Answer;

/** DNS server state. Zero-initialize, then call WiFiPanel_DNS_Init(). */
typedef struct {
    int      sockfd;                            /**< UDP socket fd, -1 if not open.      */
    uint32_t redirect_ip;                       /**< Reply IP for A-queries, NBO.        */
    uint16_t port;                              /**< UDP port to listen on (usually 53). */
    uint8_t  buf[WP_DNS_MAX_PACKET_SIZE];       /**< Reusable receive buffer, 512 B.     */
    bool     running;                           /**< true after Start(), false after Stop(). */
} WiFiPanel_DNS_t;

/* ================================================================
 * DNS - Internal helpers
 * ================================================================ */

static const char *WiFiPanel_DNS_StatusToStr(WiFiPanel_DNS_Status status)
{
    switch (status) {
        case WP_DNS_OK:              return "WP_DNS_OK";
        case WP_DNS_ERR_SOCKET:      return "WP_DNS_ERR_SOCKET";
        case WP_DNS_ERR_BIND:        return "WP_DNS_ERR_BIND";
        case WP_DNS_ERR_RECV:        return "WP_DNS_ERR_RECV";
        case WP_DNS_ERR_SEND:        return "WP_DNS_ERR_SEND";
        case WP_DNS_ERR_TRUNCATED:   return "WP_DNS_ERR_TRUNCATED";
        case WP_DNS_ERR_NOT_QUERY:   return "WP_DNS_ERR_NOT_QUERY";
        case WP_DNS_ERR_UNSUPPORTED: return "WP_DNS_ERR_UNSUPPORTED";
        case WP_DNS_ERR_BUILD:       return "WP_DNS_ERR_BUILD";
        case WP_DNS_ERR_INVALID_IP:  return "WP_DNS_ERR_INVALID_IP";
        default:                     return "WP_DNS_ERR_UNKNOWN";
    }
}

/**
 * @brief Skip a QNAME in a raw DNS buffer and return the offset after it.
 *
 * Handles label sequences terminated by 0x00.
 * Pointer compression (0xC0 prefix) counts as a 2-byte leaf - we do not
 * follow the pointer because queries from captive-portal clients virtually
 * never use compression.
 *
 * @param  buf  Raw packet buffer.
 * @param  len  Total packet length.
 * @param  pos  Starting offset (first byte of QNAME).
 * @return Offset of the byte AFTER the QNAME, or 0 on error.
 */
static size_t _dns_skip_qname(const uint8_t *buf, size_t len, size_t pos)
{
    size_t limit = pos + WP_DNS_NAME_MAX_LEN + 1;

    while (pos < len && pos < limit) {
        uint8_t b = buf[pos];

        if (b == 0x00) {
            return pos + 1;                 /* null terminator - stop */
        }
        if (WP_DNS_IS_PTR(b)) {
            return pos + 2;                 /* compression pointer - 2-byte leaf */
        }
        if (b > WP_DNS_LABEL_MAX_LEN) {
            ESP_LOGE(TAG_DNS, "label length %u exceeds max %u", b, WP_DNS_LABEL_MAX_LEN);
            return 0;
        }
        pos += 1 + (size_t)b;
    }

    ESP_LOGE(TAG_DNS, "QNAME not terminated within packet bounds");
    return 0;
}

/**
 * @brief Validate a raw DNS query packet and locate the end of the Question section.
 *
 * @param  buf           Raw packet buffer
 * @param  len           Packet length in bytes
 * @param  out_hdr       If not NULL, the parsed header (host byte order) is written here
 * @param  out_qname_end If not NULL, offset of the first byte AFTER the Question section
 * @return Pointer to the byte after the Question section on success, NULL on error.
 */
static const uint8_t *WiFiPanel_DNS_ParseQuery(const uint8_t *buf, size_t len, WiFiPanel_DNS_Header *out_hdr, size_t *out_qname_end)
{
    if (!buf || len < WP_DNS_HEADER_SIZE) {
        ESP_LOGE(TAG_DNS, "packet too short (%zu bytes)", len);
        return NULL;
    }

    /* Parse header (wire → host byte order) */
    WiFiPanel_DNS_Header hdr;
    memcpy(&hdr, buf, WP_DNS_HEADER_SIZE);
    hdr.id       = ntohs(hdr.id);
    hdr.flags    = ntohs(hdr.flags);
    hdr.qd_count = ntohs(hdr.qd_count);
    hdr.an_count = ntohs(hdr.an_count);
    hdr.ns_count = ntohs(hdr.ns_count);
    hdr.ar_count = ntohs(hdr.ar_count);

    if (WP_DNS_FLAG_GET_QR(hdr.flags)) {
        ESP_LOGD(TAG_DNS, "ignoring response packet (QR=1)");
        return NULL;
    }
    if (WP_DNS_FLAG_GET_OPCODE(hdr.flags) != 0) {
        ESP_LOGE(TAG_DNS, "unsupported OPCODE %u", WP_DNS_FLAG_GET_OPCODE(hdr.flags));
        return NULL;
    }
    if (hdr.qd_count == 0) {
        ESP_LOGE(TAG_DNS, "no questions in query");
        return NULL;
    }

    size_t pos = _dns_skip_qname(buf, len, WP_DNS_HEADER_SIZE);
    if (pos == 0){
        return NULL;
    }

    if (pos + 4 > len) {
        ESP_LOGE(TAG_DNS, "packet too short for QTYPE/QCLASS (pos=%zu len=%zu)", pos, len);
        return NULL;
    }
    pos += 4; /* consume QTYPE + QCLASS */

    if (out_hdr)       *out_hdr       = hdr;
    if (out_qname_end) *out_qname_end = pos;
    return buf + pos;
}

/**
 * @brief Build a DNS response packet into out_buf.
 *
 * @param  query_buf   Original query packet
 * @param  qname_end   Offset returned by WiFiPanel_DNS_ParseQuery
 * @param  redirect_ip IPv4 address to place in RDATA (network byte order)
 * @param  out_buf     Output buffer - must be at least WP_DNS_MAX_PACKET_SIZE bytes
 * @param  out_len     Number of bytes written to out_buf
 * @return WP_DNS_OK or WP_DNS_ERR_BUILD
 */
static WiFiPanel_DNS_Status WiFiPanel_DNS_BuildResponse(const uint8_t *query_buf, size_t qname_end, uint32_t redirect_ip, uint8_t *out_buf, size_t *out_len)
{
    if (!query_buf || !out_buf || !out_len){
        return WP_DNS_ERR_BUILD;
    }

    if (qname_end < WP_DNS_HEADER_SIZE + 4) {
        ESP_LOGE(TAG_DNS, "qname_end=%zu looks wrong", qname_end);
        return WP_DNS_ERR_BUILD;
    }

    /* Determine type from question section:
     * Layout: [QNAME...][QTYPE 2B][QCLASS 2B]  → QTYPE starts at qname_end-4 */
    uint16_t qtype;
    memcpy(&qtype, query_buf + qname_end - 4, sizeof(uint16_t));
    qtype = ntohs(qtype);

    const bool answer_with_a = (qtype == WP_DNS_TYPE_A || qtype == WP_DNS_TYPE_ANY);

    size_t question_len = qname_end - WP_DNS_HEADER_SIZE;
    size_t resp_len     = WP_DNS_HEADER_SIZE + question_len +
                          (answer_with_a ? sizeof(WiFiPanel_DNS_Answer) : 0);

    if (resp_len > WP_DNS_MAX_PACKET_SIZE) {
        ESP_LOGE(TAG_DNS, "response would exceed WP_DNS_MAX_PACKET_SIZE (%zu > %d)", resp_len, WP_DNS_MAX_PACKET_SIZE);
        return WP_DNS_ERR_BUILD;
    }

    /* Build response header */
    const WiFiPanel_DNS_Header *req_hdr = (const WiFiPanel_DNS_Header *)query_buf;
    uint16_t flags = ntohs(req_hdr->flags);
    flags = WP_DNS_FLAG_SET_QR(flags);
    flags = WP_DNS_FLAG_SET_AA(flags);
    flags = WP_DNS_FLAG_SET_RCODE(flags, WP_DNS_RCODE_NOERROR);

    WiFiPanel_DNS_Header resp_hdr;
    resp_hdr.id       = req_hdr->id;           /* keep wire order */
    resp_hdr.flags    = htons(flags);
    resp_hdr.qd_count = htons(1);
    resp_hdr.an_count = htons(answer_with_a ? 1u : 0u);
    resp_hdr.ns_count = 0;
    resp_hdr.ar_count = 0;

    /* Write to output buffer */
    uint8_t *p = out_buf;

    memcpy(p, &resp_hdr, WP_DNS_HEADER_SIZE);
    p += WP_DNS_HEADER_SIZE;

    /* Copy the Question section verbatim (QNAME + QTYPE + QCLASS) */
    memcpy(p, query_buf + WP_DNS_HEADER_SIZE, question_len);
    p += question_len;

    if (answer_with_a) {
        WiFiPanel_DNS_Answer ans;
        ans.name_ptr = htons((uint16_t)WP_DNS_PTR_COMPRESSION);
        ans.type     = htons(WP_DNS_TYPE_A);
        ans.rr_class = htons(WP_DNS_CLASS_IN);
        ans.ttl      = htonl(WP_DNS_TTL_DEFAULT);
        ans.rdlength = htons(4u);
        ans.rdata    = redirect_ip;             /* already NBO */
        memcpy(p, &ans, sizeof(WiFiPanel_DNS_Answer));
        p += sizeof(WiFiPanel_DNS_Answer);
    }

    *out_len = (size_t)(p - out_buf);
    return WP_DNS_OK;
}

/**
 * @brief Initialise WiFiPanel_DNS_t struct. Does not open a socket.
 *
 * @param  srv         Pointer to an uninitialised WiFiPanel_DNS_t
 * @param  redirect_ip IPv4 string, e.g. "192.168.4.1"
 * @param  port        UDP port to bind, usually WP_DNS_PORT (53)
 * @return WP_DNS_OK or WP_DNS_ERR_INVALID_IP
 */
static WiFiPanel_DNS_Status WiFiPanel_DNS_Init(WiFiPanel_DNS_t *srv, const char *redirect_ip, uint16_t port)
{
    if (!srv || !redirect_ip){
        return WP_DNS_ERR_INVALID_IP;
    }

    memset(srv, 0, sizeof(*srv));
    srv->sockfd  = -1;
    srv->port    = port;
    srv->running = false;

    struct in_addr addr;
    if (inet_pton(AF_INET, redirect_ip, &addr) != 1) {
        ESP_LOGE(TAG_DNS, "inet_pton failed for \"%s\"", redirect_ip);
        return WP_DNS_ERR_INVALID_IP;
    }
    srv->redirect_ip = addr.s_addr;            /* NBO from pton */
    ESP_LOGI(TAG_DNS, "init: redirect=%s port=%u", redirect_ip, port);
    return WP_DNS_OK;
}

/**
 * @brief Create UDP socket and bind to the configured port.
 *        Must be called after WiFiPanel_DNS_Init() and before WiFiPanel_DNS_Poll().
 *
 * @return WP_DNS_OK, WP_DNS_ERR_SOCKET, or WP_DNS_ERR_BIND
 */
static WiFiPanel_DNS_Status WiFiPanel_DNS_Start(WiFiPanel_DNS_t *srv)
{
    if (!srv){
        return WP_DNS_ERR_SOCKET;
    }

    srv->sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (srv->sockfd < 0) {
        ESP_LOGE(TAG_DNS, "socket() failed: %d", errno);
        return WP_DNS_ERR_SOCKET;
    }

    /* Allow immediate re-bind after restart */
    int yes = 1;
    setsockopt(srv->sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    /* Non-blocking so Poll() returns immediately when no packet is waiting */
    int fl = fcntl(srv->sockfd, F_GETFL, 0);
    fcntl(srv->sockfd, F_SETFL, fl | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(srv->port);

    if (bind(srv->sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG_DNS, "bind() failed on port %u: %d", srv->port, errno);
        close(srv->sockfd);
        srv->sockfd = -1;
        return WP_DNS_ERR_BIND;
    }

    srv->running = true;
    ESP_LOGI(TAG_DNS, "listening on UDP port %u", srv->port);
    return WP_DNS_OK;
}

/**
 * @brief Process one pending DNS packet (non-blocking).
 *        Call this repeatedly from a loop or FreeRTOS task.
 *
 * @return WP_DNS_OK if a packet was handled or no packet was waiting.
 *         WP_DNS_ERR_RECV / WP_DNS_ERR_SEND on socket errors.
 */
static WiFiPanel_DNS_Status WiFiPanel_DNS_Poll(WiFiPanel_DNS_t *srv)
{
    if (!srv || srv->sockfd < 0){
        return WP_DNS_ERR_RECV;
    }

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);

    ssize_t recv_len = recvfrom(srv->sockfd, srv->buf, WP_DNS_MAX_PACKET_SIZE, 0, (struct sockaddr *)&client, &client_len);
    if (recv_len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK){
            return WP_DNS_OK; /* no packet - normal */
        }
        ESP_LOGE(TAG_DNS, "recvfrom() error: %d", errno);
        return WP_DNS_ERR_RECV;
    }

    ESP_LOGD(TAG_DNS, "received %zd bytes from %s:%u", recv_len, inet_ntoa(client.sin_addr), ntohs(client.sin_port));

    /* Parse */
    WiFiPanel_DNS_Header hdr;
    size_t qname_end;
    if (!WiFiPanel_DNS_ParseQuery(srv->buf, (size_t)recv_len, &hdr, &qname_end))
        return WP_DNS_OK; /* bad/unsupported packet - discard silently */

    /* Build response */
    uint8_t resp[WP_DNS_MAX_PACKET_SIZE];
    size_t  resp_len = 0;
    WiFiPanel_DNS_Status status = WiFiPanel_DNS_BuildResponse(srv->buf, qname_end, srv->redirect_ip, resp, &resp_len);
    if (status != WP_DNS_OK) {
        ESP_LOGE(TAG_DNS, "WiFiPanel_DNS_BuildResponse: %s", WiFiPanel_DNS_StatusToStr(status));
        return status;
    }

    /* Send */
    ssize_t sent = sendto(srv->sockfd, resp, resp_len, 0, (struct sockaddr *)&client, client_len);
    if (sent < 0) {
        ESP_LOGE(TAG_DNS, "sendto() error: %d", errno);
        return WP_DNS_ERR_SEND;
    }

    ESP_LOGD(TAG_DNS, "sent %zd-byte response (id=0x%04X)", sent, hdr.id);
    return WP_DNS_OK;
}

/**
 * @brief Close socket and reset server state.
 */
static void WiFiPanel_DNS_Stop(WiFiPanel_DNS_t *srv)
{
    if (!srv){
        return;
    }
    if (srv->sockfd >= 0) {
        close(srv->sockfd);
        srv->sockfd = -1;
    }
    srv->running = false;
    ESP_LOGI(TAG_DNS, "stopped");
}

/* ================================================================
 * DNS server (public wrappers - declared in WiFiPanel_Priv.h)
 * ================================================================ */

typedef struct {
    WiFiPanel_DNS_t dns;
    TaskHandle_t    task;
} _WiFiPanel_DNS_Handle_t;

static void _dns_task(void *arg)
{
    WiFiPanel_DNS_t *dns = (WiFiPanel_DNS_t *)arg;
    ESP_LOGI(TAG_DNS, "Task started on port %u", dns->port);
    while (1) {
        WiFiPanel_DNS_Status err = WiFiPanel_DNS_Poll(dns);
        if (err != WP_DNS_OK)
            ESP_LOGE(TAG_DNS, "Poll error: %s", WiFiPanel_DNS_StatusToStr(err));
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void *WiFiPanel_StartDNS(esp_ip4_addr_t ip)
{
    char ip_str[INET_ADDRSTRLEN];
    inet_ntoa_r(ip.addr, ip_str, sizeof(ip_str));

    _WiFiPanel_DNS_Handle_t *h = calloc(1, sizeof(_WiFiPanel_DNS_Handle_t));
    if (!h){
        return NULL;
    }

    if (WiFiPanel_DNS_Init(&h->dns, ip_str, WP_DNS_PORT) != WP_DNS_OK ||
        WiFiPanel_DNS_Start(&h->dns) != WP_DNS_OK) {
        free(h);
        return NULL;
    }

    if (xTaskCreate(_dns_task, "dns_srv", 4096, &h->dns, 5, &h->task) != pdPASS) {
        WiFiPanel_DNS_Stop(&h->dns);
        free(h);
        return NULL;
    }

    ESP_LOGI(TAG_DNS, "Started → %s:%u", ip_str, WP_DNS_PORT);
    return h;
}

void WiFiPanel_StopDNS(void *handle)
{
    if (!handle){
        return;
    }
    _WiFiPanel_DNS_Handle_t *h = handle;
    if (h->task) { vTaskDelete(h->task); h->task = NULL; }
    WiFiPanel_DNS_Stop(&h->dns);
    free(h);
    ESP_LOGI(TAG_DNS, "Stopped");
}

/* ================================================================
 * Helpers
 * ================================================================ */

static void _url_decode(char *dst, const char *src, size_t dst_size)
{
    char *end = dst + dst_size - 1;
    while (*src && dst < end) {
        if (*src == '+') {
            *dst++ = ' '; src++;
        } else if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static esp_err_t _parse_post(WiFiPanel *wp, char *body)
{
    char *sp_pair, *sp_kv;
    char *pair = strtok_r(body, "&", &sp_pair);
    while (pair) {
        char *key = strtok_r(pair, "=", &sp_kv);
        char *val = strtok_r(NULL, "=", &sp_kv);
        if (key) {
            char dk[WP_FIELD_LEN] = {0};
            char dv[WP_FIELD_LEN] = {0};
            _url_decode(dk, key,            sizeof(dk));
            _url_decode(dv, val ? val : "", sizeof(dv));

            if (strcmp(dk, "ssid") == 0) {
                strncpy((char *)wp->sta_config.ssid, dv, sizeof(wp->sta_config.ssid) - 1);
            } else if (strcmp(dk, "password") == 0) {
                strncpy((char *)wp->sta_config.password, dv, sizeof(wp->sta_config.password) - 1);
                wp->sta_config.threshold.authmode = WIFI_AUTH_OPEN;
            } else {
                for (size_t i = 0; i < wp->page.count; i++) {
                    WiFiPanelParam_t *p = &wp->page.params[i];
                    if (strcmp(p->id, dk) == 0) {
                        snprintf(p->value, WP_FIELD_LEN, "%s", dv);
                        break;
                    }
                }
            }
        }
        pair = strtok_r(NULL, "&", &sp_pair);
    }
    return ESP_OK;
}

static esp_err_t _recv_body(httpd_req_t *req, char *buf, size_t buf_size)
{
    if (req->content_len == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    size_t to_read = MIN(req->content_len, buf_size - 1);
    int n = httpd_req_recv(req, buf, to_read);
    if (n <= 0) {
        if (n == HTTPD_SOCK_ERR_TIMEOUT) httpd_resp_send_408(req);
        return ESP_FAIL;
    }
    buf[n] = '\0';
    return ESP_OK;
}

/* ================================================================
 * Endpoint handlers
 * ================================================================ */

/* GET / - home dashboard */
static esp_err_t _handler_home_get(httpd_req_t *req)
{
    WiFiPanel *wp = req->user_ctx;
    if (!wp){
        return ESP_ERR_INVALID_ARG;
    }
    
    char *page = WiFiPanelPage_BuildHome(wp);
    if (!page) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");

    #define CHUNK_SIZE  512
    const char *p   = page;
    size_t      rem = strlen(page);
    esp_err_t   ret = ESP_OK;

    while (rem > 0) {
        size_t n = rem < CHUNK_SIZE ? rem : CHUNK_SIZE;
        if (httpd_resp_send_chunk(req, p, (ssize_t)n) != ESP_OK) {
            ret = ESP_FAIL;
            break;
        }
        p   += n;
        rem -= n;
    }
    httpd_resp_send_chunk(req, NULL, 0);

    free(page);
    return ret;
}

/* POST / - receive ssid+password from /scan form, notify waiting task */
static esp_err_t _handler_wifi_post(httpd_req_t *req)
{
    WiFiPanel *wp = req->user_ctx;
    if (!wp){
        return ESP_ERR_INVALID_ARG;
    }

    char body[WP_PORTAL_BODY_SIZE];
    if (_recv_body(req, body, sizeof(body)) != ESP_OK){
        return ESP_FAIL;
    }

    _parse_post(wp, body);
    ESP_LOGI(TAG, "WiFi POST: ssid='%s'", (char *)wp->sta_config.ssid);

    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

    if (wp->priv->portal_waiting_task){
        xTaskNotifyGive(wp->priv->portal_waiting_task);
    } else{
        ESP_LOGW(TAG, "portal_waiting_task is NULL");
    }

    return ESP_OK;
}

/* GET /scan - WiFi scan page */
static esp_err_t _handler_scan_get(httpd_req_t *req)
{
    WiFiPanel *wp = req->user_ctx;
    if (!wp){
        return ESP_ERR_INVALID_ARG;
    }

    char *inject = WiFiPanelPage_BuildScanInject(wp);
    if (!inject) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, WP_PAGE_PART1, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, inject,         HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, WP_PAGE_PART2,  HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);
    free(inject);
    return ESP_OK;
}

/* GET /config - user param form (future) */
static esp_err_t _handler_config_get(httpd_req_t *req)
{
    WiFiPanel *wp = req->user_ctx;
    if (!wp){
        return ESP_ERR_INVALID_ARG;
    }

    /* No custom parameters configured -> redirect to home */
    if (wp->page.count == 0) {
        httpd_resp_set_status(req, "302 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    char *page = WiFiPanelPage_BuildConfig(wp);
    if (!page) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
    free(page);
    return ESP_OK;
}

static esp_err_t _handler_configsave_post(httpd_req_t *req)
{
    WiFiPanel *wp = req->user_ctx;
    if (!wp){
        return ESP_ERR_INVALID_ARG;
    }

    char body[WP_PORTAL_BODY_SIZE];
    if (_recv_body(req, body, sizeof(body)) != ESP_OK){
        return ESP_FAIL;
    }

    /* _parse_post() already updates page.params for matching keys */
    _parse_post(wp, body);
    ESP_LOGI(TAG, "/configsave: %u params updated", (unsigned)wp->page.count);

    /* Return 200 OK without redirect - frontend JS handles UX */
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

    /* Notify waiting task if the application needs config-save events */
    if (wp->priv->portal_waiting_task)
        xTaskNotifyGive(wp->priv->portal_waiting_task);

    return ESP_OK;
}

/* GET /reset - erase config and reboot */
static esp_err_t _handler_reset_get(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, "Resetting...", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

 
/* Extracts the boundary string (without leading "--") from the
 * Content-Type header, e.g. "multipart/form-data; boundary=----XYZ".
 * Returns ESP_OK and writes into out (NUL-terminated) or ESP_FAIL. */
static esp_err_t _ota_get_boundary(httpd_req_t *req, char *out, size_t out_size)
{
    char ctype[160];
    if (httpd_req_get_hdr_value_str(req, "Content-Type", ctype, sizeof(ctype)) != ESP_OK) {
        return ESP_FAIL;
    }
 
    const char *key = "boundary=";
    char *p = strstr(ctype, key);
    if (!p) {
        return ESP_FAIL;
    }
    p += strlen(key);
 
    /* Boundary value may be quoted; strip quotes if present */
    if (*p == '"') {
        p++;
        char *end = strchr(p, '"');
        if (end) *end = '\0';
    }
 
    snprintf(out, out_size, "--%s", p); /* the wire boundary is "--" + value */
    return ESP_OK;
}
 
/* Locates the first occurrence of "\r\n\r\n" within buf[0..len).
 * Returns offset of the byte AFTER it, or -1 if not found. */
static int _find_header_end(const char *buf, int len)
{
    for (int i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') {
            return i + 4;
        }
    }
    return -1;
}

static esp_err_t _handler_ota_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, OTA_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* POST /ota - receive firmware via multipart/form-data and flash it */
static esp_err_t _handler_ota_post(httpd_req_t *req)
{
    WiFiPanel *wp = req->user_ctx;
    if (!wp) {
        return ESP_ERR_INVALID_ARG;
    }
 
    if (req->content_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
 
    char boundary[96];
    if (_ota_get_boundary(req, boundary, sizeof(boundary)) != ESP_OK) {
        ESP_LOGE(TAG, "/ota: missing or malformed boundary");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Content-Type");
        return ESP_FAIL;
    }
    size_t boundary_len = strlen(boundary);
    ESP_LOGI(TAG, "/ota: boundary='%s' content_len=%d", boundary, req->content_len);
 
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "/ota: writing to partition '%s' at 0x%lx",
             update_partition->label, (unsigned long)update_partition->address);
 
    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_begin failed");
        return ESP_FAIL;
    }
 
    char  *buf            = malloc(WP_OTA_RECV_BUF_SIZE);
    bool   header_skipped = false;
    bool   write_failed    = false;
    int    remaining       = req->content_len;
    size_t total_written   = 0;

    char  *tail        = malloc(WP_OTA_TAIL_MARGIN + boundary_len + 8);
    size_t tail_len    = 0;
 
    if (!buf || !tail) {
        esp_ota_abort(ota_handle);
        free(buf); free(tail);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }
 
    while (remaining > 0 && !write_failed) {
        int to_read = MIN(remaining, WP_OTA_RECV_BUF_SIZE);
        int recv_len = httpd_req_recv(req, buf, to_read);
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "/ota: recv error %d", recv_len);
            write_failed = true;
            break;
        }
        remaining -= recv_len;
 
        char  *data = buf;
        int    data_len = recv_len;
 
        if (!header_skipped) {
            int header_end = _find_header_end(buf, recv_len);
            if (header_end < 0) {
                ESP_LOGE(TAG, "/ota: multipart header not found in first chunk "
                              "(increase WP_OTA_RECV_BUF_SIZE if filename is unusually long)");
                write_failed = true;
                break;
            }
            data     = buf + header_end;
            data_len = recv_len - header_end;
            header_skipped = true;
        }
 
        size_t hold_back = boundary_len + WP_OTA_TAIL_MARGIN;
 
        if (tail_len > 0) {
            if (esp_ota_write(ota_handle, tail, tail_len) != ESP_OK) {
                write_failed = true;
                break;
            }
            total_written += tail_len;
            tail_len = 0;
        }
 
        if ((size_t)data_len <= hold_back) {
            memcpy(tail, data, data_len);
            tail_len = data_len;
        } else {
            size_t flush_len = data_len - hold_back;
            if (esp_ota_write(ota_handle, data, flush_len) != ESP_OK) {
                write_failed = true;
                break;
            }
            total_written += flush_len;
            memcpy(tail, data + flush_len, hold_back);
            tail_len = hold_back;
        }
    }

    if (!write_failed && tail_len > 0) {
        char *boundary_pos = NULL;
        for (size_t i = 0; i + boundary_len <= tail_len; i++) {
            if (memcmp(tail + i, boundary, boundary_len) == 0) {
                boundary_pos = tail + i;
                break;
            }
        }
        size_t real_len = boundary_pos ? (size_t)(boundary_pos - tail) : tail_len;
 
        if (real_len >= 2 && tail[real_len - 2] == '\r' && tail[real_len - 1] == '\n') {
            real_len -= 2;
        }
 
        if (real_len > 0 && esp_ota_write(ota_handle, tail, real_len) != ESP_OK) {
            write_failed = true;
        } else {
            total_written += real_len;
        }
    }
 
    free(buf);
    free(tail);
 
    if (write_failed) {
        esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash write failed");
        return ESP_FAIL;
    }
 
    ESP_LOGI(TAG, "/ota: %u bytes written, finalizing", (unsigned)total_written);
 
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        const char *msg = (err == ESP_ERR_OTA_VALIDATE_FAILED) ? "Image validation failed" : "esp_ota_end failed";
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, msg);
        return ESP_FAIL;
    }
 
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "set_boot_partition failed");
        return ESP_FAIL;
    }
 
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
 
    ESP_LOGI(TAG, "/ota: success, rebooting in 1s");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
 
    return ESP_OK;
}

static esp_err_t _handler_captive_probe(httpd_req_t *req)
{
    WiFiPanel *wp = req->user_ctx;

    char location[32] = "http://192.168.4.1/";
    if (wp && wp->priv->netif) {     
        esp_netif_ip_info_t ip_info = {0};    
        if (esp_netif_get_ip_info(wp->priv->netif, &ip_info) == ESP_OK) {
            char ip_str[INET_ADDRSTRLEN]; 
            inet_ntoa_r(ip_info.ip.addr, ip_str, sizeof(ip_str));
            snprintf(location, sizeof(location), "http://%s/", ip_str);
        }
    }

    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", location);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* 404 / 405 - redirect to home */
static esp_err_t _handler_404(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* ================================================================
 * Endpoint table
 *
 * To add a new endpoint:
 *   1. Write a handler function above.
 *   2. Add one WP_ENDPOINT line here.
 * ================================================================ */

#define WP_ENDPOINT(m, u, h)  { .uri = (u), .method = (m), .handler = (h), .user_ctx = NULL }

static httpd_uri_t _endpoints[] = {
    WP_ENDPOINT(HTTP_GET,   "/",                    _handler_home_get),
    WP_ENDPOINT(HTTP_POST,  "/",                    _handler_wifi_post),
    WP_ENDPOINT(HTTP_GET,   "/scan",                _handler_scan_get),
    WP_ENDPOINT(HTTP_GET,   "/config",              _handler_config_get),
    WP_ENDPOINT(HTTP_POST,  "/configsave",          _handler_configsave_post),
    WP_ENDPOINT(HTTP_GET,   "/reset",               _handler_reset_get),
    WP_ENDPOINT(HTTP_POST,  "/ota",                 _handler_ota_post),
    WP_ENDPOINT(HTTP_GET,   "/ota",                 _handler_ota_get),
    /* Captive portal probes */
    WP_ENDPOINT(HTTP_GET,   "/hotspot-detect.html", _handler_captive_probe), /* Apple   */
    WP_ENDPOINT(HTTP_GET,   "/generate_204",        _handler_captive_probe), /* Android */
    WP_ENDPOINT(HTTP_GET,   "/connecttest.txt",     _handler_captive_probe), /* Windows */
    WP_ENDPOINT(HTTP_GET,   "/ncsi.txt",            _handler_captive_probe), /* Windows */
};

#define WP_ENDPOINT_COUNT  (sizeof(_endpoints) / sizeof(_endpoints[0]))

/* ================================================================
 * Web server lifecycle
 * ================================================================ */

httpd_handle_t WiFiPanel_StartWebServer(WiFiPanel *wp)
{
    httpd_config_t cfg    = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size        = /* 4096 */8192;
    cfg.max_open_sockets  = /* 2 */7;
    cfg.lru_purge_enable  = true;
    cfg.recv_wait_timeout = 10;
    cfg.send_wait_timeout = 10;
    cfg.max_uri_handlers  = WP_ENDPOINT_COUNT + 2;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return NULL;
    }

    for (size_t i = 0; i < WP_ENDPOINT_COUNT; i++) {
        _endpoints[i].user_ctx = wp;
        httpd_register_uri_handler(server, &_endpoints[i]);
    }

    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND,          _handler_404);
    httpd_register_err_handler(server, HTTPD_405_METHOD_NOT_ALLOWED, _handler_404);

    wp->priv->server = server;
    ESP_LOGI(TAG, "Server started, %u endpoints registered", (unsigned)WP_ENDPOINT_COUNT);
    return server;
}

void WiFiPanel_StopWebServer(WiFiPanel *wp)
{
    if (wp->priv->server) {
        httpd_stop(wp->priv->server);
        wp->priv->server = NULL;
        ESP_LOGI(TAG, "Server stopped");
    }
}

/* ================================================================
 * Captive portal DHCP option 114
 * ================================================================ */

WiFiPanel_Status WiFiPanel_SetCaptivePortalURI(WiFiPanel *wp)
{
    esp_netif_t *netif = wp->priv->netif;
    if (!netif){
        return WP_ERR_NETIF;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK){
        return WP_ERR_NETIF;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntoa_r(ip_info.ip.addr, ip_str, sizeof(ip_str));

    char uri[32];
    snprintf(uri, sizeof(uri), "http://%s", ip_str);

    esp_err_t ret = esp_netif_dhcps_stop(netif);
    if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
        return WP_ERR_NETIF;

    ret = esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, uri, strlen(uri));
    if (ret != ESP_OK){
        return WP_ERR_NETIF;
    }

    ret = esp_netif_dhcps_start(netif);
    if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED){
        return WP_ERR_NETIF;
    }

    ESP_LOGI(TAG_DHCP, "Captive portal URI: %s", uri);
    return WP_OK;
}