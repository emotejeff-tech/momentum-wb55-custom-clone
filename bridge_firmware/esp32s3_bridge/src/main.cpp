// ESP32-S3 companion bridge for momentum-wb55-custom-clone.
//
// Role: Wi-Fi + HTTPS AI bridge over UART. It speaks the framed protocol in
// ../../bridge_firmware/protocol/bridge_protocol.h, answers AI_REQ by calling an
// OpenAI-compatible endpoint, and reports Wi-Fi scan telemetry.
//
// It is NOT Pwnagotchi: an ESP32 has no monitor mode / bettercap. Use the Pi
// Zero 2 W image for real Pwnagotchi.
//
// Build: pio run -e esp32s3   (see platformio.ini)
// Copy include/secrets.h.example to include/secrets.h and fill it in.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "bridge_protocol.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Create include/secrets.h from include/secrets.h.example"
#endif

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

static constexpr int kUartRxPin = 18;
static constexpr int kUartTxPin = 17;
static constexpr uint32_t kUartBaud = 921600;

static constexpr uint32_t kHeartbeatMs = 2000;
static constexpr uint32_t kTelemetryMs = 10000;

static constexpr const char* kLlmModel = "gpt-4o-mini";
static constexpr const char* kSystemPrompt =
    "You are the AI assistant inside a handheld STM32WB55 multi-tool. "
    "Answer briefly (max 4 short lines, plain ASCII). You cannot transmit RF, "
    "read files, or run commands. If asked to do hardware actions, explain that "
    "the user must use the device menu.";

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

HardwareSerial BridgeSerial(1);

static uint8_t s_seq = 0;
static uint32_t s_boot_ms = 0;

static uint8_t s_rx_encoded[1200];
static size_t s_rx_len = 0;
static uint8_t s_decoded[BRIDGE_MAX_FRAME];
static uint8_t s_tx[1200];

// ---------------------------------------------------------------------------
// Framing helpers
// ---------------------------------------------------------------------------

static void send_frame_seq(uint8_t type, uint8_t seq, uint8_t flags, const char* json) {
    size_t len = json ? strlen(json) : 0;
    if(len > BRIDGE_MAX_PAYLOAD) return;

    size_t encoded = bridge_frame_build(
        type, seq, flags, reinterpret_cast<const uint8_t*>(json), (uint16_t)len, s_tx,
        sizeof(s_tx));
    if(encoded) {
        BridgeSerial.write(s_tx, encoded);
    }
}

static void send_frame(uint8_t type, uint8_t flags, const char* json) {
    send_frame_seq(type, s_seq++, flags, json);
}

static void send_json_seq(uint8_t type, uint8_t seq, JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    send_frame_seq(type, seq, BRIDGE_FLAG_RESPONSE, out.c_str());
}

static void send_json(uint8_t type, JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    send_frame(type, 0, out.c_str());
}

static void send_error(uint8_t seq, const char* code, const char* message) {
    JsonDocument doc;
    doc["code"] = code;
    doc["msg"] = message;
    send_json_seq(BridgeTypeError, seq, doc);
}

static void send_ack(uint8_t seq) {
    send_frame_seq(BridgeTypeAck, seq, BRIDGE_FLAG_RESPONSE, nullptr);
}

// ---------------------------------------------------------------------------
// AI
// ---------------------------------------------------------------------------

static String llm_complete(const String& prompt) {
    if(WiFi.status() != WL_CONNECTED) {
        return "[offline] Wi-Fi is not connected.";
    }

    WiFiClientSecure client;
    // TODO: pin the API host certificate with client.setCACert(root_ca).
    // setInsecure() is acceptable only on a trusted bench network.
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(15000);
    if(!https.begin(client, String(LLM_BASE_URL) + "/chat/completions")) {
        return "[error] cannot open HTTPS connection";
    }
    https.addHeader("Content-Type", "application/json");
    https.addHeader("Authorization", String("Bearer ") + LLM_API_KEY);

    JsonDocument req;
    req["model"] = kLlmModel;
    JsonArray messages = req["messages"].to<JsonArray>();
    JsonObject sys = messages.add<JsonObject>();
    sys["role"] = "system";
    sys["content"] = kSystemPrompt;
    JsonObject user = messages.add<JsonObject>();
    user["role"] = "user";
    user["content"] = prompt;
    req["max_tokens"] = 220;
    req["temperature"] = 0.4;

    String body;
    serializeJson(req, body);

    int code = https.POST(body);
    if(code != 200) {
        String err = "[http " + String(code) + "] " + https.getString();
        https.end();
        return err.substring(0, 200);
    }

    String response = https.getString();
    https.end();

    JsonDocument resp;
    if(deserializeJson(resp, response)) return "[error] bad LLM response";

    const char* content = resp["choices"][0]["message"]["content"] | "";
    return String(content);
}

// ---------------------------------------------------------------------------
// Telemetry
// ---------------------------------------------------------------------------

