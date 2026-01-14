#include <stdio.h>
#include "esp_wifi.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "ping/ping_sock.h"

static const char* TAG = "Main";
esp_netif_t* esp_netif;

#define ONBOARD_LED_NUM 8
#define EXTERNAL_LED_PIN 7

#define B_WIFI_OPERATIONAL BIT0
#define B_WIFI_CONNECTED BIT1
#define B_INTERNET_REACHABLE BIT2
#define B_PHONE_REACHABLE BIT3
#define B_PINGS_STARTED BIT4


#define W_SSID ""
#define W_PASS ""

#define IP_PHONE "192.168.88.44"
#define IP_INTERNET "9.9.9.9"

#define INTERNET_CHECK_INTERVAL 60 * 1000                  // 1 min
#define PHONE_CHECK_INTERVAL 5 * 1000                      // 5 sec
#define MAX_PHONE_TIMEOUT 45ULL * 60ULL * 1000000ULL       // 45 min (in microseconds)
#define PHONE_REDUCE_PING_TIMESPAN 15ULL * 60ULL * 1000000ULL       // 15 min
#define MAX_INTERNET_TIMEOUT 90ULL * 1000000ULL            // 1.5 min

uint64_t lastPhoneSuccess = 0;
uint64_t lastInternetSuccess = 0;

esp_ping_handle_t internet_ping_handle;
esp_ping_handle_t phone_ping_handle;

EventGroupHandle_t device_event_group;


void on_wifi_sta_start(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{
    esp_wifi_connect();
}

void on_wifi_sta_connect(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{
    ESP_LOGI(TAG, "Wifi Connected");
    xEventGroupClearBits(device_event_group, B_WIFI_CONNECTED);
}

void on_wifi_sta_disconnect(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{
    ESP_LOGW(TAG, "Wifi Disconnected");
    xEventGroupClearBits(device_event_group, B_WIFI_OPERATIONAL | B_WIFI_CONNECTED);
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_wifi_connect();
}

void on_got_ip(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif, &ip_info);
    ESP_LOGI(TAG, "God bless DHCP\nIP: " IPSTR ", GW: " IPSTR ", MASK: " IPSTR, IP2STR(&ip_info.ip), IP2STR(&ip_info.gw), IP2STR(&ip_info.netmask));

    xEventGroupSetBits(device_event_group, B_WIFI_OPERATIONAL);
}

void on_lost_ip(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{
    ESP_LOGI(TAG, "IP Lost");
    xEventGroupClearBits(device_event_group, B_WIFI_OPERATIONAL);
}

void device_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif = esp_netif_create_default_wifi_sta();

    const wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);
    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = W_SSID,
            .password = W_PASS
        }
    };

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    // wifi_country_t country_config = {
    //     .cc = "UA",
    //     .schan = 1,
    //     .nchan = 13,
    //     .policy = WIFI_COUNTRY_POLICY_AUTO,
    // };
    // esp_wifi_set_country(&country_config);

    device_event_group = xEventGroupCreate();
    if (device_event_group == NULL) {
        printf("Event group somehow failed to create itself");
        abort();
    }

    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &on_wifi_sta_start, NULL);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_sta_connect, NULL);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_sta_disconnect, NULL);

    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &on_lost_ip, NULL);

    esp_wifi_start();
}


void init_gpio() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << 8),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << 7);
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_config(&io_conf);
}


void ping_end(esp_ping_handle_t hdl, void *args) {
    ESP_LOGW(TAG, "Ping ended somehow");
    // EventBits_t uxBits;
    // uxBits = xEventGroupWaitBits(device_event_group, B_WIFI_OPERATIONAL, pdFALSE, pdTRUE, portMAX_DELAY);
    // if (uxBits & B_WIFI_OPERATIONAL) {
    //     ESP_LOGI(TAG, "Restarting ping");
    //     esp_ping_start(hdl);
    // }
};

void init_ping(esp_ping_handle_t* handle, ip_addr_t addr, uint32_t interval, void(*fn_ok)(void *, void *), void(*fn_timeout)(void *, void *)) {
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = addr;
    ping_config.interval_ms = interval;
    ping_config.count = ESP_PING_COUNT_INFINITE;

    esp_ping_callbacks_t cbs;
    cbs.on_ping_success = fn_ok;
    cbs.on_ping_timeout = fn_timeout;
    cbs.on_ping_end = ping_end;
    cbs.cb_args = NULL;

    esp_ping_new_session(&ping_config, &cbs, handle);
}


void internet_ping_ok(esp_ping_handle_t hdl, void *args)
{
    ESP_LOGI(TAG, "inet ping ok");
    lastInternetSuccess = esp_timer_get_time();
    xEventGroupSetBits(device_event_group, B_INTERNET_REACHABLE);
}

