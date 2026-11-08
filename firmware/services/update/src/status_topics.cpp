#include "summit/update/status_topics.h"

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
}

}  // namespace summit::update
