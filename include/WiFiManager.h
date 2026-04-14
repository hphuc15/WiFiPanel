#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"

/* ================================================================
 * DEFINES
 * ================================================================ */

/** Default AP SSID and password used in ConfigViaAP mode. */
#define WM_AP_SSID_DEFAULT "ESP32_Config"
#define WM_AP_PSW_DEFAULT "ESP32_Config"
#define WM_AP_MAX_STA_CONN 1 /**< Max stations allowed to connect to the AP. */

/** Captive portal page limits. */
#define WM_MAX_PARAMS 5                           /**< Max number of extra input fields on the portal page. */
#define WM_FIELD_LEN 128                          /**< Max length of each field string (id, label, value, ...). */
#define WM_PORTAL_BODY_SIZE 512                   /**< Max size of HTTP POST body from the portal form. */
#define WM_PORTAL_TIMOUT_MS (5UL * 60UL * 1000Ul) /**< Max time (ms) waiting for portal form submission. */

/** WiFi event bits used with the event group. */
#define WM_EVENT_BIT_STASTART BIT0        /**< STA mode started. */
#define WM_EVENT_BIT_STADISCONNECTED BIT1 /**< STA disconnected from AP. */
#define WM_EVENT_BIT_STACONNECTED BIT2    /**< STA connected and got IP. */
#define WM_EVENT_BIT_APSTART BIT3         /**< AP mode started. */

/** Default AP config initializer. Selects WPA/WPA2 if password is non-empty, else OPEN. */
#define WM_AP_CONFIG_DEFAULT()                                                          \
    (wifi_ap_config_t)                                                                  \
    {                                                                                   \
        .ssid = WM_AP_SSID_DEFAULT,                                                     \
        .ssid_len = strlen(WM_AP_SSID_DEFAULT),                                         \
        .password = WM_AP_PSW_DEFAULT,                                                  \
        .max_connection = WM_AP_MAX_STA_CONN,                                           \
        .authmode = strlen(WM_AP_PSW_DEFAULT) ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN \
    }

/* ================================================================
 * DATA TYPES
 * ================================================================ */

/** Callback invoked when STA successfully connects to an AP. */
typedef void (*WiFiManager_Callback_ConnectedAP_t)(void);

/** Callback invoked when STA disconnects from an AP. */
typedef void (*WiFiManager_Callback_DisconnectedAP_t)(void);

/** Holds the FreeRTOS event group and registered ESP event handler instances. */
typedef struct
{
    EventGroupHandle_t group;                /**< Synchronizes WiFi states across tasks. */
    esp_event_handler_instance_t ap_handle;  /**< Handler instance for AP events. */
    esp_event_handler_instance_t sta_handle; /**< Handler instance for STA events. */
    esp_event_handler_instance_t ip_handle;  /**< Handler instance for IP_EVENT_STA_GOT_IP. */
} WiFiManagerEvent_t;

/** Describes a single dynamic input field on the captive portal form. */
typedef struct
{
    char id[WM_FIELD_LEN];          /**< HTML element id and POST key. */
    char label[WM_FIELD_LEN];       /**< Label shown above the input. */
    char placeholder[WM_FIELD_LEN]; /**< Hint text inside the input. */
    char value[WM_FIELD_LEN];       /**< Current value (filled after form submit). */
    char type[16];                  /**< Input type: "text", "password", "number", ... */
    bool required;                  /**< true = field must be non-empty before submit. */
} WiFiManagerParam_t;

/** Holds all dynamic input fields for the captive portal page. */
typedef struct
{
    WiFiManagerParam_t params[WM_MAX_PARAMS]; /**< Array of extra input fields. */
    size_t count;                             /**< Number of fields currently added. */
} WiFiManagerPage_t;

