"""Guards the fixed 32 MB partition table (CLAUDE.md hard rule 1). Runs without ESP-IDF."""
import csv
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
CSV = ROOT / "firmware" / "partitions.csv"
FLASH = 32 * 1024 * 1024
PART_TABLE_END = 0x9000

# (type, subtype, offset, size) — frozen at v0.1. Do not edit.
EXPECTED = {
    "nvs":      ("data", "nvs",      0x9000,    0x100000),
    "otadata":  ("data", "ota",      0x109000,  0x2000),
    "phy_init": ("data", "phy",      0x10B000,  0x1000),
    "coredump": ("data", "coredump", 0x10C000,  0x10000),
    "factory":  ("app",  "factory",  0x120000,  0x300000),
    "ota_0":    ("app",  "ota_0",    0x420000,  0x800000),
    "ota_1":    ("app",  "ota_1",    0xC20000,  0x800000),
    "assets":   ("data", "littlefs", 0x1420000, 0xBE0000),
}
MB = 1024 * 1024


def read_csv():
    rows = {}
    with CSV.open() as f:
        lines = (l for l in f if l.strip() and not l.lstrip().startswith("#"))
        for p in csv.reader(lines):
            p = [x.strip() for x in p]
            rows[p[0]] = (p[1], p[2], int(p[3], 0), int(p[4], 0))
    return rows


def main():
    rows = read_csv()
    assert rows == EXPECTED, f"partitions.csv changed:\n{rows}\n!=\n{EXPECTED}"

    spans = sorted((o, o + s, n) for n, (_, _, o, s) in rows.items())
    assert spans[0][0] >= PART_TABLE_END, "overlaps bootloader/partition table"
    for (_, end, a), (start, _, b) in zip(spans, spans[1:]):
        assert end <= start, f"{a} overlaps {b}"
    assert spans[-1][1] == FLASH, f"flash not fully used: ends at {spans[-1][1]:#x}"

    for n, (t, _, o, s) in rows.items():
        align = 0x10000 if t == "app" else 0x1000
        assert o % align == 0 and s % 0x1000 == 0, f"{n} misaligned"

    # Sizes from docs/ARCHITECTURE.md
    assert rows["nvs"][3] == 1 * MB and rows["factory"][3] == 3 * MB
    assert rows["ota_0"][3] == rows["ota_1"][3] == 8 * MB
    assert rows["otadata"][3] + rows["phy_init"][3] == 12 * 1024
    assert rows["coredump"][3] == 64 * 1024
    print("PASS: partition table matches the fixed v0.1 layout; 32 MB fully allocated")


if __name__ == "__main__":
    main()
