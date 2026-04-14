/**
 * @file example_06_factory_reset.c
 * @brief Factory reset bằng nút bấm: giữ GPIO0 > 3s để xóa NVS
 *        và mở lại captive portal cấu hình.
 *
 * Thường dùng cho:
 *   - Thiết bị đã cấu hình sai, cần cấu hình lại
 *   - Chuyển thiết bị sang mạng WiFi khác
 *   - Reset về trạng thái xuất xưởng
 *
 * Nút bấm:  GPIO0 (boot button trên hầu hết dev boards)
 * LED:       GPIO2 (LED xanh trên nhiều board; đảo logic)
 *
 * Cách dùng:
 *   • Khởi động bình thường → AutoConnect dùng credentials cũ
 *   • Giữ nút > 3s → xóa NVS → mở portal → cấu hình lại
 *   • LED nháy nhanh khi đang ở chế độ portal
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "WiFiManager.h"

static const char *TAG = "Example06";

#define BUTTON_GPIO       GPIO_NUM_0
#define LED_GPIO          GPIO_NUM_2
#define FACTORY_RESET_MS  3000   /* Giữ nút bao lâu để reset */

static WiFiManager_t s_wm = {0};

/* ── LED helpers ───────────────────────────────────────────────────── */

static void led_init(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);
}

static void led_set(bool on)
{
    gpio_set_level(LED_GPIO, on ? 1 : 0);
}

/* LED nháy trong portal mode — chạy trên task riêng */
static void led_blink_task(void *arg)
{
    while (1)
    {
        led_set(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        led_set(false);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ── Button helper ─────────────────────────────────────────────────── */

static void button_init(void)
{
    gpio_reset_pin(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);
}

/** Trả về true nếu nút được giữ đủ FACTORY_RESET_MS */
static bool button_held_for_reset(void)
{
    if (gpio_get_level(BUTTON_GPIO) != 0)
        return false; /* Nút không ấn */

    ESP_LOGW(TAG, "Nút đang được giữ, đếm %d ms...", FACTORY_RESET_MS);
    vTaskDelay(pdMS_TO_TICKS(FACTORY_RESET_MS));

    if (gpio_get_level(BUTTON_GPIO) == 0)
    {
        ESP_LOGW(TAG, "Factory reset được kích hoạt!");
        return true;
    }
    return false;
}

/* ── Factory reset: xóa NVS WiFi credentials ──────────────────────── */

static void do_factory_reset(void)
{
    ESP_LOGW(TAG, "Đang xóa WiFi credentials trong NVS...");

    /*
     * ESP-IDF lưu WiFi config trong namespace "nvs.net80211".
     * Xóa toàn bộ NVS để đảm bảo sạch.
     * Trong production, nên xóa có chọn lọc hơn.
     */
    nvs_flash_erase();
    nvs_flash_init();

    ESP_LOGI(TAG, "NVS đã xóa. Khởi động portal cấu hình...");
}

/* ── Callbacks ─────────────────────────────────────────────────────── */

static void on_connected(void)
{
    led_set(true); /* LED sáng = đã kết nối */
    ESP_LOGI(TAG, "WiFi kết nối thành công");
}

static void on_disconnected(void)
{
    led_set(false);
    ESP_LOGW(TAG, "WiFi bị ngắt");
}

/* ── Button monitor task — chạy song song với app ──────────────────── */

static TaskHandle_t s_blink_task = NULL;

static void button_monitor_task(void *arg)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(200));

        if (!button_held_for_reset())
            continue;

        /* Dừng LED blink cũ nếu có */
        if (s_blink_task)
        {
            vTaskDelete(s_blink_task);
            s_blink_task = NULL;
        }

        /* Bắt đầu LED nháy (đang ở portal mode) */
        xTaskCreate(led_blink_task, "led_blink", 1024, NULL, 3, &s_blink_task);

        /* Thực hiện reset và mở portal */
        do_factory_reset();
        WiFiManager_Stop(&s_wm);
        WiFiManager_ConfigViaAP(&s_wm);

        /* Sau khi portal xong, dừng LED nháy */
        if (s_blink_task)
        {
            vTaskDelete(s_blink_task);
            s_blink_task = NULL;
        }

        /* Kiểm tra kết nối mới */
        EventBits_t bits = xEventGroupGetBits(s_wm.event.group);
        if (bits & WM_EVENT_BIT_STACONNECTED)
            led_set(true);
    }
}

/* ── app_main ──────────────────────────────────────────────────────── */

void app_main(void)
{
    led_init();
    button_init();

    s_wm.ConnectedAP_Cb    = on_connected;
    s_wm.DisconnectedAP_Cb = on_disconnected;

    /* Kiểm tra nút ngay khi boot */
    if (button_held_for_reset())
    {
        xTaskCreate(led_blink_task, "led_blink", 1024, NULL, 3, &s_blink_task);
        do_factory_reset();
        WiFiManager_Init(&s_wm);
        WiFiManager_ConfigViaAP(&s_wm);
        if (s_blink_task) { vTaskDelete(s_blink_task); s_blink_task = NULL; }
    }
    else
    {
        WiFiManager_Init(&s_wm);
        WiFiManager_AutoConnect(&s_wm);
    }

    /* Spawn button monitor để xử lý reset bất cứ lúc nào */
    xTaskCreate(button_monitor_task, "btn_mon", 2048, NULL, 3, NULL);

    /* Vòng lặp ứng dụng chính */
    while (1)
    {
        EventBits_t bits = xEventGroupGetBits(s_wm.event.group);
        if (bits & WM_EVENT_BIT_STACONNECTED)
        {
            ESP_LOGI(TAG, "Đang chạy... (nhấn giữ GPIO0 để factory reset)");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