/** Main WiFi Manager instance. Zero-initialize before use. */
typedef struct
{
    wifi_config_t config;                                    /**< WiFi config: SSID, password, authmode, ... */
    WiFiManagerEvent_t event;                                /**< Event group and handler instances. */
    esp_netif_t *netif;                                      /**< Active network interface (STA or AP). */
    uint8_t sta_retry_num;                                   /**< Max STA reconnect attempts on disconnect. */
    WiFiManagerPage_t page;                                  /**< Captive portal page with extra input fields. */
    httpd_handle_t server;                                   /**< HTTP server handle for the captive portal. */
    TaskHandle_t portal_waiting_task;                        /**< Task handle blocked in ConfigViaAP, notified when portal form is submitted. */
    WiFiManager_Callback_ConnectedAP_t ConnectedAP_Cb;       /**< Called on STA connected. */
    WiFiManager_Callback_DisconnectedAP_t DisconnectedAP_Cb; /**< Called on STA disconnected. */
} WiFiManager_t;

/* ================================================================
 * PUBLIC API: WiFi Lifecycle
 * ================================================================ */

/**
 * @brief Initialize event group, NVS flash, and WiFi driver.
 * @param wm Pointer to WiFiManager instance.
 */
void WiFiManager_Init(WiFiManager_t *wm);

/**
 * @brief Start WiFi in Station (STA) mode.
 * @param wm Pointer to WiFiManager instance.
 */
void WiFiManager_StartSTA(WiFiManager_t *wm);

/**
 * @brief Start WiFi in Access Point (AP) mode.
 * @param wm Pointer to WiFiManager instance.
 */
void WiFiManager_StartAP(WiFiManager_t *wm);

/**
 * @brief Collect STA credentials via AP + Captive Portal, then switch to STA mode.
 * @param wm Pointer to WiFiManager instance.
 */
void WiFiManager_ConfigViaAP(WiFiManager_t *wm);

/**
 * @brief Try connect using saved STA credentials, fallback to AP if failed.
 *
 * Load WiFi config from NVS, attempt STA connection, wait for result,
 * and start AP configuration mode if connection fails or no valid SSID.
 *
 * @param wm Pointer to WiFiManager instance
 */
void WiFiManager_AutoConnect(WiFiManager_t *wm);

/**
 * @brief Stop WiFi and unregister all event handlers.
 * @param wm Pointer to WiFiManager instance.
 */
void WiFiManager_Stop(WiFiManager_t *wm);

/**
 * @brief Deinitialize WiFi driver, NVS, and event loop.
 * @param wm Pointer to WiFiManager instance.
 */
void WiFiManager_Deinit(WiFiManager_t *wm);

/**
 * @brief Get the current WiFi mode.
 * @return Current wifi_mode_t (WIFI_MODE_STA, WIFI_MODE_AP, ...),
 *         or WIFI_MODE_NULL if WiFi is not initialized.
 */
wifi_mode_t WiFiManager_GetMode(void);

/* ================================================================
 * PUBLIC API: Captive Portal Page
 * ================================================================ */

/**
 * @brief Zero-initialize a WiFiManagerPage.
 * @param wm Pointer to wm instance.
 */
void WiFiManagerPage_Init(WiFiManager_t *wm);

/**
 * @brief Add a dynamic input field to the captive portal form.
 * @param wm          Pointer to wm instance.
 * @param id          HTML element id and POST key.
 * @param label       Label shown above the field.
 * @param placeholder Hint text inside the input (NULL = none).
 * @param value       Pre-filled value (NULL = empty).
 * @param type        Input type: "text", "password", ... (NULL → "text").
 * @param required    true if field must be non-empty before submit.
 * @return 0 on success, -1 if WM_MAX_PARAMS exceeded.
 */
int WiFiManagerPage_AddParam(WiFiManager_t *wm,
                             const char *id, const char *label,
                             const char *placeholder, const char *value,
                             const char *type, bool required);

/**
 * @brief Get the current value of a field by id.
 * @param wm    Pointer to wm instance.
 * @param id    Field id to look up.
 * @return Pointer to value string (valid while page is alive), or NULL if not found.
 */
const char *WiFiManagerPage_GetParam(const WiFiManager_t *wm, const char *id);

/**
 * @brief Build the full HTML configuration page.
 * @param wm    Pointer to wm instance.
 * @return Heap-allocated HTML string, or NULL on failure. Caller must free().
 */
char *WiFiManagerPage_Build(const WiFiManager_t *wm);



#endif /* WIFIMANAGER_H */