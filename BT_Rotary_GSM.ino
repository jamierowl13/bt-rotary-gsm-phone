/*
 * ============================================================================
 *  BT Rotary GSM Phone
 * ============================================================================
 *  Turns an old BT / GPO rotary dial telephone into a working mobile phone.
 *
 *  Hardware : Arduino Uno
 *             SIM900 GSM module (wired with jumper leads, NOT stacked - see README)
 *             L293D motor / stepper shield (Adafruit v1 style, uses AFMotor)
 *  Power    : 12 V rechargeable battery
 *
 *  What it does
 *    - Lift handset ................ BT dial tone (350 Hz + 440 Hz)
 *    - Turn the dial ............... dial tone stops, pulses are counted into digits
 *    - 4 s after the last digit .... the call is placed (999 / 112 go immediately)
 *    - Dial during a call .......... digits are sent as DTMF (for phone menus)
 *    - Replace handset ............. call ends / is rejected
 *    - Incoming call ............... bell rung with UK cadence
 *                                    (0.4 s on, 0.2 s off, 0.4 s on, 2.0 s off) at 25 Hz
 *                                    half-cycle on channel A (M1), half on channel B (M2)
 *    - Lift handset while ringing .. call answered
 *    - Far end hangs up / busy ..... BT busy tone; failed call -> number unobtainable
 *    - No network on pick-up ....... number unobtainable tone instead of dial tone
 *
 *  Libraries
 *    - "Adafruit Motor Shield library" (AFMotor, v1)  -> Arduino Library Manager
 *    - SoftwareSerial (built in)
 *
 *  Pins used by the L293D shield (do NOT reuse): D3, D4, D5, D6, D7, D8, D11, D12
 * ============================================================================
 */

#include <SoftwareSerial.h>
#include <AFMotor.h>

// ---------------------------------------------------------------------------
//  Pin assignments
// ---------------------------------------------------------------------------
const uint8_t PIN_SIM_RX  = A0;  // Arduino RX  <-  SIM900 TX
const uint8_t PIN_SIM_TX  = A1;  // Arduino TX  ->  SIM900 RX
const uint8_t PIN_HOOK    = A2;  // Hook switch contact -> GND
const uint8_t PIN_DIAL    = A3;  // Rotary dial pulse contacts -> GND
const uint8_t PIN_SIM_PWR = 9;   // SIM900 PWRKEY (shield "Servo 2" header signal pin)
const uint8_t PIN_TONE    = 10;  // Tone output, Timer1 OC1B (shield "Servo 1" header)
                                 // Fixed by hardware - do not change.

// ---------------------------------------------------------------------------
//  User configuration
// ---------------------------------------------------------------------------
// Hook switch: level read when the handset is LIFTED.
// Switch wired pin->GND with internal pull-up, contacts close on lift => LOW.
#define HOOK_OFF_LEVEL      LOW

// Dial pulse contacts are normally closed and open (break) for each pulse.
// Wired pin->GND with pull-up, a "break" therefore reads HIGH.
#define DIAL_BREAK_LEVEL    HIGH

// Bell drive mode
//   1 = two-coil mode: coil A on M1, coil B on M2, each driven for half of
//       every 25 Hz cycle (as requested).
//   0 = bridge mode: both bell coils in series on M1, polarity reversed
//       every half cycle (true AC from one channel).
#define RING_MODE_TWO_COIL  1

// Set to 1 if the bell only "clunks" in two-coil mode - reverses coil B drive.
#define RING_COIL_B_REVERSE 0

