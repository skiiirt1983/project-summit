# Brief for the design doc: system Timer, Stopwatch and Alarm (2026-11-24, with Paul's answers)

Not a design. Paul's feedback from the alpha.34/35 audio tests, written down so it can go into the Claude design doc, where design decisions are made (CLAUDE.md rule 10). Nothing here is built. Answer the questions at the end, then the build can be planned.

## What Paul asked for
- **A system timer, not a recipe timer.** Today a timer belongs to a recipe step and can only be found and stopped inside that recipe. It should be a system thing; the recipe keeps a link that starts or shows it.
- **A place on screen.** A tile on the Home screen, or on the Calendar page with its own section. Big and visual, in the spirit of the printer status page but newer and fancier: something impressive to count the time down. "This is supposed to be a tech-forward device."
- **Timer, Stopwatch and Alarm together** in one space, very visual, with a menu of timer settings.
- **Easy to stop.** Today the alarm is very hard to reach and to turn off (the only toggle is inside the recipe). Wanted: a new toggle in the pull-down menu, or simply tapping the ringing text there; "more ways to stop a timer".
- **Selectable sounds.** Default system alarm sounds and timer sounds the user picks from. Theme: alpine ski summit, but still standard: think kitchen timer noises and silly ringing noises. "Needs to feel like very personalised attention to detail."

## What exists today (so the design starts from facts)
- `firmware/services/timers` (host-tested): up to 8 timers, name, label from a recipe step, `timers/ringing` flag, rings until stopped or 1 minute (OPEN_QUESTIONS R5), kept across deep sleep (D-012), blocks deep sleep while ringing and counts as running for the update guard (D-011).
- The recipe panel shows timers on its own screen and dismisses them (`kCancelTimer`, `kDismissTimers`).
- Audio (`firmware/services/audio`): one alarm sound, the cowbell, repeating while `timers/ringing`; never below 40% volume; ignores do-not-disturb; stops within a fraction of a second once dismissed. Sounds are built into the firmware as 8-bit mu-law at 16 kHz (`tools/make_sounds.py`, about 150 KB for the 9 s cowbell). Flash and RAM for more sounds are a budget question.
- Paul's sound rules still stand: the device is quiet and carefree; only critical things and alarms make a sound.

