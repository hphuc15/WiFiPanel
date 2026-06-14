
#include "WiFiPanel_Priv.h"

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_event.h"

static const char *TAG_INIT   = "[WP][INIT]";
static const char *TAG_STA    = "[WP][STA]";
static const char *TAG_AP     = "[WP][AP]";
static const char *TAG_STOP   = "[WP][STOP]";
static const char *TAG_DEINIT = "[WP][DEINIT]";
static const char *TAG_PORTAL = "[WP][PORTAL]";
static const char *TAG_AUTO   = "[WP][AUTO]";

/* ===================== INTERNAL: WIFI CORE ========================== */

/** @brief WiFi station event handler */
static void _wp_handler_sta_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    WiFiPanel *wp = (WiFiPanel *)arg;
    esp_err_t err;

    if (event_base == WIFI_EVENT)
    {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
            {
                xEventGroupSetBits(wp->priv->event.group, WP_EVENT_BIT_STASTART);
                wp->priv->sta_retry_remaining = wp->sta_retry_num;
                err = esp_wifi_connect();
                if (err != ESP_OK) {
                    ESP_LOGE(TAG_STA, "Failed to connect: %s", esp_err_to_name(err));
                }
                break;
            }

            case WIFI_EVENT_STA_DISCONNECTED:
            {
                if (!(xEventGroupGetBits(wp->priv->event.group) & WP_EVENT_BIT_STASTART)) {
                    break;
                }

                wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
                ESP_LOGE(TAG_STA, "Disconnected, reason: %d", event->reason);

                if(!(xEventGroupGetBits(wp->priv->event.group) & WP_EVENT_BIT_STADISCONNECTED)){
                    if(wp->DisconnectedAP_Cb){
                        wp->DisconnectedAP_Cb();
                    }
                }
                xEventGroupClearBits(wp->priv->event.group, WP_EVENT_BIT_STACONNECTED);
                xEventGroupSetBits(wp->priv->event.group, WP_EVENT_BIT_STADISCONNECTED);

                if (wp->priv->sta_retry_remaining != 0)
                {
                    if (wp->priv->sta_retry_remaining > 0) {
                        wp->priv->sta_retry_remaining--;
                    }
                    ESP_LOGW(TAG_STA, "Retrying... (%d left)", wp->priv->sta_retry_remaining);
                    esp_wifi_connect();
                }
                else
                {
                    wp->priv->sta_retry_remaining = wp->sta_retry_num;
                }
                break;
            }
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wp->priv->sta_retry_remaining = wp->sta_retry_num;
        xEventGroupClearBits(wp->priv->event.group, WP_EVENT_BIT_STADISCONNECTED);
        xEventGroupSetBits(wp->priv->event.group, WP_EVENT_BIT_STACONNECTED);
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_STA, "Connected. IP: " IPSTR, IP2STR(&event->ip_info.ip));
        if (wp->ConnectedAP_Cb) {
            wp->ConnectedAP_Cb();
        }
    }
}

/** @brief WiFi access point event handler */
static void _wp_handler_ap_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    WiFiPanel *wp = (WiFiPanel *)arg;
    esp_err_t err;

    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_AP_START:
            {
                xEventGroupSetBits(wp->priv->event.group, WP_EVENT_BIT_APSTART);

                esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
                esp_netif_ip_info_t ip_info;
                err = esp_netif_get_ip_info(ap_netif, &ip_info);
                if (err != ESP_OK)
                {
                    return;
                }
                ESP_LOGI(TAG_AP, "AP START. SSID: %s, IP: " IPSTR ".", (char *)wp->ap_config.ssid, IP2STR(&ip_info.ip));
                break;
            }

            case WIFI_EVENT_AP_STACONNECTED:
            {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                ESP_LOGI(TAG_AP, "Station [" MACSTR "] joined.", MAC2STR(event->mac));
                break;
            }
            
            case WIFI_EVENT_AP_STADISCONNECTED:
            {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                ESP_LOGI(TAG_AP, "Station [" MACSTR "] left: %d", MAC2STR(event->mac));
                break;
            }

            case WIFI_EVENT_AP_STOP:
            {
                xEventGroupClearBits(wp->priv->event.group, WP_EVENT_BIT_APSTART);
                ESP_LOGI(TAG_AP, "AP STOPPED.");
                break;
            }
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wp->priv->sta_retry_remaining = wp->sta_retry_num;
        xEventGroupClearBits(wp->priv->event.group, WP_EVENT_BIT_STADISCONNECTED);
        xEventGroupSetBits(wp->priv->event.group, WP_EVENT_BIT_STACONNECTED);
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_STA, "Connected. IP: " IPSTR, IP2STR(&event->ip_info.ip));
        if (wp->ConnectedAP_Cb) {
            wp->ConnectedAP_Cb();
        }
    }
}

