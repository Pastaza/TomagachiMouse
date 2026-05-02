# TomagachiMouse

An ESP32-S3 / ESP32-S2 USB HID mouse jiggler. Plug it into any computer and
the cursor slowly drifts left, keeping the OS from locking the screen or
showing a screensaver. A button pauses/resumes movement; an LED shows state.

---

## Table of Contents

1. [Recommended Hardware](#1-recommended-hardware)
2. [Wiring](#2-wiring)
3. [Firmware](#3-firmware)
   - [Minimal version](#31-minimal-version)
   - [Full version (button + LED)](#32-full-version-button--led)
4. [Build & Flash](#4-build--flash)
   - [Arduino IDE 2.x](#41-arduino-ide-2x)
   - [PlatformIO](#42-platformio)
5. [Configuration Reference](#5-configuration-reference)
6. [Test Checklist](#6-test-checklist)
7. [Troubleshooting](#7-troubleshooting)

---

## 1. Recommended Hardware

| Board | Why |
|---|---|
| **ESP32-S3-DevKitC-1** *(primary)* | Xtensa LX7 dual-core, native USB-OTG on dedicated USB port, widely available, good community support. |
| **ESP32-S2-DevKitM-1** *(secondary)* | Smaller, single-core, native USB-OTG. Slightly cheaper; same firmware works. |

**Key requirement:** the board must have a **USB-OTG** (native USB) port wired directly to the ESP32-S3/S2 chip, not through a USB-Serial bridge. Both DevKit boards above have this as a second USB-C connector labelled "USB" (as opposed to "UART").

> ⚠️ Standard ESP32 (original), ESP32-C3, and ESP32-H2 do **not** have USB-OTG and cannot run this firmware.

---

## 2. Wiring

### Minimal (no extra components)

Both DevKit boards include a **BOOT button** on GPIO 0 and a USB-OTG port.
No extra wiring is needed to test the full feature set:

```
USB-OTG port  ──►  host computer USB port   (the HID mouse)
GPIO 0 (BOOT) ──►  already connected to GND via button on the board
```

### Adding a dedicated button and external LED

```
                   ESP32-S3/S2 Dev Board
                  ┌─────────────────────┐
           GND ───┤ GND                 │
                  │                     │
GPIO_BUTTON ──────┤ GPIO 0  (or any IO) ├──[ button ]──► GND
                  │                     │   (internal pull-up, active-LOW)
    GPIO_LED ─────┤ GPIO 2  (or any IO) ├──[330 Ω]──[LED]──► GND
                  │                     │
              ────┤ USB-OTG             ├──────────────► computer
                  └─────────────────────┘
```

- **Button**: any momentary push-button between the chosen GPIO and GND.
  No external resistor needed (internal pull-up is enabled in firmware).
- **LED**: standard 3 mm / 5 mm LED in series with a **330 Ω resistor** to GND.
  Adjust the resistor for desired brightness (220 Ω for brighter, 1 kΩ for dimmer).

> **RGB LED boards** (official Espressif DevKitC-1 / DevKitM-1 ship with a
> WS2812 RGB LED, not a plain GPIO-driven LED). Either wire an external LED as
> above, or set `LED_PIN -1` to disable the indicator.

---

## 3. Firmware

### 3.1 Minimal Version

A 30-line sketch with no button or LED — good for quickly verifying the concept:

```cpp
// ── Minimal TomagachiMouse ────────────────────────────────────────────
#define DX           -1   // cursor pixels per report (left)
#define DY            0
#define INTERVAL_MS  50   // ms between reports
#define START_DELAY_MS 3000

#include "USB.h"
#include "USBHIDMouse.h"

USBHIDMouse Mouse;

void setup() {
  Mouse.begin();
  USB.begin();
}

void loop() {
  static bool     started  = false;
  static uint32_t bootTime = millis();
  static uint32_t lastMove = 0;

  if (!started && (millis() - bootTime) >= START_DELAY_MS) started = true;

  if (started && (millis() - lastMove) >= INTERVAL_MS) {
    lastMove = millis();
    Mouse.move(DX, DY, 0);
  }
}
```

### 3.2 Full Version (button + LED)

See [`firmware/TomagachiMouse/TomagachiMouse.ino`](firmware/TomagachiMouse/TomagachiMouse.ino).

Features:
- Configurable motion constants at the top of the file.
- Waits for USB enumeration + `START_DELAY_MS` before first report.
- Pause/resume toggle on button press (debounced, 50 ms).
- LED solid = active; LED blinking (1 Hz) = paused.
- LED and button pins settable to any GPIO, or LED disabled with `-1`.

---

## 4. Build & Flash

### 4.1 Arduino IDE 2.x

#### One-time setup

1. Open **File → Preferences**.
2. In *Additional Boards Manager URLs* add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Open **Tools → Board → Boards Manager**, search `esp32`, install
   **"esp32 by Espressif Systems"** version **≥ 2.0.14**.

#### Per-project board settings (ESP32-S3)

| Setting | Value |
|---|---|
| Board | `ESP32S3 Dev Module` |
| USB Mode | **USB-OTG (TinyUSB)** |
| USB CDC On Boot | **Disabled** |
| Upload Mode | `UART0 / Hardware CDC` |
| Flash Size | `4MB (32Mb)` |
| Partition Scheme | `Default 4MB with spiffs` |
| CPU Frequency | `240MHz (WiFi)` |

For **ESP32-S2**, select `ESP32S2 Dev Module` and use the same USB settings.

#### Flashing

1. Hold the **BOOT** button on the board, click **Upload** in Arduino IDE, release BOOT once upload starts.
2. After flashing, press **RESET** (or unplug/replug). The board re-enumerates as a HID mouse.

> If the upload fails, ensure you are uploading through the **UART** port (the USB-Serial bridge connector), not the USB-OTG port.

---

### 4.2 PlatformIO

#### One-time setup

```bash
pip install platformio          # or install the VS Code PlatformIO extension
```

#### Build and flash (ESP32-S3)

```bash
# from the repo root
pio run -e esp32s3 -t upload
```

For ESP32-S2:

```bash
pio run -e esp32s2 -t upload
```

If the device is not detected automatically:

```bash
pio run -e esp32s3 -t upload --upload-port /dev/ttyUSB0   # Linux/macOS
pio run -e esp32s3 -t upload --upload-port COM3            # Windows
```

Hold **BOOT**, trigger upload, release **BOOT** once the upload starts, then press **RESET**.

---

## 5. Configuration Reference

All tunable values are `#define` constants at the top of
`firmware/TomagachiMouse/TomagachiMouse.ino`.

| Constant | Default | Description |
|---|---|---|
| `DX` | `-1` | Horizontal pixels per report. Negative = left, positive = right. Range: -128 … 127. |
| `DY` | `0` | Vertical pixels per report. Negative = up, positive = down. |
| `INTERVAL_MS` | `50` | Milliseconds between reports. Lower = faster cursor movement. Minimum practical value ≈ 10 ms. |
| `START_DELAY_MS` | `3000` | Milliseconds after USB mount before the first report. Increase if the cursor moves during OS login. |
| `BUTTON_PIN` | `0` | GPIO for pause/resume button. GPIO 0 = BOOT button on DevKit boards. |
| `LED_PIN` | `2` | GPIO for LED indicator. Set to `-1` to disable. |

**Tuning cursor speed:**

```
speed (px/s) = |DX| × 1000 / INTERVAL_MS
```

Examples:

| INTERVAL_MS | Speed |
|---|---|
| 10 | ~100 px/s (fast) |
| 50 | ~20 px/s (default, barely visible) |
| 200 | ~5 px/s (very slow) |
| 1000 | ~1 px/s (imperceptible) |

---

## 6. Test Checklist

### Enumeration

- [ ] **Windows**: Open *Device Manager → Human Interface Devices*. A new
  "HID-compliant mouse" entry should appear within 3 seconds of plugging in.
- [ ] **macOS**: Open *System Settings → Mouse* or run
  `ioreg -p IOUSB -l | grep -i mouse`. The device should appear.
- [ ] **Linux**: Run `lsusb` before and after plugging in. A new entry should
  appear. Also check `dmesg | tail -20` for `hid-generic` binding.

### Behaviour

- [ ] After `START_DELAY_MS` (default 3 s) the mouse cursor drifts left.
- [ ] Pressing the button once pauses movement; the LED starts blinking.
- [ ] Pressing the button again resumes movement; the LED goes solid.
- [ ] Unplugging and re-plugging re-enumerates correctly and movement
  resumes after `START_DELAY_MS`.
- [ ] The host screensaver / lock timer is reset while the jiggler is active.

### Verify on Windows (PowerShell)

```powershell
# List HID devices — look for "HID-compliant mouse"
Get-PnpDevice -Class Mouse | Select-Object FriendlyName, Status
```

### Verify on Linux

```bash
# Watch raw HID events (run before plugging in)
sudo evtest   # select the new mouse device when prompted
# You should see REL_X events with value -1 every 50 ms
```

---

## 7. Troubleshooting

### Device not detected at all

| Symptom | Likely cause | Fix |
|---|---|---|
| Nothing in Device Manager / lsusb | Wrong USB port | Plug into the **USB-OTG** port, not the UART port |
| Nothing after correct port | USB Mode not set | Arduino IDE: set *USB Mode* to **USB-OTG (TinyUSB)**; PlatformIO: ensure `-DARDUINO_USB_MODE=0` in build_flags |
| "Unknown device" / error code 43 | CDC-on-boot enabled | Set *USB CDC On Boot* to **Disabled** |
| Still nothing | Charge-only cable | Use a data-capable USB cable |

### Upload fails

- Hold **BOOT** before clicking upload; release after "Connecting…" appears.
- On Linux, add your user to the `dialout` group:
  ```bash
  sudo usermod -aG dialout $USER
  # log out and back in
  ```
- On macOS/Linux, verify the UART port appears: `ls /dev/tty.*` (macOS) or
  `ls /dev/ttyUSB*` (Linux).

### Mouse cursor moves erratically or too fast

- Increase `INTERVAL_MS` (e.g., `100` or `200`).
- Reduce `|DX|` to `1` if it is currently higher.
- Some hosts apply pointer acceleration; the physical HID delta is always `DX`.

### Cursor doesn't move after upload

- Wait the full `START_DELAY_MS` (3 s default) after plug-in.
- Check that the board is NOT paused (LED should be solid, not blinking).
- Confirm enumeration with `lsusb` / Device Manager.

### Linux: permission denied on `/dev/ttyUSB*`

```bash
sudo usermod -aG dialout $USER
```

Log out and back in (or reboot) for group membership to take effect.

### macOS: cursor moves but screensaver still triggers

macOS Ventura+ has a separate "prevent sleep" mechanism. The mouse jiggler
resets the **input idle timer**, which should prevent the screensaver. If it
does not, check *System Settings → Lock Screen* and increase the
"Require password after screensaver begins" delay.

---

## Licence

MIT — see [LICENSE](LICENSE) file (or add one as needed).
