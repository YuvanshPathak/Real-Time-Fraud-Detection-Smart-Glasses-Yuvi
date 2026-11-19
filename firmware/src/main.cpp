// Real-Time Fraud Detection Smart Glasses.
//
// Default build (env seeed_xiao_esp32s3) — voice only:
//   connect Wi-Fi -> init PDM mic -> loop { record 5s -> POST /voice-check
//   -> POST /risk-score with voice_liveness_score only -> LED verdict }.
//
// glasses_camera build (-DENABLE_CAMERA) adds, each cycle after the voice
// check: capture one JPEG -> POST /face-check (video-call scenario), or
// /document-check while the BOOT button is held (in-person ID scenario) ->
// that score joins voice in the same /risk-score call.
//
// The phone Gateway relay is out of scope — see CLAUDE.md "Current build status".

#include <Arduino.h>
#include <WiFi.h>

#include "api.h"
#include "config.h"
#include "microphone.h"
#include "wifi_manager.h"

#ifdef ENABLE_CAMERA
#include "camera.h"

// XIAO's BOOT button (GPIO 0, active low) is free once booted: hold it while
// the photo is taken to check a document instead of a face.
#define MODE_BUTTON_PIN 0

static bool camReady = false;

// Photographs whatever the wearer faces and fills in the matching score.
static void cameraCheck(RiskInputs &inputs) {
  if (!camReady) {
    camReady = initCamera();
    if (!camReady) return;
  }
  const bool docMode = digitalRead(MODE_BUTTON_PIN) == LOW;
  const uint8_t *jpg = nullptr;
  size_t len = 0;
  if (!captureJpeg(&jpg, &len)) return;
  Serial.printf("[loop] captured %u-byte JPEG (%s mode)\n", (unsigned)len, docMode ? "document" : "face");

  if (docMode) {
    ImageCheckResult r = postImageCheck("/document-check", "document", "doc_authenticity_score", jpg, len);
    if (r.ok) {
      inputs.docAuthenticityScore = r.score;
      Serial.printf("[loop] doc_authenticity_score=%.2f\n", r.score);
    }
  } else {
    ImageCheckResult r = postImageCheck("/face-check", "image", "face_liveness_score", jpg, len);
    if (r.ok) {
      inputs.faceLivenessScore = r.score;
      Serial.printf("[loop] face_liveness_score=%.2f\n", r.score);
    }
  }
  releaseFrame();  // the frame never outlives this cycle
}
#endif

static void blinkLed(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(ALERT_LED_PIN, HIGH);
    delay(onMs);
    digitalWrite(ALERT_LED_PIN, LOW);
    if (i < times - 1) delay(offMs);
  }
}

// Single onboard LED, so severity is encoded as blink count/speed rather
// than color: SAFE = one slow blink, SUSPICIOUS = three medium blinks,
// FRAUD = five fast blinks.
static void showVerdict(const char *riskLevel) {
  if (strcmp(riskLevel, "SAFE") == 0) {
    blinkLed(1, 600, 0);
  } else if (strcmp(riskLevel, "SUSPICIOUS") == 0) {
    blinkLed(3, 250, 200);
  } else if (strcmp(riskLevel, "FRAUD") == 0) {
    blinkLed(5, 100, 100);
  } else {
    // Unrecognized/empty risk level — distinct pattern so it's visibly
    // different from a real verdict, not silently treated as SAFE.
    blinkLed(2, 800, 400);
  }
}

static bool micReady = false;

