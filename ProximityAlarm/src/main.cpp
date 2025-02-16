#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <esp_http_server.h>
#include "FS.h"
#include "Keypad/Keypad.h"
#include "credentials.h"

// Define HC-SR04 pins
#define TRIG_PIN 18
#define ECHO_PIN 19

// Define Buzzer pin
#define R1_PIN 13
#define R2_PIN 12
#define R3_PIN 14
#define R4_PIN 27
#define C1_PIN 26
#define C2_PIN 25
#define C3_PIN 33
#define C4_PIN 32
#define BUZZER_PIN 4

// Keypad setup
const int numRows = 4;
const int numCols = 4;
const int rowPins[numRows] = {R1_PIN, R2_PIN, R3_PIN, R4_PIN};
const int colPins[numCols] = {C1_PIN, C2_PIN, C3_PIN, C4_PIN};

// Keypad layout (4x4 matrix)
const char *keys[] = {
    "123A",
    "456B",
    "789C",
    "*0#D"};

Keypad keypad(numRows, numCols, rowPins, colPins, keys);

// Network and hub configuration
const char *ssid = SSID;         // WiFi SSID
const char *password = PASSWORD; // WiFi password
String hub_address = HUB;        // Hub address

const char *board_name = "ProximityBoard"; // Board name

// Password variables
String enteredPassword = "";           // Stores entered password
const String correctPassword = "1523"; // Set correct password
char lastKeyPressed = '\0';            // To track debouncing
unsigned long lastTimeKeyPressed = 0;  // Timestamp for last key press
int wrongGuessCount = 0;               // Tracks wrong guesses

bool alarmActive = false;
unsigned long systemDisabledUntil = 0; // Timestamp to disable system for 60 seconds

int board_status_milis = 0;

// HTTP Server handle
httpd_handle_t server = NULL;

// Function to connect to WiFi
bool connectToWiFi(const char *ssid, const char *password)
{
    Serial.println("Attempting to connect to WiFi: " + String(ssid));
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();
    const unsigned long timeout = 10000; // 10 seconds timeout

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout)
    {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nConnected to WiFi: " + String(ssid));
        Serial.println("IP Address: " + WiFi.localIP().toString());
        return true;
    }
    else
    {
        Serial.println("\nFailed to connect to WiFi: " + String(ssid));
        return false;
    }
}

// Function to register the board with the hub
void registerBoard()
{
    if (hub_address.isEmpty())
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

// Function to measure distance with HC-SR04
long measureDistance()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH);
    long distance = duration * 0.034 / 2; // Convert to cm
    return distance;
}

// Send notification to the hub
void sendNotificationToHub(String message)
{
    if (!hub_address.isEmpty())
    {
        HTTPClient http;
        String url = hub_address + "/front_door_alarm";
        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        String payload = "{\"message\":\"" + message + "\",\"timestamp\":\"" + String(millis()) + "\"}";

        int httpCode = http.POST(payload);
        if (httpCode == 200)
        {
            Serial.println("Notification sent to hub.");
        }
        else
        {
            Serial.println("Failed to send notification. HTTP code: " + String(httpCode));
        }
        http.end();
    }
    else
    {
        Serial.println("Hub address is empty. Cannot send notification.");
    }
}

// Function to start the alarm
void startAlarm()
{
    if (!alarmActive)
    {
        alarmActive = true;
        Serial.println("Alarm triggered!");
        digitalWrite(BUZZER_PIN, LOW);             // Turn on buzzer
        sendNotificationToHub("Alarm triggered!"); // Notify hub
    }
}

// Function to stop the alarm
void stopAlarm()
{
    if (alarmActive)
    {
        alarmActive = false;
        Serial.println("Alarm deactivated. System will restart in 60 seconds.");
        digitalWrite(BUZZER_PIN, HIGH);         // Turn off buzzer
        systemDisabledUntil = millis() + 60000; // Disable for 60 seconds
    }
}

