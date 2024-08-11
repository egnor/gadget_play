#include <Arduino.h>
#include <WiFi.h>

constexpr char const* WIFI_SSID = "oneruling classic";
constexpr char const* WIFI_PASSWORD = "oneruling";

extern "C" int ets_printf(const char *fmt, ...);

static void on_event(arduino_event_id_t id) {
  for (int i = 0; i < 80; ++i) ets_printf(".");  // Serial.print(".");
  ets_printf("\n");
  // Serial.println();
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  for (int n = 5; n > 0; --n) {
    Serial.print("SETUP COUNTDOWN: ");
    Serial.println(n);
    delay(1000);
  }

  WiFi.onEvent(on_event);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Waiting for wifi...");
  }
}

void loop() {
  Serial.println("LOOP");
  delay(500);
}
