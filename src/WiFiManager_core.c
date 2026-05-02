#include "WiFiManager_private.h"

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_event.h"

// static const char *TAG          = "[WM]";
static const char *TAG_INIT     = "[WM][INIT]";
static const char *TAG_STA      = "[WM][STA]";
static const char *TAG_AP       = "[WM][AP]";
static const char *TAG_STOP     = "[WM][STOP]";
static const char *TAG_DEINIT   = "[WM][DEINIT]";
static const char *TAG_PORTAL   = "[WM][PORTAL]";
static const char *TAG_AUTO     = "[WM][AUTO]";

/* ===================== INTERNAL: WIFI CORE ========================== */

/** Dispatch WiFi and IP events to update event group bits and invoke callbacks. */
static void _wm_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    WiFiManager_t *wm = (WiFiManager_t *)arg;
    esp_err_t err;

    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_AP_START:
        {
            xEventGroupSetBits(wm->event.group, WM_EVENT_BIT_APSTART);

            esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
            esp_netif_ip_info_t ip_info;
            err = esp_netif_get_ip_info(ap_netif, &ip_info);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG_AP, "Failed to get IP: %s", esp_err_to_name(err));
                return;
            }
            ESP_LOGI(TAG_AP, "SoftAP Started. SSID: %s, IP: " IPSTR, (char *)wm->ap_config.ssid, IP2STR(&ip_info.ip));
            break;
        }

        case WIFI_EVENT_AP_STACONNECTED:
        {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG_AP, "Station " MACSTR " join, AID: %d", MAC2STR(event->mac), event->aid);
            break;
        }

        case WIFI_EVENT_AP_STOP:
        {
            xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_APSTART);
            ESP_LOGI(TAG_AP, "AP Stopped");
            break;
        }

        case WIFI_EVENT_STA_START:
        {
            xEventGroupSetBits(wm->event.group, WM_EVENT_BIT_STASTART);
            vTaskDelay(pdMS_TO_TICKS(100));
            err = esp_wifi_connect();
            if (err != ESP_OK){
                ESP_LOGE(TAG_STA, "Failed to connect to the AP: %s", esp_err_to_name(err));
            }
            break;
        }

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            if (!(xEventGroupGetBits(wm->event.group) & WM_EVENT_BIT_STASTART)){
                break;
            }

            wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
            ESP_LOGE(TAG_STA, "Disconnected to AP, reason: %d", event->reason);

            if (wm->sta_retry_num > 0)
            {
                wm->sta_retry_num--;
                ESP_LOGW(TAG_STA, "Retrying... (%d left)", wm->sta_retry_num);
                esp_wifi_connect();
            }
            else
            {
                xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_STACONNECTED);
                xEventGroupSetBits(wm->event.group, WM_EVENT_BIT_STADISCONNECTED);
                if (wm->DisconnectedAP_Cb){
                    wm->DisconnectedAP_Cb();
                }
            }
            break;
        }
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_STADISCONNECTED);
        xEventGroupSetBits(wm->event.group, WM_EVENT_BIT_STACONNECTED);
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_STA, "Connected to the AP, ip: " IPSTR, IP2STR(&event->ip_info.ip));
        if (wm->ConnectedAP_Cb){
            wm->ConnectedAP_Cb();
        }
    }
}

/* ===================== PUBLIC: WIFI LIFECYCLE ======================= */

void WiFiManager_Init(WiFiManager_t *wm)
{
    esp_err_t ret;

    ret = esp_netif_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_INIT, "Failed to initialize network interface (netif)!");
        return;
    }
    if (wm->event.group == NULL){
        wm->event.group = xEventGroupCreate();
    }

    ret = esp_event_loop_create_default();
    if (ret == ESP_ERR_INVALID_STATE)
        ESP_LOGW(TAG_INIT, "Default event loop has already been created");
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_INIT, "Failed to create event loop, error: %s", esp_err_to_name(ret));
        return;
    }

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_mode_t mode;
    ret = esp_wifi_get_mode(&mode);
    if (ret != ESP_ERR_WIFI_NOT_INIT){
        WiFiManager_Stop(wm);
    }

    wifi_init_config_t wifi_drv_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_drv_cfg));
}

void WiFiManager_StartSTA(WiFiManager_t *wm)
{
    if (wm->netif == NULL){
        wm->netif = esp_netif_create_default_wifi_sta();
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START, _wm_event_handler, wm, &wm->event.sta_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, _wm_event_handler, wm, &wm->event.sta_disc_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, _wm_event_handler, wm, &wm->event.ip_handle));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t config = {
        .sta = wm->sta_config
    };
    if (strlen((char *)wm->sta_config.ssid) > 0){
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    }
    else{
        ESP_LOGI(TAG_STA, "Attempt to load saved credentials.");
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(pdMS_TO_TICKS(100));
}

