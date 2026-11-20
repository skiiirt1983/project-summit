# Dimensions (single source for `cad/params.py`)

Every value is marked one of three ways:

- **drawing**: Waveshare's 2D drawing (`ESP32-P4-WIFI6-Touch-LCD-5-dimensions-20260408.pdf`) or 3D STEP
  (`ESP32-P4-WIFI6-Touch-LCD-5_3d-20260812.stp`), from github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-5,
  commit `a334bad` (2026-11-10). "(STEP)" means it is only in the 3D file.
- **measured**: measured by Paul, or read off the LyricFlash 3MF in `ref/lyricflash-enclosure/` (meshes, ±0.05).
- **PLACEHOLDER**: a guess with extra room, so the part won't come out too small. Measure before a real print
  (list and caliper instructions: `COUPON-RESULTS.md`, "Measure these").

Board frame: looking at the **back** of the board (components facing you), USB-C ports on the left.
x = along the long side from the USB end, y = up from the 40-pin-header edge, heights from the board's
component face.

## Screen module (glass + LCD + board)

| What | Value (mm) | Source | `params.py` |
|---|---|---|---|
| Glass outline | 126.9 × 70.7, corners R3 | drawing | `GLASS_L`, `GLASS_W`, `GLASS_R` |
| Visible screen area | 110.32 × 62.28 (±0.15) | drawing | `VIEW_L`, `VIEW_W` |
| Black border on glass: short ends / long sides | 8.29 / 4.21 | drawing | — |
| Board | 118.5 × 64.5 × 1.6 | drawing | `BOARD_L`, `BOARD_W` |
| Board edge inside glass edge: USB end / other end / long sides | 1.55 / 6.85 / 3.1 | drawing | `BOARD_INSET_USB_END` |
| Glass front → board component face | 7.6 | drawing (STEP) | `GLASS_TO_BOARD` |
| Standoffs | 4× M2.5 thread, 4.0 tall, at 112 × 57 centres, 3.25 / 3.75 from board edges | drawing | `STANDOFFS`, `STANDOFF_H` |
| Tallest part on board (40-pin header) | 5.2 | drawing (STEP) | — |
| Total depth, glass front → top of header | 12.8 used. The STEP says 12.8, the 2D drawing 11.6 | PLACEHOLDER | `MODULE_DEPTH` |
| Glass thickness alone | ~1.3 | PLACEHOLDER | — (render only) |

## Ports, buttons, mics (board frame, centres)

| What | x | y | Notes | Source | `params.py` |
|---|---|---|---|---|---|
| USB-C ×2 | −1.3 (tip 0.2 inside glass edge) | 23.26, 41.21 | on the short end; centre 1.2 above board | drawing | `USB_Y`, `USB_H` |
| Buttons ×3 (side-push) | 72.76, 84.44, 96.12 | 65.0 (tip 2.6 inside glass edge) | centre 0.7 above board. **The newest board revision may differ: measure (checklist D9)** | drawing | `BUTTONS_X`, `BUTTON_H` |
| microSD slot | 51.1–67.2 | opens on the buttons edge | centre 0.6 above board | drawing (STEP) | `SD_X`, `SD_H` |
| Mic 1 / Mic 2 | 14.55 | 2.37 / 62.17 | no hole in the board under them, so they face the back | drawing (STEP) | `MICS` |
| 40-pin header | 54.55–105.85 | 1.12–7.72 | 5.2 tall; pokes through the carrier plate | drawing (STEP) | `HEADER_BOX` |
| 2-pin socket J5 | 26.31–32.07 | 57.93–62.78 | 4.3 tall; pokes through the carrier plate | drawing (STEP) | `J5_BOX` |
| 2-pin sockets J2, H3 | 30.9–38.6 / 112–117 | 1.0–6.2 / 13.4–16.8 | under 4 tall | drawing (STEP) | — |
| Which button is POWER / BOOT / RESET; which USB-C is UART; which socket is battery / speaker | | | read the board's printed labels | PLACEHOLDER | — (no geometry depends on it yet) |
| Wire path battery + accelerometer → board | notch 14 × 6 in the plate, USB end | | position is a guess | PLACEHOLDER | `WIRE_NOTCH` |
| Wi-Fi antenna keep-out | far end from USB, buttons edge | | battery sits ≥ 10 mm behind it in this layout; confirm on the real board | PLACEHOLDER | — |

