#ifndef WIFIMANAGER_DEFS_H
#define WIFIMANAGER_DEFS_H

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
#define WM_AP_SSID_DEFAULT   "ESP32_Config"
#define WM_AP_PSW_DEFAULT    "ESP32_Config"
#define WM_AP_MAX_STA_CONN   1                       /**< Max stations allowed to connect to the AP. */

#define WM_MAX_PARAMS        5                       /**< Max number of extra input fields on the portal page. */
#define WM_FIELD_LEN         128                     /**< Max length of each field string (id, label, value, ...). */
#define WM_PORTAL_BODY_SIZE  1024                    /**< Max size of HTTP POST body from the portal form. */
#define WM_PORTAL_TIMOUT_MS  (5UL * 60UL * 1000UL)  /**< Max time (ms) waiting for portal form submission. */

/* ================================================================
 * Event bits
 * ================================================================ */

/** WiFi event bits used with the event group. */
#define WM_EVENT_BIT_STASTART        BIT0   /**< STA mode started. */
#define WM_EVENT_BIT_STADISCONNECTED BIT1   /**< STA disconnected from AP. */
#define WM_EVENT_BIT_STACONNECTED    BIT2   /**< STA connected and got IP. */
#define WM_EVENT_BIT_APSTART         BIT3   /**< AP mode started. */

/* ================================================================
 * Macros
 * ================================================================ */

/** Default AP config initializer. Selects WPA/WPA2 if password is non-empty, else OPEN. */
#define WM_AP_CONFIG_DEFAULT()                                                           \
    (wifi_ap_config_t) {                                                                 \
        .ssid           = WM_AP_SSID_DEFAULT,                                            \
        .ssid_len       = strlen(WM_AP_SSID_DEFAULT),                                    \
        .password       = WM_AP_PSW_DEFAULT,                                             \
        .max_connection = WM_AP_MAX_STA_CONN,                                            \
        .authmode       = strlen(WM_AP_PSW_DEFAULT) ? WIFI_AUTH_WPA_WPA2_PSK             \
                                                    : WIFI_AUTH_OPEN                     \
    }

/* ================================================================
 * Callbacks
 * ================================================================ */

/** Called when STA connects to an AP. */
typedef void (*WiFiManager_Callback_ConnectedAP_t)(void);

/** Called when STA disconnects from an AP. */
typedef void (*WiFiManager_Callback_DisconnectedAP_t)(void);

/* ================================================================
 * Structs
 * ================================================================ */

/** Internal event group and handler instances. */
typedef struct
{
    EventGroupHandle_t group;
    esp_event_handler_instance_t ap_connected_handle;    /**< Handler instance for WIFI_EVENT_AP_STACONNECTED. */
    esp_event_handler_instance_t ap_disconnected_handle; /**< Handler instance for WIFI_EVENT_AP_STADISCONNECTED. */
    esp_event_handler_instance_t ap_start_handle;        /**< Handler instance for WIFI_EVENT_AP_START. */
    esp_event_handler_instance_t sta_handle;             /**< Handler instance for WIFI_EVENT_STA_START. */
    esp_event_handler_instance_t sta_disc_handle;        /**< Handler instance for WIFI_EVENT_STA_DISCONNECTED. */
    esp_event_handler_instance_t ip_handle;              /**< Handler instance for IP_EVENT_STA_GOT_IP. */
} WiFiManagerEvent_t;

/** Single dynamic input field on the captive portal form. */
typedef struct {
    char id[WM_FIELD_LEN];          /**< HTML element id and POST key. */
    char label[WM_FIELD_LEN];       /**< Label shown above the input. */
    char placeholder[WM_FIELD_LEN]; /**< Hint text inside the input. */
    char value[WM_FIELD_LEN];       /**< Value populated after form submit. */
    char type[16];                  /**< Input type: "text", "password", "number", etc. */
    bool required;                  /**< Must be non-empty before submit. */
} WiFiManagerParam_t;

/** Collection of dynamic portal form fields. */
typedef struct {
    WiFiManagerParam_t params[WM_MAX_PARAMS];   /**< Array of extra input fields. */
    size_t             count;                   /**< Number of fields. */
} WiFiManagerPage_t;

/** Main WiFi Manager instance. Zero-initialize before use. */
typedef struct {
    wifi_ap_config_t                      ap_config;            /**< AP config: SSID, password, max_connection, ... */
    wifi_sta_config_t                     sta_config;           /**< STA config: SSID, password, authmode, ... */
    WiFiManagerEvent_t                    event;                /**< Event group and handler instances. */
    esp_netif_t                          *netif;                /**< Active network interface (STA or AP). */
    int                                   sta_retry_num;        /**< Max STA reconnect attempts on disconnect. */
    WiFiManagerPage_t                     page;                 /**< Captive portal page with extra input fields. */
    httpd_handle_t                        server;               /**< HTTP server handle for the captive portal. */
    TaskHandle_t                          portal_waiting_task;  /**< Task handle blocked in ConfigViaAP, notified when portal form is submitted. */
    WiFiManager_Callback_ConnectedAP_t    ConnectedAP_Cb;       /**< Called on STA connected. */
    WiFiManager_Callback_DisconnectedAP_t DisconnectedAP_Cb;    /**< Called on STA disconnected. */
} WiFiManager_t;

#endif /* WIFIMANAGER_DEFS_H */