void WiFiManager_StartAP(WiFiManager_t *wm)
{
    if (!wm->netif){
        wm->netif = esp_netif_create_default_wifi_ap();
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, _wm_event_handler, wm, &wm->event.ap_connected_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, _wm_event_handler, wm, &wm->event.ap_disconnected_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_START, _wm_event_handler, wm, &wm->event.ap_start_handle));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    wifi_config_t config = {
        .ap = wm->ap_config
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(pdMS_TO_TICKS(100));
}

void WiFiManager_Stop(WiFiManager_t *wm)
{
    if (!wm)
    {
        ESP_LOGE(TAG_STOP, "WiFi manager is NULL");
        return;
    }

    esp_err_t err;
    wifi_mode_t mode;

    err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG_STOP, "Failed to get WiFi mode: %s", esp_err_to_name(err));
        mode = WIFI_MODE_NULL;
    }
    ESP_LOGI(TAG_STOP, "Stopping WiFi (mode: %d)", mode);

    if (wm->event.ap_connected_handle) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, wm->event.ap_connected_handle);
        wm->event.ap_connected_handle = NULL;
    }
    if (wm->event.ap_disconnected_handle) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, wm->event.ap_disconnected_handle);
        wm->event.ap_disconnected_handle = NULL;
    }
    if (wm->event.ap_start_handle) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_AP_START, wm->event.ap_start_handle);
        wm->event.ap_start_handle = NULL;
    }
    if (wm->event.sta_handle) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, wm->event.sta_handle);
        wm->event.sta_handle = NULL;
    }
    if (wm->event.sta_disc_handle) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wm->event.sta_disc_handle);
        wm->event.sta_disc_handle = NULL;
    }
    if (wm->event.ip_handle) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wm->event.ip_handle);
        wm->event.ip_handle = NULL;
    }

    /* Only for STA and APSTA mode */
    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
        err = esp_wifi_disconnect();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED){
            ESP_LOGW(TAG_STOP, "Disconnect failed: %s", esp_err_to_name(err));
        }
        else{
            ESP_LOGI(TAG_STOP, "STA disconnected requested");
        }
    }

    err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED){
        ESP_LOGE(TAG_STOP, "Failed to stop WiFi: %s", esp_err_to_name(err));
    }

    xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_APSTART);
    xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_STASTART);
    xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_STACONNECTED);
    xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_STADISCONNECTED);

    if (wm->netif)
    {
        esp_netif_destroy_default_wifi(wm->netif);
        wm->netif = NULL;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG_STOP, "WiFi stopped.");
}

void WiFiManager_Deinit(WiFiManager_t *wm)
{
    esp_err_t err;
    WiFiManager_Stop(wm);

    err = esp_wifi_deinit();
    if (err == ESP_ERR_WIFI_NOT_INIT)
    {
        ESP_LOGE(TAG_DEINIT, "WiFi driver was not installed by esp_wifi_init");
        return;
    }

    err = nvs_flash_deinit();
    if (err == ESP_ERR_NVS_NOT_INITIALIZED)
    {
        ESP_LOGE(TAG_DEINIT, "The storage driver is not initialized");
        return;
    }

    err = esp_event_loop_delete_default();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_DEINIT, "error: %s", esp_err_to_name(err));
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG_DEINIT, "WiFi deinitialized");
}