// Timings (ms)
const uint16_t RING_HALF_PERIOD_MS  = 20;     // 25 Hz ringing current
const uint16_t RING_CADENCE_MS[4]   = {400, 200, 400, 2000}; // UK ring cadence
const uint32_t RING_LOST_MS         = 8000;   // no "RING" for this long = caller gave up
const uint16_t HOOK_DEBOUNCE_MS     = 60;
const uint16_t DIAL_DEBOUNCE_MS     = 5;
const uint16_t DIGIT_GAP_MS         = 250;    // gap after last pulse = digit complete
const uint32_t NUMBER_COMPLETE_MS   = 4000;   // gap after last digit = dial the number
const uint32_t DIALTONE_TIMEOUT_MS  = 30000;  // no dialling -> number unobtainable
const uint32_t CREG_POLL_MS         = 20000;  // network registration check interval

// SIM900 audio settings
#define SIM_SPEAKER_LEVEL   "90"     // AT+CLVL 0-100
#define SIM_MIC_GAIN        "10"     // AT+CMIC 0-15
#define SIM_AUDIO_CHANNEL   "0"      // AT+CHFA 0 = main (MIC1/SPK1), 1 = aux (MIC2/SPK2)

const uint8_t MAX_DIGITS = 20;

// ---------------------------------------------------------------------------
//  Objects
// ---------------------------------------------------------------------------
SoftwareSerial sim(PIN_SIM_RX, PIN_SIM_TX);
AF_DCMotor bellA(1, MOTOR12_64KHZ);   // channel A  (M1 terminals)
AF_DCMotor bellB(2, MOTOR12_64KHZ);   // channel B  (M2 terminals)

// ---------------------------------------------------------------------------
//  Call state machine
// ---------------------------------------------------------------------------
enum State : uint8_t {
  ST_IDLE,       // on-hook, waiting
  ST_RINGING,    // incoming call, bell ringing
  ST_DIALTONE,   // off-hook, dial tone playing
  ST_DIALLING,   // off-hook, collecting digits
  ST_IN_CALL,    // outgoing call placed or incoming call answered
  ST_CLEARED     // off-hook after call ended / failed, tone playing
};
const char* const STATE_NAMES[] = {
  "IDLE", "RINGING", "DIALTONE", "DIALLING", "IN_CALL", "CLEARED"
};

State    state        = ST_IDLE;
uint32_t stateSinceMs = 0;

void setState(State s) {
  state = s;
  stateSinceMs = millis();
  Serial.print(F("[STATE] "));
  Serial.println(STATE_NAMES[s]);
}

// ---------------------------------------------------------------------------
//  Debounced inputs
// ---------------------------------------------------------------------------
struct Input {
  uint8_t  pin;
  uint16_t debounceMs;
  uint8_t  stable;
  uint8_t  lastRaw;
  uint32_t lastChange;
};

Input hookIn = {PIN_HOOK, HOOK_DEBOUNCE_MS, HIGH, HIGH, 0};
Input dialIn = {PIN_DIAL, DIAL_DEBOUNCE_MS, LOW,  LOW,  0};

void inputBegin(Input &in) {
  pinMode(in.pin, INPUT_PULLUP);
  in.stable = in.lastRaw = digitalRead(in.pin);
  in.lastChange = millis();
}

// Returns true when the debounced state changes.
bool inputUpdate(Input &in, uint32_t now) {
  uint8_t raw = digitalRead(in.pin);
  if (raw != in.lastRaw) {
    in.lastRaw = raw;
    in.lastChange = now;
  }
  if (raw != in.stable && (now - in.lastChange) >= in.debounceMs) {
    in.stable = raw;
    return true;
  }
  return false;
}

bool isOffHook() { return hookIn.stable == HOOK_OFF_LEVEL; }

// ===========================================================================
//  TONE GENERATOR  (BT dial / busy / number unobtainable)
//  Timer1 fast PWM, 10-bit, 15.625 kHz on pin 10, two-oscillator DDS.
//  Filter the output with a 2-stage RC low-pass (see README).
// ===========================================================================
#define SAMPLE_RATE 15625UL
#define TONE_MID    512

enum ToneType : uint8_t { TONE_NONE, TONE_DIAL, TONE_BUSY, TONE_NU };

