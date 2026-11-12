# Open questions and defaults chosen (for Paul to review in batches)

Each entry: the question, the choice made, why. Newest first. Say if any should change.

## R1 — Modules

- **D-007 item 4 (web UI pairing lock) is still in DECISIONS.md, but Paul said in chat (2026-11-08) it was pasted by mistake and to remove it.** Choice: not built; ARCHITECTURE.md says "no login". Please delete D-007.4 or confirm you want it after all.
- **"Delete recipes" on Remove.** D-008 specifies it, D-009 says stop and ask for anything that deletes user data. Choice: Remove only hides the module and keeps all data; the delete option is not built yet. It needs your go-ahead (proposed: separate button, confirm dialog naming the recipe count, device + web UI).
- **How to reach the Modules screen on the device.** Choice: a "Modules" button (next to "Updates") in the quick-settings sheet, and `ui/open_screen` = `settings/modules`. Why: there is no settings menu yet; quick settings is the only system surface.
- **Where an installed module's page goes.** Choice: appended after the existing pages unless `ui/page_order` lists it. Why: matches the existing page-order rule.
- **SD card state topics.** Choice: `storage/sd_present` (bool) and `storage/sd_free_mb` (int), published by the storage service. Install re-checks requirements at the moment of install only; a module stays installed if the card is removed later (panel shows "Insert the SD card", per D-008).
- **Module enabled flag key.** Choice: `modules/<id>/installed` (bool, default false), additive, no schema bump.
