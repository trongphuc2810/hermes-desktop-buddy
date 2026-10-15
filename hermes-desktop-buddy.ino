#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include "LGFX_ES3C28P.h"
#include "es8311.h"

extern "C" {
  #include "muse_pixel.h"
  #include "muse_state.h"
}

// ----------------- Pin Definitions (ES3C28P) -----------------
#define PIN_PA_EN       1   // Audio Power Amplifier Enable (Active LOW)
#define PIN_I2S_MCLK    4   // ES8311 MCLK
#define PIN_I2S_BCLK    5   // ES8311 BCLK
#define PIN_I2S_DIN     6   // Mic In
#define PIN_I2S_LRCK    7   // LRCK (WS)
#define PIN_I2S_DOUT    8   // Speaker Out
#define PIN_BAT_ADC     9   // Battery ADC
#define PIN_I2C_SCL     15  // Shared I2C SCL (Touch + ES8311)
#define PIN_I2C_SDA     16  // Shared I2C SDA (Touch + ES8311)
#define PIN_RGB_LED     42  // WS2812B RGB LED
#define PIN_BTN_BOOT    0   // BOOT Button

// ----------------- Hardware Objects -----------------
LGFX_ES3C28P lcd;
ES8311 codec(&Wire);
WebSocketsClient webSocket;

// ----------------- Configuration & State -----------------
const char* FIRMWARE_VERSION = "1.0.0";
const char* GITHUB_REPO = "trongphuc2810/hermes-desktop-buddy";

muse_mode_t current_mode = MUSE_MODE_BOOT;
unsigned long mode_start_ms = 0;
float audio_level = 0.0f;
bool is_touched = false;

// 240-wide scanline buffer for avatar DMA push (8 rows = 240 * 8 * 2 = 3840 bytes)
static uint16_t line_buffer[240 * 8];

void switch_mode(muse_mode_t new_mode) {
    current_mode = new_mode;
    mode_start_ms = millis();
}

// ----------------- GitHub OTA Updater -----------------
void check_github_ota() {
    Serial.println("[OTA] Checking GitHub for updates...");
    lcd.fillRect(0, 280, 240, 40, 0x0000);
    lcd.setTextColor(0xFFFF, 0x0000);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.drawString("Checking OTA...", 120, 300);

    WiFiClientSecure client;
    client.setInsecure(); // GitHub redirect to AWS S3 CDN

    HTTPClient http;
    String url = String("https://api.github.com/repos/") + GITHUB_REPO + "/releases/latest";
    http.begin(client, url);
    http.addHeader("User-Agent", "ESP32-S3-Hermes");

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            const char* tag_name = doc["tag_name"];
            Serial.printf("[OTA] Latest release tag: %s, current: %s\n", tag_name, FIRMWARE_VERSION);

            if (tag_name && strcmp(tag_name, FIRMWARE_VERSION) != 0) {
                // Find firmware.bin download URL
                JsonArray assets = doc["assets"];
                String bin_url = "";
                for (JsonObject asset : assets) {
                    const char* name = asset["name"];
                    if (name && strstr(name, ".bin")) {
                        bin_url = asset["browser_download_url"].as<String>();
                        break;
                    }
                }

                if (bin_url.length() > 0) {
                    Serial.printf("[OTA] Downloading update from: %s\n", bin_url.c_str());
                    switch_mode(MUSE_MODE_LAPTOP);
                    lcd.drawString("Updating firmware...", 120, 300);

                    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                    t_httpUpdate_return ret = httpUpdate.update(client, bin_url);
                    if (ret == HTTP_UPDATE_OK) {
                        Serial.println("[OTA] Update OK! Rebooting...");
                        ESP.restart();
                    } else {
                        Serial.printf("[OTA] Update failed: %s\n", httpUpdate.getLastErrorString().c_str());
                    }
                }
            } else {
                Serial.println("[OTA] Already on the latest version.");
            }
        }
    }
    http.end();
}

// ----------------- WebSocket Event Handler -----------------
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("[WS] Disconnected from Hermes Gateway");
            break;
        case WStype_CONNECTED:
            Serial.println("[WS] Connected to Hermes Gateway");
            webSocket.sendTXT("{\"type\":\"auth\",\"device\":\"esp32s3_display\"}");
            break;
        case WStype_TEXT: {
            Serial.printf("[WS] Received: %s\n", payload);
            JsonDocument doc;
            if (!deserializeJson(doc, payload)) {
                const char* msg_type = doc["type"];
                if (msg_type) {
                    if (strcmp(msg_type, "stt") == 0) {
                        const char* text = doc["text"];
                        lcd.fillRect(0, 280, 240, 40, 0x0000);
                        lcd.drawString(text ? text : "", 120, 300);
                        switch_mode(MUSE_MODE_THINKING);
                    } else if (strcmp(msg_type, "tts_start") == 0) {
                        switch_mode(MUSE_MODE_SPEAKING);
                    } else if (strcmp(msg_type, "tts_end") == 0) {
                        switch_mode(MUSE_MODE_IDLE);
                    }
                }
            }
            break;
        }
        case WStype_BIN:
            // Received audio chunk (PCM16) from Hermes TTS
            // Forward directly to I2S speaker DMA
            break;
        default:
            break;
    }
}

