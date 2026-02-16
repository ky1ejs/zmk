# Tornblue ZMK Upgrade Plan

Upgrade the tornblue board firmware from Zephyr v3.0 / old ZMK to Zephyr v4.1 / latest ZMK.

The branch is **1,256 commits behind** `upstream/main` (zmkfirmware/zmk).

---

## Step 0: Build and start the Dev Container

The repo includes a `.devcontainer/` configuration that provides a complete,
pre-built toolchain (west, cmake, ninja, dtc, ARM Zephyr toolchain, Python deps)
via a Docker image. This is the same environment used by ZMK's CI pipeline.

We use the `devcontainer` CLI (v0.83.0, already installed) to build and run
the container, and `devcontainer exec` to run commands inside it.

**Prerequisites already installed on this machine:** Docker, devcontainer CLI.

### Actions
```bash
# Build and start the container (from the repo root)
devcontainer up --workspace-folder /Users/kylejs/Developer/open-source/zmk
```

**Note:** After Step 1 (merging upstream), the Dockerfile will update from
`zmk-dev-arm:3.0` to `zmk-dev-arm:4.1-branch`. The container must be rebuilt:
```bash
devcontainer up --workspace-folder /Users/kylejs/Developer/open-source/zmk --remove-existing-container
```

### Verification

```bash
devcontainer exec --workspace-folder /Users/kylejs/Developer/open-source/zmk \
  bash -c "west --version && cmake --version | head -1 && ninja --version && dtc --version && arm-zephyr-eabi-gcc --version | head -1 && python3 -c \"import elftools; import yaml; print('OK')\""
```
All commands should succeed (no "not found" errors).

---

## Step 1: Merge upstream/main into the branch

Bring in all 1,256 upstream commits. This will include the Zephyr v4.1 migration and
all ZMK improvements. The tornblue files will have merge conflicts since the
`app/boards/arm/` directory was removed upstream.

**Important:** After this merge, rebuild the Dev Container since the Dockerfile
will change from `zmk-dev-arm:3.0` to `zmk-dev-arm:4.1-branch`:
```bash
devcontainer up --workspace-folder /Users/kylejs/Developer/open-source/zmk --remove-existing-container
```

