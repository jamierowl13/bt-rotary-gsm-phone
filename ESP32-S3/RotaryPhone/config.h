// =====================================================================
//  RotaryPhone - user configuration
//  ESP32-S3 + SIM900 GSM + BT rotary telephone (e.g. GPO/BT 746)
// =====================================================================
#pragma once

// ---------------- WiFi ----------------
#define WIFI_SSID        "YourWiFiName"
#define WIFI_PASS        "YourWiFiPassword"
#define HOSTNAME         "rotaryphone"      // -> http://rotaryphone.local
// If the WiFi above can't be joined, the ESP32 starts its own access point:
#define AP_SSID          "RotaryPhone"
#define AP_PASS          "ringring123"      // min 8 chars

// Web page login (protects against strangers making calls on your SIM).
// Leave WEB_USER empty ("") to disable the login.
#define WEB_USER         "admin"
#define WEB_PASS         "rotary"

// ---------------- SIM900 ----------------
#define PIN_SIM_RX       18   // ESP32 RX  <- SIM900 TXD
#define PIN_SIM_TX       17   // ESP32 TX  -> SIM900 RXD
#define PIN_SIM_PWRKEY   16   // -> SIM900 PWRKEY (D9 on most shields). -1 = not wired
#define SIM_BAUD         9600
#define SIM_PIN_CODE     ""   // SIM card PIN, "" if the SIM has no PIN lock

#define SPEAKER_VOLUME   90   // AT+CLVL 0..100 (handset earpiece volume)
#define MIC_GAIN         10   // AT+CMIC 0..15  (handset mic gain)
#define USE_SIM_TONES    1    // 1 = play dial/busy tone in the earpiece using AT+STTONE

// ---------------- Handset / hook switch ----------------
// Hook switch wired between this pin and GND (internal pull-up used).
#define PIN_HOOK         4
#define HOOK_OFF_LEVEL   HIGH // pin level when the handset is LIFTED. Swap to LOW if reversed.
#define HOOK_DEBOUNCE_MS 60

// ---------------- Rotary dial ----------------
// Pulse contacts (normally closed, open once per pulse) between pin and GND.
#define PIN_DIAL_PULSE        5
#define DIAL_PULSE_BREAK_LEVEL HIGH  // level while the pulse contact is OPEN
// Off-normal contacts (change state while the dial is off its rest). -1 = not wired.
#define PIN_DIAL_OFFNORMAL    6
#define DIAL_OFFNORMAL_ACTIVE LOW    // level while the dial is wound / returning
#define DIAL_DEBOUNCE_MS      5
#define DIAL_DIGIT_GAP_MS     250    // pause after the last pulse that ends a digit

// UK-style automatic dialling
#define DIAL_TIMEOUT_MS       4000   // dial this long after the last digit (min 3 digits)
#define DIALTONE_TIMEOUT_MS   30000  // off-hook with no dialling -> number-unobtainable tone

// ---------------- Bell driver ----------------
// BELL_STEPDIR : stepper driver with STEP/DIR inputs (A4988, DRV8825, TMC2208 standalone).
//                Bell coils go across ONE motor winding (1A/1B). Leave MS pins low (full step).
// BELL_HBRIDGE : dual H-bridge "stepper" board (L298N, TB6612, DRV8871 ...).
//                Bell coils go across OUT1/OUT2.
#define BELL_STEPDIR  1
#define BELL_HBRIDGE  2
#define BELL_DRIVER   BELL_STEPDIR

// STEPDIR : PIN_BELL_A = STEP, PIN_BELL_B = DIR, PIN_BELL_EN = /ENABLE (active LOW)
// HBRIDGE : PIN_BELL_A = IN1,  PIN_BELL_B = IN2, PIN_BELL_EN = ENA (active HIGH)
#define PIN_BELL_A    7
#define PIN_BELL_B    15
#define PIN_BELL_EN   8      // -1 if hard-wired enabled

// UK (BT) ringing: 25 Hz, cadence 0.4 s on, 0.2 s off, 0.4 s on, 2.0 s off
#define BELL_FREQ_HZ  25
static const uint16_t UK_RING_CADENCE[] = { 400, 200, 400, 2000 };  // on, off, on, off (ms)