// Notify the hub about three wrong guesses
void notifyThreeWrongGuesses()
{
    if (!hub_address.isEmpty())
    {
        HTTPClient http;
        String url = hub_address + "/three_wrong_guesses";
        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        String payload = "{\"message\":\"Three wrong guesses made\",\"timestamp\":\"" + String(millis()) + "\"}";

        int httpCode = http.POST(payload);
        if (httpCode == 200)
        {
            Serial.println("Three wrong guesses notification sent.");
        }
        else
        {
            Serial.println("Failed to send three wrong guesses notification. HTTP code: " + String(httpCode));
        }
        http.end();
    }
}

// Handle password entry logic
void handlePasswordEntry(char key)
{
    if (key == 'D')
    { // Reset the password if '9' is pressed
        enteredPassword = "";
        Serial.println("Password reset.");
        return;
    }

    // Avoid recognizing the same keypress repeatedly
    if (key != lastKeyPressed)
    {
        lastKeyPressed = key;
        lastTimeKeyPressed = millis();

        // Add key to the entered password
        enteredPassword += key;
        Serial.println("Key pressed: " + String(key));
        Serial.println("Current Password: " + enteredPassword);

        // Check password length and validate
        if (enteredPassword.length() == 4)
        {
            if (enteredPassword == correctPassword)
            {
                stopAlarm();         // Deactivate the alarm
                wrongGuessCount = 0; // Reset wrong guess count
            }
            else
            {
                Serial.println("Incorrect password!");
                wrongGuessCount++;
                if (wrongGuessCount >= 3)
                {
                    notifyThreeWrongGuesses(); // Notify the hub
                }
            }
            enteredPassword = ""; // Reset the password for next attempt
        }
    }
}

// HTTP handler for activating the alarm
static esp_err_t activate_alarm_handler(httpd_req_t *req)
{
    startAlarm();
    const char *resp = "Alarm activated!";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// HTTP handler for deactivating the alarm
static esp_err_t deactivate_alarm_handler(httpd_req_t *req)
{
    stopAlarm();
    const char *resp = "Alarm deactivated!";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// test route
static esp_err_t test_handler(httpd_req_t *req)
{
    const char *resp = "Board works!";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// Start HTTP server and register routes
void startServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t activate_alarm = {
        .uri = "/activate_alarm",
        .method = HTTP_POST,
        .handler = activate_alarm_handler,
        .user_ctx = NULL};

    httpd_uri_t deactivate_alarm = {
        .uri = "/deactivate_alarm",
        .method = HTTP_POST,
        .handler = deactivate_alarm_handler,
        .user_ctx = NULL};

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &activate_alarm);
        httpd_register_uri_handler(server, &deactivate_alarm);
        Serial.println("HTTP server started.");
    }
    else
    {
        Serial.println("Failed to start HTTP server.");
    }
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

    http.end();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Proximity Alarm System with Network Hub Integration");

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(BUZZER_PIN, HIGH); // Ensure buzzer is off initially
    keypad.initialize();

    // Connect to WiFi
    if (!connectToWiFi(ssid, password))
    {
        return;
    }

    // Register the board with the hub if connected
    registerBoard();

    // Start the HTTP server
    startServer();
}

void loop()
{
    if (millis() - board_status_milis > 1000)
    {
        send_board_status();
        board_status_milis = millis();
    }

    // Wait for 60 seconds if system is disabled
    if (millis() < systemDisabledUntil)
    {
        return;
    }

    // Measure distance
    long distance = measureDistance();

    // Trigger alarm if proximity is detected
    if (distance > 10 && distance < 30 && !alarmActive)
    {
        startAlarm();
    }

    // Handle keypad input
    char key = keypad.getKey();
    if (key != '\0' && millis() - lastTimeKeyPressed > 250)
    { // Debouncing
        handlePasswordEntry(key);
    }
}
