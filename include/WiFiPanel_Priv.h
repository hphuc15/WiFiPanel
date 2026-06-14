/* WiFiPanel_Priv.h */
#pragma once

#include "WiFiPanel.h"

/* ================================================================
 * INTERNAL: Portal & DNS (WiFiPanel_portal.c)
 * ================================================================ */

/**
 * @brief Start the DNS server that resolves every A query to the given IP.
 * @param ip IPv4 address to return for all queries.
 * @return Opaque handle, pass to WiFiPanel_StopDNS(). NULL on failure.
 */
void *WiFiPanel_StartDNS(esp_ip4_addr_t ip);

/**
 * @brief Stop the DNS server and free all resources.
 * @param handle Opaque handle returned by WiFiPanel_StartDNS().
 */
void WiFiPanel_StopDNS(void *handle);

/**
 * @brief Set DHCP option 114 to redirect connecting clients to the captive portal.
 * @param wp Pointer to WiFiPanel instance.
 * @return WP_OK on success, WP_ERR_NETIF on failure.
 */
WiFiPanel_Status WiFiPanel_SetCaptivePortalURI(WiFiPanel *wp);

/**
 * @brief Start the HTTP server and register portal URI handlers.
 * @param wp Pointer to WiFiPanel instance.
 * @return httpd_handle_t on success, NULL on failure.
 */
httpd_handle_t WiFiPanel_StartWebServer(WiFiPanel *wp);

/**
 * @brief Stop the HTTP server.
 * @param wp Pointer to WiFiPanel instance.
 */
void WiFiPanel_StopWebServer(WiFiPanel *wp);