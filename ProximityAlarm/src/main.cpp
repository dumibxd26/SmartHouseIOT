#include "Keypad/Keypad.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_http_server.h>
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
const char* ssid_primary = SSID_PRIMARY;
const char* password_primary = PASSWORD_PRIMARY;
const char* ssid_secondary = SSID_SECONDARY;
const char* password_secondary = PASSWORD_SECONDARY;

const char* hub_primary = HUB_PRIMARY // Bound to primary network
const char* hub_secondary = HUB_SECONDARY // Bound to secondary network
const char* PORT = PORT_C;
String hub_address = "";

const char* board_name = "ProximityBoard"; // Board name

// Password variables
String enteredPassword = "";           // Stores entered password
const String correctPassword = "1523"; // Set correct password
char lastKeyPressed = '\0';            // To track debouncing
unsigned long lastTimeKeyPressed = 0;  // Timestamp for last key press
int wrongGuessCount = 0;               // Tracks wrong guesses

bool alarmActive = false;
unsigned long systemDisabledUntil = 0; // Timestamp to disable system for 60 seconds

// HTTP Server handle
httpd_handle_t server = NULL;

// Function to test hub connectivity
bool testHubConnection(const char* hub_ip) {
    HTTPClient http;
    String test_url = String("http://") + hub_ip + ":" + PORT + "/test";
    http.begin(test_url);
    int httpCode = http.GET();
    http.end();
    return (httpCode == 200);
}

// Function to connect to WiFi
bool connectToWiFi(const char* ssid, const char* password) {
    Serial.println("Attempting to connect to WiFi: " + String(ssid));
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();
    const unsigned long timeout = 10000; // 10 seconds timeout

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi: " + String(ssid));
        Serial.println("IP Address: " + WiFi.localIP().toString());
        return true;
    } else {
        Serial.println("\nFailed to connect to WiFi: " + String(ssid));
        return false;
    }
}

// Function to decide the hub address
void decideHubAddress() {
    if (WiFi.SSID() == ssid_primary) {
        hub_address = hub_primary;
    } else if (WiFi.SSID() == ssid_secondary) {
        hub_address = hub_secondary;
    } else {
        hub_address = "";
    }

    if (!hub_address.isEmpty() && testHubConnection(hub_address.c_str())) {
        Serial.println("Hub connection successful: " + hub_address);
    } else {
        Serial.println("Failed to connect to hub. Check configuration.");
        hub_address = ""; // Clear hub address if connection fails
    }
}

