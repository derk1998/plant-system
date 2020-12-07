/* Simple HTTP Server Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"

#include <esp_http_server.h>
#include "http.h"
#include <cJSON.h>

static const char *TAG = "example";
static httpd_handle_t server = NULL;
static http_callbacks_t callbacks;

static esp_err_t configure_wifi_sta_handler(httpd_req_t *req);

static const httpd_uri_t configure_wifi_sta = {
    .uri       = "/wificonfig",
    .method    = HTTP_POST,
    .handler   = configure_wifi_sta_handler,
    .user_ctx  = NULL
};

static void save_wifi_credentials(const char* ssid, const char* password)
{
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, "password", password));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);
}

void register_on_receive_credentials_cb(http_cb_t callback)
{
    callbacks.on_receive_credentials = callback;
}

/* An HTTP POST handler */
static esp_err_t configure_wifi_sta_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char buf[MAX_BUF_SIZE];
    int received = 0;
    if (total_len >= MAX_BUF_SIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post wifi config");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    char* ssid = cJSON_GetObjectItem(root, "ssid")->valuestring;
    char* pass = cJSON_GetObjectItem(root, "password")->valuestring;
    ESP_LOGI(TAG, "Wifi config: ssid = %s, password = %s", ssid, pass);
    save_wifi_credentials(ssid, pass);
    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Post wifi config successfully");
    if(callbacks.on_receive_credentials)
        xTaskCreate(callbacks.on_receive_credentials, "on_receive_credentials", 2048, NULL, 5, NULL);
    return ESP_OK;
}

void start_webserver(void * arg)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &configure_wifi_sta);
    }

    for(;;)
    {
        vTaskDelay(100);
    }

    ESP_LOGI(TAG, "Error starting server!");
}

void stop_webserver(void)
{
    // Stop the httpd server
    ESP_LOGI(TAG, "Trying to stop the webserver");
    ESP_ERROR_CHECK(httpd_stop(server));
    ESP_LOGI(TAG, "Stopped");
}