struct ToneDef { uint16_t f1, f2, onMs, offMs; };   // offMs 0 = continuous
const ToneDef TONES[] = {
  {  0,   0,   0,   0},   // TONE_NONE
  {350, 440,   0,   0},   // TONE_DIAL : BT dial tone, continuous
  {400,   0, 375, 375},   // TONE_BUSY : BT busy / engaged tone
  {400,   0,   0,   0},   // TONE_NU   : BT number unobtainable, continuous
};

int8_t            sineTable[256];
volatile uint16_t phase1 = 0, phase2 = 0;
volatile uint16_t toneInc1 = 0, toneInc2 = 0;
volatile bool     toneGate = false;
ToneType          currentTone = TONE_NONE;
uint32_t          toneStartMs = 0;

uint16_t phaseInc(uint16_t freq) {
  return (uint16_t)(((uint32_t)freq * 65536UL) / SAMPLE_RATE);
}

void toneBegin() {
  for (int i = 0; i < 256; i++) {
    sineTable[i] = (int8_t)round(127.0 * sin(2.0 * PI * i / 256.0));
  }
  pinMode(PIN_TONE, OUTPUT);
  // Mode 14 fast PWM, TOP = ICR1, non-inverting on OC1B, no prescaler
  TCCR1A = _BV(COM1B1) | _BV(WGM11);
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
  ICR1   = 1023;
  OCR1B  = TONE_MID;
  TIMSK1 = 0;                       // sample interrupt off until a tone plays
}

ISR(TIMER1_OVF_vect) {
  if (toneGate) {
    phase1 += toneInc1;
    int16_t s = sineTable[phase1 >> 8];
    if (toneInc2) {
      phase2 += toneInc2;
      s += sineTable[phase2 >> 8];
    } else {
      s += s;                       // single tone at same peak level
    }
    OCR1B = TONE_MID + s;           // +/-254 around mid-scale
  } else {
    OCR1B = TONE_MID;
  }
}

void stopTone() {
  noInterrupts();
  TIMSK1 &= ~_BV(TOIE1);
  toneGate = false;
  OCR1B = TONE_MID;                 // hold at mid-scale: no click
  interrupts();
  currentTone = TONE_NONE;
}

void startTone(ToneType t) {
  if (t == TONE_NONE) { stopTone(); return; }
  uint16_t i1 = phaseInc(TONES[t].f1);
  uint16_t i2 = TONES[t].f2 ? phaseInc(TONES[t].f2) : 0;
  noInterrupts();
  toneInc1 = i1;
  toneInc2 = i2;
  phase1 = phase2 = 0;
  toneGate = true;
  TIMSK1 |= _BV(TOIE1);
  interrupts();
  currentTone = t;
  toneStartMs = millis();
}

void toneUpdate(uint32_t now) {
  if (currentTone == TONE_NONE) return;
  const ToneDef &t = TONES[currentTone];
  if (t.offMs == 0) { toneGate = true; return; }
  uint32_t period = (uint32_t)t.onMs + t.offMs;
  toneGate = ((now - toneStartMs) % period) < t.onMs;
}

// ===========================================================================
//  BELL RINGER  (L293D shield, M1 = channel A, M2 = channel B)
// ===========================================================================
bool     ringerOn        = false;
uint8_t  cadenceStep     = 0;      // even = ringing burst, odd = silence
uint32_t cadenceStepMs   = 0;
uint32_t halfCycleMs     = 0;
bool     halfPhase       = false;

void bellRelease() {
  bellA.run(RELEASE);
  bellB.run(RELEASE);
}

void bellDrive(bool phase) {
#if RING_MODE_TWO_COIL
  if (!phase) {                     // first half cycle -> channel A
    bellB.run(RELEASE);
    bellA.run(FORWARD);
  } else {                          // second half cycle -> channel B
    bellA.run(RELEASE);
    bellB.run(RING_COIL_B_REVERSE ? BACKWARD : FORWARD);
  }
#else
  bellA.run(phase ? BACKWARD : FORWARD);   // AC through series coils
#endif
}

