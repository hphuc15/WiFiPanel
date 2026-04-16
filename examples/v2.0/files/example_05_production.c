/**
 * @file example_05_production.c
 * @brief Pattern production hoàn chỉnh: AutoConnect + watchdog tự reconnect
 *        + task riêng biệt + phân tách logic WiFi khỏi ứng dụng.
 *
 * Kiến trúc task:
 *
 *   app_main
 *     ├─ wifi_manager_task   ← quản lý kết nối, tự reconnect khi mất
 *     └─ app_task            ← logic ứng dụng, chỉ chạy khi có WiFi
 *
 * Giao tiếp giữa các task qua EventGroup của WiFiManager:
 *   WM_EVENT_BIT_STACONNECTED    → app_task được phép gửi data
 *   WM_EVENT_BIT_STADISCONNECTED → app_task dừng, wifi_task reconnect
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "WiFiManager.h"

static const char *TAG_WIFI = "WiFiTask";
static const char *TAG_APP  = "AppTask";

/* Shared WiFiManager instance */
static WiFiManager_t s_wm = {0};

/* Bit báo hiệu cho app_task thoát vòng lặp khi hệ thống shutdown */
#define APP_STOP_BIT BIT4

/* ── WiFi callbacks ────────────────────────────────────────────────── */

static void on_connected(void)
{
    ESP_LOGI(TAG_WIFI, "WiFi CONNECTED → SSID: %s", (char *)s_wm.config.sta.ssid);
}

static void on_disconnected(void)
{
    ESP_LOGW(TAG_WIFI, "WiFi DISCONNECTED — watchdog sẽ reconnect");
}

/* ── App task — chỉ chạy khi WiFi đã kết nối ──────────────────────── */

static void app_task(void *arg)
{
    ESP_LOGI(TAG_APP, "App task khởi động, chờ WiFi...");

    while (1)
    {
        /* Chờ đến khi có kết nối (hoặc lệnh stop) */
        EventBits_t bits = xEventGroupWaitBits(
            s_wm.event.group,
            WM_EVENT_BIT_STACONNECTED | APP_STOP_BIT,
            pdFALSE, pdFALSE,
            portMAX_DELAY);

        if (bits & APP_STOP_BIT)
        {
            ESP_LOGI(TAG_APP, "Nhận lệnh dừng, thoát app_task");
            break;
        }

        if (bits & WM_EVENT_BIT_STACONNECTED)
        {
            ESP_LOGI(TAG_APP, "WiFi ready — bắt đầu gửi dữ liệu");

            /* Vòng lặp gửi data, chạy khi còn kết nối */
            while (xEventGroupGetBits(s_wm.event.group) & WM_EVENT_BIT_STACONNECTED)
            {
                /* === Đặt logic ứng dụng ở đây === */
                ESP_LOGI(TAG_APP, "Gửi telemetry...");
                /* http_post_sensor_data(); */
                /* mqtt_publish("sensor/temp", "25.3"); */

                vTaskDelay(pdMS_TO_TICKS(5000));
            }

            ESP_LOGW(TAG_APP, "Mất WiFi, tạm dừng gửi data, chờ kết nối lại...");
        }
    }

    vTaskDelete(NULL);
}

/* ── WiFi manager task — xử lý kết nối và reconnect ───────────────── */

static void wifi_manager_task(void *arg)
{
    ESP_LOGI(TAG_WIFI, "WiFi manager task khởi động");

    /* Lần đầu: AutoConnect (load NVS → thử kết nối → portal nếu cần) */
    WiFiManager_AutoConnect(&s_wm);

    /* Watchdog: tự reconnect khi mất kết nối */
    static const uint32_t RECONNECT_DELAY_MS = 5000;
    static const uint32_t CONNECT_TIMEOUT_MS = 15000;
    static const uint8_t  MAX_RETRIES        = 5;

    uint8_t retry_count = 0;

    while (1)
    {
        /* Chờ sự kiện disconnect */
        EventBits_t bits = xEventGroupWaitBits(
            s_wm.event.group,
            WM_EVENT_BIT_STADISCONNECTED,
            pdTRUE,   /* xóa bit sau khi nhận */
            pdFALSE,
            portMAX_DELAY);

        if (!(bits & WM_EVENT_BIT_STADISCONNECTED))
            continue;

        retry_count++;
        ESP_LOGW(TAG_WIFI, "Lần thử kết nối lại: %d/%d", retry_count, MAX_RETRIES);

        if (retry_count > MAX_RETRIES)
        {
            ESP_LOGE(TAG_WIFI, "Vượt quá số lần thử, mở portal cấu hình lại...");
            retry_count = 0;
            WiFiManager_Stop(&s_wm);
            WiFiManager_ConfigViaAP(&s_wm);
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));

        /* Thử kết nối lại với credentials hiện tại */
        WiFiManager_Stop(&s_wm);
        WiFiManager_StartSTA(&s_wm);

        bits = xEventGroupWaitBits(
            s_wm.event.group,
            WM_EVENT_BIT_STACONNECTED | WM_EVENT_BIT_STADISCONNECTED,
            pdFALSE, pdFALSE,
            pdMS_TO_TICKS(CONNECT_TIMEOUT_MS));

        if (bits & WM_EVENT_BIT_STACONNECTED)
        {
            ESP_LOGI(TAG_WIFI, "Kết nối lại thành công!");
            retry_count = 0;
        }
        /* Nếu disconnect, vòng lặp tiếp tục và thử lại */
    }
}

/* ── app_main — khởi tạo và spawn tasks ───────────────────────────── */

void app_main(void)
{
    /* Cấu hình WiFiManager */
    s_wm.ConnectedAP_Cb    = on_connected;
    s_wm.DisconnectedAP_Cb = on_disconnected;
    s_wm.sta_retry_num     = 3;

    WiFiManager_Init(&s_wm);

    /* Spawn wifi manager task — ưu tiên cao hơn app task */
    xTaskCreate(wifi_manager_task, "wifi_mgr", 4096, NULL, 5, NULL);

    /* Spawn app task */
    xTaskCreate(app_task, "app", 4096, NULL, 4, NULL);

    /* app_main kết thúc — FreeRTOS scheduler tiếp quản */
}
