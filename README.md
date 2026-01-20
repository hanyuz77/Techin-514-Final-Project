# Techin-514-Final-Project

# Plant Watering Detection
This is a two-device system that helps plant owners understand soil moisture.  
A soil sensor sends moisture estimates over BLE to a stepper-driven gauge that gently indicates **Dry → Okay → Overwatered**, with a soft LED and a calibration button.


## 1) Overall Concept (Title + 1–3 sentences + general sketch)
**What it does:** A plant-pot sensor measures soil moisture and sends a smoothed moisture score wirelessly to a physical gauge display.  
**Why:** The gauge is glanceable and calm—no numbers by default, no push notifications—just ambient awareness.

**Physical features highlighted:**
- Pot sensor: capacitive probe + small waterproof-ish top enclosure + battery
- Gauge: stepper needle + one LED + one button + battery + custom enclosure



## 2) Sensor Device (detailed sketch + how it works + part numbers)
**Device:** Soil Sensor Node (in/near the plant pot)


**How it works (summary):**
- A capacitive soil moisture probe outputs an analog voltage correlated with moisture.
- The ESP32-C3 samples the analog value, applies smoothing (moving average / EMA), converts it into a normalized `moisture_score (0–100)`, and transmits it over BLE at a low duty cycle to save power.

**Parts (with part numbers):**
- Soil moisture sensor: **Capacitive Soil Moisture Sensor v1.2** (analog output)
- MCU + BLE: Seeed Studio **XIAO ESP32C3** (ESP32-C3)
- Battery: 1S LiPo (final capacity TBD, target 500–1000 mAh)

---

## 3) Display Device (detailed sketch + how it works + part numbers)
**Device:** Gauge Display Node (physical needle + LED + button)


**How it works (summary):**
- The display receives `moisture_score` via BLE and maps it to a needle angle (e.g., 0–180°) with hysteresis to prevent jitter.
- One button triggers calibration (set “Okay” baseline) and/or logs watering; one LED provides a soft cue (e.g., dry warning or “happy/okay” glow).

**Parts (with part numbers):**
- MCU + BLE: Seeed Studio **XIAO ESP32C3** (ESP32-C3)
- Stepper motor: **28BYJ-48 5V**
- Stepper driver: **ULN2003A**
- LED: **WS2812B** (NeoPixel) ×1
- Button: tact switch ×1
- Battery: 1S LiPo (target 1200–2000 mAh)

**Custom PCB plan (meets course requirement):**
- A custom PCB will be designed for the display node integrating MCU headers, ULN2003A stepper driver, WS2812B LED, button, and power connectors.


## 4) Communication + System Diagram (2 figures total)
### Figure A — Block diagram (device-to-device communication)

### Figure B — Algorithm flow (DSP + mapping to needle)

**DSP/ML plan (where computation happens):**
- On the sensor node: filtering (EMA/moving average) + normalization to `moisture_score`.
- On the display node: hysteresis + needle smoothing (rate limiting) to create calm motion.


## Datasheets
All datasheets are stored in `datasheets/` and committed to the repo.