### Actions
1. Ensure the upstream remote exists: `git remote add upstream https://github.com/zmkfirmware/zmk.git` (if not already)
2. `git fetch upstream`
3. `git merge upstream/main` — resolve conflicts, keeping the tornblue board files aside
   (they'll be rewritten in the new format in subsequent steps)
4. The old `app/west.yml` (pointing to Zephyr v3.0.0+zmk-fixes) will be replaced by upstream's
   (pointing to Zephyr v4.1.0+zmk-fixes)
5. Rebuild the Dev Container so it picks up the new `zmk-dev-arm:4.1-branch` image

### Verification
- `git log --oneline -1 upstream/main` commit hash appears in `git log --oneline -5 HEAD`
  (i.e., the merge commit includes upstream/main)
- `cat app/west.yml` shows `revision: v4.1.0+zmk-fixes`
- `git diff upstream/main -- app/src/ app/CMakeLists.txt app/Kconfig` shows no diff
  (core ZMK code matches upstream exactly)

---

## Step 2: Initialize the Zephyr workspace

After the merge and Dev Container rebuild, the west manifest has changed. The workspace
must be re-initialized to pull the correct Zephyr v4.1 and module dependencies.

All `west` / build commands run inside the Dev Container via `devcontainer exec`.
For brevity, subsequent steps use `dcexec` as shorthand for:
```bash
devcontainer exec --workspace-folder /Users/kylejs/Developer/open-source/zmk
```

### Actions
```bash
dcexec west init -l app
dcexec west update
dcexec west zephyr-export
```

### Verification
- `.west/` directory exists in the workspace
- `dcexec cat zephyr/VERSION` shows a Zephyr 4.1.x version
- `dcexec west list` completes without errors and lists `zephyr`, `hal_nordic`, `cmsis`, etc.

---

## Step 3: Move board files to vendor-based directory

The HWMv2 board format requires `app/boards/<vendor>/<board>/` instead of
`app/boards/arm/<board>/`.

### Actions
1. `mkdir -p app/boards/rtitmuss/tornblue`
2. Move all tornblue files from `app/boards/arm/tornblue/` to `app/boards/rtitmuss/tornblue/`
3. Delete `app/boards/arm/tornblue/`

### Verification
- `ls app/boards/rtitmuss/tornblue/` shows all board files
- `ls app/boards/arm/tornblue/` returns "No such file or directory"

---

## Step 4: Create `board.yml` (Zephyr HWMv2 metadata)

This is a **new required file** for Zephyr v4.1 boards.

### Actions
Create `app/boards/rtitmuss/tornblue/board.yml`:
```yaml
boards:
  - name: tornblue_left
    vendor: rtitmuss
    socs:
      - name: nrf52840
        variants:
          - name: zmk
  - name: tornblue_right
    vendor: rtitmuss
    socs:
      - name: nrf52840
        variants:
          - name: zmk
```

### Verification
- File exists at `app/boards/rtitmuss/tornblue/board.yml`
- Content uses `boards:` (plural) with two entries for left and right halves
- Each entry has `socs: [{name: nrf52840, variants: [{name: zmk}]}]`

---

## Step 5: Rename DTS files to HWMv2 naming convention

The new naming convention is `<board>_<soc>_<variant>.dts`.

### Actions
1. `tornblue_left.dts` → `tornblue_left_nrf52840_zmk.dts`
2. `tornblue_right.dts` → `tornblue_right_nrf52840_zmk.dts`
3. `tornblue_left.keymap` → `tornblue_left_nrf52840_zmk.keymap`
   (or confirm keymaps can stay as-is — check adv360pro reference)
4. `tornblue_right.keymap` → `tornblue_right_nrf52840_zmk.keymap`
5. The shared `tornblue.dtsi` keeps its name (it's included, not directly compiled)

### Verification
- `ls app/boards/rtitmuss/tornblue/tornblue_left_nrf52840_zmk.dts` exists
- `ls app/boards/rtitmuss/tornblue/tornblue_right_nrf52840_zmk.dts` exists
- No files named `tornblue_left.dts` or `tornblue_right.dts` remain

---

## Step 6: Rename defconfig files

### Actions
1. `tornblue_left_defconfig` → `tornblue_left_nrf52840_zmk_defconfig`
2. `tornblue_right_defconfig` → `tornblue_right_nrf52840_zmk_defconfig`

### Verification
- `ls app/boards/rtitmuss/tornblue/tornblue_left_nrf52840_zmk_defconfig` exists
- `ls app/boards/rtitmuss/tornblue/tornblue_right_nrf52840_zmk_defconfig` exists

---

## Step 7: Update defconfig contents

Remove auto-generated SoC/board selection lines and deprecated options.
Add pinctrl enablement.

### Actions

In both `tornblue_left_nrf52840_zmk_defconfig` and `tornblue_right_nrf52840_zmk_defconfig`:

**Remove these lines:**
- `CONFIG_SOC_SERIES_NRF52X=y`
- `CONFIG_SOC_NRF52840_QIAA=y`
- `CONFIG_BOARD_TORNBLUE_LEFT=y` / `CONFIG_BOARD_TORNBLUE_RIGHT=y`
- `CONFIG_WS2812_STRIP=y`

**Add:**
- `CONFIG_PINCTRL=y`

### Verification
- Neither defconfig file contains `CONFIG_SOC_SERIES_`, `CONFIG_SOC_NRF52840_`, `CONFIG_BOARD_`,
  or `CONFIG_WS2812_STRIP`
- Both defconfig files contain `CONFIG_PINCTRL=y`

---

## Step 8: Restructure Kconfig files

Replace the single `Kconfig.board` with per-half Kconfig files, and update
`Kconfig.defconfig` for the renamed split role symbol.

### Actions

**Delete** `Kconfig.board`.

**Create** `Kconfig.tornblue_left`:
```kconfig
config BOARD_TORNBLUE_LEFT
    select SOC_NRF52840_QIAA
```

**Create** `Kconfig.tornblue_right`:
```kconfig
config BOARD_TORNBLUE_RIGHT
    select SOC_NRF52840_QIAA
```

**Update** `Kconfig.defconfig`:
- Change `ZMK_SPLIT_BLE_ROLE_CENTRAL` → `ZMK_SPLIT_ROLE_CENTRAL`
- Remove any `USB_NRFX`, `USB_DEVICE_STACK`, `BT_CTLR`, `ZMK_BLE`, `ZMK_USB` defaults
  if they are now handled elsewhere (check adv360pro for reference)

### Verification
- `Kconfig.board` does not exist
- `Kconfig.tornblue_left` exists and contains `select SOC_NRF52840_QIAA`
- `Kconfig.tornblue_right` exists and contains `select SOC_NRF52840_QIAA`
- `grep -r "ZMK_SPLIT_BLE_ROLE_CENTRAL" app/boards/rtitmuss/tornblue/` returns no results
- `grep "ZMK_SPLIT_ROLE_CENTRAL" app/boards/rtitmuss/tornblue/Kconfig.defconfig` returns a match

---

## Step 9: Create pinctrl definitions

Replace the old-style `mosi-pin`/`sck-pin`/`miso-pin` SPI properties with the
pinctrl subsystem.

### Actions

**Create** `app/boards/rtitmuss/tornblue/tornblue_left-pinctrl.dtsi`:
```dts
&pinctrl {
    spi0_default: spi0_default {
        group1 {
            psels = <NRF_PSEL(SPIM_MOSI, 0, 23)>;
        };
    };
    spi0_sleep: spi0_sleep {
        group1 {
            psels = <NRF_PSEL(SPIM_MOSI, 0, 23)>;
            low-power-enable;
        };
    };
};
```

**Create** `app/boards/rtitmuss/tornblue/tornblue_right-pinctrl.dtsi`:
```dts
&pinctrl {
    spi0_default: spi0_default {
        group1 {
            psels = <NRF_PSEL(SPIM_MOSI, 0, 22)>;
        };
    };
    spi0_sleep: spi0_sleep {
        group1 {
            psels = <NRF_PSEL(SPIM_MOSI, 0, 22)>;
            low-power-enable;
        };
    };
};
```

**Update** both DTS files (`tornblue_left_nrf52840_zmk.dts` and `tornblue_right_nrf52840_zmk.dts`):
1. Add `#include "tornblue_left-pinctrl.dtsi"` (or right) near the top
2. Replace the `&spi0` block — remove `mosi-pin`, `sck-pin`, `miso-pin` and add:
   ```dts
   pinctrl-0 = <&spi0_default>;
   pinctrl-1 = <&spi0_sleep>;
   pinctrl-names = "default", "sleep";
   ```

### Verification
- Both pinctrl dtsi files exist
- `grep -r "mosi-pin\|sck-pin\|miso-pin" app/boards/rtitmuss/tornblue/` returns no results
- Both DTS files contain `pinctrl-0` and `pinctrl-names`

---

## Step 10: Add NFC pin and regulator DeviceTree nodes

P0.09 and P0.10 are NFC pins repurposed as GPIOs. The Kconfig option for this
was removed; it must be declared in DeviceTree now.

### Actions

**Add to `tornblue.dtsi`** (so it applies to both halves):
```dts
&uicr {
    nfct-pins-as-gpios;
};

&reg1 {
    regulator-initial-mode = <NRF5X_REG_MODE_DCDC>;
};
```

### Verification
- `grep "nfct-pins-as-gpios" app/boards/rtitmuss/tornblue/tornblue.dtsi` returns a match
- `grep "regulator-initial-mode" app/boards/rtitmuss/tornblue/tornblue.dtsi` returns a match

---

## Step 11: Update `tornblue.dtsi` for Zephyr v4.1

### Actions
1. Add `#include <common/nordic/nrf52840_uf2_boot_mode.dtsi>` to support the new
   bootloader retention mechanism
2. Remove deprecated `label` properties from device nodes (`kscan0`, `vbatt`,
   `cdc_acm_uart`, flash partition labels can stay as they may still be used)
3. Add `#include <dt-bindings/regulator/nrf5x.h>` for the `NRF5X_REG_MODE_DCDC` constant

### Verification
- `grep "nrf52840_uf2_boot_mode" app/boards/rtitmuss/tornblue/tornblue.dtsi` returns a match
- `grep 'label = "KSCAN"' app/boards/rtitmuss/tornblue/tornblue.dtsi` returns no results
- `grep "nrf5x.h" app/boards/rtitmuss/tornblue/tornblue.dtsi` returns a match

---

## Step 12: Remove deprecated `label` properties from DTS files

### Actions

Remove `label = "..."` lines from:
- `tornblue_left_nrf52840_zmk.dts`: encoder label, LED labels, ext-power label, WS2812 label
- `tornblue_right_nrf52840_zmk.dts`: LED labels, ext-power label, WS2812 label

### Verification
- `grep -c 'label =' app/boards/rtitmuss/tornblue/tornblue_left_nrf52840_zmk.dts` returns 0
- `grep -c 'label =' app/boards/rtitmuss/tornblue/tornblue_right_nrf52840_zmk.dts` returns 0

---

## Step 13: Update `led_driver.c` to modern Zephyr APIs

### Actions

1. **Update includes:**
   ```c
   #include <zephyr/init.h>
   #include <zephyr/device.h>
   #include <zephyr/devicetree.h>
   #include <zephyr/drivers/gpio.h>
   #include <zephyr/logging/log.h>
   ```

2. **Replace GPIO pattern** — change from `device_get_binding` + `DT_GPIO_LABEL` to
   `GPIO_DT_SPEC_GET`:
   ```c
   static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
   static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
   static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);
   ```

3. **Update init function** — use `gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE)`
   instead of the separate dev/pin/flags calls.

4. **Update pin set function** — use `gpio_pin_set_dt(&led1, value)`.

5. **Update SYS_INIT callback signature** — change from `static int led_init(const struct device *port)`
   to `static int led_init(void)`.

6. **Update Kconfig guard** — change `CONFIG_ZMK_SPLIT_BLE_ROLE_CENTRAL` to
   `CONFIG_ZMK_SPLIT_ROLE_CENTRAL`.

### Verification
- `grep "device_get_binding\|DT_GPIO_LABEL\|DT_GPIO_PIN\|DT_GPIO_FLAGS" app/boards/rtitmuss/tornblue/led_driver.c`
  returns no results
- `grep "GPIO_DT_SPEC_GET" app/boards/rtitmuss/tornblue/led_driver.c` returns 3 matches
  (one per LED)
- `grep "gpio_pin_set_dt\|gpio_pin_configure_dt" app/boards/rtitmuss/tornblue/led_driver.c`
  returns matches
- `grep "ZMK_SPLIT_BLE_ROLE_CENTRAL" app/boards/rtitmuss/tornblue/led_driver.c`
  returns no results
- `grep "ZMK_SPLIT_ROLE_CENTRAL" app/boards/rtitmuss/tornblue/led_driver.c`
  returns a match
- File compiles without errors (verified in Step 15)

---

## Step 14: Update `tornblue.zmk.yml` metadata

### Actions

Update the `siblings` field to include the SoC/variant qualifier:
```yaml
siblings:
  - tornblue_left//zmk
  - tornblue_right//zmk
```

### Verification
- `grep "//zmk" app/boards/rtitmuss/tornblue/tornblue.zmk.yml` returns 2 matches

---

## Step 15: Create `pre_dt_board.cmake`

All modern ZMK boards include this file.

### Actions

Create `app/boards/rtitmuss/tornblue/pre_dt_board.cmake`:
```cmake
list(APPEND EXTRA_DTC_FLAGS "-Wno-simple_bus_reg")
```

### Verification
- File exists and contains `-Wno-simple_bus_reg`

---

## Step 16: Build firmware (left half)

This is the primary validation that all changes are correct.

### Actions
```bash
dcexec west build -p -d app/build/left -b tornblue_left//zmk app
```

### Verification
- Build completes with exit code 0
- `ls app/build/left/zephyr/zmk.uf2` exists (UF2 output)
- No errors in build output (warnings about deprecated features are acceptable
  but should be noted for potential follow-up)

---

## Step 17: Build firmware (right half)

### Actions
```bash
dcexec west build -p -d app/build/right -b tornblue_right//zmk app
```

### Verification
- Build completes with exit code 0
- `ls app/build/right/zephyr/zmk.uf2` exists
- No errors in build output

---

## Step 18: Clean up and commit

### Actions
1. Remove stale files:
   - `torn#1.log`, `torn#2.log`, `torn#2.2` (debug log artifacts)
   - `tornblue.yaml` (old MCU metadata file, replaced by `board.yml`)
   - Any leftover files from old directory structure
2. Run `dcexec clang-format -i app/boards/rtitmuss/tornblue/led_driver.c`
3. Commit all changes

### Verification
- `git status` shows a clean working tree after commit
- No files remain under `app/boards/arm/tornblue/`
- `git diff upstream/main -- app/boards/rtitmuss/tornblue/` shows only tornblue-specific
  additions (no unrelated changes)

---

## File Inventory: Final State

After all steps, `app/boards/rtitmuss/tornblue/` should contain:

```
board.yml                              (NEW - Step 4)
board.cmake                            (existing, possibly unchanged)
pre_dt_board.cmake                     (NEW - Step 15)
CMakeLists.txt                         (existing, possibly unchanged)
Kconfig.tornblue_left                  (NEW - Step 8, replaces Kconfig.board)
Kconfig.tornblue_right                 (NEW - Step 8, replaces Kconfig.board)
Kconfig.defconfig                      (UPDATED - Step 8)
tornblue.zmk.yml                       (UPDATED - Step 14)
tornblue.dtsi                          (UPDATED - Steps 10, 11)
tornblue.keymap                        (existing, unchanged)
tornblue_left_nrf52840_zmk.dts         (RENAMED + UPDATED - Steps 5, 9, 12)
tornblue_right_nrf52840_zmk.dts        (RENAMED + UPDATED - Steps 5, 9, 12)
tornblue_left_nrf52840_zmk_defconfig   (RENAMED + UPDATED - Steps 6, 7)
tornblue_right_nrf52840_zmk_defconfig  (RENAMED + UPDATED - Steps 6, 7)
tornblue_left-pinctrl.dtsi             (NEW - Step 9)
tornblue_right-pinctrl.dtsi            (NEW - Step 9)
led_driver.c                           (UPDATED - Step 13)
README.md                              (existing, optionally updated)
```

**Deleted files:**
- `Kconfig.board`
- `tornblue.yaml`
- `tornblue_left.dts`, `tornblue_right.dts`
- `tornblue_left_defconfig`, `tornblue_right_defconfig`
- `tornblue_left.keymap`, `tornblue_right.keymap`
- `torn#1.log`, `torn#2.log`, `torn#2.2`
