#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include "WiFiManager_defs.h"

/* ================================================================
 * WiFi lifecycle
 * ================================================================ */

/** @brief Initialize NVS, event loop, and WiFi driver. */
void WiFiManager_Init(WiFiManager_t *wm);

/** @brief Start WiFi in STA mode. */
void WiFiManager_StartSTA(WiFiManager_t *wm);

/** @brief Start WiFi in AP mode. */
void WiFiManager_StartAP(WiFiManager_t *wm);

/**
 * @brief Open captive portal AP, collect credentials, then switch to STA.
 * Blocks until form is submitted or WM_PORTAL_TIMOUT_MS elapses.
 */
void WiFiManager_ConfigViaAP(WiFiManager_t *wm);

/** @brief Connect using saved NVS credentials; fall back to ConfigViaAP on failure. */
void WiFiManager_AutoConnect(WiFiManager_t *wm);

/** @brief Stop WiFi and unregister all event handlers. */
void WiFiManager_Stop(WiFiManager_t *wm);

/** @brief Deinitialize WiFi driver, NVS, and event loop. */
void WiFiManager_Deinit(WiFiManager_t *wm);

/** @brief Return true if STA is connected and has an IP. */
bool WiFiManager_IsConnected(WiFiManager_t *wm);

/** @brief Return current wifi_mode_t, or WIFI_MODE_NULL if not initialized. */
wifi_mode_t WiFiManager_GetMode(void);

/* ================================================================
 * Captive portal page
 * ================================================================ */

/** @brief Zero-initialize the portal page inside @p wm. */
void WiFiManagerPage_Init(WiFiManager_t *wm);

/**
 * @brief Add a dynamic input field to the portal form.
 * @return 0 on success, -1 if WM_MAX_PARAMS exceeded.
 */
int WiFiManagerPage_AddParam(WiFiManager_t *wm, const char *id, const char *label, const char *placeholder, const char *value, const char *type, bool required);

/**
 * @brief Look up a field value by id.
 * @return Pointer to value string (valid while page is alive), NULL if not found.
 */
const char *WiFiManagerPage_GetParam(const WiFiManager_t *wm, const char *id);

/** @brief Return static HTML head + CSS + fixed SSID/Password fields (flash). */
const char *WiFiManagerPage_GetHead(void);

/** @brief Return static HTML tail + submit button + JS (flash). */
const char *WiFiManagerPage_GetTail(void);

/**
 * @brief Build only the dynamic extra-field HTML fragment.
 * @return Heap-allocated string. Caller must free(). NULL on failure.
 */
char *WiFiManagerPage_BuildFields(const WiFiManager_t *wm);

#endif /* WIFIMANAGER_H */