/**
 * @brief Load saved STA credentials from NVS into wp->sta_config.
 * @return true  if valid credentials found.
 * @return false if no credentials, load failed, or SSID matches config AP.
 */
static bool _wp_nvs_load_saved_wifi(WiFiPanel *wp) {
    wifi_config_t saved_config;
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &saved_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_AUTO, "Failed to get config from NVS: %s", esp_err_to_name(err));
        return false;
    }

    if (strlen((char *)saved_config.sta.ssid) == 0) {
        ESP_LOGW(TAG_AUTO, "No saved SSID found in NVS");
        return false;
    }

    if (strcmp((char *)saved_config.sta.ssid, WP_AP_SSID_DEFAULT) == 0) {
        ESP_LOGW(TAG_AUTO, "Saved SSID matches config AP (%s), ignoring", (char *)saved_config.sta.ssid);
        return false;
    }

    memcpy(&wp->sta_config, &saved_config.sta, sizeof(wifi_sta_config_t));
    ESP_LOGI(TAG_AUTO, "Loaded saved SSID: %s", (char *)wp->sta_config.ssid);
    return true;
}

/* ===================== PUBLIC: WIFI LIFECYCLE ======================= */

WiFiPanel_Status WiFiPanel_Init(WiFiPanel *wp) {
    esp_err_t ret;

    if (wp->priv == NULL) {
        wp->priv = calloc(1, sizeof(WiFiPanel_Internal));
        if (!wp->priv) {
            ESP_LOGE(TAG_INIT, "Failed to alloc internal state");
            return WP_ERR_NOMEM;
        }
    }

    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_INIT, "netif init failed: %s", esp_err_to_name(ret));
        return WP_ERR_NETIF;
    }

    if (wp->priv->event.group == NULL) {
        wp->priv->event.group = xEventGroupCreate();
        if (!wp->priv->event.group) {
            ESP_LOGE(TAG_INIT, "Failed to create event group");
            return WP_ERR_NOMEM;
        }
    }

    ret = esp_event_loop_create_default();
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG_INIT, "Default event loop already created");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG_INIT, "Event loop create failed: %s", esp_err_to_name(ret));
        return WP_ERR_INIT;
    }

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG_INIT, "NVS erase failed: %s", esp_err_to_name(ret));
            return WP_ERR_INIT;
        }
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_INIT, "NVS init failed: %s", esp_err_to_name(ret));
        return WP_ERR_INIT;
    }

    wifi_mode_t mode;
    ret = esp_wifi_get_mode(&mode);
    if (ret != ESP_ERR_WIFI_NOT_INIT) {
        WiFiPanel_Stop(wp);
    }

    wifi_init_config_t wifi_drv_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_drv_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_INIT, "WiFi driver init failed: %s", esp_err_to_name(ret));
        return WP_ERR_WIFI;
    }

    return WP_OK;
}

