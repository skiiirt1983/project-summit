#include "summit/value.h"

namespace summit {

bool valid_topic(const std::string &topic)
{
    if (topic.empty() || topic.size() > 64) return false;
    bool segment_empty = true;
    for (char c : topic) {
        if (c == '/') {
            if (segment_empty) return false;
            segment_empty = true;
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
            segment_empty = false;
        } else {
            return false;
        }
    }
    return !segment_empty;
}

}  // namespace summit
