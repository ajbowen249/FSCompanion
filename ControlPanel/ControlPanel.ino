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

#define MAIN_SCREEN_ADDR 0x3C
#define SCND_SCREEN_ADDR 0x3D

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define HTTP_BUFFER_SIZE 1024
char httpRecvBuffer[HTTP_BUFFER_SIZE];

#define SH_IN_DAT 14
#define SH_IN_CLK 12
#define SH_IN_LAT 13

#define THROTTLE_SENSITIVITY 2

#define ATTACH_TIMER()                            \
    timer1_attachInterrupt(pollInputReg);         \
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); \
    timer1_write(10000);                          \

Adafruit_SSD1306 mainDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_SSD1306 secondaryDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

struct SimProps {
    double throttle;
};

struct SimPropsState {
    bool throttleDirty;
};

struct DPad {
    bool left;
    bool up;
    bool down;
    bool right;
};

struct Encoder {
    bool a;
    bool b;
    bool btn;
    bool inc;
    bool dec;
};

struct Inputs {
    DPad dpad;
    Encoder enc1;
    Encoder enc2;
    bool a;
    bool b;
    bool tl;
    bool bl;
    bool tr;
    bool br;
};

Inputs inputs;

SimProps props;
SimPropsState propsState;
bool transacting = false;

byte customShiftIn(int myDataPin, int myClockPin) {
    int temp = 0;
    byte myDataIn = 0;

    for (int i=7; i>=0; i--) {
        digitalWrite(myClockPin, 0);
        delayMicroseconds(2);
        temp = digitalRead(myDataPin);
        if (temp) {
            myDataIn = myDataIn | (1 << i);
        }

        digitalWrite(myClockPin, 1);
    }

    return myDataIn;
}

uint16_t poll() {
    digitalWrite(SH_IN_LAT, HIGH);
    delayMicroseconds(20);
    digitalWrite(SH_IN_LAT, LOW);
    digitalWrite(SH_IN_CLK, LOW);
    uint16_t out = customShiftIn(SH_IN_DAT, SH_IN_CLK);
    out <<= 8;
    return out | customShiftIn(SH_IN_DAT, SH_IN_CLK);
}

void processInputs(uint16_t raw) {
    inputs.dpad.left  = raw & 0x0100;
    inputs.dpad.up    = raw & 0x0200;
    inputs.dpad.down  = raw & 0x0400;
    inputs.dpad.right = raw & 0x0800;
    inputs.enc1.btn = raw & 0x0040;
    inputs.enc2.btn = raw & 0x0080;
    inputs.a =  raw & 0x1000;
    inputs.b =  raw & 0x2000;
    inputs.tl = raw & 0x4000;
    inputs.bl = raw & 0x8000;
    inputs.tr = raw & 0x0020;
    inputs.br = raw & 0x0010;

    inputs.enc1.inc = false;
    inputs.enc1.dec = false;
    inputs.enc2.inc = false;
    inputs.enc2.dec = false;

    auto newA1 = raw & 0x0008;
    auto newB1 = raw & 0x0004;
    auto newA2 = raw & 0x00001;
    auto newB2 = raw & 0x00002;

    if (newA1 && !inputs.enc1.a) {
        if (!newB1) {
            inputs.enc1.inc = true;
        } else {
            inputs.enc1.dec = true;
        }
    }

    if (newA2 && !inputs.enc2.a) {
        if (!newB2) {
            inputs.enc2.inc = true;
        } else {
            inputs.enc2.dec = true;
        }
    }

    inputs.enc1.a = newA1;
    inputs.enc1.b = newB1;
    inputs.enc2.a = newA2;
    inputs.enc2.b = newB2;
}

void ICACHE_RAM_ATTR pollInputReg() {
    if (transacting) {
        return;
    }

    timer1_detachInterrupt();

    auto b = poll();
    processInputs(b);

    if (inputs.enc1.inc) {
        props.throttle += THROTTLE_SENSITIVITY;
        propsState.throttleDirty = true;
    } else if (inputs.enc1.dec) {
        props.throttle -= THROTTLE_SENSITIVITY;
        propsState.throttleDirty = true;
    }

    ATTACH_TIMER()

    return;
}

