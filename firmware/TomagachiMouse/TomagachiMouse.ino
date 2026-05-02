/**
 * TomagachiMouse — ESP32-S3 / ESP32-S2 USB HID Mouse Jiggler
 *
 * Enumerates as a standard USB HID mouse and sends small relative-motion
 * reports so the host OS cursor never goes idle.
 *
 * ═══════════════════════════════════════════════════════════════════════
 *  CONFIGURATION  — edit these values to change behaviour
 * ═══════════════════════════════════════════════════════════════════════
 */

// Movement per HID report.  Both values are int8_t (-128 … 127).
#define DX  -1   // horizontal delta   (-1 = drift left,  +1 = drift right)
#define DY   0   // vertical delta     (-1 = drift up,    +1 = drift down)

// Milliseconds between reports.
//   Perceived cursor speed ≈ |DX| pixels / INTERVAL_MS milliseconds
//   Examples:  50 ms → ~20 px/s (default, barely perceptible)
//             200 ms → ~5  px/s (very slow)
//              10 ms → fast, may trigger host "jitter" detection
#define INTERVAL_MS  50

// How long to wait (ms) after USB is mounted before the first move.
// Gives the OS time to finish driver setup.
#define START_DELAY_MS  3000

// ── GPIO: pause/resume button ─────────────────────────────────────────
// Default = GPIO 0, which is the BOOT button already present on most
// ESP32-S3-DevKitC-1 and ESP32-S2-DevKitM-1 boards (no extra wiring).
// To add a dedicated button: wire it between any free GPIO and GND;
// the internal pull-up means you only need two wires.
#define BUTTON_PIN  0

// ── GPIO: LED indicator ───────────────────────────────────────────────
// Solid ON  → active (sending movement)
// Blinking  → paused
//
// Boards with a plain through-hole LED (common on many clones): GPIO 2.
// ESP32-S3-DevKitC-1 / S2-DevKitM-1 official boards have an RGB LED
// (NeoPixel) instead; set LED_PIN to -1 and wire an external LED via a
// 330 Ω resistor between your chosen GPIO and GND.
// Set to -1 to disable the LED entirely.
#define LED_PIN  2

// ═══════════════════════════════════════════════════════════════════════
//  END OF CONFIGURATION  — nothing below normally needs changing
// ═══════════════════════════════════════════════════════════════════════

#include "USB.h"
#include "USBHIDMouse.h"

USBHIDMouse Mouse;

// ── State ──────────────────────────────────────────────────────────────
static volatile bool     usbMounted = false;  // set by USB event callback
static volatile uint32_t mountedAt  = 0;      // millis() when USB was mounted
static bool          started    = false;  // true after START_DELAY_MS
static bool          paused     = false;  // toggled by button
static uint32_t      lastMove   = 0;      // timestamp of last HID report

// Button debounce
static bool     btnRaw      = HIGH;   // last raw GPIO reading
static bool     btnStable   = HIGH;   // last debounced state
static uint32_t btnTime     = 0;      // timestamp of last raw change
#define DEBOUNCE_MS  50

// LED blink (paused state)
static uint32_t lastBlink = 0;
static bool     ledState  = false;
#define BLINK_HALF_MS  500   // 1-second period, 50 % duty cycle

// ── Helpers ────────────────────────────────────────────────────────────
static inline void ledWrite(bool on) {
#if LED_PIN >= 0
  ledState = on;
  digitalWrite(LED_PIN, on ? HIGH : LOW);
#endif
}

// ── USB event callback ────────────────────────────────────────────────
// Registered with USB.onEvent() in setup().
// ARDUINO_USB_RESUME_EVENT fires both on initial host enumeration and on
// wake-from-suspend; ARDUINO_USB_SUSPEND_EVENT / STOP_EVENT fire on
// disconnect or host sleep.
static void onUsbEvent(void* /*arg*/, esp_event_base_t base,
                       int32_t id, void* /*data*/) {
  if (base != ARDUINO_USB_EVENTS) return;

  switch (id) {
    case ARDUINO_USB_RESUME_EVENT:
      if (!usbMounted) {
        usbMounted = true;
        mountedAt  = millis();
      }
      break;

    case ARDUINO_USB_SUSPEND_EVENT:
    case ARDUINO_USB_STOP_EVENT:
      usbMounted = false;
      started    = false;   // re-apply start delay after reconnect
      break;

    default:
      break;
  }
}

// ── setup ──────────────────────────────────────────────────────────────
void setup() {
  // LED
#if LED_PIN >= 0
  pinMode(LED_PIN, OUTPUT);
  ledWrite(false);
#endif

  // Button — active-LOW with internal pull-up
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Register USB event listener before starting the stack
  USB.onEvent(onUsbEvent);

  // USB HID device descriptor: one mouse, no keyboard
  Mouse.begin();

  // Start TinyUSB stack; after this call the host can enumerate the device
  USB.begin();
}

// ── loop ───────────────────────────────────────────────────────────────
void loop() {
  const uint32_t now = millis();

  // ── 1. Button debounce ───────────────────────────────────────────────
  const bool raw = digitalRead(BUTTON_PIN);
  if (raw != btnRaw) {
    btnRaw  = raw;
    btnTime = now;
  }
  if ((now - btnTime) >= DEBOUNCE_MS && raw != btnStable) {
    btnStable = raw;
    if (btnStable == LOW) {          // falling edge = button pressed
      paused = !paused;
      // Immediately update LED to reflect new state
      if (paused) {
        ledWrite(false);             // start blink cycle from OFF
        lastBlink = now;
      } else {
        ledWrite(true);              // solid ON
      }
    }
  }

  // ── 2. Start-delay gate ──────────────────────────────────────────────
  if (!started && usbMounted && (now - mountedAt) >= START_DELAY_MS) {
    started = true;
  }

  // ── 3. LED management ────────────────────────────────────────────────
#if LED_PIN >= 0
  if (started && !paused) {
    // Active — solid ON (only set once to avoid repeated writes)
    if (!ledState) ledWrite(true);
  } else if (paused) {
    // Paused — 1 Hz blink
    if ((now - lastBlink) >= BLINK_HALF_MS) {
      lastBlink = now;
      ledWrite(!ledState);
    }
  } else {
    // Before first move (USB not yet mounted or in start delay) — LED off
    if (ledState) ledWrite(false);
  }
#endif

  // ── 4. Send HID report ───────────────────────────────────────────────
  if (started && !paused && (now - lastMove) >= INTERVAL_MS) {
    lastMove = now;
    // Alternate direction each report so the cursor oscillates in-place
    // rather than drifting off the screen edge.
    static int8_t dx = DX;
    static int8_t dy = DY;
    Mouse.move(dx, dy, /*wheel=*/0);
    dx = -dx;
    dy = -dy;
  }
}
