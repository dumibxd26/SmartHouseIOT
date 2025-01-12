#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Fingerprint.h>
#include <ESP32Servo.h>
#include <esp_http_server.h>
#include "FS.h"
#include "credentials.h"

// Pin definitions
#define RX_PIN 16
#define TX_PIN 17
#define RED_PIN 14
#define GREEN_PIN 27
#define BLUE_PIN 26
#define SERVO_PIN 25
#define TRIG_PIN 5
#define ECHO_PIN 18
#define BUZZER_PIN 12

// Network and hub configuration
const char *ssid = SSID;
const char *passowrd = PASSWORD;

String hub_address = String(HUB);

int alarmActivated = false;

int board_status_milis = 1000;

const char *board_name = "FrontDoorESP32";
// Fingerprint sensor
Adafruit_Fingerprint finger(&Serial2);

// Servo motor
Servo myServo;

// Variables for fingerprint waiting mechanism
unsigned long detectionStartTime = 0;
unsigned long presenceEndTime = 0;
bool personDetected = false;
bool waitingForFingerprint = false;
unsigned long fingerprintStartTime = 0;

// Function to test hub connectivity
bool testHubConnection(const char *hub_ip)
{
    HTTPClient http;
    String test_url = hub_address + "/test";
    http.begin(test_url);
    int httpCode = http.GET();
    http.end();
    return (httpCode == 200);
}

// Register the board with the hub
void registerBoard()
{
    if (hub_address == "")
        return;
    HTTPClient http;
    http.begin(hub_address + "/register");
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"name\":\"" + String(board_name) + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
    int httpCode = http.POST(payload);
    if (httpCode == 200)
    {
        Serial.println("Board registered successfully.");
    }
    else
    {
        Serial.println("Failed to register board. HTTP code: " + String(httpCode));
    }
    http.end();
}

// Attempt to connect to Wi-Fi
bool connectToWiFi(const char *ssid, const char *password)
{
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(ssid);

    int retries = 20; // Retry limit
    while (WiFi.status() != WL_CONNECTED && retries > 0)
    {
        delay(500);
        Serial.print(".");
        retries--;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nConnected to Wi-Fi: " + String(ssid));
        Serial.println("IP Address: " + WiFi.localIP().toString());
        return true;
    }
    else
    {
        Serial.println("\nFailed to connect to Wi-Fi: " + String(ssid));
        return false;
    }
}

// Function to send proximity event
void sendProximityEvent(int distance)
{
    if (hub_address == "")
        return;
    HTTPClient http;
    http.begin(hub_address + "/proximity_event");
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"name\":\"FrontDoorESP32\",\"distance\":" + String(distance) + "}";
    int httpCode = http.POST(payload);
    if (httpCode == 200)
    {
        Serial.println("Proximity event sent successfully.");
    }
    else
    {
        Serial.println("Failed to send proximity event. HTTP code: " + String(httpCode));
    }
    http.end();
}

// Function to send fingerprint result
void sendFingerprintResult(String status, int id = -1)
{
    if (hub_address == "")
        return;
    HTTPClient http;
    http.begin(hub_address + "/fingerprint_result");
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"name\":\"FrontDoorESP32\",\"status\":\"" + status + "\"";
    if (id != -1)
    {
        payload += ",\"id\":" + String(id);
    }
    payload += "}";

    int httpCode = http.POST(payload);
    if (httpCode == 200)
    {
        Serial.println("Fingerprint result sent successfully.");
    }
    else
    {
        Serial.println("Failed to send fingerprint result. HTTP code: " + String(httpCode));
    }
    http.end();
}

// Ultrasonic sensor
long getDistance()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH);
    long distance = duration * 0.034 / 2; // Convert duration to distance
    return distance;
}

// RGB LED
void setRGBColor(int red, int green, int blue)
{
    analogWrite(RED_PIN, red);
    analogWrite(GREEN_PIN, green);
    analogWrite(BLUE_PIN, blue);
}

// Servo control
void rotateServo(int durationMillis, int direction)
{
    if (direction == 0)
    {
        myServo.write(0);
    }
    else if (direction == 1)
    {
        myServo.write(180);
    }
    delay(durationMillis);
    myServo.write(90);
}

