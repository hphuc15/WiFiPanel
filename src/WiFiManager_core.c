#include "WiFiManager_private.h"

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_event.h"

static const char *TAG = "[WiFi]";

/* ===================== INTERNAL: WIFI CORE ========================== */

/** Dispatch WiFi and IP events to update event group bits and invoke callbacks. */
static void WiFiManager_EventHandler(void *arg, esp_event_base_t event_base,
                                     int32_t event_id, void *event_data)
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
                ESP_LOGE(TAG, "[AP] Failed to get IP: %s", esp_err_to_name(err));
                return;
            }
            ESP_LOGI(TAG, "[AP] SoftAP Started. SSID: %s, IP: " IPSTR,
                     (char *)wm->config.ap.ssid, IP2STR(&ip_info.ip));
            break;
        }

        case WIFI_EVENT_AP_STACONNECTED:
        {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "[AP] Station " MACSTR " join, AID: %d",
                     MAC2STR(event->mac), event->aid);
            break;
        }

        case WIFI_EVENT_AP_STOP:
        {
            xEventGroupClearBits(wm->event.group, WM_EVENT_BIT_APSTART);
            ESP_LOGI(TAG, "[AP] AP Stopped");
            break;
        }

        case WIFI_EVENT_STA_START:
        {
            xEventGroupSetBits(wm->event.group, WM_EVENT_BIT_STASTART);
            err = esp_wifi_connect();
            if (err != ESP_OK)
                ESP_LOGE(TAG, "[STA] Failed to connect to the AP: %s", esp_err_to_name(err));
            break;
        }

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
            ESP_LOGE(TAG, "[STA] Disconnected from AP, reason: %d", event->reason);

            if (wm->sta_retry_num > 0)
            {
                wm->sta_retry_num--;
                ESP_LOGW(TAG, "[STA] Retrying... (%d left)", wm->sta_retry_num);
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
        ESP_LOGI(TAG, "[STA] Connected to the AP, ip: " IPSTR,
                 IP2STR(&event->ip_info.ip));
        if (wm->ConnectedAP_Cb)
            wm->ConnectedAP_Cb();
    }
}

/* ===================== PUBLIC: WIFI LIFECYCLE ======================= */

void WiFiManager_Init(WiFiManager_t *wm)
{
    esp_err_t ret;

    ret = esp_netif_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[Init] Failed to initialize network interface (netif)!");
        return;
    }
    if (wm->event.group == NULL)
        wm->event.group = xEventGroupCreate();

    ret = esp_event_loop_create_default();
    if (ret == ESP_ERR_INVALID_STATE)
        ESP_LOGW(TAG, "Default event loop has already been created");
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create event loop, error: %s", esp_err_to_name(ret));
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
    if (ret != ESP_ERR_WIFI_NOT_INIT)
        WiFiManager_Stop(wm);

    wifi_init_config_t wifi_drv_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_drv_cfg));
}

void WiFiManager_StartSTA(WiFiManager_t *wm)
{
    if (wm->netif == NULL)
        wm->netif = esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START,
                                                        WiFiManager_EventHandler, wm, &wm->event.sta_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                                        WiFiManager_EventHandler, wm, &wm->event.sta_disc_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        WiFiManager_EventHandler, wm, &wm->event.ip_handle));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (strlen((char *)wm->config.sta.ssid) > 0)
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wm->config));
    else
        ESP_LOGI(TAG, "[STA] Attempt to load saved credentials.");

    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(pdMS_TO_TICKS(100));
}

void WiFiManager_StartAP(WiFiManager_t *wm)
{
    if (!wm->netif)
        wm->netif = esp_netif_create_default_wifi_ap();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED,
                                                        WiFiManager_EventHandler, wm, &wm->event.ap_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED,
                                                        WiFiManager_EventHandler, wm, &wm->event.ap_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_START,
                                                        WiFiManager_EventHandler, wm, &wm->event.ap_handle));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wm->config));
    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(pdMS_TO_TICKS(100));
}

void WiFiManager_Stop(WiFiManager_t *wm)
{
    if (!wm)
    {
        ESP_LOGE(TAG, "[Stop] WiFi manager is NULL");
        return;
    }

    esp_err_t err;
    wifi_mode_t mode;

    err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "[Stop] Failed to get WiFi mode: %s", esp_err_to_name(err));
        mode = WIFI_MODE_NULL;
    }
    ESP_LOGI(TAG, "[Stop] Stopping WiFi (mode: %d)", mode);

    if (wm->event.ap_handle)
    {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wm->event.ap_handle);
        wm->event.ap_handle = NULL;
    }
    if (wm->event.sta_handle)
    {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wm->event.sta_handle);
        wm->event.sta_handle = NULL;
    }
    if (wm->event.sta_disc_handle)
    {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wm->event.sta_disc_handle);
        wm->event.sta_disc_handle = NULL;
    }
    if (wm->event.ip_handle)
    {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wm->event.ip_handle);
        wm->event.ip_handle = NULL;
    }

    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA)
    {
        err = esp_wifi_disconnect();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
        {
            ESP_LOGW(TAG, "[Stop] Disconnect failed: %s", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(TAG, "[Stop] STA disconnected requested");
        }
    }

    err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
    {
        ESP_LOGE(TAG, "[Stop] Failed to stop WiFi: %s", esp_err_to_name(err));
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
    ESP_LOGI(TAG, "[Stop] WiFi stopped.");
}

void WiFiManager_Deinit(WiFiManager_t *wm)
{
    esp_err_t err;
    WiFiManager_Stop(wm);

    err = esp_wifi_deinit();
    if (err == ESP_ERR_WIFI_NOT_INIT)
    {
        ESP_LOGE(TAG, "[Deinit] WiFi driver was not installed by esp_wifi_init");
        return;
    }

    err = nvs_flash_deinit();
    if (err == ESP_ERR_NVS_NOT_INITIALIZED)
    {
        ESP_LOGE(TAG, "[Deinit] The storage driver is not initialized");
        return;
    }

    err = esp_event_loop_delete_default();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "[Deinit] error: %s", esp_err_to_name(err));
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG, "[Deinit] - WiFi deinitialized");
}