WiFiPanel_Status WiFiPanel_StartSTA(WiFiPanel *wp) {
    esp_err_t ret;

    xEventGroupClearBits(wp->priv->event.group, WP_EVENT_BIT_STACONNECTED | WP_EVENT_BIT_STADISCONNECTED);

    if (wp->priv->netif) {
        esp_netif_destroy_default_wifi(wp->priv->netif);
    }
    wp->priv->netif = esp_netif_create_default_wifi_sta();
    if (!wp->priv->netif) {
        return WP_ERR_NETIF;
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START, _wp_handler_sta_event, wp, &wp->priv->event.sta_handle);
    if (ret != ESP_OK){
        return WP_ERR_WIFI;
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, _wp_handler_sta_event, wp, &wp->priv->event.sta_disc_handle);
    if (ret != ESP_OK){
        return WP_ERR_WIFI;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, _wp_handler_sta_event, wp, &wp->priv->event.ip_handle);
    if (ret != ESP_OK){
        return WP_ERR_WIFI;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK){
        return WP_ERR_WIFI;
    }

    if (strlen((char *)wp->sta_config.ssid) > 0) {
        wifi_config_t config = { .sta = wp->sta_config };
        ret = esp_wifi_set_config(WIFI_IF_STA, &config);
        if (ret != ESP_OK){
            return WP_ERR_WIFI;
        }
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK){
        return WP_ERR_WIFI;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    return WP_OK;
}

WiFiPanel_Status WiFiPanel_StartAP(WiFiPanel *wp) {
    esp_err_t ret;

    xEventGroupClearBits(wp->priv->event.group, WP_EVENT_BIT_APSTART);

    if (wp->priv->netif) {
        esp_netif_destroy_default_wifi(wp->priv->netif);
    }
    wp->priv->netif = esp_netif_create_default_wifi_ap();
    if (!wp->priv->netif) {
        return WP_ERR_NETIF;
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, _wp_handler_ap_event, wp, &wp->priv->event.ap_connected_handle);
    if (ret != ESP_OK){
        return WP_ERR_WIFI;
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, _wp_handler_ap_event, wp, &wp->priv->event.ap_disconnected_handle);
    if (ret != ESP_OK){
        return WP_ERR_WIFI;
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_START, _wp_handler_ap_event, wp, &wp->priv->event.ap_start_handle);
    if (ret != ESP_OK){
        return WP_ERR_WIFI;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK){
        return WP_ERR_WIFI;
    }

    wifi_config_t config = { .ap = wp->ap_config };
    ret = esp_wifi_set_config(WIFI_IF_AP, &config);
    if (ret != ESP_OK){
        return WP_ERR_WIFI;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK){
        return WP_ERR_WIFI;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    return WP_OK;
}

WiFiPanel_Status WiFiPanel_Stop(WiFiPanel *wp) {
    if (!wp) {
        ESP_LOGE(TAG_STOP, "WiFiPanel instance is NULL");
        return WP_ERR_INVALID_ARG;
    }

    esp_err_t err;
    wifi_mode_t mode;

    err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_STOP, "Failed to get WiFi mode: %s", esp_err_to_name(err));
        mode = WIFI_MODE_NULL;
    }
    ESP_LOGI(TAG_STOP, "Stopping WiFi (mode: %d)", mode);

    /* Disconnect STA before unregistering handlers so events can still be processed. */
    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
        err = esp_wifi_disconnect();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
            ESP_LOGW(TAG_STOP, "Disconnect failed: %s", esp_err_to_name(err));
        }
    }

    err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGE(TAG_STOP, "Failed to stop WiFi: %s", esp_err_to_name(err));
    }

    /* Safe to unregister handlers now — no more events will fire. */
    if (wp->priv->event.ap_connected_handle) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, wp->priv->event.ap_connected_handle);
        wp->priv->event.ap_connected_handle = NULL;
    }
    if (wp->priv->event.ap_disconnected_handle) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, wp->priv->event.ap_disconnected_handle);
        wp->priv->event.ap_disconnected_handle = NULL;
    }
    if (wp->priv->event.ap_start_handle) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_AP_START, wp->priv->event.ap_start_handle);
        wp->priv->event.ap_start_handle = NULL;
    }
    if (wp->priv->event.sta_handle) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, wp->priv->event.sta_handle);
        wp->priv->event.sta_handle = NULL;
    }
    if (wp->priv->event.sta_disc_handle) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wp->priv->event.sta_disc_handle);
        wp->priv->event.sta_disc_handle = NULL;
    }
    if (wp->priv->event.ip_handle) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wp->priv->event.ip_handle);
        wp->priv->event.ip_handle = NULL;
    }

    xEventGroupClearBits(wp->priv->event.group, WP_EVENT_BIT_APSTART | WP_EVENT_BIT_STASTART | WP_EVENT_BIT_STACONNECTED | WP_EVENT_BIT_STADISCONNECTED);

    if (wp->priv->netif) {
        esp_netif_destroy_default_wifi(wp->priv->netif);
        wp->priv->netif = NULL;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG_STOP, "WiFi stopped.");
    return WP_OK;
}

WiFiPanel_Status WiFiPanel_Deinit(WiFiPanel *wp) {
    esp_err_t err;
    WiFiPanel_Stop(wp);

    err = esp_wifi_deinit();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG_DEINIT, "WiFi driver was not installed");
        return WP_ERR_WIFI;
    }

    err = nvs_flash_deinit();
    if (err == ESP_ERR_NVS_NOT_INITIALIZED) {
        ESP_LOGE(TAG_DEINIT, "NVS storage not initialized");
        return WP_ERR_INIT;
    }

    err = esp_event_loop_delete_default();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_DEINIT, "Event loop delete failed: %s", esp_err_to_name(err));
        return WP_ERR_INIT;
    }

    if (wp->priv) {
        free(wp->priv);
        wp->priv = NULL;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG_DEINIT, "WiFiPanel deinitialized");
    return WP_OK;
}