void ringerStart() {
  uint32_t now = millis();
  ringerOn      = true;
  cadenceStep   = 0;
  cadenceStepMs = now;
  halfCycleMs   = now;
  halfPhase     = false;
  bellDrive(halfPhase);
  Serial.println(F("[BELL] ringing"));
}

void ringerStop() {
  ringerOn = false;
  bellRelease();
  Serial.println(F("[BELL] stopped"));
}

void ringerUpdate(uint32_t now) {
  if (!ringerOn) return;

  // Advance through the UK cadence: 400 on / 200 off / 400 on / 2000 off
  if (now - cadenceStepMs >= RING_CADENCE_MS[cadenceStep]) {
    cadenceStep   = (cadenceStep + 1) & 0x03;
    cadenceStepMs = now;
    if (cadenceStep & 1) {
      bellRelease();                // entering a silent period
    } else {
      halfPhase   = false;          // entering a ringing burst
      halfCycleMs = now;
      bellDrive(halfPhase);
    }
  }

  // 25 Hz alternation during a burst
  if (!(cadenceStep & 1) && (now - halfCycleMs >= RING_HALF_PERIOD_MS)) {
    halfCycleMs += RING_HALF_PERIOD_MS;
    halfPhase = !halfPhase;
    bellDrive(halfPhase);
  }
}

// ===========================================================================
//  SIM900 COMMUNICATION
// ===========================================================================
char     simLine[80];
uint8_t  simLen          = 0;
bool     networkOk       = false;
bool     awaitingDial    = false;
uint32_t lastRingMs      = 0;
uint32_t lastCregPollMs  = 0;

void handleSimLine(const char *line);

// Blocking command - used only during setup.
bool simCommand(const __FlashStringHelper *cmd, const char *expect, uint32_t timeoutMs) {
  while (sim.available()) sim.read();
  sim.println(cmd);
  Serial.print(F("[SIM >] "));
  Serial.println(cmd);

  char buf[80];
  uint8_t len = 0;
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (sim.available()) {
      char c = sim.read();
      if (c == '\n') {
        buf[len] = 0;
        if (len) {
          Serial.print(F("[SIM <] "));
          Serial.println(buf);
          if (strstr(buf, expect)) return true;
          if (strstr(buf, "ERROR"))  return false;
        }
        len = 0;
      } else if (c != '\r' && len < sizeof(buf) - 1) {
        buf[len++] = c;
      }
    }
  }
  return false;
}

void simPowerOn() {
  if (simCommand(F("AT"), "OK", 1000)) return;   // already on

  Serial.println(F("[SIM] pulsing PWRKEY"));
  digitalWrite(PIN_SIM_PWR, HIGH);
  delay(1200);
  digitalWrite(PIN_SIM_PWR, LOW);
  delay(3000);

  for (uint8_t i = 0; i < 10; i++) {
    if (simCommand(F("AT"), "OK", 1000)) return;
  }
  Serial.println(F("[SIM] NOT RESPONDING - check wiring, power and baud rate"));
}

void simConfigure() {
  simCommand(F("ATE0"),      "OK", 1000);   // echo off
  simCommand(F("AT+CLIP=1"), "OK", 1000);   // caller ID (+CLIP lines)
  simCommand(F("AT+CVHU=0"), "OK", 1000);   // ATH hangs up voice calls
  simCommand(F("AT+CRSL=0"), "OK", 1000);   // mute module's own ring sound
  simCommand(F("AT+CHFA=" SIM_AUDIO_CHANNEL), "OK", 1000);
  simCommand(F("AT+CLVL=" SIM_SPEAKER_LEVEL), "OK", 1000);
  simCommand(F("AT+CMIC=" SIM_AUDIO_CHANNEL "," SIM_MIC_GAIN), "OK", 1000);
  sim.println(F("AT+CREG?"));
  lastCregPollMs = millis();
}