// Fingerprint handling
int getFingerprintID()
{
    int p = finger.getImage();
    if (p != FINGERPRINT_OK)
        return -1;

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK)
        return -1;

    p = finger.fingerFastSearch();
    if (p != FINGERPRINT_OK)
        return -1;

    return finger.fingerID;
}

// Buzzer
void buzz(int duration)
{
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
}

void handleSuccess()
{
    setRGBColor(0, 255, 0);
    buzz(200);
    rotateServo(2000, 1);
    delay(2000);
    rotateServo(2000, 0);
    setRGBColor(0, 0, 0);
}

void handleWrongFingerprint()
{
    alarmActivated = true;

    if (!hub_address.isEmpty())
    {
        HTTPClient http;
        String url = hub_address + "/front_door_alarm";
        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        String payload = "{\"message\":\"Front Door Alarm Triggered\",\"timestamp\":\"" + String(millis()) + "\"}";

        int httpCode = http.POST(payload);
        if (httpCode == 200)
        {
            Serial.println("Front Door Alarm notification sent successfully.");
        }
        else
        {
            Serial.println("Failed to send Front Door Alarm notification. HTTP code: " + String(httpCode));
        }
        http.end();
    }
    else
    {
        Serial.println("Hub address is empty. Cannot send Front Door Alarm notification.");
    }

    for (int i = 0; i < 3; i++)
    {
        setRGBColor(255, 0, 0); // Red color for alarm
        buzz(200);              // Sound the buzzer
        delay(500);
        setRGBColor(0, 0, 0); // Turn off LED
        delay(500);
    }
}

void sendMovementEvent(int distance)
{
    if (hub_address == "")
        return;

    HTTPClient http;
    http.begin(hub_address + "/movement_event");
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"name\":\"FrontDoorESP32\",\"distance\":" + String(distance) + "}";
    int httpCode = http.POST(payload);
    if (httpCode == 200)
    {
        Serial.println("Movement event sent successfully.");
    }
    else
    {
        Serial.println("Failed to send movement event. HTTP code: " + String(httpCode));
    }
    http.end();
}

// Handler for activating the alarm

esp_err_t activate_alarm_handler(httpd_req_t *req)
{
    // Activate the alarm
    alarmActivated = true;

    // Turn on the buzzer and RGB LED as alarm indicators
    digitalWrite(BUZZER_PIN, HIGH);
    setRGBColor(255, 0, 0); // Red color for active alarm

    Serial.println("Alarm activated!");

    // Send response
    const char *resp = "Alarm activated!";
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}

// Function to start the alarm
void startAlarm()
{
    alarmActivated = true;

    // Turn on the buzzer and RGB LED as alarm indicators
    digitalWrite(BUZZER_PIN, HIGH);
    setRGBColor(255, 0, 0); // Red color for active alarm

    Serial.println("Alarm activated!");
}

// Function to stop the alarm
void stopAlarm()
{
    alarmActivated = false;

    // Turn off the buzzer and RGB LED
    digitalWrite(BUZZER_PIN, LOW);
    setRGBColor(0, 0, 0); // Turn off the LED

    Serial.println("Alarm deactivated!");
}

// Handler for deactivating the alarm
esp_err_t deactivate_alarm_handler(httpd_req_t *req)
{
    // Deactivate the alarm
    alarmActivated = false;

    // Turn off the buzzer and RGB LED
    digitalWrite(BUZZER_PIN, LOW);
    setRGBColor(0, 0, 0); // Turn off the LED

    Serial.println("Alarm deactivated!");

    // Send response
    const char *resp = "Alarm deactivated!";
    httpd_resp_send(req, resp, strlen(resp));

    return ESP_OK;
}