void WiFiManager_ConfigViaAP(WiFiManager_t *wm)
{
    wm->ap_config = WM_AP_CONFIG_DEFAULT();
    WiFiManager_StartAP(wm);
    EventBits_t bits = xEventGroupWaitBits(wm->event.group, WM_EVENT_BIT_APSTART, pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));
    if (!(bits & WM_EVENT_BIT_APSTART))
    {
        ESP_LOGE(TAG_PORTAL, "Timeout, AP failed to start");
        WiFiManager_Stop(wm);
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    WiFiManager_SetCaptivePortalURI(wm);
    WiFiManager_StartWebServer(wm);

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(wm->netif, &ip_info);
    void *dns = WiFiManager_StartDNS(ip_info.ip);

    wm->portal_waiting_task = xTaskGetCurrentTaskHandle();
    BaseType_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WM_PORTAL_TIMOUT_MS));
    wm->portal_waiting_task = NULL;

    if (!notified)
    {
        ESP_LOGE(TAG_PORTAL, "Timeout no credentials received");
        WiFiManager_StopDNS(dns);
        WiFiManager_StopWebServer(wm);

        ESP_LOGI(TAG_PORTAL, "Stopping AP...");
        WiFiManager_Stop(wm);
        ESP_LOGI(TAG_PORTAL, "AP stopped");
        return;
    }

    WiFiManager_StopDNS(dns);
    WiFiManager_StopWebServer(wm);

    ESP_LOGI(TAG_PORTAL, "Stopping AP...");
    WiFiManager_Stop(wm);
    ESP_LOGI(TAG_PORTAL, "AP stopped");
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Copy credentials into clean STA config */
    wifi_auth_mode_t authmode = wm->sta_config.threshold.authmode;
    char ssid[33], password[65];
    snprintf(ssid, sizeof(ssid), "%s", (char *)wm->sta_config.ssid);
    snprintf(password, sizeof(password), "%s", (char *)wm->sta_config.password);

    strncpy((char *)wm->sta_config.ssid, ssid, sizeof(wm->sta_config.ssid) - 1);
    strncpy((char *)wm->sta_config.password, password, sizeof(wm->sta_config.password) - 1);
    wm->sta_config.threshold.authmode = authmode;
    wm->sta_config.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wm->sta_config.failure_retry_cnt = wm->sta_retry_num;

    ESP_LOGI(TAG_PORTAL, "Switching to STA...");
    WiFiManager_StartSTA(wm);
}

void WiFiManager_AutoConnect(WiFiManager_t *wm)
{
    if (!wm)
    {
        ESP_LOGE(TAG_AUTO, "WiFiManager instance is NULL");
        return;
    }

    ESP_LOGI(TAG_AUTO, "Attempting to load saved credentials from NVS...");

    /* Get saved STA configuration from NVS */
    wifi_config_t saved_config;
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &saved_config);
    // wm->sta_retry_num = 3;

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_AUTO, "Failed to get config from NVS: %s", esp_err_to_name(err));
        ESP_LOGI(TAG_AUTO, "Starting AP configuration mode");
        WiFiManager_ConfigViaAP(wm);
        return;
    }

    /* Check if saved credentials exist */
    if (strlen((char *)saved_config.sta.ssid) == 0)
    {
        ESP_LOGW(TAG_AUTO, "No saved SSID found in NVS, starting AP configuration mode");
        WiFiManager_ConfigViaAP(wm);
        return;
    }

    /* Avoid using captive portal AP config as STA */
    if (strcmp((char *)saved_config.sta.ssid, WM_AP_SSID_DEFAULT) == 0)
    {
        ESP_LOGW(TAG_AUTO, "Saved SSID is config AP (%s), ignoring...",(char *)saved_config.sta.ssid);

        WiFiManager_ConfigViaAP(wm);
        return;
    }

    /* Copy saved config to wm structure */
    memcpy(&wm->sta_config, &saved_config.sta, sizeof(wifi_sta_config_t));

    ESP_LOGI(TAG_AUTO, "Found saved SSID: %s, attempting to connect...", (char *)saved_config.sta.ssid);

    xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_STACONNECTED | WM_EVENT_BIT_STADISCONNECTED);
    /* Try to connect to saved AP */
    WiFiManager_StartSTA(wm);

    /* Wait for connection or timeout */
    EventBits_t bits = xEventGroupWaitBits(
        wm->event.group,
        WM_EVENT_BIT_STACONNECTED | WM_EVENT_BIT_STADISCONNECTED,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(15000));

    /* Check if we got connected */
    if (bits & WM_EVENT_BIT_STACONNECTED)
    {
        ESP_LOGI(TAG_AUTO, "Successfully connected to AP: %s", (char *)wm->sta_config.ssid);
        return;
    }

    /* Connection failed - disconnect and clean up */
    ESP_LOGW(TAG_AUTO, "Failed to connect to saved AP: %s", (char *)wm->sta_config.ssid);

    /* Stop STA mode before starting AP mode */
    WiFiManager_Stop(wm);

    /* Enter AP configuration mode after STA connect failure */
    ESP_LOGI(TAG_AUTO, "Falling back to AP configuration mode");
    WiFiManager_ConfigViaAP(wm);
}

wifi_mode_t WiFiManager_GetMode(void)
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK){
        mode = WIFI_MODE_NULL;
    }
    return mode;
}

bool WiFiManager_IsConnectedAP(WiFiManager_t *wm)
{
    if (!wm || !wm->event.group){
        return false;
    }
    return (xEventGroupGetBits(wm->event.group) & WM_EVENT_BIT_STACONNECTED) != 0;
}