void setup() {
    pinMode(SH_IN_DAT, INPUT);
    pinMode(SH_IN_CLK, OUTPUT);
    pinMode(SH_IN_LAT, OUTPUT);

    digitalWrite(SH_IN_LAT, LOW);
    digitalWrite(SH_IN_CLK, HIGH);
    delay(100);

    Serial.begin(9600);
    initDisplays();

    mainDisplay.clearDisplay();
    mainDisplay.setTextSize(1);
    mainDisplay.setTextColor(SSD1306_WHITE);
    mainDisplay.setCursor(0,0);
    mainDisplay.println("Bottom screen!");
    mainDisplay.display();

    secondaryDisplay.clearDisplay();
    secondaryDisplay.setTextSize(1);
    secondaryDisplay.setTextColor(SSD1306_WHITE);
    secondaryDisplay.setCursor(0,0);
    secondaryDisplay.println("Top screen!");
    secondaryDisplay.display();

    connectToWifi(DEFAULT_WIFI_NETWORK, DEFAULT_WIFI_PASSWORD);

    // Get initial state so tracking is correct
    auto raw = poll();
    processInputs(raw);
    inputs.enc1.inc = false;
    inputs.enc1.dec = false;
    inputs.enc2.inc = false;
    inputs.enc2.dec = false;

    ATTACH_TIMER()
}

void loop() {
    displayButtonDebugState();

    if (!transactSimProps(props)) {
        return;
    }

    secondaryDisplay.clearDisplay();
    secondaryDisplay.setTextSize(1);
    secondaryDisplay.setTextColor(SSD1306_WHITE);
    secondaryDisplay.setCursor(0,0);
    secondaryDisplay.print("Throttle: ");
    secondaryDisplay.print(props.throttle);
    secondaryDisplay.println("%");

    secondaryDisplay.display();

    delay(50);
}

bool transactSimProps(SimProps& props) {
    transacting = true;
    StaticJsonDocument<1024> updateJson;
    if (propsState.throttleDirty) {
        updateJson["throttle"] = props.throttle;
    }

    String out;
    serializeJson(updateJson, out);

    WiFiClient client;
    client.setTimeout(500);
    if (!client.connect(DEFAULT_IP_ADDRESS, DEFAULT_PORT)) {
        Serial.println("Failed to connect.");
        transacting = false;
        return false;
    }

    client.println("POST /sim_props HTTP/1.1");
    client.print("Host: ");
    client.println(DEFAULT_IP_ADDRESS);
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(out.length());
    client.println();
    client.println(out);

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
        transacting = false;
        return false;
    }

    char* withoutHeaders = strstr(httpRecvBuffer, "\r\n\r\n");
    if (withoutHeaders == NULL) {
        Serial.println("did not find body");
        transacting = false;
        return false;
    }

    withoutHeaders += 4;

    withoutHeaders = strstr(withoutHeaders, "\r\n");
    if (withoutHeaders == NULL) {
        Serial.println("did not find body after length");
        transacting = false;
        return false;
    }

    withoutHeaders += 2;

    client.stop();

    DynamicJsonDocument doc(1042);
    if (deserializeJson(doc, withoutHeaders) != DeserializationError::Ok) {
        Serial.println("failed to parse");
        Serial.println(withoutHeaders);
        transacting = false;
        return false;
    }

    props.throttle = doc["throttle"];

    transacting = false;
    propsState.throttleDirty = false;
    return true;
}

void initDisplays() {
    mainDisplay.begin(SSD1306_SWITCHCAPVCC, MAIN_SCREEN_ADDR);
    mainDisplay.clearDisplay();

    secondaryDisplay.begin(SSD1306_SWITCHCAPVCC, SCND_SCREEN_ADDR);
    secondaryDisplay.clearDisplay();
}

