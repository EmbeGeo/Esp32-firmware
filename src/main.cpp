#include "config.h"
#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// =======================
// 🔥 DEBUG
// =======================
static int frameCount = 0;
static unsigned long lastLog = 0;

// =======================
// 🔥 STREAM HANDLER
// =======================
static esp_err_t stream_handler(httpd_req_t *req) {

    camera_fb_t *fb = NULL;

    Serial.println("[STREAM] Client connected");

    httpd_resp_set_type(req,
        "multipart/x-mixed-replace; boundary=frame");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "30");

    while (true) {

        fb = esp_camera_fb_get();

        if (!fb) {
            Serial.println("[ERROR] Camera capture failed");
            delay(50);
            continue;
        }

        frameCount++;

        esp_err_t res = ESP_OK;

        res = httpd_resp_send_chunk(req, "--frame\r\n", 9);
        if (res != ESP_OK) {
            esp_camera_fb_return(fb);
            Serial.println("[STREAM] Client disconnected");
            return res;
        }

        res = httpd_resp_send_chunk(req,
            "Content-Type: image/jpeg\r\n\r\n", 28);
        if (res != ESP_OK) {
            esp_camera_fb_return(fb);
            Serial.println("[STREAM] Client disconnected");
            return res;
        }

        res = httpd_resp_send_chunk(req,
            (const char *)fb->buf, fb->len);
        if (res != ESP_OK) {
            esp_camera_fb_return(fb);
            Serial.println("[STREAM] Client disconnected");
            return res;
        }

        res = httpd_resp_send_chunk(req, "\r\n", 2);

        esp_camera_fb_return(fb);

        if (res != ESP_OK) {
            Serial.println("[STREAM] Client disconnected");
            return res;
        }

        // =======================
        // 🔥 DEBUG
        // =======================
        if (millis() - lastLog > 1000) {
            lastLog = millis();

            Serial.println("========== ESP32 DEBUG ==========");
            Serial.printf("[FPS] %d\n", frameCount);
            Serial.printf("[FREE HEAP] %d\n", ESP.getFreeHeap());
            Serial.printf("[WiFi RSSI] %d\n", WiFi.RSSI());
            Serial.println("================================");

            frameCount = 0;
        }

        delay(30);
    }

    return ESP_OK;
}

// =======================
// 🔥 CAMERA INIT
// =======================
bool initCamera() {

    camera_config_t config;

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;

    config.pin_d0 = 5;
    config.pin_d1 = 18;
    config.pin_d2 = 19;
    config.pin_d3 = 21;
    config.pin_d4 = 36;
    config.pin_d5 = 39;
    config.pin_d6 = 34;
    config.pin_d7 = 35;

    config.pin_xclk = 0;
    config.pin_pclk = 22;
    config.pin_vsync = 25;
    config.pin_href = 23;

    config.pin_sccb_sda = 26;
    config.pin_sccb_scl = 27;

    config.pin_pwdn  = 32;
    config.pin_reset = -1;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // =======================
    // 🔥 UPGRADED SETTINGS
    // =======================
    config.frame_size   = FRAMESIZE_VGA;   // 🔥 업그레이드
    config.jpeg_quality = 12;              // 🔥 화질 개선
    config.fb_count     = 2;               // 🔥 안정성 증가

    esp_err_t err = esp_camera_init(&config);

    if (err != ESP_OK) {
        Serial.printf("[CAMERA FAIL] %d\n", err);
        return false;
    }

    // =======================
    // 🔥 SENSOR TUNING
    // =======================
    sensor_t * s = esp_camera_sensor_get();

    s->set_brightness(s, 1);
    s->set_contrast(s, 1);
    s->set_saturation(s, 0);

    Serial.println("[CAMERA INIT OK]");
    return true;
}

// =======================
// 🔥 WIFI CONNECT
// =======================
void connectWiFi() {

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("[WiFi] Connecting");

    int retry = 0;

    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
        retry++;

        if (retry > 40) {
            Serial.println("\n[WiFi FAIL]");
            return;
        }
    }

    Serial.println("\n[WiFi OK]");
    Serial.print("[IP] ");
    Serial.println(WiFi.localIP());
}

// =======================
// 🔥 START SERVER
// =======================
void startServer() {

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 1;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t stream_uri = {
            .uri = STREAM_PATH,
            .method = HTTP_GET,
            .handler = stream_handler
        };

        httpd_register_uri_handler(server, &stream_uri);

        Serial.println("[SERVER STARTED]");
        Serial.print("[STREAM URL] http://");
        Serial.print(WiFi.localIP());
        Serial.println(STREAM_PATH);
    }
}

// =======================
// 🔥 SETUP
// =======================
void setup() {

    Serial.begin(115200);
    Serial.println("\n=== ESP32 CAMERA START ===");

    if (!initCamera()) {
        Serial.println("[FATAL] Camera init failed");
        while (true);
    }

    connectWiFi();
    startServer();
}

// =======================
// 🔥 LOOP
// =======================
void loop() {
    delay(1000);
}