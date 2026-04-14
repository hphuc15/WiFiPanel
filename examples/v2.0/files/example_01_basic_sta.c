/**
 * @file example_01_basic_sta.c
 * @brief Ví dụ đơn giản nhất: kết nối đến một Access Point cố định.
 *
 * Luồng hoạt động:
 *   1. Khởi tạo WiFiManager.
 *   2. Điền SSID + password vào config.
 *   3. Gọi WiFiManager_StartSTA() và chờ kết nối.
 *   4. Thực hiện công việc chính (ở đây chỉ in log).
 *   5. Dọn dẹp khi xong.
 *
 * Phù hợp khi credentials đã biết trước và không thay đổi
 * (ví dụ: thiết bị nhà máy, demo, môi trường dev).
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "WiFiManager.h"

static const char *TAG = "Example01";

/* ── Credentials (cứng trong code — chỉ dùng cho dev/demo) ────────── */
#define WIFI_SSID     "YourSSID"
#define WIFI_PASSWORD "YourPassword"

/* ── WiFiManager instance — zero-init bắt buộc ────────────────────── */
static WiFiManager_t s_wm = {0};

/* ── Callbacks ─────────────────────────────────────────────────────── */

static void on_connected(void)
{
    ESP_LOGI(TAG, "✓ Đã kết nối WiFi. Bắt đầu gửi dữ liệu...");
}

static void on_disconnected(void)
{
    ESP_LOGW(TAG, "✗ Mất kết nối WiFi. Đang thử lại...");
}

/* ── app_main ──────────────────────────────────────────────────────── */

void app_main(void)
{
    /* 1. Gán callbacks (tuỳ chọn) */
    s_wm.ConnectedAP_Cb    = on_connected;
    s_wm.DisconnectedAP_Cb = on_disconnected;

    /* 2. Khởi tạo driver */
    WiFiManager_Init(&s_wm);

    /* 3. Điền credentials vào STA config */
    strncpy((char *)s_wm.config.sta.ssid,     WIFI_SSID,
            sizeof(s_wm.config.sta.ssid) - 1);
    strncpy((char *)s_wm.config.sta.password, WIFI_PASSWORD,
            sizeof(s_wm.config.sta.password) - 1);
    s_wm.config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    s_wm.config.sta.scan_method        = WIFI_ALL_CHANNEL_SCAN;

    /* 4. Khởi động STA */
    WiFiManager_StartSTA(&s_wm);

    /* 5. Chờ kết nối hoặc timeout 10 s */
    EventBits_t bits = xEventGroupWaitBits(
        s_wm.event.group,
        WM_EVENT_BIT_STACONNECTED | WM_EVENT_BIT_STADISCONNECTED,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(10000));

    if (bits & WM_EVENT_BIT_STACONNECTED)
    {
        ESP_LOGI(TAG, "Kết nối thành công đến '%s'", WIFI_SSID);

        /* ── Vòng lặp chính ── */
        while (1)
        {
            ESP_LOGI(TAG, "Đang chạy... (WiFi mode: %d)", WiFiManager_GetMode());
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
    else
    {
        ESP_LOGE(TAG, "Không thể kết nối đến '%s'", WIFI_SSID);
    }

    /* 6. Dọn dẹp */
    WiFiManager_Deinit(&s_wm);
}
