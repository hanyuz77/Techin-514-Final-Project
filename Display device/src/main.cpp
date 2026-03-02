#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Adafruit_NeoPixel.h>
#include <AccelStepper.h>
#include <Preferences.h>

// --------------------
// Pin compatibility helpers
// --------------------
#ifndef D0
  #define D0 0
#endif
#ifndef D1
  #define D1 1
#endif
#ifndef D2
  #define D2 2
#endif
#ifndef D3
  #define D3 3
#endif
#ifndef D10
  #define D10 10
#endif
// Button pin is U1 pad10 on your PCB; footprint labels it "10" but it's physically where D9 usually is.
// Try D9 first if your variant defines it; otherwise fall back to 9 or 10-position pin.
// If your build fails because D9 doesn't exist, just change PIN_BTN to 9 (or the GPIO you find).
#ifndef D9
  #define D9 9
#endif

// =====================
// ✅ YOUR PCB PIN MAP
// =====================

// Stepper driver inputs (U2 I1..I4) -> XIAO D0..D3
static const int PIN_STP_IN1 = D0;  // U2 I1
static const int PIN_STP_IN2 = D1;  // U2 I2
static const int PIN_STP_IN3 = D2;  // U2 I3
static const int PIN_STP_IN4 = D3;  // U2 I4

// NeoPixel DIN goes through R3 to XIAO D10
static const int PIN_NEOPIXEL = D10;

// Button SW2 -> Net-(R4-Pad2) -> U1 pad10 (likely D9 position)
static const int PIN_BTN = D9;
static const bool BTN_ACTIVE_LOW = true; // SW2 to GND

// =====================
// BLE UUID (same as earlier)
// =====================
static const NimBLEUUID SERVICE_UUID("6e3a9c1a-4b7b-4e2e-9a1b-9d0f7a0d5d01");

// =====================
// Gauge config
// =====================
static const float ANGLE_MIN_DEG = 0.0f;
static const float ANGLE_MAX_DEG = 180.0f;

// 28BYJ-48 typical
static const int   STEPS_PER_REV = 2048;
static const float DEG_PER_STEP  = 360.0f / (float)STEPS_PER_REV;

// Hysteresis
static const int MOISTURE_HYST = 2;

// Motion tuning
static const float MAX_SPEED = 600.0f;
static const float ACCEL     = 400.0f;

// NeoPixel
static const int N_PIXELS = 1;

// =====================
// Globals
// =====================
Adafruit_NeoPixel pixels(N_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
AccelStepper stepper(AccelStepper::HALF4WIRE, PIN_STP_IN1, PIN_STP_IN3, PIN_STP_IN2, PIN_STP_IN4);
Preferences prefs;

volatile uint8_t  g_moisture = 0;
volatile uint16_t g_battMv   = 0;
volatile bool     g_hasReading = false;

int  g_lastMoistureShown = -999;
long g_zeroOffsetSteps   = 0;

// =====================
// Helpers
// =====================
static long angleDegToSteps(float deg) {
  deg = constrain(deg, ANGLE_MIN_DEG, ANGLE_MAX_DEG);
  return lroundf(deg / DEG_PER_STEP);
}

static float moistureToAngle(uint8_t m) {
  float t = (float)m / 100.0f;
  return ANGLE_MIN_DEG + t * (ANGLE_MAX_DEG - ANGLE_MIN_DEG);
}

static void setLedForMoisture(uint8_t m) {
  uint32_t color;
  if (m < 30)      color = pixels.Color(255, 0, 0);
  else if (m < 60) color = pixels.Color(255, 180, 0);
  else             color = pixels.Color(0, 255, 0);
  pixels.setPixelColor(0, color);
  pixels.show();
}

static bool buttonPressed() {
  int v = digitalRead(PIN_BTN);
  return BTN_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

static void flashBlue(uint8_t times) {
  for (uint8_t i=0;i<times;i++) {
    pixels.setPixelColor(0, pixels.Color(0, 80, 255));
    pixels.show();
    delay(120);
    pixels.setPixelColor(0, 0);
    pixels.show();
    delay(120);
  }
}

static void handleButton() {
  static bool wasPressed = false;
  static uint32_t tDown = 0;

  bool pressed = buttonPressed();
  uint32_t now = millis();

  if (pressed && !wasPressed) {
    tDown = now;
    wasPressed = true;
  } else if (!pressed && wasPressed) {
    uint32_t held = now - tDown;
    wasPressed = false;

    if (held > 800) {
      // Long press: calibrate zero
      g_zeroOffsetSteps = stepper.currentPosition();
      prefs.putLong("zero", g_zeroOffsetSteps);
      Serial.printf("[BTN] Calibrated zero at steps=%ld\n", g_zeroOffsetSteps);
      flashBlue(3);
    } else {
      // Short press: cue
      Serial.println("[BTN] Log watering (cue)");
      flashBlue(1);
    }
  }
}

// =====================
// BLE scan callback
// =====================
class AdvCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* dev) override {
    if (!dev->isAdvertisingService(SERVICE_UUID)) return;

    std::string mfg = dev->getManufacturerData();
    if (mfg.size() != 5) return;

    const uint8_t* b = (const uint8_t*)mfg.data();
    if (b[0] != 1) return;

    g_moisture = b[1];
    g_battMv = (uint16_t)b[2] | ((uint16_t)b[3] << 8);
    g_hasReading = true;

    Serial.printf("[BLE] moisture=%u battMv=%u rssi=%d\n", g_moisture, g_battMv, dev->getRSSI());
  }
};

static void startScan() {
  NimBLEDevice::init("PlantGauge");
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new AdvCallbacks(), true);
  scan->setActiveScan(true);
  scan->setInterval(45);
  scan->setWindow(15);
  scan->start(0, nullptr, false); // continuous
  Serial.println("[BLE] scanning...");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_BTN, BTN_ACTIVE_LOW ? INPUT_PULLUP : INPUT);

  pixels.begin();
  pixels.setBrightness(40);
  pixels.show();

  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCEL);

  prefs.begin("gauge", false);
  g_zeroOffsetSteps = prefs.getLong("zero", 0);
  stepper.setCurrentPosition(g_zeroOffsetSteps);
  Serial.printf("[CAL] zero steps=%ld\n", g_zeroOffsetSteps);

  startScan();
}

void loop() {
  handleButton();
  stepper.run();

  if (g_hasReading) {
    g_hasReading = false;

    int m = (int)g_moisture;
    if (abs(m - g_lastMoistureShown) >= MOISTURE_HYST) {
      g_lastMoistureShown = m;

      setLedForMoisture((uint8_t)m);

      float angle = moistureToAngle((uint8_t)m);
      long targetSteps = g_zeroOffsetSteps + angleDegToSteps(angle);

      Serial.printf("[GAUGE] m=%d angle=%.1f targetSteps=%ld\n", m, angle, targetSteps);
      stepper.moveTo(targetSteps);
    }
  }

  delay(5);
}