#include "summit/update/status_topics.h"

#include "summit/json_writer.h"

namespace summit::update {

void publish_status(const UpdateEngine &engine, StateStore &st)
{
    const Status &s = engine.status();
    st.set("update/stage", std::string(stage_name(s.stage)));
    st.set("update/progress_pct", s.total > 0 ? int64_t{s.bytes * 100 / s.total} : int64_t{0});
    st.set("update/error", s.error);
    st.set("update/in_progress", engine.busy());
    st.set("update/target_version", s.target_version);
    const Candidate *a = engine.available();
    st.set("update/available", a != nullptr);
    st.set("update/available_version", a ? a->build.version : std::string());
    st.set("update/notes", a ? a->build.notes : std::string());

    // Every catalog build for this device's view, with the reason it can't be installed.
    JsonWriter w;
    w.begin_array();
    for (const Candidate &c : engine.candidates()) {
        if (c.reject == Reject::WrongHardware) continue;  // other boards / chip revisions: not shown
        w.begin_object().key("version").value(c.build.version).key("channel").value(c.build.channel);
        w.key("released").value(c.build.released).key("notes").value(c.build.notes);
        w.key("size").value(c.build.size).key("installable").value(c.reject == Reject::None);
        w.key("reason").value(std::string(reject_reason(c.reject))).end_object();
    }
    w.end_array();
    st.set("update/catalog", w.str());
}

}  // namespace summit::update