void WiFiManager_ConfigViaAP(WiFiManager_t *wm)
{
    memset(&wm->config, 0, sizeof(wifi_config_t));
    wm->config.ap = WM_AP_CONFIG_DEFAULT();
    WiFiManager_StartAP(wm);
    EventBits_t bits = xEventGroupWaitBits(wm->event.group, WM_EVENT_BIT_APSTART, pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));
    if (!(bits & WM_EVENT_BIT_APSTART))
    {
        ESP_LOGE(TAG, "[ConfigViaAP] Timeout,AP failed to start");
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
        ESP_LOGE(TAG, "[ConfigViaAP] Timeout no credentials received");
        WiFiManager_StopDNS(dns);
        WiFiManager_StopWebServer(wm);
        WiFiManager_Stop(wm);
        return;
    }

    WiFiManager_StopDNS(dns);
    WiFiManager_StopWebServer(wm);
    WiFiManager_Stop(wm);

    /* Copy credentials into clean STA config */
    wifi_auth_mode_t authmode = wm->config.sta.threshold.authmode;
    char ssid[32], password[64];
    strncpy(ssid, (char *)wm->config.sta.ssid, sizeof(ssid) - 1);
    strncpy(password, (char *)wm->config.sta.password, sizeof(password) - 1);
    ssid[sizeof(ssid) - 1] = '\0';
    password[sizeof(password) - 1] = '\0';

    memset(&wm->config, 0, sizeof(wm->config));
    strncpy((char *)wm->config.sta.ssid, ssid, sizeof(wm->config.sta.ssid) - 1);
    strncpy((char *)wm->config.sta.password, password, sizeof(wm->config.sta.password) - 1);
    wm->config.sta.threshold.authmode = authmode;
    wm->config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wm->config.sta.failure_retry_cnt = wm->sta_retry_num;

    ESP_LOGI(TAG, "[STA_ConfigViaAP] Switch to STA Mode");
    WiFiManager_StartSTA(wm);
}

void WiFiManager_AutoConnect(WiFiManager_t *wm)
{
    if (!wm)
    {
        ESP_LOGE(TAG, "[AutoConnect] WiFiManager instance is NULL");
        return;
    }

    ESP_LOGI(TAG, "[AutoConnect] Attempting to load saved credentials from NVS...");

    /* Get saved STA configuration from NVS */
    wifi_config_t saved_config;
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &saved_config);
    wm->sta_retry_num = 5;

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "[AutoConnect] Failed to get config from NVS: %s", esp_err_to_name(err));
        ESP_LOGI(TAG, "[AutoConnect] Starting AP configuration mode");
        WiFiManager_ConfigViaAP(wm);
        return;
    }

    /* Check if saved credentials exist */
    if (strlen((char *)saved_config.sta.ssid) == 0)
    {
        ESP_LOGW(TAG, "[AutoConnect] No saved SSID found in NVS, starting AP configuration mode");
        WiFiManager_ConfigViaAP(wm);
        return;
    }

    /* Avoid using captive portal AP config as STA */
    if (strcmp((char *)saved_config.sta.ssid, "ESP32_Config") == 0)
    {
        ESP_LOGW(TAG, "[AutoConnect] Saved SSID is config AP (%s), ignoring...",
                 (char *)saved_config.sta.ssid);

        WiFiManager_ConfigViaAP(wm);
        return;
    }

    /* Copy saved config to wm structure */
    memset(&wm->config, 0, sizeof(wifi_config_t));
    memcpy(&wm->config.sta, &saved_config.sta, sizeof(wifi_sta_config_t));

    ESP_LOGI(TAG, "[AutoConnect] Found saved SSID: %s, attempting to connect...", (char *)saved_config.sta.ssid);

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
        ESP_LOGI(TAG, "[AutoConnect] Successfully connected to AP: %s", (char *)wm->config.sta.ssid);
        return;
    }

    /* Connection failed - disconnect and clean up */
    ESP_LOGW(TAG, "[AutoConnect] Failed to connect to saved AP: %s", (char *)wm->config.sta.ssid);

    /* Stop STA mode before starting AP mode */
    WiFiManager_Stop(wm);

    /* Enter AP configuration mode after STA connect failure */
    ESP_LOGI(TAG, "[AutoConnect] Falling back to AP configuration mode");
    WiFiManager_ConfigViaAP(wm);
}

wifi_mode_t WiFiManager_GetMode(void)
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK)
        mode = WIFI_MODE_NULL;
    return mode;
}