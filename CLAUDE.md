# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ZMK is an open-source keyboard firmware built on Zephyr RTOS, targeting ARM microcontrollers (primarily Nordic nRF52840). It supports wired USB and wireless BLE keyboards, including split keyboard configurations.

## Build Commands

All firmware builds use Zephyr's `west` meta-tool. Commands run from the `app/` directory.

```bash
# Initial workspace setup (one-time)
west init -l app
west update
west zephyr-export

# Build for a board + shield
cd app
west build -b nice_nano_v2 -- -DSHIELD=corne_left

# Build with extra config
west build -b nice_nano_v2 -- -DSHIELD=kyria_left -DCONFIG_ZMK_DISPLAY=y

# Flash firmware
west flash
```

### Testing

Tests use the `native_posix_64` board to simulate key events and compare output against snapshots.

```bash
# Run all tests (from app/)
west test

# Run a single test
./run-test.sh tests/hold-tap/balanced/1-dn-up

# Run a test category
./run-test.sh tests/hold-tap

# Parallel test execution (default 4 jobs)
J=8 ./run-test.sh all
```

Each test case directory contains:

- `native_posix_64.keymap` — test keymap with mock key events
- `events.patterns` — sed patterns to filter relevant log output
- `keycode_events.snapshot` — expected output (diff'd against actual)
- `pending` (optional) — marks test as allowed to fail

### Formatting and Linting

```bash
# C code formatting
clang-format -i <file>

# Install pre-commit hooks (runs clang-format + prettier on commit)
pip3 install pre-commit && pre-commit install

# Documentation linting (from docs/)
npm run lint
npm run prettier:check
npm run typecheck
```

### Documentation Site

```bash
cd docs
npm ci
npm start          # Dev server on port 3000
npm run build      # Production build
```

## Architecture

### Event-Driven Core

ZMK uses a publish-subscribe event system with compile-time registration via linker sections. Events are declared with `ZMK_EVENT_DECLARE`/`ZMK_EVENT_IMPL`, listeners with `ZMK_LISTENER`/`ZMK_SUBSCRIPTION`. Listeners can bubble (pass through), handle (consume), or capture (hold) events.

### Data Flow Pipeline

```
KSCAN (key matrix scan) → position_state_changed event
    → Keymap (layer lookup) → invokes Behavior
        → keycode_state_changed event
            → HID Listener → HID report update
                → Endpoints → USB or BLE transport
```

### Key Source Directories (under `app/`)

- **`src/behaviors/`** — Behavior implementations (key press, hold-tap, sticky key, macros, layers, etc.). Each is a Zephyr device driver implementing the behavior API. `behavior_hold_tap.c` is the most complex.
- **`src/events/`** — Event type definitions (position_state_changed, keycode_state_changed, layer_state_changed, etc.)
- **`src/split/bluetooth/`** — Split keyboard communication. `central.c` is the main half; `service.c` is the peripheral that sends position events over BLE GATT.
- **`src/`** — Core modules: `keymap.c` (layer stack, behavior dispatch), `ble.c` (BLE management/profiles), `combo.c` (key combinations), `hid.c` (HID report state), `endpoints.c` (USB/BLE selection), `event_manager.c` (pub-sub core)
- **`drivers/kscan/`** — Key scanning drivers (GPIO matrix, direct wire, composite, mock for testing)
- **`dts/`** — DeviceTree source files. `behaviors/` has .dtsi includes for each behavior; `bindings/` has YAML schemas for DT bindings.
- **`boards/`** — Board definitions (MCU boards under `arm/`) and shield definitions (keyboard PCBs under `shields/`)
- **`include/zmk/`** — Public headers for the core API

### Configuration Systems

- **Kconfig** (`app/Kconfig`) — Compile-time feature toggles (BLE, USB, display, RGB, split mode, etc.)
- **DeviceTree** — Hardware description and keymap definitions. `.keymap` files are DTS overlays that define layer bindings. `.overlay` files customize board/shield hardware.
- **`cmake/zmk_config.cmake`** — Resolves keymaps, overlays, and .conf files based on board/shield names. Supports user config via `ZMK_CONFIG` env var.

### Hardware Metadata

`.zmk.yml` files describe boards, shields, and interconnects. Validated against `schema/hardware-metadata.schema.json` via `west metadata check`.

### Split Keyboard Model

The central half runs the full keymap/behavior pipeline and connects to the host. Peripherals scan keys and send position events to the central over BLE. Behaviors have a locality property (CENTRAL, EVENT_SOURCE, or GLOBAL) determining where they execute.

## Conventions

- **Commit messages**: conventional commits style (e.g., `feat(behaviors):`, `fix(ble):`)
- **C formatting**: LLVM style, 4-space indent, 100-column limit (see `.clang-format`)
- **YAML/Markdown**: formatted with prettier
- **Behaviors**: implemented as Zephyr device drivers with DT bindings
- **New hardware**: boards go in `app/boards/arm/`, shields in `app/boards/shields/`, each needs a `.zmk.yml` metadata file

## CI

GitHub Actions workflows run on changes to relevant paths:

- **build.yml** — Builds board/shield combos. On core changes, uses matrix from `core-coverage.yml`. On board changes, builds only affected combinations.
- **test.yml** — Runs `west test` on src/test changes
- **clang-format-lint.yml** — Checks C formatting
- **doc-checks.yml** — Runs ESLint, prettier, and TypeScript checks on docs/
- **hardware-metadata-validation.yml** — Validates `.zmk.yml` files
