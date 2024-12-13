#include "esp_camera.h"
#include "WiFi.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "camera_pins.h"
#include <PubSubClient.h> 

// Wi-Fi credentials
const char *ssid = "DIGI-27Xy";
const char *password = "3CfkabA5aa";

// Hub configuration
const char *hub_address = "192.168.1.100"; // Replace with the hub's IP
const int hub_port = 5000;
const char *board_name = "EntranceCamera";

// MQTT configuration
const int mqtt_port = 1883;
const char *topic_capture_image = "capture_image";
const char *topic_live_video = "live_video";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

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
void connectToWiFi();
void startMQTT();
void mqttCallback(char *topic, byte *message, unsigned int length);
void sendImageToMQTT();
void reconnectMQTT();
void startServer();
void registerCamera();
static esp_err_t capture_handler(httpd_req_t *req);
static esp_err_t live_video_handler(httpd_req_t *req);
static esp_err_t simple_get_handler(httpd_req_t *req);

// Wi-Fi setup
void connectToWiFi()
{
    WiFi.begin(ssid, password);
    Serial.println("Connecting to Wi-Fi...");

    int retries = 30; // Allow up to 30 seconds to connect
    while (WiFi.status() != WL_CONNECTED && retries > 0)
    {
        delay(1000);
        Serial.print(".");
        retries--;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi connected!");
        Serial.println("IP Address: " + WiFi.localIP().toString());
    }
    else
    {
        Serial.println("Failed to connect to Wi-Fi. Check credentials and network availability.");
        while (true)
        {
            delay(1000); // Halt execution
        }
    }
}

// MQTT setup
void startMQTT()
{
    mqttClient.setServer(hub_address, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    reconnectMQTT();
}

void reconnectMQTT()
{
    while (!mqttClient.connected())
    {
        Serial.print("Attempting MQTT connection...");
        if (mqttClient.connect(board_name))
        {
            Serial.println("Connected to MQTT broker!");
            mqttClient.subscribe(topic_live_video); // Subscribe to the live video command topic
        }
        else
        {
            Serial.print("Failed to connect, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" retrying in 5 seconds...");
            delay(5000);
        }
    }
}

// MQTT message callback
void mqttCallback(char *topic, byte *message, unsigned int length)
{
    String msg;
    for (unsigned int i = 0; i < length; i++)
    {
        msg += (char)message[i];
    }

    Serial.println("Received message on topic: " + String(topic));
    Serial.println("Message: " + msg);

    if (String(topic) == topic_capture_image && msg == "{\"action\":\"capture\"}")
    {
        Serial.println("Capture request received via MQTT.");
        sendImageToMQTT();
    }
}

// Send captured image to MQTT
void sendImageToMQTT()
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("Camera capture failed");
        return;
    }

    if (mqttClient.connected())
    {
        mqttClient.publish(topic_capture_image, fb->buf, fb->len); // Publish the image buffer
        Serial.println("Image published to MQTT broker!");
    }
    else
    {
        Serial.println("MQTT client not connected. Cannot send image.");
    }

    esp_camera_fb_return(fb); // Free the frame buffer
}

// Camera registration with hub
void registerCamera()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi not connected. Cannot register camera.");
        return;
    }

    WiFiClient client;
    if (client.connect(hub_address, hub_port))
    {
        String postData = "{\"name\":\"" + String(board_name) + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
        client.println("POST /register HTTP/1.1");
        client.println("Host: " + String(hub_address) + ":" + String(hub_port));
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println("Content-Length: " + String(postData.length()));
        client.println();
        client.println(postData);

        String response;
        bool headersEnded = false;
        while (client.connected())
        {
            String line = client.readStringUntil('\n');
            if (!headersEnded)
            {
                if (line == "\r")
                {
                    headersEnded = true;
                }
            }
            else
            {
                response += line;
            }
        }

        Serial.println("Hub response: " + response);

        if (response.indexOf("\"status\":\"success\"") != -1)
        {
            Serial.println("Camera successfully registered with the hub.");
        }
        else
        {
            Serial.println("Camera registration failed. Response: " + response);
        }
    }
    else
    {
        Serial.println("Failed to connect to hub for registration.");
    }
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

    httpd_uri_t simple_get_uri = {
        .uri = "/simple_get",
        .method = HTTP_GET,
        .handler = simple_get_handler,
        .user_ctx = NULL};

    if (httpd_start(&camera_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(camera_httpd, &capture_uri);
        httpd_register_uri_handler(camera_httpd, &live_video_uri);
        httpd_register_uri_handler(camera_httpd, &simple_get_uri);
    }
}

// HTTP Capture Route
static esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return res;
}

// simple get test route
static esp_err_t simple_get_handler(httpd_req_t *req)
{
    httpd_resp_send(req, "Simple Get", strlen("Simple Get"));
    return ESP_OK;
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
    // Serial.println("Starting ESP32-CAM...");

    delay(500);

    // Initialize Wi-Fi
    connectToWiFi();

    // Initialize the camera
    if (esp_camera_init(&camera_config) != ESP_OK)
    {
        Serial.println("Camera initialization failed. Please check the wiring and power supply.");
        while (true)
        {
            delay(1000); // Halt execution
        }
    }
    // Start the HTTP server
    startServer();

    // Register the camera with the hub
    registerCamera();

    // Start MQTT
    startMQTT();
}

void loop()
{
    if (!mqttClient.connected())
    {
        reconnectMQTT();
    }
    mqttClient.loop(); // Process MQTT messages
}