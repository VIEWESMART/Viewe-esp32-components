#include "bsp/wifi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

static const char *TAG = "bsp_wifi";

static bool s_initialized = false;
static bool s_connected = false;
static volatile bool s_scanning = false;
static int8_t s_rssi = -100;
static bool s_sntp_started = false;
static char s_status[96] = "Disconnected";
static char s_connected_ssid[BSP_WIFI_SSID_MAX + 1] = "";
static SemaphoreHandle_t s_scan_done = NULL;
static SemaphoreHandle_t s_wifi_lock = NULL;
static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;

static void set_status_locked(const char *text) {
  portENTER_CRITICAL(&s_status_lock);
  snprintf(s_status, sizeof(s_status), "%s", text);
  portEXIT_CRITICAL(&s_status_lock);
}

static void bsp_wifi_update_rssi(void) {
  wifi_ap_record_t ap = {};
  if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
    s_rssi = ap.rssi;
  }
}

static void bsp_wifi_start_sntp(void) {
  if (s_sntp_started) {
    return;
  }
  setenv("TZ", "CST-8", 1);
  tzset();

  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "ntp.aliyun.com");
  esp_sntp_set_sync_interval(3600000);
  esp_sntp_init();
  s_sntp_started = true;
  ESP_LOGI(TAG, "SNTP started (CST-8, 1h interval)");
}

static void rssi_refresh_task(void *arg) {
  (void)arg;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(15000));
    if (!s_connected || s_scanning || s_wifi_lock == NULL) {
      continue;
    }
    if (xSemaphoreTake(s_wifi_lock, pdMS_TO_TICKS(200)) != pdTRUE) {
      continue;
    }
    bsp_wifi_update_rssi();
    xSemaphoreGive(s_wifi_lock);
  }
}

static void set_status_disconnected(void) {
  s_connected = false;
  s_rssi = -100;
  set_status_locked("Disconnected");
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id,
                               void *data) {
  if (base == WIFI_EVENT) {
    switch (id) {
    case WIFI_EVENT_STA_DISCONNECTED: {
      wifi_event_sta_disconnected_t *ev =
          (wifi_event_sta_disconnected_t *)data;
      s_connected = false;
      if (ev != NULL && ev->reason != WIFI_REASON_ASSOC_LEAVE) {
        char msg[96];
        snprintf(msg, sizeof(msg), "Failed (%d)", ev->reason);
        set_status_locked(msg);
        ESP_LOGW(TAG, "Disconnected, reason=%d", ev->reason);
      } else {
        set_status_disconnected();
      }
      break;
    }
    case WIFI_EVENT_SCAN_DONE:
      if (s_scan_done != NULL) {
        xSemaphoreGive(s_scan_done);
      }
      break;
    default:
      break;
    }
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
    s_connected = true;
    bsp_wifi_update_rssi();
    bsp_wifi_start_sntp();
    char msg[96];
    snprintf(msg, sizeof(msg), "Connected %s\nIP: " IPSTR, s_connected_ssid,
             IP2STR(&ev->ip_info.ip));
    set_status_locked(msg);
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&ev->ip_info.ip));
  }
}

esp_err_t bsp_wifi_init(void) {
  if (s_initialized) {
    return ESP_OK;
  }

  if (s_wifi_lock == NULL) {
    s_wifi_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_wifi_lock != NULL, ESP_ERR_NO_MEM, TAG, "wifi lock");
  }

  s_scan_done = xSemaphoreCreateBinary();
  ESP_RETURN_ON_FALSE(s_scan_done != NULL, ESP_ERR_NO_MEM, TAG, "scan sem");

  ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");

  esp_err_t err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init");

  ESP_RETURN_ON_ERROR(
      esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                          wifi_event_handler, NULL, NULL),
      TAG, "wifi evt");
  ESP_RETURN_ON_ERROR(
      esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                          wifi_event_handler, NULL, NULL),
      TAG, "ip evt");

  ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode");
  ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");

  if (xTaskCreatePinnedToCore(rssi_refresh_task, "wifi_rssi", 2048, NULL, 2,
                              NULL, 1) != pdPASS) {
    ESP_LOGW(TAG, "RSSI refresh task skipped");
  }

  s_initialized = true;
  ESP_LOGI(TAG, "WiFi STA ready");
  return ESP_OK;
}

bool bsp_wifi_is_initialized(void) { return s_initialized; }

bool bsp_wifi_is_connected(void) { return s_connected; }

bool bsp_wifi_is_scanning(void) { return s_scanning; }

int8_t bsp_wifi_get_rssi(void) {
  return s_rssi;
}

const char *bsp_wifi_status_text(void) { return s_status; }