// Function to start the HTTP server and register routes
void startServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // URI for activating the alarm
    httpd_uri_t activate_alarm = {
        .uri = "/activate_alarm",
        .method = HTTP_POST,
        .handler = activate_alarm_handler,
        .user_ctx = NULL};

    // URI for deactivating the alarm
    httpd_uri_t deactivate_alarm = {
        .uri = "/deactivate_alarm",
        .method = HTTP_POST,
        .handler = deactivate_alarm_handler,
        .user_ctx = NULL};

    // Start the HTTP server
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Register the routes
        httpd_register_uri_handler(server, &activate_alarm);
        httpd_register_uri_handler(server, &deactivate_alarm);
        Serial.println("HTTP server started and routes registered.");
    }
    else
    {
        Serial.println("Failed to start the HTTP server.");
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Initializing system...");

    // RGB LED setup
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);

    // Servo setup
    myServo.attach(SERVO_PIN);
    myServo.write(90);

    // Ultrasonic sensor setup
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // Buzzer setup
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    // Fingerprint sensor setup
    Serial2.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);
    finger.begin(57600);

    if (finger.verifyPassword())
    {
        Serial.println("Fingerprint sensor detected successfully.");
        setRGBColor(0, 0, 255);
        delay(2000);
        setRGBColor(0, 0, 0);
    }
    else
    {
        Serial.println("Fingerprint sensor not found. Check wiring.");
        setRGBColor(255, 0, 0);
        while (1)
            ;
    }

    // Connect to Wi-Fi
    if (!connectToWiFi(ssid, passowrd))
    {
        Serial.println("Failed to connect to Wi-Fi. Check credentials.");
        return;
    }

    registerBoard();

    // Start the HTTP server
    startServer();
}

void openDoor()
{
    setRGBColor(0, 255, 0); // Green: Access granted
    handleSuccess();
    personDetected = false;
    waitingForFingerprint = false;
    setRGBColor(0, 0, 0); // Turn off LED
}

static void send_board_status()
{
    HTTPClient http;
    String url = hub_address + "/send_status";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"message\":\"Board is up\", \"name\":\"" + String(board_name) + "\"}";

    int httpCode = http.POST(payload);
    // http code 202 -> received no command
    // http code 203 -> activate alarm command
    // http code 204 -> deactivate alarm command
    Serial.println("HTTP code: " + String(httpCode));
    if (httpCode == 202)
    {
        Serial.println("No command received.");
    }
    else if (httpCode == 203)
    {
        Serial.println("Activate alarm command received.");
        startAlarm();
    }
    else if (httpCode == 204)
    {
        Serial.println("Deactivate alarm command received.");
        stopAlarm();
    }
    else if (httpCode == 205)
    {
        Serial.println("Door is opened");
        openDoor();
    }

    http.end();
}

void loop()
{
    long distance = getDistance();
    unsigned long currentMillis = millis();

    if (millis() - board_status_milis > 1000)
    {
        send_board_status();
        board_status_milis = millis();
    }

    // Check for presence
    if (distance > 0 && distance < 25)
    {
        if (!personDetected)
        {
            detectionStartTime = currentMillis;
            personDetected = true;
            Serial.println("Presence detected. Monitoring...");
            setRGBColor(255, 255, 0); // Yellow: Detecting movement
        }

        // Confirm presence if within range for 2 seconds
        if (currentMillis - detectionStartTime >= 2000 && !waitingForFingerprint)
        {
            Serial.println("Confirmed presence. Waiting for fingerprint...");
            setRGBColor(0, 0, 255); // Blue: Waiting for fingerprint
            waitingForFingerprint = true;
            fingerprintStartTime = currentMillis;

            sendMovementEvent(distance);
        }

        presenceEndTime = currentMillis; // Update the presence end time
    }
    else if (personDetected)
    {
        // Reset if no presence for 5 seconds
        if (currentMillis - presenceEndTime >= 5000)
        {
            Serial.println("No presence detected. Resetting...");
            personDetected = false;
            waitingForFingerprint = false;
            setRGBColor(0, 0, 0); // Turn off LED
        }
    }

    // Handle fingerprint verification
    if (waitingForFingerprint)
    {
        if (currentMillis - fingerprintStartTime <= 20000)
        {
            int fingerprintID = getFingerprintID();
            if (fingerprintID != -1)
            {
                Serial.print("Fingerprint matched! ID: ");
                Serial.println(fingerprintID);
                setRGBColor(0, 255, 0); // Green: Access granted
                handleSuccess();
                personDetected = false;
                waitingForFingerprint = false;
                setRGBColor(0, 0, 0); // Turn off LED
            }
        }
        else
        {
            Serial.println("Fingerprint not recognized within 20 seconds.");
            setRGBColor(255, 0, 0); // Red: Access denied
            handleWrongFingerprint();
            waitingForFingerprint = false;
            personDetected = false;
            setRGBColor(0, 0, 0); // Turn off LED
        }
    }
}
