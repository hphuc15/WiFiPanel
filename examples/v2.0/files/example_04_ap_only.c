/**
 * @file example_04_ap_only.c
 * @brief Chạy thuần AP mode — thiết bị là điểm truy cập Wi-Fi,
 *        không kết nối internet. Dùng cho setup wizard, local config UI.
 *
 * Ứng dụng:
 *   - Thiết bị cầm tay tự host web dashboard nội bộ
 *   - Setup wizard lần đầu trước khi cấu hình STA
 *   - Data logger offline, truy cập qua browser
 *
 * Sau khi StartAP():
 *   • SSID: "ESP32_AP"  |  Password: "12345678"
 *   • IP:   192.168.4.1 (mặc định softAP)
 *   • Trình duyệt mở http://192.168.4.1 → giao diện cấu hình
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "WiFiManager.h"
#include "WiFiManager_private.h"  /* StartWebServer / StopWebServer */

static const char *TAG = "Example04";

#define AP_SSID     "ESP32_AP"
#define AP_PASSWORD "12345678"

static WiFiManager_t s_wm = {0};

/* ── app_main ──────────────────────────────────────────────────────── */

void app_main(void)
{
    WiFiManager_Init(&s_wm);

    /* Cấu hình AP thủ công */
    memset(&s_wm.config, 0, sizeof(s_wm.config));
    strncpy((char *)s_wm.config.ap.ssid,     AP_SSID,     sizeof(s_wm.config.ap.ssid) - 1);
    strncpy((char *)s_wm.config.ap.password, AP_PASSWORD, sizeof(s_wm.config.ap.password) - 1);
    s_wm.config.ap.ssid_len       = strlen(AP_SSID);
    s_wm.config.ap.max_connection = 4;
    s_wm.config.ap.authmode       = WIFI_AUTH_WPA2_PSK;

    /* Khởi động AP */
    WiFiManager_StartAP(&s_wm);

    /* Chờ AP sẵn sàng */
    EventBits_t bits = xEventGroupWaitBits(
        s_wm.event.group,
        WM_EVENT_BIT_APSTART,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(10000));

    if (!(bits & WM_EVENT_BIT_APSTART))
    {
        ESP_LOGE(TAG, "AP không khởi động được");
        WiFiManager_Deinit(&s_wm);
        return;
    }

    ESP_LOGI(TAG, "AP đang chạy. SSID: '%s'", AP_SSID);
    ESP_LOGI(TAG, "Kết nối và truy cập http://192.168.4.1");

    /* Bật captive portal URI để browser tự mở */
    WiFiManager_SetCaptivePortalURI(&s_wm);

    /* Thêm trường tùy chỉnh vào trang portal */
    WiFiManagerPage_Init(&s_wm);
    WiFiManagerPage_AddParam(&s_wm, "device_id", "Device ID",
                             "ví dụ: node-01", "esp32-node", "text", true);
    WiFiManagerPage_AddParam(&s_wm, "sample_rate", "Sample Rate (ms)",
                             "ví dụ: 1000", "1000", "number", false);

    /* Khởi động web server */
    httpd_handle_t server = WiFiManager_StartWebServer(&s_wm);
    if (!server)
    {
        ESP_LOGE(TAG, "Không thể khởi động web server");
        WiFiManager_Stop(&s_wm);
        WiFiManager_Deinit(&s_wm);
        return;
    }

    /* Chờ form submit */
    s_wm.portal_waiting_task = xTaskGetCurrentTaskHandle();
    BaseType_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5 * 60 * 1000));
    s_wm.portal_waiting_task = NULL;

    if (notified)
    {
        const char *dev_id      = WiFiManagerPage_GetParam(&s_wm, "device_id");
        const char *sample_rate = WiFiManagerPage_GetParam(&s_wm, "sample_rate");
        ESP_LOGI(TAG, "Cấu hình nhận được:");
        ESP_LOGI(TAG, "  device_id   = %s", dev_id      ? dev_id      : "(trống)");
        ESP_LOGI(TAG, "  sample_rate = %s ms", sample_rate ? sample_rate : "1000");
    }
    else
    {
        ESP_LOGW(TAG, "Timeout — không nhận được cấu hình");
    }

    /* Dọn dẹp */
    WiFiManager_StopWebServer(&s_wm);
    WiFiManager_Stop(&s_wm);
    WiFiManager_Deinit(&s_wm);
}
