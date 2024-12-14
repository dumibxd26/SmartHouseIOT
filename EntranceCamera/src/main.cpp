#include "esp_camera.h"
#include "WiFi.h"
#include "esp_http_server.h"
#include <HTTPClient.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "camera_pins.h"
#include "credentials.h"

// Wi-Fi credentials
const char* ssid_primary = SSID_PRIMARY;
const char* password_primary = PASSWORD_PRIMARY;
const char* ssid_secondary = SSID_SECONDARY;
const char* password_secondary = PASSWORD_SECONDARY;

// Hub configuration
const char* hub_primary = HUB_PRIMARY // Bound to primary network
const char* hub_secondary = HUB_SECONDARY // Bound to secondary network
const char* PORT = PORT_C;
String hub_address = "";
const char *board_name = "EntranceCamera";

// HTTP server handle
httpd_handle_t camera_httpd = NULL;

// Camera configuration
camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,
    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_SVGA,
    .jpeg_quality = 12,
    .fb_count = 1};

// Function prototypes
bool connectToWiFi(const char* ssid, const char* password);
void startServer();
static esp_err_t capture_handler(httpd_req_t *req);
static esp_err_t live_video_handler(httpd_req_t *req);

// Wi-Fi setup
bool connectToWiFi(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(ssid);

    int retries = 20; // Retry limit
    while (WiFi.status() != WL_CONNECTED && retries > 0) {
        delay(500);
        Serial.print(".");
        retries--;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to Wi-Fi: " + String(ssid));
        Serial.println("IP Address: " + WiFi.localIP().toString());
        return true;
    } else {
        Serial.println("\nFailed to connect to Wi-Fi: " + String(ssid));
        return false;
    }
}

// Function to test hub connectivity
bool testHubConnection(const char* hub_ip) {
    HTTPClient http;
    String test_url = String("http://") + hub_ip + ":" + PORT + "/test";
    http.begin(test_url);
    int httpCode = http.GET();
    http.end();
    return (httpCode == 200);
}

// Connect to the appropriate Wi-Fi and assign hub address
void setupWiFiAndHub() {
    if (connectToWiFi(ssid_primary, password_primary)) {
        hub_address = hub_primary;
    } else if (connectToWiFi(ssid_secondary, password_secondary)) {
        hub_address = hub_secondary;
    } else {
        Serial.println("Failed to connect to any Wi-Fi network. Halting...");
        while (true); // Halt execution if no Wi-Fi connection
    }

    if (!testHubConnection(hub_address.c_str())) {
        Serial.println("[ERROR] Hub connection failed. Retrying...");
        while (true) delay(5000); // Halt or retry logic
    }
}

// Register the board with the hub
void registerBoard() {
    if (hub_address == "") return;
    HTTPClient http;
    http.begin(String("http://") + hub_address + ":" + PORT + "/register");
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"name\":\"" + String(board_name) + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
    int httpCode = http.POST(payload);
    if (httpCode == 200) {
        Serial.println("Board registered successfully.");
    } else {
        Serial.println("Failed to register board. HTTP code: " + String(httpCode));
    }
    http.end();
}

esp_err_t test_handler(httpd_req_t *req) {
    const char *resp = "Board works!";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// HTTP server setup
void startServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t capture_uri = {
        .uri = "/capture",
        .method = HTTP_GET,
        .handler = capture_handler,
        .user_ctx = NULL};

    httpd_uri_t live_video_uri = {
        .uri = "/live_video",
        .method = HTTP_GET,
        .handler = live_video_handler,
        .user_ctx = NULL};

    httpd_uri_t test = {
        .uri = "/test",
        .method = HTTP_GET,
        .handler = test_handler,
        .user_ctx = NULL
    };

    if (httpd_start(&camera_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(camera_httpd, &capture_uri);
        httpd_register_uri_handler(camera_httpd, &live_video_uri);
        httpd_register_uri_handler(camera_httpd, &test);
    }
}

static esp_err_t capture_handler(httpd_req_t *req)
{
    for (int retries = 0; retries < 3; retries++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            // Send image if capture is successful
            httpd_resp_set_type(req, "image/jpeg");
            httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
            esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
            esp_camera_fb_return(fb);
            return res;
        } else {
            Serial.println("Camera capture failed. Retrying...");
            delay(100);
        }
    }
    // After retries, send an error response
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

// Live video stream route
static esp_err_t live_video_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");

    while (true)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb)
        {
            Serial.println("Camera frame failed");
            return ESP_FAIL;
        }

        char part_buf[64];
        size_t hlen = snprintf((char *)part_buf, 64, "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n", fb->len);
        if (httpd_resp_send_chunk(req, (const char *)part_buf, hlen) != ESP_OK)
        {
            esp_camera_fb_return(fb);
            Serial.println("Client disconnected.");
            break;
        }

        if (httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len) != ESP_OK)
        {
            esp_camera_fb_return(fb);
            Serial.println("Client disconnected.");
            break;
        }

        esp_camera_fb_return(fb);

        if (httpd_resp_send_chunk(req, "\r\n", 2) != ESP_OK)
        {
            Serial.println("Client disconnected.");
            break;
        }

        delay(100); // Adjust delay for frame rate
    }

    return ESP_OK;
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting ESP32-CAM...");

    delay(1000);

    // Initialize the camera
    if (esp_camera_init(&camera_config) != ESP_OK)
    {
        Serial.println("Camera initialization failed. Please check the wiring and power supply.");
        while (true)
        {
            delay(1000); // Halt execution
        }
    }
    // Connect to Wi-Fi and set hub address
    setupWiFiAndHub();

    registerBoard();

    // Register the camera with the hub
    startServer();
}

void loop()
{
    
}