void setup() {
  Serial.begin(115200);
  delay(1500); // let native USB CDC settle before the first prints
  Serial.println("\n[boot] Real-Time Fraud Detection Smart Glasses — voice module");

  pinMode(ALERT_LED_PIN, OUTPUT);
  digitalWrite(ALERT_LED_PIN, LOW);

  Serial.printf("[boot] PSRAM found: %s, size: %u bytes\n", psramFound() ? "yes" : "no", ESP.getPsramSize());

  // TEMPORARY DIAGNOSTIC — remove once Wi-Fi connects reliably. Reports the
  // actual encryption type of nearby networks so we can tell a wrong
  // password apart from an auth type WiFi.begin(ssid, password) can't do
  // at all (e.g. WPA2-Enterprise, which needs a username too).
  Serial.println("[boot] scanning for WIFI_SSID...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == String(WIFI_SSID)) {
      wifi_auth_mode_t enc = WiFi.encryptionType(i);
      const char *encName = "unknown";
      switch (enc) {
        case WIFI_AUTH_OPEN: encName = "OPEN"; break;
        case WIFI_AUTH_WEP: encName = "WEP"; break;
        case WIFI_AUTH_WPA_PSK: encName = "WPA_PSK"; break;
        case WIFI_AUTH_WPA2_PSK: encName = "WPA2_PSK"; break;
        case WIFI_AUTH_WPA_WPA2_PSK: encName = "WPA_WPA2_PSK"; break;
        case WIFI_AUTH_WPA2_ENTERPRISE: encName = "WPA2_ENTERPRISE (needs username, not just password!)"; break;
        case WIFI_AUTH_WPA3_PSK: encName = "WPA3_PSK"; break;
        default: break;
      }
      Serial.printf("[boot] found %s: RSSI=%d encryption=%s\n", WIFI_SSID, WiFi.RSSI(i), encName);
    }
  }
  if (n == 0) Serial.println("[boot] scan found no networks at all");

  if (!connectWifi()) {
    Serial.println("[boot] Wi-Fi connect failed — will retry each cycle");
  }

  micReady = initMicrophone();
  if (!micReady) {
    Serial.println("[boot] Microphone init failed — voice checks will be skipped");
  }

#ifdef ENABLE_CAMERA
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  camReady = initCamera();
  if (!camReady) {
    Serial.println("[boot] Camera init failed — will retry each cycle");
  }
#endif
}

void loop() {
  if (!isWifiConnected()) {
    Serial.println("[loop] Wi-Fi not connected, retrying...");
    connectWifi();
    delay(LOOP_INTERVAL_MS);
    return;
  }

  if (!micReady) {
    Serial.println("[loop] Microphone not ready, retrying init...");
    micReady = initMicrophone();
    delay(LOOP_INTERVAL_MS);
    return;
  }

  Serial.println("[loop] recording...");
  size_t sampleCount = 0;
  int16_t *samples = recordToBuffer(&sampleCount);
  if (!samples || sampleCount == 0) {
    Serial.println("[loop] recording failed, skipping cycle");
    if (samples) free(samples);
    delay(LOOP_INTERVAL_MS);
    return;
  }
  Serial.printf("[loop] captured %u samples\n", (unsigned)sampleCount);

  VoiceCheckResult voiceRes = postVoiceCheck(samples, sampleCount, SAMPLE_RATE);
  free(samples);

  if (!voiceRes.ok) {
    Serial.println("[loop] /voice-check failed, skipping this cycle's verdict");
    delay(LOOP_INTERVAL_MS);
    return;
  }
  Serial.printf("[loop] voice_liveness_score=%.2f spoof_detected=%s\n",
                voiceRes.voiceLivenessScore, voiceRes.spoofDetected ? "true" : "false");

  RiskInputs inputs;
  inputs.voiceLivenessScore = voiceRes.voiceLivenessScore;
#ifdef ENABLE_CAMERA
  cameraCheck(inputs);
#endif

  RiskScoreResult riskRes = postRiskScore(inputs);
  if (!riskRes.ok) {
    Serial.println("[loop] /risk-score failed, skipping verdict display");
    delay(LOOP_INTERVAL_MS);
    return;
  }
  Serial.printf("[loop] fraud_risk_score=%.2f risk_level=%s\n", riskRes.fraudRiskScore, riskRes.riskLevel);

  showVerdict(riskRes.riskLevel);

  delay(LOOP_INTERVAL_MS);
}
