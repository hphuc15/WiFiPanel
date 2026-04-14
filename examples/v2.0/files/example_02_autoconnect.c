/**
 * @file example_02_autoconnect.c
 * @brief AutoConnect — tự động load credentials từ NVS, fallback về
 *        captive portal nếu chưa cấu hình hoặc kết nối thất bại.
 *
 * Luồng hoạt động:
 *   ┌──────────────────────────────────────────────┐
 *   │  Khởi động                                   │
 *   │       ↓                                      │
 *   │  NVS có SSID? ──Không──→ Mở AP + Portal      │
 *   │       │                       ↓              │
 *   │      Có              User nhập credentials   │
 *   │       ↓                       ↓              │
 *   │  Thử kết nối STA ←───────────┘              │
 *   │       ↓                                      │
 *   │  Thành công? ──Không──→ Mở AP + Portal lại   │
 *   │       │                                      │
 *   │      Có                                      │
 *   │       ↓                                      │
 *   │  Chạy ứng dụng                               │
 *   └──────────────────────────────────────────────┘
 *
 * Đây là pattern phổ biến nhất cho IoT production devices.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "WiFiManager.h"

static const char *TAG = "Example02";

static WiFiManager_t s_wm = {0};

/* ── Callbacks ─────────────────────────────────────────────────────── */

static void on_connected(void)
{
    ESP_LOGI(TAG, "✓ WiFi kết nối thành công");
}

static void on_disconnected(void)
{
    ESP_LOGW(TAG, "WiFi bị ngắt kết nối");
    /*
     * Ở production, nên trigger reconnect logic ở đây.
     * Ví dụ: xTaskNotify(s_main_task, RECONNECT_BIT, eSetBits);
     */
}

/* ── Vòng lặp ứng dụng ─────────────────────────────────────────────── */

static void run_application(void)
{
    ESP_LOGI(TAG, "Ứng dụng đang chạy (SSID: %s)", (char *)s_wm.config.sta.ssid);

    uint32_t tick = 0;
    while (1)
    {
        /* Kiểm tra còn kết nối không trước khi làm việc */
        EventBits_t bits = xEventGroupGetBits(s_wm.event.group);
        if (!(bits & WM_EVENT_BIT_STACONNECTED))
        {
            ESP_LOGW(TAG, "Mất kết nối, dừng vòng lặp");
            break;
        }

        ESP_LOGI(TAG, "[tick %lu] Đang gửi dữ liệu sensor...", tick++);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/* ── app_main ──────────────────────────────────────────────────────── */

void app_main(void)
{
    s_wm.ConnectedAP_Cb    = on_connected;
    s_wm.DisconnectedAP_Cb = on_disconnected;
    s_wm.sta_retry_num     = 3; /* số lần thử lại khi mất kết nối */

    WiFiManager_Init(&s_wm);

    /*
     * AutoConnect xử lý toàn bộ:
     *   - Load NVS → thử kết nối → nếu lỗi mở portal → nhận credentials → thử lại
     * Hàm này block cho đến khi kết nối thành công hoặc portal timeout.
     */
    WiFiManager_AutoConnect(&s_wm);

    /* Kiểm tra kết quả */
    EventBits_t bits = xEventGroupGetBits(s_wm.event.group);
    if (bits & WM_EVENT_BIT_STACONNECTED)
    {
        run_application();
    }
    else
    {
        ESP_LOGE(TAG, "Không thể kết nối WiFi sau khi qua portal. Reset sau 5s...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }

    WiFiManager_Deinit(&s_wm);
}
