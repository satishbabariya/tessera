#ifndef TESSERA_SYNC_CLOCK_HPP
#define TESSERA_SYNC_CLOCK_HPP

#include <chrono>

namespace tessera {
namespace sync {

class Clock {
public:
    using clock = std::chrono::system_clock;
    using time_point = clock::time_point;
    using duration = clock::duration;

    virtual ~Clock() {}

    /// Implementation must be thread-safe.
    virtual time_point now() const noexcept = 0;
};

} // namespace sync
} // namespace tessera

#endif // TESSERA_SYNC_CLOCK_HPP