// Function to register the board with the hub
void registerBoard() {
    if (hub_address.isEmpty()) return;

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

// Function to measure distance with HC-SR04
long measureDistance() {
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
void sendNotificationToHub(String message) {
    if (!hub_address.isEmpty()) {
        HTTPClient http;
        String url = String("http://") + hub_address + ":" + PORT + "/front_door_alarm";
        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        String payload = "{\"message\":\"" + message + "\",\"timestamp\":\"" + String(millis()) + "\"}";

        int httpCode = http.POST(payload);
        if (httpCode == 200) {
            Serial.println("Notification sent to hub.");
        } else {
            Serial.println("Failed to send notification. HTTP code: " + String(httpCode));
        }
        http.end();
    } else {
        Serial.println("Hub address is empty. Cannot send notification.");
    }
}

// Function to start the alarm
void startAlarm() {
    if (!alarmActive) {
        alarmActive = true;
        Serial.println("Alarm triggered!");
        digitalWrite(BUZZER_PIN, LOW); // Turn on buzzer
        sendNotificationToHub("Alarm triggered!"); // Notify hub
    }
}

// Function to stop the alarm
void stopAlarm() {
    if (alarmActive) {
        alarmActive = false;
        Serial.println("Alarm deactivated. System will restart in 60 seconds.");
        digitalWrite(BUZZER_PIN, HIGH); // Turn off buzzer
        systemDisabledUntil = millis() + 60000; // Disable for 60 seconds
    }
}

// Notify the hub about three wrong guesses
void notifyThreeWrongGuesses() {
    if (!hub_address.isEmpty()) {
        HTTPClient http;
        String url = String("http://") + hub_address + ":" + PORT + "/three_wrong_guesses";
        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        String payload = "{\"message\":\"Three wrong guesses made\",\"timestamp\":\"" + String(millis()) + "\"}";

        int httpCode = http.POST(payload);
        if (httpCode == 200) {
            Serial.println("Three wrong guesses notification sent.");
        } else {
            Serial.println("Failed to send three wrong guesses notification. HTTP code: " + String(httpCode));
        }
        http.end();
    }
}

// Handle password entry logic
void handlePasswordEntry(char key) {
    if (key == 'D') { // Reset the password if '9' is pressed
        enteredPassword = "";
        Serial.println("Password reset.");
        return;
    }

    // Avoid recognizing the same keypress repeatedly
    if (key != lastKeyPressed) {
        lastKeyPressed = key;
        lastTimeKeyPressed = millis();

        // Add key to the entered password
        enteredPassword += key;
        Serial.println("Key pressed: " + String(key));
        Serial.println("Current Password: " + enteredPassword);

        // Check password length and validate
        if (enteredPassword.length() == 4) {
            if (enteredPassword == correctPassword) {
                stopAlarm(); // Deactivate the alarm
                wrongGuessCount = 0; // Reset wrong guess count
            } else {
                Serial.println("Incorrect password!");
                wrongGuessCount++;
                if (wrongGuessCount >= 3) {
                    notifyThreeWrongGuesses(); // Notify the hub
                }
            }
            enteredPassword = ""; // Reset the password for next attempt
        }
    }
}

// HTTP handler for activating the alarm
static esp_err_t activate_alarm_handler(httpd_req_t *req) {
    startAlarm();
    const char *resp = "Alarm activated!";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// HTTP handler for deactivating the alarm
static esp_err_t deactivate_alarm_handler(httpd_req_t *req) {
    stopAlarm();
    const char *resp = "Alarm deactivated!";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// test route
static esp_err_t test_handler(httpd_req_t *req) {
    const char *resp = "Board works!";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// Start HTTP server and register routes
void startServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t activate_alarm = {
        .uri = "/activate_alarm",
        .method = HTTP_POST,
        .handler = activate_alarm_handler,
        .user_ctx = NULL
    };

    httpd_uri_t deactivate_alarm = {
        .uri = "/deactivate_alarm",
        .method = HTTP_POST,
        .handler = deactivate_alarm_handler,
        .user_ctx = NULL
    };

    httpd_uri_t test = {
        .uri = "/test",
        .method = HTTP_GET,
        .handler = test_handler,
        .user_ctx = NULL
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &activate_alarm);
        httpd_register_uri_handler(server, &deactivate_alarm);
        httpd_register_uri_handler(server, &test);
        Serial.println("HTTP server started.");
    } else {
        Serial.println("Failed to start HTTP server.");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Proximity Alarm System with Network Hub Integration");

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(BUZZER_PIN, HIGH); // Ensure buzzer is off initially
    keypad.initialize();

    // Attempt to connect to primary network
    if (!connectToWiFi(ssid_primary, password_primary)) {
        // If primary network fails, try secondary network
        if (!connectToWiFi(ssid_secondary, password_secondary)) {
            Serial.println("Failed to connect to any network. System will restart.");
            ESP.restart();
        }
    }

    // Decide the hub address based on the connected network
    decideHubAddress();

    // Register the board with the hub if connected
    registerBoard();

    // Start the HTTP server
    startServer();
}

void loop() {
    // Wait for 60 seconds if system is disabled
    if (millis() < systemDisabledUntil) {
        return;
    }

    // Measure distance
    long distance = measureDistance();

    // Trigger alarm if proximity is detected
    if (distance > 10 && distance < 30 && !alarmActive) {
        startAlarm();
    }

    // Handle keypad input
    char key = keypad.getKey();
    if (key != '\0' && millis() - lastTimeKeyPressed > 250) { // Debouncing
        handlePasswordEntry(key);
    }
}