bool connectToWifi(const char* networkName, const char* password) {
    mainDisplay.clearDisplay();
    mainDisplay.setTextSize(2);
    mainDisplay.setTextColor(SSD1306_WHITE);
    mainDisplay.setCursor(0,0);
    mainDisplay.cp437(true);
    mainDisplay.print("Connecting");
    mainDisplay.setTextSize(1);
    mainDisplay.setCursor(0, 20);
    mainDisplay.print(networkName);
    mainDisplay.display();

    resetActivityIndicator();

    int timeoutIterations = (30 * 1000) / 100;
    int iterations = 0;

    WiFi.begin(networkName, password);
    while (WiFi.status() != WL_CONNECTED) {
        iterateActivityIndictor();
        delay(100);
        iterations++;
        if (iterations > timeoutIterations) {
            mainDisplay.clearDisplay();
            mainDisplay.setTextSize(2);
            mainDisplay.setTextColor(SSD1306_WHITE);
            mainDisplay.setCursor(0,0);
            mainDisplay.println("Timed Out");
            const auto status = WiFi.status();
            if (status == WL_NO_SHIELD) {
                mainDisplay.print("WL_NO_SHIELD");
            } else if (status == WL_IDLE_STATUS) {
                mainDisplay.print("WL_IDLE_STATUS");
            } else if (status == WL_NO_SSID_AVAIL) {
                mainDisplay.print("WL_NO_SSID_AVAIL");
            } else if (status == WL_SCAN_COMPLETED) {
                mainDisplay.print("WL_SCAN_COMPLETED");
            } else if (status == WL_CONNECT_FAILED) {
                mainDisplay.print("WL_CONNECT_FAILED");
            } else if (status == WL_CONNECTION_LOST) {
                mainDisplay.print("WL_CONNECTION_LOST");
            } else if (status == WL_DISCONNECTED) {
                mainDisplay.print("WL_DISCONNECTED");
            }
            mainDisplay.display();
            delay(1000);
            return false;
        }
    }

    mainDisplay.clearDisplay();
    mainDisplay.setTextSize(2);
    mainDisplay.setTextColor(SSD1306_WHITE);
    mainDisplay.setCursor(0,0);
    mainDisplay.print("Connected!");
    mainDisplay.display();
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
    mainDisplay.fillRect(
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

    mainDisplay.fillCircle(
        ACTIVITY_INDICATOR_X,
        ACTIVITY_INDICATOR_Y,
        radius,
        SSD1306_WHITE
    );

    mainDisplay.display();

    activityIndicatorFrame = (activityIndicatorFrame + 1) % ACTIVITY_INDICATOR_FRAMES;
}

void drawButtonState(int x, int y, bool state) {
    if (state) {
        mainDisplay.fillCircle(x, y, 5, SSD1306_WHITE);
    } else {
        mainDisplay.drawCircle(x, y, 5, SSD1306_WHITE);
    }
}

void displayButtonDebugState() {
    mainDisplay.clearDisplay();
    drawButtonState(  5, 48, inputs.dpad.left);
    drawButtonState( 15, 38, inputs.dpad.up);
    drawButtonState( 15, 58, inputs.dpad.down);
    drawButtonState( 25, 48, inputs.dpad.right);
    drawButtonState( 45, 48, inputs.a);
    drawButtonState( 57, 48, inputs.b);
    drawButtonState(  5,  6, inputs.tl);
    drawButtonState(  5, 18, inputs.bl);
    drawButtonState( 17,  6, inputs.tr);
    drawButtonState( 17, 18, inputs.br);
    drawButtonState(120,  6, inputs.enc1.btn);
    drawButtonState(108,  6, inputs.enc1.a);
    drawButtonState( 96,  6, inputs.enc1.b);
    drawButtonState(120, 18, inputs.enc2.btn);
    drawButtonState(108, 18, inputs.enc2.a);
    drawButtonState( 96, 18, inputs.enc2.b);

    mainDisplay.display();
}
