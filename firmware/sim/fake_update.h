// Simulator stand-in for the device update service: the REAL UpdateEngine with a
// fake flash writer, a fixture catalog and generated images (correct headers and
// SHA-256). "Downloads" advance a chunk per tick. Commands and published state
// match the device (update/command, update/* topics).
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "summit/event_bus.h"
#include "summit/settings.h"
#include "summit/state_store.h"
#include "summit/update/engine.h"

namespace sim {

class FakeUpdateService {
public:
    FakeUpdateService(summit::EventBus &bus, summit::StateStore &state, summit::Settings &settings,
                      const std::string &version);
    ~FakeUpdateService();
    void tick(uint32_t now_ms);
    summit::update::UpdateEngine &engine() { return *engine_; }

private:
    struct Writer;
    struct Hash;
    summit::EventBus &bus_;
    summit::StateStore &state_;
    summit::Settings &settings_;
    std::unique_ptr<Writer> writer_;
    std::unique_ptr<Hash> hash_;
    std::unique_ptr<summit::update::UpdateEngine> engine_;
    std::vector<std::vector<uint8_t>> images_;
    summit::update::Catalog catalog_;
    summit::EventBus::SubscriptionId sub_ = 0, channel_sub_ = 0;
    uint32_t now_ = 0, check_due_ = 0;
    bool checking_ = false;
    const std::vector<uint8_t> *download_ = nullptr;
    size_t offset_ = 0;
};

}  // namespace sim