void simPoll() {
  while (sim.available()) {
    char c = sim.read();
    if (c == '\n') {
      simLine[simLen] = 0;
      if (simLen) handleSimLine(simLine);
      simLen = 0;
    } else if (c != '\r' && simLen < sizeof(simLine) - 1) {
      simLine[simLen++] = c;
    }
  }
}

// ===========================================================================
//  DIALLING
// ===========================================================================
char     number[MAX_DIGITS + 1];
uint8_t  numLen      = 0;
uint8_t  pulseCount  = 0;
uint32_t lastPulseMs = 0;
uint32_t lastDigitMs = 0;

void clearNumber() {
  numLen = 0;
  number[0] = 0;
  pulseCount = 0;
}

void placeCall() {
  Serial.print(F("[CALL] dialling "));
  Serial.println(number);
  sim.print(F("ATD"));
  sim.print(number);
  sim.println(F(";"));
  awaitingDial = true;
  setState(ST_IN_CALL);
}

void onDigit(uint8_t d, uint32_t now) {
  Serial.print(F("[DIAL] digit "));
  Serial.println(d);

  if (state == ST_DIALLING) {
    if (numLen < MAX_DIGITS) {
      number[numLen++] = '0' + d;
      number[numLen] = 0;
    }
    lastDigitMs = now;
    if (strcmp(number, "999") == 0 || strcmp(number, "112") == 0) {
      placeCall();                  // emergency numbers go straight away
    }
  } else if (state == ST_IN_CALL) {
    sim.print(F("AT+VTS="));        // DTMF during a call
    sim.println(d);
  }
}

void dialUpdate(uint32_t now) {
  bool changed = inputUpdate(dialIn, now);

  // Only count pulses while off-hook in a dialling-capable state
  bool canDial = (state == ST_DIALTONE || state == ST_DIALLING || state == ST_IN_CALL);
  if (!canDial) { pulseCount = 0; return; }

  if (changed && dialIn.stable == DIAL_BREAK_LEVEL) {
    pulseCount++;
    lastPulseMs = now;
    if (state == ST_DIALTONE) {     // first pulse kills dial tone
      stopTone();
      clearNumber();
      pulseCount = 1;
      lastDigitMs = now;
      setState(ST_DIALLING);
    }
  }

  if (pulseCount && dialIn.stable != DIAL_BREAK_LEVEL &&
      (now - lastPulseMs) > DIGIT_GAP_MS) {
    if (pulseCount <= 10) onDigit(pulseCount == 10 ? 0 : pulseCount, now);
    pulseCount = 0;
  }
}

// ===========================================================================
//  EVENTS
// ===========================================================================
void onCallEnded(ToneType tone) {
  awaitingDial = false;
  if (state == ST_RINGING) {
    ringerStop();
    setState(ST_IDLE);
  } else if (state == ST_IN_CALL) {
    Serial.println(F("[CALL] ended"));
    startTone(tone);
    setState(ST_CLEARED);
  }
}