void bsp_wifi_copy_status(char *out, size_t out_len) {
  if (out == NULL || out_len == 0) {
    return;
  }
  portENTER_CRITICAL(&s_status_lock);
  snprintf(out, out_len, "%s", s_status);
  portEXIT_CRITICAL(&s_status_lock);
}

static int ap_cmp_rssi(const void *a, const void *b) {
  const bsp_wifi_ap_info_t *ap_a = (const bsp_wifi_ap_info_t *)a;
  const bsp_wifi_ap_info_t *ap_b = (const bsp_wifi_ap_info_t *)b;
  return (int)ap_b->rssi - (int)ap_a->rssi;
}

esp_err_t bsp_wifi_scan(bsp_wifi_ap_info_t *results, uint16_t *count,
                          uint16_t max_count) {
  if (!s_initialized || results == NULL || count == NULL || max_count == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_wifi_lock == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  if (xSemaphoreTake(s_wifi_lock, pdMS_TO_TICKS(15000)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  s_scanning = true;
  esp_err_t ret = ESP_OK;

  xSemaphoreTake(s_scan_done, 0);

  wifi_scan_config_t scan_cfg = {
      .ssid = NULL,
      .bssid = NULL,
      .channel = 0,
      .show_hidden = false,
      .scan_type = WIFI_SCAN_TYPE_ACTIVE,
  };

  esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
  if (err != ESP_OK) {
    ret = err;
    goto done;
  }

  if (xSemaphoreTake(s_scan_done, pdMS_TO_TICKS(10000)) != pdTRUE) {
    esp_wifi_scan_stop();
    ret = ESP_ERR_TIMEOUT;
    goto done;
  }

  uint16_t ap_num = 0;
  err = esp_wifi_scan_get_ap_num(&ap_num);
  if (err != ESP_OK) {
    ret = err;
    goto done;
  }

  if (ap_num == 0) {
    *count = 0;
    goto done;
  }

  if (ap_num > 32) {
    ap_num = 32;
  }

  wifi_ap_record_t *records =
      (wifi_ap_record_t *)calloc(ap_num, sizeof(wifi_ap_record_t));
  if (records == NULL) {
    ret = ESP_ERR_NO_MEM;
    goto done;
  }

  err = esp_wifi_scan_get_ap_records(&ap_num, records);
  if (err != ESP_OK) {
    free(records);
    ret = err;
    goto done;
  }

  uint16_t out = 0;
  for (uint16_t i = 0; i < ap_num && out < max_count; i++) {
    if (records[i].ssid[0] == '\0') {
      continue;
    }

    bool dup = false;
    for (uint16_t j = 0; j < out; j++) {
      if (strcmp(results[j].ssid, (char *)records[i].ssid) == 0) {
        dup = true;
        if (records[i].rssi > results[j].rssi) {
          results[j].rssi = records[i].rssi;
          results[j].auth = records[i].authmode;
        }
        break;
      }
    }
    if (dup) {
      continue;
    }

    strncpy(results[out].ssid, (char *)records[i].ssid, BSP_WIFI_SSID_MAX);
    results[out].ssid[BSP_WIFI_SSID_MAX] = '\0';
    results[out].rssi = records[i].rssi;
    results[out].auth = records[i].authmode;
    out++;
  }

  free(records);
  qsort(results, out, sizeof(bsp_wifi_ap_info_t), ap_cmp_rssi);
  *count = out;

done:
  s_scanning = false;
  xSemaphoreGive(s_wifi_lock);
  return ret;
}

esp_err_t bsp_wifi_connect(const char *ssid, const char *password) {
  if (!s_initialized || ssid == NULL || ssid[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_wifi_lock == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  if (xSemaphoreTake(s_wifi_lock, pdMS_TO_TICKS(15000)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  wifi_config_t cfg = {};
  strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
  if (password != NULL) {
    strncpy((char *)cfg.sta.password, password, sizeof(cfg.sta.password) - 1);
  }

  strncpy(s_connected_ssid, ssid, BSP_WIFI_SSID_MAX);
  s_connected_ssid[BSP_WIFI_SSID_MAX] = '\0';

  char msg[96];
  snprintf(msg, sizeof(msg), "Connecting %s...", ssid);
  set_status_locked(msg);

  esp_err_t err = esp_wifi_disconnect();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT) {
    xSemaphoreGive(s_wifi_lock);
    return err;
  }

  err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
  if (err != ESP_OK) {
    xSemaphoreGive(s_wifi_lock);
    return err;
  }

  err = esp_wifi_connect();
  xSemaphoreGive(s_wifi_lock);
  return err;
}
