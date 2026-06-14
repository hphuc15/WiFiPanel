/* WiFiPanel.h */
#ifndef WIFIPANEL_H
#define WIFIPANEL_H

#include "WiFiPanel_Defs.h"

/** Main WiFiPanel instance. Zero-initialize before use. */
typedef struct {
    wifi_ap_config_t              ap_config;             /**< AP config: SSID, password, max_connection, ... */
    wifi_sta_config_t             sta_config;            /**< STA config: SSID, password, authmode, ... */
    int                           sta_retry_num;         /**< Max STA reconnect attempts (-1 = infinite). */
    uint16_t                      scan_max_count;        /**< Max SSIDs to return from scan (0 = use default). */
    WiFiPanelPage_t               page;                  /**< Captive portal page with extra input fields. */
    WiFiPanel_ConnectedAP_Cb_t    ConnectedAP_Cb;        /**< Called on STA connected. */
    WiFiPanel_DisconnectedAP_Cb_t DisconnectedAP_Cb;     /**< Called on STA disconnected. */
    WiFiPanel_Internal            *priv;
} WiFiPanel;

/* ================================================================
 * WiFi lifecycle
 * ================================================================ */

/** @brief Initialize NVS, event loop, and WiFi driver. */
WiFiPanel_Status WiFiPanel_Init(WiFiPanel *wp);

/** @brief Start WiFi in STA mode. */
WiFiPanel_Status WiFiPanel_StartSTA(WiFiPanel *wp);

/** @brief Start WiFi in AP mode. */
WiFiPanel_Status WiFiPanel_StartAP(WiFiPanel *wp);

/**
 * @brief Open captive portal AP, collect credentials, then switch to STA.
 * Blocks until form is submitted or WP_PORTAL_TIMEOUT_MS elapses.
 */
WiFiPanel_Status WiFiPanel_ConfigViaAP(WiFiPanel *wp);

/** @brief Connect using saved NVS credentials; fall back to ConfigViaAP on failure. */
WiFiPanel_Status WiFiPanel_AutoConnect(WiFiPanel *wp);

/** @brief Stop WiFi and unregister all event handlers. */
WiFiPanel_Status WiFiPanel_Stop(WiFiPanel *wp);

/** @brief Deinitialize WiFi driver, NVS, and event loop. */
WiFiPanel_Status WiFiPanel_Deinit(WiFiPanel *wp);

/** @brief Return true if STA is connected and has an IP. */
bool WiFiPanel_IsConnected(const WiFiPanel *wp);

/** @brief Return current wifi_mode_t, or WIFI_MODE_NULL if not initialized. */
wifi_mode_t WiFiPanel_GetMode(void);

/* ================================================================
 * Captive portal page
 * ================================================================ */

/** @brief Zero-initialize the portal page inside @p wp. */
void WiFiPanelPage_Init(WiFiPanel *wp);

/**
 * @brief Add a dynamic input field to the portal form.
 * @return WP_OK on success, WP_ERR_NOMEM if WP_MAX_PARAMS exceeded.
 */
WiFiPanel_Status WiFiPanelPage_AddParam(WiFiPanel *wp, const char *id, const char *label, const char *placeholder, const char *value, const char *type, bool required);

/**
 * @brief Look up a field value by id.
 * @return Pointer to value string (valid while page is alive), NULL if not found.
 */
const char *WiFiPanelPage_GetParam(const WiFiPanel *wp, const char *id);

/** @brief Return static HTML head + CSS + fixed SSID/Password fields (flash). */
const char *WiFiPanelPage_GetHead(void);

/** @brief Return static HTML tail + submit button + JS (flash). */
const char *WiFiPanelPage_GetTail(void);

/**
 * @brief Build only the dynamic extra-field HTML fragment.
 * @return Heap-allocated string. Caller must free(). NULL on failure.
 */
char *WiFiPanelPage_BuildFields(const WiFiPanel *wp);

#endif /* WIFIPANEL_H */