void internet_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint64_t now = esp_timer_get_time();
    uint64_t delta = now - lastInternetSuccess;
    if (delta >= MAX_INTERNET_TIMEOUT) {
        ESP_LOGI(TAG, "Internte timeout reached");
        xEventGroupClearBits(device_event_group, B_INTERNET_REACHABLE);
    }
}


void phone_ping_ok(esp_ping_handle_t hdl, void *args)
{
    ESP_LOGI(TAG, "phone ping ok");
    lastPhoneSuccess = esp_timer_get_time();
    xEventGroupSetBits(device_event_group, B_PHONE_REACHABLE);
}

void phone_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint64_t now = esp_timer_get_time();
    uint64_t delta = (uint64_t)(now - lastPhoneSuccess);
    if (delta >= MAX_PHONE_TIMEOUT) {
        xEventGroupClearBits(device_event_group, B_PHONE_REACHABLE);
    }
}

void init_pings() {
    ip_addr_t internet_addr;
    ipaddr_aton(IP_INTERNET, &internet_addr);
    ip_addr_t phone_addr;
    ipaddr_aton(IP_PHONE, &phone_addr);

    init_ping(&internet_ping_handle, internet_addr, INTERNET_CHECK_INTERVAL, internet_ping_ok, internet_ping_timeout);
    init_ping(&phone_ping_handle, phone_addr, PHONE_CHECK_INTERVAL, phone_ping_ok, phone_ping_timeout);
}

void taskStartPingsWhenConnectedToNetwork(void * pvParameters) {
    EventBits_t uxBits;
    uxBits = xEventGroupWaitBits(device_event_group, B_WIFI_OPERATIONAL, pdFALSE, pdTRUE, portMAX_DELAY);
    if (uxBits & B_WIFI_OPERATIONAL) {
        ESP_LOGI(TAG, "Srarting pings");
        esp_ping_start(phone_ping_handle);
        esp_ping_start(internet_ping_handle);
    }
    xEventGroupSetBits(device_event_group, B_PINGS_STARTED);
    vTaskDelete(NULL);
}

void taskReducePhonePings(void * pvParameters) {
    EventBits_t uxBits;
    bool was_started;
    for (;;) {
        uxBits = xEventGroupWaitBits(device_event_group, B_INTERNET_REACHABLE , pdFALSE, pdTRUE, pdMS_TO_TICKS(100));
        was_started = uxBits & B_PINGS_STARTED;
        uint64_t now = esp_timer_get_time();
        uint64_t delta = now - lastPhoneSuccess;
        if (!was_started && uxBits & B_INTERNET_REACHABLE && (delta > PHONE_REDUCE_PING_TIMESPAN)) {
            ESP_LOGI(TAG, "restoring phone pings");
            esp_ping_start(phone_ping_handle);
            xEventGroupSetBits(device_event_group, B_PINGS_STARTED);
        }
        else if (was_started && (!(uxBits & B_INTERNET_REACHABLE) || (delta < PHONE_REDUCE_PING_TIMESPAN && lastPhoneSuccess != 0))) {
            ESP_LOGI(TAG, "reducing phone pings");
            esp_ping_stop(phone_ping_handle);
            xEventGroupClearBits(device_event_group, B_PINGS_STARTED);
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void blink_once() {
    gpio_set_level(ONBOARD_LED_NUM, 0);  // LOW | ON
    gpio_set_level(EXTERNAL_LED_PIN, 1);  // HIGH | ON

    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(EXTERNAL_LED_PIN, 0);  // LOW | OFF

    vTaskDelay(pdMS_TO_TICKS(900));
    gpio_set_level(ONBOARD_LED_NUM, 1);  // HIGH | OFF
}

void app_main(void)
{
    init_gpio();

    device_init();

    init_pings();

    blink_once();

    TaskHandle_t taskStartPingsWhenConnectedToNetworkHandle = NULL;
    static uint8_t ucParameterToPass;
    xTaskCreate(taskStartPingsWhenConnectedToNetwork, "pings on wifi", 2048, &ucParameterToPass, 10, &taskStartPingsWhenConnectedToNetworkHandle);

    TaskHandle_t taskReducePhonePingsHandle = NULL;
    xTaskCreate(taskReducePhonePings, "recude ping", 2048, &ucParameterToPass, 10, &taskReducePhonePingsHandle);

    EventBits_t uxBits;
    bool led_state_was = false;
    for (;;) {
            uxBits = xEventGroupGetBits(device_event_group);
            if (!led_state_was && (uxBits & B_INTERNET_REACHABLE) && (uxBits & B_PHONE_REACHABLE)) {
                gpio_set_level(EXTERNAL_LED_PIN, 1);  // HIGH | ON
                led_state_was = true;
            }
            else if (led_state_was && !((uxBits & B_INTERNET_REACHABLE) && (uxBits & B_PHONE_REACHABLE))) {
                gpio_set_level(EXTERNAL_LED_PIN, 0);  // LOW | OFF
                led_state_was = false;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