## Battery, accelerometer, speaker

| What | Value (mm) | Source | `params.py` |
|---|---|---|---|
| Battery L × W × T incl. protection board | 105 × 66 × 13 (typical 8000 mAh packs are ≤ 100 × 65 × 12) | PLACEHOLDER | `BATT_L`, `BATT_W`, `BATT_T` |
| Battery bay as modelled | 106 × 70.4 × 14.6 (incl. 1.0 swell room + 0.6 air gap) | derived | — |
| Battery wire exit + lead length | assumed to reach the USB end | PLACEHOLDER | — |
| LIS2DW12 breakout | 26 × 18 × 5 | PLACEHOLDER | `ACC_L`, `ACC_W`, `ACC_T` |
| Free strip for accelerometer + wires + mics | 20.6 wide, at the USB end of the battery bay | derived | — |
| Speaker | not fitted, no room reserved (default, Paul to confirm) | PLACEHOLDER | — |

## LyricFlash (family look)

From `ref/lyricflash-enclosure/lyricflash_print_plates.3mf`: `top_shell.stl`, `base_smooth.stl`, assembly guide.

| What | Value (mm) | Source | `params.py` |
|---|---|---|---|
| Top shell outline | 52.86 × 66.83, 5.15 thick | measured | — |
| Visible bezel width at its narrowest | 2.95 | measured | (we use 4.5: `BEZEL_WALL` + `LIP_OVERLAP`) |
| Lip over the glass: thickness | 1.0 | measured | `LIP_T` |
| Lip over the glass: overlap | 1.35 (opening Ø46.96, glass pocket Ø49.66) | measured | `LIP_OVERLAP` |
| Gap between lip and glass | 0.25 | measured (assembly guide) | `LIP_GAP` |
| Screen depth below bezel face | 1.25 | measured | `GLASS_RECESS` |
| Outer corner radius | 6.7 | measured | (we use 8.25: `BEZEL_R`) |
| Outer edge bevel | 1.45 wide × 1.4 high | measured | — (our bezel is flush in the body) |
| Mic holes | 2× Ø2.0, straight, no chamfer | measured | `MIC_HOLE_D` |
| Wall thickness at the mic hole | 1.6 | measured | `MIC_WALL` |

## The Peak, as built in CAD (2026-11-12)

| What | Value (mm) |
|---|---|
| Body | 155.8 wide × 119.6 deep × 107.9 tall (+1.0 on pads) |
| Front wall thickness | 34.7 (driven by the placeholder battery) |
| Screen module (cream face) | 133.2 × 77.0, 31.1 deep |
| Inner frame | 137.1 × 80.9 |
| USB-C plug tunnel through body + frame + bezel wall | 14.2 long |

## Button pins (CAD, 2026-11-16; all untested)

| What | Value (mm) | `params.py` |
|---|---|---|
| Pin shaft / head | Ø2.6 / Ø6.0 x 1.2, length ~25.1 | `PIN_D`, `PIN_HEAD_D`, `PIN_HEAD_T`, `PIN_LEN` |
| Guide tunnel (frame + TPU) / module hole / dimple | Ø3.0 / Ø3.4 / Ø6.5 | `TUNNEL_D`, `BUTTON_HOLE_D`, `CBORE_D` |
| Axis | 9.55 behind the bezel face, up the slope, 25.7 from the button tip to the ridge | `BTN_Z`, `BTN_TIP_Y`, `BTN_EXIT_Y` |

## Measured on the real (newer) board, 2026-11-20

| What | Value (mm) | Source |
|---|---|---|
| Glass front to top of the 40-pin header | 11.6 | measured |
| Glass front to top of a standoff | 11.8 | measured |
| Glass outline | 126.93 x 70.71 | measured (matches the drawing) |
| Buttons from the USB-C end (centres) | 74.54, 86.50, 98.05 (assumed from the board edge) | measured |
| Buttons: stick-out past the top edge; centre height above the board | 0.75; 1.4 | measured |
| Button order from the USB-C end | RESET, BOOT, POWER | read from the board |
| UART USB-C port | the upper one (nearer the buttons) | read from the board |
| Sockets | X = BAT (J2), Y = SPK (J5), Z = RTC (H3) | read from the board |
| Speaker | 25.9 x 26.1 x 4.52, wires from the middle of the side | measured |
| White adhesive | on the back of the glass, facing the board; not interfering | Paul |