## Questions the design has to answer
1. Where does it live: a Home tile, a Calendar section, or its own page? What does the tile show when nothing is running?
2. Timer, Stopwatch and Alarm: one panel with three modes? Does the Alarm (a clock time) join the same service as timers, and does it survive deep sleep and power loss (needs the RTC, HARDWARE_TODO)?
3. The countdown visual: what is it (ring, mountain/summit climb, big digits)? It must run smoothly on the 720x1280 display without hurting frame rate (UI load was a problem before), in both orientations, and work with size tokens.
4. Stopping: tap anywhere on the ringing tile, a toggle in quick settings, the physical button, the double-tap when the accelerometer exists. Which, and is snooze wanted?
5. Sounds: how many, which names, and where do they come from (Paul makes or licenses them; licences must be GPL-compatible or free to ship)? Separate lists for timer and alarm? A per-timer sound? A rising volume while it rings?
6. Are timer settings (default sound, volume floor, ring time limit, snooze) part of the existing settings schema? New keys with defaults need no version bump (rule 4).
7. Alarm minimum volume (currently 40%, OPEN_QUESTIONS): should a timer ever be silent at volume 0?
8. Sound storage: more clips than flash allows built in means loading from the SD card, but the product must not depend on an SD card (Paul's rule). Built-in default set plus optional extras on the card?

## Constraints the design must respect
Panels talk only to the framework (state store, event bus, settings); size tokens, never hard-coded pixels; works at 1280x720 and 720x1280; new state topics, bus events and settings keys are listed in `docs/ARCHITECTURE.md`. Hardware proof is separate from a green build.

## Paul's answers (2026-11-24, evening): the screen, as he described it
**Entry.** The clock on the Home screen is a button. Tapping it opens the clock page.

**Clock page**, four areas, each tappable (tap = see or change settings, or start):
- top left: Clock
- top right: Alarm clock (shows the alarm time when one is set)
- bottom left: Timer (shows the running timer, if any)
- bottom right: Stopwatch

**Timer ending: the countdown overlay.** For the last 10 seconds a semi-transparent box covers most of the screen, with the page blurred behind it, counting down from 10. Touching the screen before it finishes opens the specific timer page, where the timer can be stopped. When it finishes it rings; touching the screen then disables it.

**Status tray.** A running timer shows in the top menu strip like a system tray item; tapping it opens the same timer page. Same for alarms: setting an alarm adds an item with its time to the top menu; tapping it opens the same page.

**Stopwatch.** Independent of timers and counts up. A Lap button records a lap and keeps the stopwatch running, logging each lap time and the total time.

**Sounds and storage.** Paul asked what the sound-storage question meant: the SD card is optional (recipes are a module), so sounds must never need it. They do not have to: the flash layout already has an `assets` LittleFS partition of about 11 MB (`docs/ARCHITECTURE.md` partition table, "fonts, sounds, web extras") and the firmware image itself has room. Proposed answer for the design doc to confirm: the default sound set ships inside the firmware or the assets partition (no SD card needed, same on every device); an SD card could later hold extra user sounds, but nothing depends on it. At 16 kHz 8-bit a 5 s clip is about 80 KB, so a set of ten to fifteen sounds is about 1 MB.

## Paul's answers, round 2 (2026-11-24)
- **Blur: yes.** Frosted look from a darkened, downscaled snapshot of the page under the box.
- **Several timers:** named Timer 1, Timer 2, Timer 3 and so on; each also gets its own item in the top tray.
- **Alarm options: yes.** Repeat days and snooze are wanted.
- **Stopwatch:** laps are kept until the stopwatch is reset (so they survive leaving the page).
- **Sounds.** The cowbell (cut to 9 s to loop) stays the Summit alarm; `summit warning hit` is the sound for all notifications; `summit usb plug-in` is for plugging in and for disconnecting USB (disconnect = the same clip played in reverse). A free, attribution-free library is attached, each file named for its use. Source files are in `firmware/services/audio/assets/library/` (not built in yet):

| File | Length | Use |
| --- | --- | --- |
| alarm-summit-cowbell | 11.0 s (the shipped alarm is cut to 9.3 s) | alarm (default, already built in) |
| alarm-beep-beep-beep | 8.6 s | alarm |
| alarm-hi-low | 13.5 s | alarm |
| alarm-old_timey | 5.8 s | alarm |
| alarm-qwarky | 3.5 s | alarm |
| alarm-ytv | 6.5 s | alarm |
| timer-kitchen | 30.2 s | timer |
| timer-airplane | 6.1 s | timer |
| timer-marimba | 4.5 s | timer |
| timer-levelup | 3.6 s | timer |
| timer-chisper | 0.7 s | timer |

Size if all are built in as 16 kHz 8-bit mu-law like the existing sounds: about 1.5 MB (the 30 s kitchen timer alone is 0.5 MB), on top of the 0.23 MB used now. Every OTA image grows by that much (the update download is about 3.6 MB today). Licence and source of each file must be written into `THIRD_PARTY_NOTICES.md` before the public release (GPL-3.0 project).

## Paul's answers, round 3 (2026-11-24)
- **Notifications and sound:** Paul prefers no toasts at all: only critical system errors or things the user should be warned about. Where a toast has to exist, it may use the warning hit. (So today's rule stands: only critical toasts play it; the design should keep ordinary toasts to a minimum.)
- **Tray items:** a timer opens the timer page, an alarm opens the alarm page.
- **Defaults:** default timer sound is the kitchen timer; default alarm is the cowbell. Paul will supply better prepared (shorter) files; the library sources in `assets/library/` are placeholders until then. Do not build them in before his files arrive.
- **Countdown overlay:** applies to timers and alarms (both can be used for recipes).
- **Licence:** Paul points to Pixabay's content licence summary (https://pixabay.com/service/license-summary/). Needs a check for this project: Pixabay's licence allows free use without attribution, but as far as I know it forbids redistributing the files on their own, and it is not an open-source licence. This repo is public GPL-3.0 and ships the sounds in the source tree and firmware image, so decide before the public release whether that is acceptable, or whether the sounds should be CC0/GPL-compatible instead, or kept out of the public repo. This is not legal advice.

## Still open
- Alarm across power loss needs the RTC (HARDWARE_TODO).
- Several timers in the tray: one item each (Paul), the exact look when there are many (more than about 3) is for the design.
- Paul's trimmed sound files (kitchen timer, cowbell) and the licence decision above.
