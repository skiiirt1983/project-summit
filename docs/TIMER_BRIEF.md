# Brief for the design doc: system Timer, Stopwatch and Alarm (2026-11-24)

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
