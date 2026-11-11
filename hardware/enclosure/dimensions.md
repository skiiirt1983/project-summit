# Dimensions (single source for `cad/params.py`)

Sources:
- `STEP` = `hardware/mechanical/ESP32-P4-WIFI6-Touch-LCD-5_3d-20260812.stp`
- `drawing` = `hardware/mechanical/ESP32-P4-WIFI6-Touch-LCD-5-dimensions-20260408.pdf`
  both from github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-5, commit `a334bad` (2026-11-10).
- `measured` = Paul's calipers. `lyricflash` = LyricFlash enclosure CAD/3MF.

Status: ✅ sourced · 🔍 sourced, verify on the real part · ❌ missing.

Board frame used below: looking at the **back** of the board (components facing you),
USB-C ports on the left. x = along the long side from the USB end, y = up from the
40-pin-header edge, heights measured from the board's component face.

## Screen module (glass + LCD + board)

| What | Value (mm) | Source | Status |
|---|---|---|---|
| Glass outline | 126.9 × 70.7, corners R3 | drawing + STEP | ✅ |
| Visible screen area | 110.32 × 62.28 (±0.15) | drawing | ✅ |
| Black border on glass: short ends / long sides | 8.29 / 4.21 | drawing | ✅ |
| Glass overhang past LCD body: short ends / long sides | 1.45 & 6.75 / 3.0 | STEP | 🔍 |
| LCD body under the glass | 118.7 × 64.7 | STEP | 🔍 |
| Glass front → LCD back (metal) | 3.0 | STEP | 🔍 |
| Foam spacer LCD → board | 3.0 | STEP | 🔍 |
| Board | 118.5 × 64.5 × 1.6 | drawing + STEP | ✅ |
| Board position: USB end sits inside glass edge by | 1.55 (other end 6.85) | drawing + STEP | ✅ |
| Board position: long edges inside glass edge by | 3.1 each | STEP | ✅ |
| Glass front → board component face | 7.6 | STEP | 🔍 |
| Tallest part on board (40-pin header) | 5.2 | STEP | 🔍 |
| **Total depth, glass front → top of header** | **12.8** (STEP) vs **11.6** (drawing) | conflict | ❌ measure |
| Standoffs | 4× M2.5 thread, 4.0 tall, Ø5.5, at 112 × 57 centres, 3.25 from board edges | drawing + STEP | ✅ |
| Glass thickness alone | ~1.1–1.3 | STEP (inferred) | ❌ measure |

## Ports, buttons, mics (board frame, centres)

| What | x | y | Notes | Source | Status |
|---|---|---|---|---|---|
| USB-C ×2 | −1.3 (stick out past board edge; 0.2 inside glass edge) | 23.26 and 41.21 | on the **short end**; body 9.6 wide × 3.25 tall above board | drawing + STEP | ✅ |
| Which USB-C is UART vs OTG | | | read the silkscreen | measured | ❌ |
| Buttons ×3 (side-push) | 72.76, 84.44, 96.12 | 65.0 (tip 0.5 past board edge, 2.6 inside glass edge) | on a **long edge**; body 4.55 wide, 1.8 above board | drawing + STEP | ✅ |
| Which button is POWER / BOOT / RESET | | | read the silkscreen | measured | ❌ |
| microSD slot | 51.1–67.2 | opens on the same long edge as the buttons | card direction to confirm | STEP | 🔍 |
| Mic 1 / Mic 2 | 14.55 | 2.37 / 62.17 | 2.65 × 3.5 × 1.0; **no hole in the board under them, so they listen out of the back**, not through the glass side | STEP | 🔍 |
| 40-pin header | 54.6–105.9 | 1.1–7.7 | opposite long edge from buttons | STEP | ✅ |
| 2-pin sockets (battery / speaker / RTC) | J2 at 30.9–38.6, 1.0–6.2 · J5 at 26.3–32.1, 57.9–62.8 · H3 at 112–117, 13.4–16.8 | | which is which: read the silkscreen | STEP | ❌ |
| Wi-Fi antenna (C6 module) keep-out | | | far end from USB, near buttons edge; mark on a photo | drawing | ❌ |

## Battery, accelerometer, speaker

| What | Value | Source | Status |
|---|---|---|---|
| Battery L × W × T incl. protection board | — | measured | ❌ |
| Battery wire exit side + lead length | — | measured | ❌ |
| LIS2DW12 breakout L × W × T, hole Ø and centres | — | measured | ❌ |
| Speaker Ø and depth (if fitted) | — | measured | ❌ |

## LyricFlash (family look)

Source `lyricflash` = `ref/lyricflash-enclosure/lyricflash_print_plates.3mf` (meshes `top_shell.stl`,
`base_smooth.stl`, and the assembly guide inside the file). Meshes, so values are ±0.05.

| What | Value (mm) | Source | Status |
|---|---|---|---|
| Top shell outline | 52.86 × 66.83, 5.15 thick | lyricflash | ✅ |
| Visible bezel width at its narrowest (opening edge → outer edge) | 2.95 | lyricflash | ✅ |
| Lip over the glass: thickness | 1.0 | lyricflash | ✅ |
| Lip over the glass: overlap past the glass pocket edge | 1.35 (opening Ø46.96, glass pocket Ø49.66) | lyricflash | ✅ |
| Gap between lip and glass | 0.25 | lyricflash (assembly guide) | ✅ |
| Screen depth below bezel face | 1.25 (= lip 1.0 + gap 0.25) | lyricflash | ✅ |
| Wall outside the glass pocket | 1.6 | lyricflash | ✅ |
| Outer corner radius | 6.7 | lyricflash | ✅ |
| Outer edge treatment | 1.45 wide × 1.4 high bevel around the front face | lyricflash | ✅ |
| Mic holes | 2× Ø2.0, straight, no chamfer or countersink | lyricflash (mesh + guide) | ✅ |
| Wall thickness at the mic hole | 1.6 | lyricflash | 🔍 hole face sits 2.8 back from the outer wall; confirm it is in a recess |
