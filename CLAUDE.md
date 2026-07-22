# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for the **Pixel2LED / BlackLED** controller: a Teensy 3.1/3.2 that receives Art-Net (DMX-over-Ethernet) through a WIZ820io Ethernet chip and drives up to 6 strands of addressable LEDs (APA102 via FastLED). This is not a PC program — the sketch runs on the MCU.

## Build & flash

There is no command-line toolchain, no Makefile, no tests. Building and flashing happen exclusively through the **Arduino IDE + Teensyduino**:

- Arduino IDE 1.8.1, Teensyduino 1.35
- `Tools -> Board`: **Teensy 3.1/3.2**
- `Tools -> CPU Speed`: **120 MHz (overclock)** — required so all Art-Net packets are read in time (see the `F_BUS` check in `Pixel2LED.ino`)
- Open `Pixel2LED.ino` (the sketch name must match the folder — the other `.h`/`.cpp` files are compiled in automatically)

### Dependencies (install manually into `~/Documents/Arduino/libraries/`)

- [ArtNode](https://github.com/vertigo-dk/ArtNode) — Art-Net base class, extended by `ArtNetFrameExtension`
- Modified Ethernet lib with **2 sockets**: `Ethernet-2-socket.zip` (in the repo) — the stock Ethernet lib is not enough, since UDP (Art-Net) **and** TCP (web interface on port 80) run at the same time
- FastLED, TextFinder, EEPROM (Teensy standard)

**⚠️ Critical install-order pitfall (see "Silent wrong Ethernet library" in Gotchas below):** Teensyduino ships its own library also named `Ethernet` (Stoffregen, v2.0.0) inside the board package itself, not in the sketchbook. Since it has the exact same `name=Ethernet` as `Ethernet-2-socket.zip`, the toolchain silently prefers the bundled one over the one you install — the sketch still compiles fine, just against the wrong library, and the board will drop most incoming DMX universes despite looking correctly set up. You must remove/rename the bundled copy (`<Arduino15>/packages/teensy/hardware/avr/<ver>/libraries/Ethernet`) so the sketchbook's `Ethernet-2-socket` is the only candidate left.

## Architecture

Everything essential lives in `Pixel2LED.ino`. `loop()` calls only two things: `artNetNode()` and `webInterface()`.

### Art-Net path (`artNetNode()`)

Parses UDP packets by the 8-byte ID in the header and dispatches by OpCode:

- **`Art-Net` / OpDmx** — writes DMX data into the global `leds[]` buffer. The port→LED-index math (`portOffset`) is the trickiest part: each physical output starts at DMX address 1 of a new universe, and leftover channels at the end of a universe are skipped.
- **`Art-Net` / OpSync** (`0x5200`) and **`MadrixN`** — trigger `FastLED.show()` and compute `fps` / `avgUniUpdated` for the node report.
- **`Art-Net` / OpPoll** — sends `ArtPollReply` including a temperature/FPS status string.
- **`Art-Net` / OpAddress** — remote configuration of names/universes by the controller (e.g. MadMapper).
- **`Art-Ext`** — vendor-specific frame extension understood by **MadMapper** (see below).

### `ArtNetFrameExtension.{h,cpp}` — ArtNodeExtended

Inherits from `ArtNode` and adds:

- `getAddress()` / `getStartAddress()` — universe address calculation from net/subnet/port
- `createExtendedPollReply()` — proprietary `Art-Ext` reply that can report more than 4 ports (standard Art-Net is limited to 4 ports/node); used by MadMapper
- `pollReport[64]` — status string embedded into the standard PollReply

### Web interface (`webInterface()`)

Minimal HTTP server on port 80 under `/setup`. The HTML is stored as chopped-up `PROGMEM` strings (to save flash). Form fields are named `DT<n>`; `TextFinder` parses the values and writes IP/MAC/subnet/universe/outputs/LED-count into **EEPROM**, then does a soft reset via `WRITE_RESTART`.

### EEPROM layout (persistent configuration)

Hard-coded addresses — **do not renumber**:

```text
0        checkID (must be 0x92, otherwise defaults are loaded)
1–6      MAC        7–10   IP        11–14  Subnet
15,16    Universe (low,high)         17     Number of outputs
18,19    LEDs/output (low,high)      20     Controller ID (fixed, NEVER change!)
```

`controllerID` (address 20) determines the default IP `2.2.2.<ID>` and the node name `Pixel2LED#<ID>`.

### Network conventions

- Everything lives on the **2.x.x.x** network, subnet `255.0.0.0` (Art-Net 3 spec). Host PC e.g. `2.0.0.1`.
- The MAC is read at runtime from the Teensy chip (`TeensyMAC.h`, `mac_addr` reads the bytes Paul burned into the READ-ONCE flash area) — the top 3 bytes in the `config` literal are placeholders.

## Gotchas

- **Silent wrong Ethernet library = massive Art-Net packet loss.** This is the single most consequential pitfall in this codebase. `Ethernet-2-socket.zip` and the Ethernet lib bundled with Teensyduino both declare `name=Ethernet` in `library.properties`. Arduino IDE / arduino-cli resolve by that `name=` field, not by folder name — renaming a folder to `Ethernet-2-socket.disabled` does **not** exclude it from consideration, and the platform-bundled copy wins the collision. Symptom: sketch compiles and uploads without any error, the board boots and shows `Serial` output, but only the first output's universes (and the odd stray one) ever arrive — most DMX universes are silently dropped, look like random/CPU/buffer issues, and are **not** fixable by raising CPU clock, SPI clock, or `REFRESH_RATE_KHZ`. Verified fix: compile with `arduino-cli ... --verbose` and check the "Used library" table in the output for the `Ethernet` row's path — it must point into the sketchbook's `Ethernet-2-socket` folder, not `<Arduino15>/packages/teensy/hardware/avr/<ver>/libraries/Ethernet`. If it shows the wrong path, physically move the bundled folder out of the packages tree (not just rename) and recompile. With the correct library actually linked, all universes arrive reliably at ~100% even at 5 outputs × 300 LEDs; none of the other tuning below is actually needed to fix packet loss (it was chasing a symptom of this).
- **Key config is overwritten from `EEPROM`**: `NUM_OF_OUTPUTS` and `hardware_num_led_per_output` in `setup()` come from EEPROM, not from the constants at the top of the sketch (those only apply on the very first flash, when `checkID != 0x92`).
- **README vs. code**: the version history in the README mentions "remove FastLED option" — but the current code uses FastLED (APA102). README notes about WS2811/OctoWS2811 are stale relative to the actual sketch.
- **Known limit**: above ~18 DMX universes the controller starts dropping packets. FPS/pixel trade-offs are noted as a comment at the top of `Pixel2LED.ino`.
- The per-output pins in the `FastLED.addLeds<...>` calls in `setup()` are hardware-specific (clock/data pairs of the BlackLED board) — when changing them, match the actual wiring.
- `FastLED.show()` for 5×300 APA102 pixels blocks the loop for ~20–30ms (all outputs are software bit-banged SPI — none of the wired pins match Teensy 3.x's real hardware SPI pins, and FastLED has no parallel-output controller for clocked/SPI-style chipsets like APA102, only for clockless WS281x). This sounds like it should cause packet loss under load, and raising `REFRESH_RATE_KHZ` does measurably shrink it — but empirically it is *not* the actual bottleneck; see the Ethernet library gotcha above.
