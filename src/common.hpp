#ifndef CORRIDOR_COMMON_HPP
#define CORRIDOR_COMMON_HPP
#include <chrono>

namespace corridor
{
    using timestamp_type = uint64_t;
    using time_precision = std::chrono::nanoseconds;
    using index_type = uint64_t;
    using viwer_id_type = uint32_t;

    template <typename T>
    inline int64_t get_timestamp()
    {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<T>(now).count();
    }
}
#endif