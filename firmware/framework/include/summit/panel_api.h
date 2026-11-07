// Panel contract: the scaling seam (ARCHITECTURE.md "Panel contract").
//
// Panels see only PanelContext: state store reads, their own settings, and
// events for topics declared in the manifest (CLAUDE.md hard rule 2).
// Lifecycle: create -> show -> tick... -> hide -> destroy.
// Hidden panels hold no LVGL objects: show() builds them, hide() deletes them.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "summit/settings.h"
#include "summit/value.h"

// LVGL's object type, forward-declared so the framework builds and is tested
// without LVGL but stays type-checked. Matches LVGL 9.3+ (9.2 and older use
// `struct lv_obj_t` and would fail to compile here, which is intended).
struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

namespace summit {

enum SizeClass : uint8_t { kSizeFull = 1, kSizeHalf = 2, kSizeTile = 4 };
enum Orientation : uint8_t { kLandscape = 1, kPortrait = 2 };
constexpr uint8_t kBothOrientations = kLandscape | kPortrait;

// A glance chip in the strip at the top of every panel (ARCHITECTURE.md "Glance strip").
// Fed by a state-store topic: a non-empty string shows icon + text, `true` shows the
// icon only, anything else (missing, empty, false) hides the chip.
enum class ChipSide : uint8_t { Left, Right };

struct GlanceChipDef {
    std::string id;          // unique across all chips, topic-segment syntax
    std::string topic;       // state key with the chip's value
    std::string icon;        // static icon (UTF-8 / LVGL symbol), may be empty
    std::string icon_topic;  // optional state key overriding the icon
    ChipSide side = ChipSide::Left;
    int order = 0;           // left-to-right within its side
    uint8_t drop_rank = 0;   // narrow width: highest rank drops first; 0 = never drops
    std::string panel_id;    // tap target; filled from the manifest that declares it
};

struct PanelManifest {
    std::string id;       // topic-segment syntax, e.g. "home_clock"
    std::string name;
    std::string icon;
    std::string version;
    uint8_t sizes = 0;                       // SizeClass bits
    uint8_t orientations = kBothOrientations;  // must be both (hard rule 3)
    std::vector<std::string> subscriptions;  // topics delivered to on_event while shown
    std::vector<SettingDef> settings;        // keys relative to "panels/<id>/"
    std::vector<GlanceChipDef> glances;      // chips this panel adds to the strip
};

// Size tokens: the only lengths a panel uses (hard rule 3). Derived by the shell
// from the screen's short side, so they are identical in both orientations.
struct SizeTokens {
    int16_t unit = 0;     // one token: ~6% of the short side (44 px on 720); strip height
    int16_t space_s = 0;  // unit / 4
    int16_t space_m = 0;  // unit / 2
    int16_t space_l = 0;  // unit
    int16_t radius = 0;   // unit / 4
    int16_t text_s = 0;   // target font pixel sizes; use the shell's font_for()
    int16_t text_m = 0;
    int16_t text_l = 0;
    int16_t text_xl = 0;
};

// Where the panel is shown: its parent object (the content area below the
// glance strip), the area's size and orientation, and the size tokens.
struct PanelView {
    lv_obj_t *lv_parent = nullptr;
    SizeClass size = kSizeFull;
    Orientation orientation = kLandscape;
    int16_t width = 0;
    int16_t height = 0;
    SizeTokens tokens;
};

class PanelContext {
public:
    virtual ~PanelContext() = default;
    virtual Value state(const std::string &key) const = 0;
    // Own settings only: key is relative to "panels/<id>/".
    virtual Value setting(const std::string &key) const = 0;
    virtual SetResult set_setting(const std::string &key, Value value) = 0;
};

class Panel {
public:
    virtual ~Panel() = default;
    virtual const PanelManifest &manifest() const = 0;
    virtual bool create(PanelContext &ctx) = 0;  // allocate data, no LVGL objects
    virtual void show(PanelContext &ctx, const PanelView &view) = 0;
    virtual void tick(PanelContext &ctx, uint32_t now_ms) = 0;
    virtual void on_event(PanelContext &ctx, const std::string &topic, const Value &value) = 0;
    virtual void hide(PanelContext &ctx) = 0;  // delete every LVGL object
    virtual void destroy(PanelContext &ctx) = 0;
};

bool validate_manifest(const PanelManifest &m, std::string *error);

// Full setting key for a panel setting: "panels/<id>/<key>".
std::string panel_setting_key(const std::string &panel_id, const std::string &key);

}  // namespace summit
