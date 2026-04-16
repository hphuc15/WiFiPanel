# WiFiManager — Examples

Bộ ví dụ từ đơn giản đến production-ready, mỗi file độc lập và có thể copy thẳng vào `main.c`.

---

## Danh sách ví dụ

| File | Mô tả | Khi nào dùng |
|---|---|---|
| `example_01_basic_sta.c` | Kết nối STA với credentials cứng trong code | Dev, demo, credentials không đổi |
| `example_02_autoconnect.c` | AutoConnect: load NVS → STA, fallback về portal | **Pattern phổ biến nhất** cho sản phẩm |
| `example_03_custom_portal.c` | Portal với trường tuỳ chỉnh (MQTT, device name) | Thiết bị cần nhiều thông số cấu hình |
| `example_04_ap_only.c` | Chỉ chạy AP mode + web server nội bộ | Dashboard offline, setup wizard |
| `example_05_production.c` | Multi-task + watchdog reconnect tự động | Production firmware hoàn chỉnh |
| `example_06_factory_reset.c` | Nút giữ 3s để reset NVS và mở portal lại | Thiết bị cần factory reset |

---

## Chọn example phù hợp

```
Credentials biết trước?
├── Có → example_01_basic_sta.c
└── Không → Cần cấu hình qua portal?
    ├── Lần đầu boot / sản phẩm → example_02_autoconnect.c
    ├── Có thêm thông số app (MQTT, API key...) → example_03_custom_portal.c
    ├── Chỉ cần AP, không cần internet → example_04_ap_only.c
    ├── Production, cần reconnect tự động → example_05_production.c
    └── Cần factory reset bằng nút → example_06_factory_reset.c
```

---

## Tích hợp vào project ESP-IDF

### CMakeLists.txt của component WiFiManager

```cmake
idf_component_register(
    SRCS
        "WiFiManager.c"
        "WiFiManager_page.c"
        "WiFiManager_portal.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
        nvs_flash
        freertos
        lwip
)
```

### CMakeLists.txt của main

```cmake
idf_component_register(
    SRCS "main.c"   # hoặc example_0X_xxx.c
    INCLUDE_DIRS "."
    REQUIRES wifi_manager
)
```

---

## Cấu trúc WiFiManager instance

Luôn zero-init trước khi dùng:

```c
static WiFiManager_t s_wm = {0};  // ✅ đúng
WiFiManager_t wm;                  // ❌ sai — chứa rác
```

### Các trường quan trọng

```c
typedef struct {
    wifi_config_t config;          // SSID, password, authmode
    WiFiManagerEvent_t event;      // event.group để wait bits
    esp_netif_t *netif;            // netif hiện tại (STA hoặc AP)
    uint8_t sta_retry_num;         // số lần retry khi disconnect
    WiFiManagerPage_t page;        // trường tùy chỉnh trên portal
    httpd_handle_t server;         // handle HTTP server
    TaskHandle_t portal_waiting_task; // task đang block chờ portal
    WiFiManager_Callback_ConnectedAP_t    ConnectedAP_Cb;
    WiFiManager_Callback_DisconnectedAP_t DisconnectedAP_Cb;
} WiFiManager_t;
```

---

## Event bits — đồng bộ giữa các task

```c
// Chờ kết nối hoặc fail, timeout 15s
EventBits_t bits = xEventGroupWaitBits(
    s_wm.event.group,
    WM_EVENT_BIT_STACONNECTED | WM_EVENT_BIT_STADISCONNECTED,
    pdFALSE,           // không xóa bits sau khi nhận
    pdFALSE,           // chỉ cần 1 trong 2 bits (OR)
    pdMS_TO_TICKS(15000));

if (bits & WM_EVENT_BIT_STACONNECTED) { /* OK */ }
if (bits & WM_EVENT_BIT_STADISCONNECTED) { /* Fail */ }

// Kiểm tra trạng thái hiện tại (không block)
EventBits_t current = xEventGroupGetBits(s_wm.event.group);
bool is_connected = (current & WM_EVENT_BIT_STACONNECTED) != 0;
```

| Bit | Ý nghĩa |
|---|---|
| `WM_EVENT_BIT_STASTART` | STA mode đã khởi động |
| `WM_EVENT_BIT_STACONNECTED` | Đã kết nối và có IP |
| `WM_EVENT_BIT_STADISCONNECTED` | Bị ngắt kết nối |
| `WM_EVENT_BIT_APSTART` | AP đã bắt đầu phát sóng |

---

## Thứ tự gọi API đúng

```
Init → StartSTA / StartAP / AutoConnect / ConfigViaAP
                ↓
           [đang chạy]
                ↓
             Stop
                ↓
            Deinit
```

**Không gọi** `StartSTA` / `StartAP` nhiều lần liên tiếp mà không `Stop` ở giữa.
