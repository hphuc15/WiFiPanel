#ifndef WIFIPANEL_DEFS_H
#define WIFIPANEL_DEFS_H

#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"

/* ================================================================
 * Configuration
 * ================================================================ */

/** Default AP SSID and password used in ConfigViaAP mode. */
#define WP_AP_SSID_DEFAULT   "ESP32_Config"
#define WP_AP_PSW_DEFAULT    "ESP32_Config"
#define WP_AP_MAX_STA_CONN   1                       /**< Max stations allowed to connect to the AP. */

#define WP_MAX_PARAMS        10                       /**< Max number of extra input fields on the portal page. */
#define WP_FIELD_LEN         128                     /**< Max length of each field string (id, label, value, ...). */
#define WP_PORTAL_BODY_SIZE  1024                    /**< Max size of HTTP POST body from the portal form. */
#define WP_PORTAL_TIMEOUT_MS (5UL * 60UL * 1000UL)  /**< Max time (ms) waiting for portal form submission. */

#define WP_SCAN_DEFAULT_MAX  10

/* ================================================================
 * Status codes
 * ================================================================ */

typedef enum {
    WP_OK              =  0,  /**< Success. */
    WP_ERR_INVALID_ARG = -1,  /**< NULL or invalid parameter. */
    WP_ERR_INIT        = -2,  /**< NVS or event loop init failure. */
    WP_ERR_WIFI        = -3,  /**< esp_wifi_* call failure. */
    WP_ERR_TIMEOUT     = -4,  /**< Waited too long for an event. */
    WP_ERR_NO_CREDS    = -5,  /**< No valid credentials found in NVS. */
    WP_ERR_NETIF       = -6,  /**< Network interface error. */
    WP_ERR_NOMEM       = -7,  /**< Allocation or capacity failure. */
} WiFiPanel_Status;

/* ================================================================
 * Event bits
 * ================================================================ */

/** WiFi event bits used with the event group. */
#define WP_EVENT_BIT_STASTART        BIT0   /**< STA mode started. */
#define WP_EVENT_BIT_STADISCONNECTED BIT1   /**< STA disconnected from AP. */
#define WP_EVENT_BIT_STACONNECTED    BIT2   /**< STA connected and got IP. */
#define WP_EVENT_BIT_APSTART         BIT3   /**< AP mode started. */

/* ================================================================
 * Macros
 * ================================================================ */

/** Default AP config initializer. Selects WPA/WPA2 if password is non-empty, else OPEN. */
#define WP_AP_CONFIG_DEFAULT()                                                         \
    (wifi_ap_config_t) {                                                               \
        .ssid           = WP_AP_SSID_DEFAULT,                                          \
        .ssid_len       = strlen(WP_AP_SSID_DEFAULT),                                  \
        .password       = WP_AP_PSW_DEFAULT,                                           \
        .max_connection = WP_AP_MAX_STA_CONN,                                          \
        .authmode       = strlen(WP_AP_PSW_DEFAULT) ? WIFI_AUTH_WPA_WPA2_PSK           \
                                                    : WIFI_AUTH_OPEN                   \
    }

/** Initializes a WiFiPanelPage_t with automatic field count. Usage: .page = WP_PAGE_COUNT(WP_PAGE_PARAM(...), ...) */
#define WP_PAGE_COUNT(...)                                                           \
    {                                                                                \
        .count  = sizeof((WiFiPanelParam_t[]){__VA_ARGS__}) / sizeof(WiFiPanelParam_t), \
        .params = { __VA_ARGS__ }                                                    \
    }

/** Initializes a single WiFiPanelParam_t entry. Use inside WP_PAGE_COUNT(). */
#define WP_PAGE_PARAM(_id, _label, _placeholder, _value, _type, _required) \
    { .id          = _id,          \
      .label       = _label,       \
      .placeholder = _placeholder, \
      .value       = _value,       \
      .type        = _type,        \
      .required    = _required     \
    }
/* ================================================================
 * Callbacks
 * ================================================================ */

/** Called when STA connects to an AP. */
typedef void (*WiFiPanel_ConnectedAP_Cb_t)(void);

/** Called when STA disconnects from an AP. */
typedef void (*WiFiPanel_DisconnectedAP_Cb_t)(void);

/* ================================================================
 * Structs
 * ================================================================ */

/** Single dynamic input field on the captive portal form. */
typedef struct {
    char id[WP_FIELD_LEN];           /**< HTML element id and POST key. */
    char label[WP_FIELD_LEN];        /**< Label shown above the input. */
    char placeholder[WP_FIELD_LEN];  /**< Hint text inside the input. */
    char value[WP_FIELD_LEN];        /**< Value populated after form submit. */
    char type[16];                   /**< Input type: "text", "password", "number", etc. */
    bool required;                   /**< Must be non-empty before submit. */
} WiFiPanelParam_t;

/** Collection of dynamic portal form fields. */
typedef struct {
    WiFiPanelParam_t params[WP_MAX_PARAMS];  /**< Array of extra input fields. */
    size_t           count;                  /**< Number of fields currently set. */
} WiFiPanelPage_t;


/** Internal event group and handler instances. */
typedef struct
{
    EventGroupHandle_t              group;
    esp_event_handler_instance_t    ap_connected_handle;     /**< Handler for WIFI_EVENT_AP_STACONNECTED. */
    esp_event_handler_instance_t    ap_disconnected_handle;  /**< Handler for WIFI_EVENT_AP_STADISCONNECTED. */
    esp_event_handler_instance_t    ap_start_handle;         /**< Handler for WIFI_EVENT_AP_START. */
    esp_event_handler_instance_t    sta_handle;              /**< Handler for WIFI_EVENT_STA_START. */
    esp_event_handler_instance_t    sta_disc_handle;         /**< Handler for WIFI_EVENT_STA_DISCONNECTED. */
    esp_event_handler_instance_t    ip_handle;               /**< Handler for IP_EVENT_STA_GOT_IP. */
} WiFiPanelEvent_t;

typedef struct {
    WiFiPanelEvent_t              event;                 /**< Event group and handler instances. */
    esp_netif_t                  *netif;                 /**< Active network interface (STA or AP). */
    int                           sta_retry_remaining;   /**< Remaining reconnect attempts. */
    httpd_handle_t                server;                /**< HTTP server handle for the captive portal. */
    TaskHandle_t                  portal_waiting_task;   /**< Task blocked in ConfigViaAP, notified on submit. */
} WiFiPanel_Internal;

#endif /* WIFIPANEL_DEFS_H */