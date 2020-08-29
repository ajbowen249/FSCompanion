/**
 * FSCompanion Physical client
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

#define DEFAULT_WIFI_NETWORK "Hanshotfirst (2G)"
#define DEFAULT_WIFI_PASSWORD "12Parsecs"
#define DEFAULT_IP_ADDRESS "10.0.0.185"
#define DEFAULT_PORT 8080

#define SCREEN_1_ADDR 0x3C

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define HTTP_BUFFER_SIZE 1024
char httpRecvBuffer[HTTP_BUFFER_SIZE];

Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

struct SimProps {
    double throttle;
};

void setup() {
    Serial.begin(9600);
    initDisplay1();
    connectToWifi(DEFAULT_WIFI_NETWORK, DEFAULT_WIFI_PASSWORD);
}

void loop() {
    delay(100);
    SimProps props;
    if (!getSimProps(props)) {
        return;
    }

    display1.clearDisplay();
    display1.setTextSize(1);
    display1.setTextColor(SSD1306_WHITE);
    display1.setCursor(0,0);
    display1.print("Throttle: ");
    display1.print(props.throttle);
    display1.println("%");

    display1.display();
}

bool getSimProps(SimProps& props) {
    WiFiClient client;
    client.setTimeout(500);
    if (!client.connect(DEFAULT_IP_ADDRESS, DEFAULT_PORT)) {
        Serial.println("Failed to connect.");
        return false;
    }

    client.println("GET /sim_props HTTP/1.1");
    client.print("Host: ");
    client.println(DEFAULT_IP_ADDRESS);
    client.println("Connection: close");
    client.println();

    for (int i = 0; i < HTTP_BUFFER_SIZE; i++) {
        httpRecvBuffer[i] = 0;
    }

    while(!client.available()) {}

    int bufferIndex = 0;

    while (client.available()) {
        char c = client.read();
        httpRecvBuffer[bufferIndex++] = c;
    }

    if (bufferIndex == 0) {
        Serial.println("No data.");
        return false;
    }

    char* withoutHeaders = strstr(httpRecvBuffer, "\r\n\r\n");
    if (withoutHeaders == NULL) {
        Serial.println("did not find body");
        return false;
    }

    withoutHeaders += 4;

    withoutHeaders = strstr(withoutHeaders, "\r\n");
    if (withoutHeaders == NULL) {
        Serial.println("did not find body after length");
        return false;
    }

    withoutHeaders += 2;

    client.stop();

    DynamicJsonDocument doc(1042);
    if (deserializeJson(doc, withoutHeaders) != DeserializationError::Ok) {
        Serial.println("failed to parse");
        Serial.println(withoutHeaders);
        return false;
    }

    props.throttle = doc["throttle"];

    return true;
}

void initDisplay1() {
    display1.begin(SSD1306_SWITCHCAPVCC, SCREEN_1_ADDR);
    display1.clearDisplay();
}

bool connectToWifi(const char* networkName, const char* password) {
    display1.clearDisplay();
    display1.setTextSize(2);
    display1.setTextColor(SSD1306_WHITE);
    display1.setCursor(0,0);
    display1.cp437(true);
    display1.print("Connecting");
    display1.setTextSize(1);
    display1.setCursor(0, 20);
    display1.print(networkName);
    display1.display();

    resetActivityIndicator();

    int timeoutIterations = (30 * 1000) / 100;
    int iterations = 0;

    WiFi.begin(networkName, password);
    while (WiFi.status() != WL_CONNECTED) {
        iterateActivityIndictor();
        delay(100);
        iterations++;
        if (iterations > timeoutIterations) {
            display1.clearDisplay();
            display1.setTextSize(2);
            display1.setTextColor(SSD1306_WHITE);
            display1.setCursor(0,0);
            display1.println("Timed Out");
            const auto status = WiFi.status();
            if (status == WL_NO_SHIELD) {
                display1.print("WL_NO_SHIELD");
            } else if (status == WL_IDLE_STATUS) {
                display1.print("WL_IDLE_STATUS");
            } else if (status == WL_NO_SSID_AVAIL) {
                display1.print("WL_NO_SSID_AVAIL");
            } else if (status == WL_SCAN_COMPLETED) {
                display1.print("WL_SCAN_COMPLETED");
            } else if (status == WL_CONNECT_FAILED) {
                display1.print("WL_CONNECT_FAILED");
            } else if (status == WL_CONNECTION_LOST) {
                display1.print("WL_CONNECTION_LOST");
            } else if (status == WL_DISCONNECTED) {
                display1.print("WL_DISCONNECTED");
            }
            display1.display();
            delay(1000);
            return false;
        }
    }

    display1.clearDisplay();
    display1.setTextSize(2);
    display1.setTextColor(SSD1306_WHITE);
    display1.setCursor(0,0);
    display1.print("Connected!");
    display1.display();
    delay(1000);

    return true;
}

#define ACTIVITY_INDICATOR_FRAMES 10
#define HALF_FRAMES (ACTIVITY_INDICATOR_FRAMES / 2)
#define ACTIVITY_INDICATOR_X 64
#define ACTIVITY_INDICATOR_Y 48
#define ACTIVITY_INDICATOR_RADIUS 16

int activityIndicatorFrame;

void resetActivityIndicator() {
    activityIndicatorFrame = 0;
}

void iterateActivityIndictor() {
    display1.fillRect(
        ACTIVITY_INDICATOR_X - ACTIVITY_INDICATOR_RADIUS,
        ACTIVITY_INDICATOR_Y - ACTIVITY_INDICATOR_RADIUS,
        ACTIVITY_INDICATOR_RADIUS * 2,
        ACTIVITY_INDICATOR_RADIUS * 2,
        SSD1306_BLACK
    );

    int radius;
    if (activityIndicatorFrame <= HALF_FRAMES) {
        radius = ((float)activityIndicatorFrame / HALF_FRAMES) * ACTIVITY_INDICATOR_RADIUS;
    } else {
        int scaler = HALF_FRAMES - (activityIndicatorFrame - HALF_FRAMES);
        radius = ((float)scaler / HALF_FRAMES) * ACTIVITY_INDICATOR_RADIUS;
    }

    display1.fillCircle(
        ACTIVITY_INDICATOR_X,
        ACTIVITY_INDICATOR_Y,
        radius,
        SSD1306_WHITE
    );

    display1.display();

    activityIndicatorFrame = (activityIndicatorFrame + 1) % ACTIVITY_INDICATOR_FRAMES;
}