static void send_wifi_telemetry() {
    int found = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false);
    if(found < 0) found = 0;

    JsonDocument doc;
    JsonObject pwn = doc["pwn"].to<JsonObject>();
    pwn["mode"] = "esp32-wifi";
    pwn["aps"] = found;
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["connected"] = (WiFi.status() == WL_CONNECTED);
    wifi["rssi"] = WiFi.RSSI();
    JsonArray nets = doc["networks"].to<JsonArray>();
    int limit = found < 5 ? found : 5;
    for(int i = 0; i < limit; i++) {
        JsonObject n = nets.add<JsonObject>();
        n["ssid"] = WiFi.SSID(i);
        n["rssi"] = WiFi.RSSI(i);
        n["ch"] = WiFi.channel(i);
    }
    WiFi.scanDelete();
    send_json(BridgeTypeTelemetry, doc);
}

// ---------------------------------------------------------------------------
// Frame handlers
// ---------------------------------------------------------------------------

static void handle_status_req(const BridgeFrame& frame) {
    JsonDocument doc;
    doc["battery"] = -1; // no battery measurement on the ESP32
    doc["temp"] = temperatureRead();
    doc["uptime"] = (millis() - s_boot_ms) / 1000;
    doc["wifi"] = (WiFi.status() == WL_CONNECTED);
    doc["ip"] = WiFi.localIP().toString();
    doc["heap"] = ESP.getFreeHeap();
    send_json_seq(BridgeTypeStatus, frame.seq, doc);
}

static void handle_ai_req(const BridgeFrame& frame) {
    JsonDocument req;
    if(deserializeJson(req, frame.payload, frame.payload_len)) {
        send_error(frame.seq, "bad_json", "AI_REQ payload is not JSON");
        return;
    }

    const char* id = req["id"] | "?";
    const char* prompt = req["prompt"] | "";
    if(prompt[0] == '\0') {
        JsonDocument err;
        err["id"] = id;
        err["text"] = "[error] empty prompt";
        send_json_seq(BridgeTypeAiResp, frame.seq, err);
        return;
    }

    String answer = llm_complete(String(prompt));

    JsonDocument resp;
    resp["id"] = id;
    resp["text"] = answer;
    resp["tool_call"] = nullptr;
    send_json_seq(BridgeTypeAiResp, frame.seq, resp);
}

static void dispatch(const BridgeFrame& frame) {
    if(frame.flags & BRIDGE_FLAG_ACK_REQ) send_ack(frame.seq);

    switch(frame.type) {
    case BridgeTypeHello: {
        JsonDocument doc;
        doc["proto"] = BRIDGE_VERSION;
        doc["fw"] = "esp32s3-bridge/0.1";
        JsonArray caps = doc["caps"].to<JsonArray>();
        caps.add("ai");
        caps.add("wifi_scan");
        send_json_seq(BridgeTypeHello, frame.seq, doc);
        break;
    }
    case BridgeTypeHeartbeat: {
        JsonDocument doc;
        doc["uptime"] = (millis() - s_boot_ms) / 1000;
        doc["state"] = "ok";
        send_json_seq(BridgeTypeHeartbeat, frame.seq, doc);
        break;
    }
    case BridgeTypeStatusReq:
        handle_status_req(frame);
        break;
    case BridgeTypeAiReq:
        handle_ai_req(frame);
        break;
    case BridgeTypeCmd:
        /* This variant cannot execute device commands; report that clearly
         * instead of silently dropping them. */
        send_error(frame.seq, "unsupported", "ESP32 bridge is AI-only");
        break;
    case BridgeTypeAck:
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// UART pump
// ---------------------------------------------------------------------------

static void poll_uart() {
    while(BridgeSerial.available()) {
        int byte = BridgeSerial.read();
        if(byte < 0) break;

        if(byte == 0x00) {
            if(s_rx_len > 0) {
                BridgeFrame frame;
                if(bridge_frame_parse(
                       s_rx_encoded, s_rx_len, s_decoded, sizeof(s_decoded), &frame)) {
                    dispatch(frame);
                }
            }
            s_rx_len = 0;
        } else if(s_rx_len < sizeof(s_rx_encoded)) {
            s_rx_encoded[s_rx_len++] = (uint8_t)byte;
        } else {
            s_rx_len = 0; // overflow: resync on the next delimiter
        }
    }
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------

void setup() {
    s_boot_ms = millis();

    BridgeSerial.begin(kUartBaud, SERIAL_8N1, kUartRxPin, kUartTxPin);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    JsonDocument hello;
    hello["proto"] = BRIDGE_VERSION;
    hello["fw"] = "esp32s3-bridge/0.1";
    JsonArray caps = hello["caps"].to<JsonArray>();
    caps.add("ai");
    caps.add("wifi_scan");
    send_json(BridgeTypeHello, hello);
}

void loop() {
    poll_uart();

    uint32_t now = millis();
    static uint32_t last_heartbeat = 0;
    static uint32_t last_telemetry = 0;
    static uint32_t last_wifi_check = 0;

    if(now - last_heartbeat >= kHeartbeatMs) {
        last_heartbeat = now;
        JsonDocument doc;
        doc["uptime"] = (now - s_boot_ms) / 1000;
        doc["state"] = (WiFi.status() == WL_CONNECTED) ? "ok" : "wifi_down";
        send_json(BridgeTypeHeartbeat, doc);
    }

    if(now - last_telemetry >= kTelemetryMs) {
        last_telemetry = now;
        send_wifi_telemetry();
    }

    // Reconnect Wi-Fi without blocking the UART pump.
    if(now - last_wifi_check >= 10000) {
        last_wifi_check = now;
        if(WiFi.status() != WL_CONNECTED) WiFi.reconnect();
    }

    delay(2);
}