// ----------------- Setup -----------------
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n======================================");
    Serial.println("  Hermes Desktop Buddy (ES3C28P 2.8\")  ");
    Serial.printf("  Firmware Version: %s\n", FIRMWARE_VERSION);
    Serial.println("======================================");

    pinMode(PIN_PA_EN, OUTPUT);
    digitalWrite(PIN_PA_EN, HIGH); // Mute speaker amplifier initially (Active LOW)
    pinMode(PIN_BTN_BOOT, INPUT_PULLUP);

    // 1. Initialize Display & Touch (LovyanGFX)
    Serial.println("[Init] Initializing ILI9341V Display + FT6336G Touch...");
    lcd.init();
    lcd.setRotation(0); // 240x320 Portrait
    lcd.setBrightness(200);
    lcd.fillScreen(0x0000); // Black

    // Draw Top Status Bar
    lcd.setTextColor(0x7BEF, 0x0000);
    lcd.setTextDatum(textdatum_t::top_left);
    lcd.drawString("HERMES BUDDY", 10, 10);
    lcd.setTextDatum(textdatum_t::top_right);
    lcd.drawString("v1.0.0", 230, 10);

    // 2. Initialize ES8311 Audio Codec
    Serial.println("[Init] Initializing ES8311 Codec over I2C...");
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
    if (codec.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000)) {
        codec.setSampleRate(16000);
        codec.setBitsPerSample(16);
        codec.setVolume(85);
        codec.enableMicrophone(true);
        codec.setMicrophoneGain(5);
        Serial.println("[Init] ES8311 Codec initialized successfully!");
        digitalWrite(PIN_PA_EN, LOW); // Unmute amplifier
    } else {
        Serial.println("[Warn] ES8311 Codec not detected (running in display-only mode)");
    }

    // 3. Initialize Procedural Avatar Engine
    Serial.println("[Init] Initializing Meta Muse Procedural Avatar (240x240)...");
    muse_pixel_set_size(240);
    switch_mode(MUSE_MODE_BOOT);

    Serial.println("[Ready] System initialized successfully!");
}

// ----------------- Main Loop -----------------
void loop() {
    webSocket.loop();

    // 1. Check touch input
    int32_t tx, ty;
    if (lcd.getTouch(&tx, &ty)) {
        is_touched = true;
    } else {
        is_touched = false;
    }

    // 2. BOOT button cycles modes for testing/debugging
    static bool last_btn_state = HIGH;
    bool btn_state = digitalRead(PIN_BTN_BOOT);
    if (last_btn_state == HIGH && btn_state == LOW) {
        // Cycle next mode
        int next_m = ((int)current_mode + 1) % MUSE_MODE_COUNT;
        switch_mode((muse_mode_t)next_m);
        Serial.printf("[Mode] Switched to mode ID: %d\n", next_m);
        delay(200); // debounce
    }
    last_btn_state = btn_state;

    // Auto-transition from BOOT -> IDLE after 3.2 seconds
    if (current_mode == MUSE_MODE_BOOT && (millis() - mode_start_ms > 3200)) {
        switch_mode(MUSE_MODE_IDLE);
    }

    // 3. Render 1 Frame of Procedural Avatar
    unsigned long now = millis();
    float t = now / 1000.0f;
    float mode_t = (now - mode_start_ms) / 1000.0f;

    muse_pose_t pose = {
        .mode = current_mode,
        .t = t,
        .mode_t = mode_t,
        .level = audio_level,
        .happy = is_touched ? 1.0f : 0.0f
    };
    muse_pixel_render(&pose);

    // 4. Push Scanlines to Display via DMA (center 240x240 block on 240x320 screen)
    // y offset = 40 (between y=40 and y=280)
    for (int y = 0; y < 240; y += 8) {
        muse_pixel_scale(line_buffer, 240, 0, 239, y, y + 7);
        lcd.pushImage(0, 40 + y, 240, 8, line_buffer);
    }

    // 25 FPS frame rate limiter (40ms per frame)
    delay(10);
}
