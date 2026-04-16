/**
 * @file example_03_custom_portal.c
 * @brief Captive portal với các trường tùy chỉnh: MQTT broker,
 *        port, device name — lưu vào NVS sau khi nhận.
 *
 * Người dùng kết nối vào AP "ESP32_Config", trình duyệt tự mở
 * trang web cấu hình. Sau khi submit, thiết bị lưu tất cả vào
 * NVS rồi chuyển sang STA mode.
 *
 * Trường mở rộng trên portal:
 *   ┌─────────────────────────────────┐
 *   │  SSID          [___________]   │
 *   │  Password      [___________]   │
 *   │  MQTT Broker   [___________]   │  ← trường tùy chỉnh
 *   │  MQTT Port     [___________]   │  ← trường tùy chỉnh
 *   │  Device Name   [___________]   │  ← trường tùy chỉnh
 *   │           [  Save  ]           │
 *   └─────────────────────────────────┘
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "WiFiManager.h"

static const char *TAG = "Example03";

/* ── NVS namespace cho app config ──────────────────────────────────── */
#define NVS_NAMESPACE  "app_cfg"
#define NVS_KEY_MQTT   "mqtt_host"
#define NVS_KEY_PORT   "mqtt_port"
#define NVS_KEY_DEVNAME "dev_name"

static WiFiManager_t s_wm = {0};

/* ── Lưu cấu hình app vào NVS ──────────────────────────────────────── */

static void save_app_config(const char *mqtt_host,
                             const char *mqtt_port,
                             const char *dev_name)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK)
    {
        ESP_LOGE(TAG, "Không mở được NVS namespace '%s'", NVS_NAMESPACE);
        return;
    }
    nvs_set_str(h, NVS_KEY_MQTT,    mqtt_host);
    nvs_set_str(h, NVS_KEY_PORT,    mqtt_port);
    nvs_set_str(h, NVS_KEY_DEVNAME, dev_name);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Đã lưu cấu hình MQTT vào NVS");
}

/* ── Đọc cấu hình app từ NVS ──────────────────────────────────────── */

static void load_app_config(char *mqtt_host, size_t mqtt_sz,
                             char *mqtt_port, size_t port_sz,
                             char *dev_name,  size_t name_sz)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK)
        return;
    nvs_get_str(h, NVS_KEY_MQTT,    mqtt_host, &mqtt_sz);
    nvs_get_str(h, NVS_KEY_PORT,    mqtt_port, &port_sz);
    nvs_get_str(h, NVS_KEY_DEVNAME, dev_name,  &name_sz);
    nvs_close(h);
}

/* ── Chạy portal và lấy credentials + app config ───────────────────── */

static bool run_config_portal(void)
{
    /* Đăng ký các trường tùy chỉnh TRƯỚC khi gọi ConfigViaAP */
    WiFiManagerPage_Init(&s_wm);

    WiFiManagerPage_AddParam(&s_wm,
        "mqtt_host",            /* id — dùng khi đọc lại */
        "MQTT Broker",          /* label hiển thị */
        "ví dụ: broker.local hoặc 192.168.1.10",
        "",                     /* giá trị mặc định */
        "text",
        true);                  /* bắt buộc */

    WiFiManagerPage_AddParam(&s_wm,
        "mqtt_port",
        "MQTT Port",
        "ví dụ: 1883",
        "1883",                 /* mặc định 1883 */
        "number",
        true);

    WiFiManagerPage_AddParam(&s_wm,
        "dev_name",
        "Device Name",
        "ví dụ: sensor-phong-khach",
        "esp32-device",
        "text",
        false);                 /* không bắt buộc */

    /* Mở AP + captive portal, block đến khi có form submit hoặc timeout */
    WiFiManager_ConfigViaAP(&s_wm);

    /* Đọc kết quả */
    const char *mqtt_host = WiFiManagerPage_GetParam(&s_wm, "mqtt_host");
    const char *mqtt_port = WiFiManagerPage_GetParam(&s_wm, "mqtt_port");
    const char *dev_name  = WiFiManagerPage_GetParam(&s_wm, "dev_name");

    if (!mqtt_host || strlen(mqtt_host) == 0)
    {
        ESP_LOGE(TAG, "Không nhận được MQTT host từ portal");
        return false;
    }

    ESP_LOGI(TAG, "Portal nhận được:");
    ESP_LOGI(TAG, "  SSID      : %s", (char *)s_wm.config.sta.ssid);
    ESP_LOGI(TAG, "  MQTT Host : %s", mqtt_host);
    ESP_LOGI(TAG, "  MQTT Port : %s", mqtt_port ? mqtt_port : "(trống)");
    ESP_LOGI(TAG, "  Dev Name  : %s", dev_name  ? dev_name  : "(trống)");

    /* Lưu app config vào NVS riêng */
    save_app_config(mqtt_host,
                    mqtt_port ? mqtt_port : "1883",
                    dev_name  ? dev_name  : "esp32");
    return true;
}

/* ── app_main ──────────────────────────────────────────────────────── */

void app_main(void)
{
    WiFiManager_Init(&s_wm);

    /* Luôn mở portal khi khởi động (ví dụ: sau factory reset) */
    bool ok = run_config_portal();
    if (!ok)
    {
        ESP_LOGE(TAG, "Cấu hình thất bại, khởi động lại...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }

    /* Đọc lại config đã lưu (demo) */
    char mqtt_host[WM_FIELD_LEN] = {0};
    char mqtt_port[8]            = {0};
    char dev_name[WM_FIELD_LEN]  = {0};
    load_app_config(mqtt_host, sizeof(mqtt_host),
                    mqtt_port, sizeof(mqtt_port),
                    dev_name,  sizeof(dev_name));

    /* Chờ STA kết nối (ConfigViaAP đã gọi StartSTA ở cuối) */
    EventBits_t bits = xEventGroupWaitBits(
        s_wm.event.group,
        WM_EVENT_BIT_STACONNECTED,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(20000));

    if (!(bits & WM_EVENT_BIT_STACONNECTED))
    {
        ESP_LOGE(TAG, "Kết nối STA thất bại sau portal");
        esp_restart();
    }

    /* Kết nối MQTT với thông số vừa cấu hình */
    ESP_LOGI(TAG, "Kết nối MQTT: %s:%s (device: %s)",
             mqtt_host, mqtt_port, dev_name);
    /* mqtt_client_start(mqtt_host, atoi(mqtt_port), dev_name); */

    while (1)
        vTaskDelay(pdMS_TO_TICKS(10000));
}
