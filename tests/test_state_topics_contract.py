"""Every state/event topic used in code must be listed in ARCHITECTURE.md "State topics".

The table is the public API between services, panels and the shell. Topics follow
domain/item. Settings change events are covered by the `settings/<key>` row.
"""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
FW = ROOT / "firmware"


def documented():
    doc = (ROOT / "docs/ARCHITECTURE.md").read_text()
    section = doc.split("### State topics", 1)[1].split("\n### ", 1)[0]
    return set(re.findall(r"^\| `([^`]+)` \|", section, re.M))


def used():
    found = {}

    def add(path, pattern, flags=0):
        text = re.sub(r"//[^\n]*", "", (FW / path).read_text())  # ignore comments
        for m in re.finditer(pattern, text, flags):
            for t in re.findall(r'"([a-z0-9_]+(?:/[a-z0-9_]+)+)"', m.group(0)):
                found.setdefault(t, path)

    add("shell/include/summit/shell/shell.h", r"namespace topics \{.*?\}", re.S)
    add("shell/src/glance.cpp", r'(?:icon_)?topic = "[^"]+"')
    add("shell/src/shell_ui.cpp", r'(?:subscribe|publish)\("[^"]+"')
    add("shell/src/shell_ui.cpp", r'd_\.state\.(?:get|set)\("[^"]+"')
    add("main/app_main.cpp", r'state\.set\("[^"]+"')
    add("sim/demo_panels.cpp", r'(?:s\.set|ctx\.state|str\(ctx,)\s*\(?"[^"]+"')
    add("sim/demo_panels.cpp", r'chip\("[^"]+", "[^"]+"')
    add("sim/demo_panels.cpp", r'manifest\("[^"]+", "[^"]+", \{[^}]*\}\)')
    # Update engine, Updates screen and the simulator's fake service: every topic-shaped literal.
    for path in ("services/update/src/status_topics.cpp", "services/update/esp/update_service.cpp",
                 "services/update/esp/ota_guard.cpp", "shell/src/updates_screen.cpp", "sim/fake_update.cpp"):
        add(path, r'"[a-z0-9_]+(?:/[a-z0-9_]+)+"')
    return found


def settings_keys():
    """Keys of the settings schema: read via Settings, not topics (their change events are settings/<key>)."""
    text = (FW / "framework/src/system_settings.cpp").read_text()
    return set(re.findall(r'\{"([a-z0-9_]+(?:/[a-z0-9_]+)+)",', text))


def main():
    doc = documented()
    skip = settings_keys()
    assert doc, "State topics table not found in docs/ARCHITECTURE.md"
    for t in doc:
        assert t == "settings/<key>" or re.fullmatch(r"[a-z0-9_]+/[a-z0-9_]+", t), f"bad topic name: {t}"
    missing = []
    for topic, where in sorted(used().items()):
        if topic in skip:
            continue
        if topic.startswith("settings/"):
            if "settings/<key>" not in doc and topic not in doc:
                missing.append((topic, where))
        elif topic not in doc:
            missing.append((topic, where))
    assert not missing, "topics used in code but missing from ARCHITECTURE.md State topics:\n" + "\n".join(
        f"  {t}  ({w})" for t, w in missing)
    print(f"PASS: {len(used())} topics used in code are documented ({len(doc)} rows)")


if __name__ == "__main__":
    main()
