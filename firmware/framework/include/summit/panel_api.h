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

struct PanelManifest {
    std::string id;       // topic-segment syntax, e.g. "home_clock"
    std::string name;
    std::string icon;
    std::string version;
    uint8_t sizes = 0;                       // SizeClass bits
    uint8_t orientations = kBothOrientations;  // must be both (hard rule 3)
    std::vector<std::string> subscriptions;  // topics delivered to on_event while shown
    std::vector<SettingDef> settings;        // keys relative to "panels/<id>/"
};

// Where and how big the panel is shown. Layout uses size tokens from the shell
// (step 4), never pixels.
struct PanelView {
    lv_obj_t *lv_parent = nullptr;
    SizeClass size = kSizeFull;
    Orientation orientation = kLandscape;
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
