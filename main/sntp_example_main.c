#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "esp_sntp.h"

void time_sync_notification_cb(struct timeval *tv) {
    printf("Evento SNTP sincronizacao data/hora \r\n");
}

static void sntp_obter_data_hora(void) {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    sntp_init();

    // Espera um tempo para ajustar a hora
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        printf("Esperando a hora do sistema ser ajustada... (%d/%d)\r\n", retry, retry_count);        
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);
}

void sntp_start(void) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    sntp_obter_data_hora();
    // atualiza com a data/hora atualizada
    time(&now);

    char strftime_buf[64];

    // todas as time zones em: https://gist.github.com/alwynallan/24d96091655391107939
    // Ajusta a time zone para horario oficial do Brasília
    setenv("TZ", "BRT3", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    printf("Data/hora atuais no Brasil: %s\r\n", strftime_buf);

    if (sntp_get_sync_mode() == SNTP_SYNC_MODE_SMOOTH) {
        struct timeval outdelta;
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) {
            adjtime(NULL, &outdelta);
            printf("Aguardando ajuste data/hora ... outdelta = %li seg: %li ms: %li us\r\n",
                    (long)outdelta.tv_sec,
                    outdelta.tv_usec/1000,
                    outdelta.tv_usec%1000);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }
}

struct tm pegar_data_hora(void) {
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo;
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    sntp_start();

    while (1) {
        struct tm tempo = pegar_data_hora();
        char strftime_buf[64];

        strftime(strftime_buf, sizeof(strftime_buf), "%c", &tempo);
        printf("Data/hora atual: %s\r\n", strftime_buf);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