void handleSimLine(const char *line) {
  Serial.print(F("[SIM <] "));
  Serial.println(line);

  if (strcmp(line, "RING") == 0) {
    lastRingMs = millis();
    if (state == ST_IDLE) {
      ringerStart();
      setState(ST_RINGING);
    } else if (state == ST_DIALTONE || state == ST_DIALLING || state == ST_CLEARED) {
      sim.println(F("ATH"));        // we're off-hook: caller gets busy
    }
  }
  else if (strncmp(line, "+CLIP:", 6) == 0) {
    Serial.print(F("[CALL] caller ID: "));
    Serial.println(line + 6);
  }
  else if (strcmp(line, "NO CARRIER") == 0) onCallEnded(TONE_BUSY);
  else if (strcmp(line, "BUSY") == 0)       onCallEnded(TONE_BUSY);
  else if (strcmp(line, "NO ANSWER") == 0)  onCallEnded(TONE_BUSY);
  else if (strcmp(line, "NO DIALTONE") == 0) onCallEnded(TONE_NU);
  else if (strcmp(line, "OK") == 0) {
    awaitingDial = false;
  }
  else if (strstr(line, "ERROR")) {
    if (awaitingDial) onCallEnded(TONE_NU);
  }
  else if (strncmp(line, "+CREG:", 6) == 0) {
    const char *comma = strchr(line, ',');
    if (comma) {
      int stat = atoi(comma + 1);
      bool ok = (stat == 1 || stat == 5);   // 1 = home, 5 = roaming
      if (ok != networkOk) {
        networkOk = ok;
        Serial.println(ok ? F("[NET] registered") : F("[NET] no network"));
      }
    }
  }
}

void hookUpdate(uint32_t now) {
  if (!inputUpdate(hookIn, now)) return;

  if (isOffHook()) {
    Serial.println(F("[HOOK] off-hook"));
    switch (state) {
      case ST_IDLE:
        clearNumber();
        if (networkOk) {
          startTone(TONE_DIAL);
          setState(ST_DIALTONE);
        } else {
          startTone(TONE_NU);
          setState(ST_CLEARED);
        }
        break;
      case ST_RINGING:
        ringerStop();
        sim.println(F("ATA"));      // answer
        setState(ST_IN_CALL);
        break;
      default:
        break;
    }
  } else {
    Serial.println(F("[HOOK] on-hook"));
    switch (state) {
      case ST_IN_CALL:
        sim.println(F("ATH"));      // hang up
        // fall through
      case ST_DIALTONE:
      case ST_DIALLING:
      case ST_CLEARED:
        stopTone();
        clearNumber();
        awaitingDial = false;
        setState(ST_IDLE);
        break;
      default:
        break;
    }
  }
}

void timeoutsUpdate(uint32_t now) {
  switch (state) {
    case ST_IDLE:
      if (now - lastCregPollMs > CREG_POLL_MS) {
        lastCregPollMs = now;
        sim.println(F("AT+CREG?"));
      }
      break;

    case ST_RINGING:
      if (now - lastRingMs > RING_LOST_MS) {   // caller gave up
        ringerStop();
        setState(ST_IDLE);
      }
      break;

    case ST_DIALTONE:
      if (now - stateSinceMs > DIALTONE_TIMEOUT_MS) {
        startTone(TONE_NU);
        setState(ST_CLEARED);
      }
      break;

    case ST_DIALLING:
      if (pulseCount == 0 && (now - lastDigitMs) > NUMBER_COMPLETE_MS) {
        if (numLen) {
          placeCall();
        } else {
          startTone(TONE_NU);
          setState(ST_CLEARED);
        }
      }
      break;

    default:
      break;
  }
}

// ===========================================================================
//  SETUP / LOOP
// ===========================================================================
void setup() {
  Serial.begin(115200);
  Serial.println(F("\nBT Rotary GSM Phone starting..."));

  pinMode(PIN_SIM_PWR, OUTPUT);
  digitalWrite(PIN_SIM_PWR, LOW);

  bellA.setSpeed(255);
  bellB.setSpeed(255);
  bellRelease();

  toneBegin();
  inputBegin(hookIn);
  inputBegin(dialIn);

  sim.begin(9600);
  simPowerOn();
  simConfigure();

  setState(ST_IDLE);

  // If the handset is already lifted at power-up, react to it straight away
  if (isOffHook()) {
    hookIn.stable = !HOOK_OFF_LEVEL;
  }
}

void loop() {
  uint32_t now = millis();
  simPoll();
  hookUpdate(now);
  dialUpdate(now);
  ringerUpdate(now);
  toneUpdate(now);
  timeoutsUpdate(now);
}