WiFiPanel_Status WiFiPanel_ConfigViaAP(WiFiPanel *wp) {
    WiFiPanel_Status st;

    if (strlen((char *)wp->ap_config.ssid) == 0) {
        wp->ap_config = WP_AP_CONFIG_DEFAULT();
    }

    st = WiFiPanel_StartAP(wp);
    if (st != WP_OK){
        return st;
    }

    EventBits_t bits = xEventGroupWaitBits(wp->priv->event.group, WP_EVENT_BIT_APSTART, pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));
    if (!(bits & WP_EVENT_BIT_APSTART)) {
        ESP_LOGE(TAG_PORTAL, "Timeout: AP failed to start");
        WiFiPanel_Stop(wp);
        return WP_ERR_TIMEOUT;
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    esp_wifi_set_mode(WIFI_MODE_APSTA);     /* Scan only active when use STA or APSTA mode */

    WiFiPanel_SetCaptivePortalURI(wp);
    WiFiPanel_StartWebServer(wp);

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(wp->priv->netif, &ip_info);
    void *dns = WiFiPanel_StartDNS(ip_info.ip);

    wp->priv->portal_waiting_task = xTaskGetCurrentTaskHandle();
    BaseType_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WP_PORTAL_TIMEOUT_MS));
    wp->priv->portal_waiting_task = NULL;

    if (!notified) {
        ESP_LOGE(TAG_PORTAL, "Timeout: no credentials received");
        WiFiPanel_StopDNS(dns);
        WiFiPanel_StopWebServer(wp);
        WiFiPanel_Stop(wp);
        return WP_ERR_TIMEOUT;
    }
    vTaskDelay(pdMS_TO_TICKS(300));

    WiFiPanel_StopDNS(dns);
    WiFiPanel_StopWebServer(wp);
    WiFiPanel_Stop(wp);
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Sanitize credentials before connecting */
    wp->sta_config.ssid[sizeof(wp->sta_config.ssid) - 1]            = '\0';
    wp->sta_config.password[sizeof(wp->sta_config.password) - 1]    = '\0';
    wp->sta_config.threshold.authmode                               = WIFI_AUTH_OPEN;
    wp->sta_config.scan_method                                      = WIFI_ALL_CHANNEL_SCAN;

    ESP_LOGI(TAG_PORTAL, "Switching to STA...");
    return WiFiPanel_StartSTA(wp);
}

WiFiPanel_Status WiFiPanel_AutoConnect(WiFiPanel *wp) {
    if (!wp) {
        ESP_LOGE(TAG_AUTO, "WiFiPanel instance is NULL");
        return WP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG_AUTO, "Loading saved credentials from NVS...");

    if (!_wp_nvs_load_saved_wifi(wp)) {
        ESP_LOGI(TAG_AUTO, "No valid saved config, starting AP configuration mode");
        return WiFiPanel_ConfigViaAP(wp);
    }

    ESP_LOGI(TAG_AUTO, "Found SSID: %s, attempting to connect...", (char *)wp->sta_config.ssid);

    WiFiPanel_Status st = WiFiPanel_StartSTA(wp);
    if (st != WP_OK){
        return st;
    }

    TickType_t timeout = (wp->sta_retry_num == -1) ? portMAX_DELAY : pdMS_TO_TICKS(30000);

    EventBits_t bits = xEventGroupWaitBits(wp->priv->event.group, WP_EVENT_BIT_STACONNECTED | WP_EVENT_BIT_STADISCONNECTED, pdFALSE, pdFALSE, timeout);

    if (bits & WP_EVENT_BIT_STACONNECTED) {
        ESP_LOGI(TAG_AUTO, "Connected to: %s", (char *)wp->sta_config.ssid);
        return WP_OK;
    }

    ESP_LOGW(TAG_AUTO, "Failed to connect to: %s, falling back to portal", (char *)wp->sta_config.ssid);

    WiFiPanel_Stop(wp);
    return WiFiPanel_ConfigViaAP(wp);
}

wifi_mode_t WiFiPanel_GetMode(void) {
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_wifi_get_mode(&mode);
    return mode;
}

bool WiFiPanel_IsConnected(const WiFiPanel *wp) {
    if (!wp || !wp->priv->event.group) {
        return false;
    }
    return (xEventGroupGetBits(wp->priv->event.group) & WP_EVENT_BIT_STACONNECTED) != 0;
}