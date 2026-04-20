#pragma once

#include "WiFiManager.h"

/* ================================================================
 * INTERNAL: Portal & DNS (WiFiManager_portal.c)
 * ================================================================ */

/**
 * @brief Start the DNS server that resolves every A query to the given IP.
 * @param ip IPv4 address to return for all queries.
 * @return Opaque handle, pass to WiFiManager_StopDNS(). NULL on failure.
 */
void *WiFiManager_StartDNS(esp_ip4_addr_t ip);

/**
 * @brief Stop the DNS server and free all resources.
 * @param handle Opaque handle returned by WiFiManager_StartDNS().
 */
void WiFiManager_StopDNS(void *handle);

/**
 * @brief Set DHCP option 114 to redirect connecting clients to the captive portal.
 * @param wm Pointer to WiFiManager instance.
 */
void WiFiManager_SetCaptivePortalURI(WiFiManager_t *wm);

/**
 * @brief Start the HTTP server and register portal URI handlers.
 * @param wm Pointer to WiFiManager instance.
 * @return httpd_handle_t on success, NULL on failure.
 */
httpd_handle_t WiFiManager_StartWebServer(WiFiManager_t *wm);

/**
 * @brief Stop the HTTP server.
 * @param wm Pointer to WiFiManager instance.
 */
void WiFiManager_StopWebServer(WiFiManager_t *wm);