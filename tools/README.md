# tools

Build and release helpers (used by CI; all host-tested in `tests/`).

- `summit_image.py`: read-only image checks (chip ID, chip-revision variant, project, version, ota_0 headroom, partition table, otadata selects ota_0, factory left empty).
- `make_flash_images.py`: writes the otadata that selects ota_0 and prints the `esptool merge_bin` layout (app in ota_0).
- `ci_build.sh`: builds one variant (`rev3` or `rev1`) and merges the full USB image.
- `package_firmware.py`: validates a build and packages OTA + full images, otadata, ESP Web Tools manifests (full install and keep-settings), checksums, build-info, zip.
- `catalog.py`: builds update catalog entries from `build-info.json` and validates a catalog with the device's rules.
