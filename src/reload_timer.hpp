#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dualsp {
// The engine field is elapsed time, not a normalized progress value. Keep
// signed offsets (a delayed start) and completed-timer overshoot in the cache.
// Only the HUD normalizes to [0,1]. Never reset a signed timer to zero, as doing
// so would shorten a cooldown. Non-finite values still stop native mutations.
inline void validateReload(float duration, float elapsed) {
    if (!std::isfinite(duration) || duration < 0 || duration > 300)
        throw std::runtime_error("Invalid reload duration");
    if (!std::isfinite(elapsed))
        throw std::runtime_error("Non-finite reload elapsed time");
}
inline float advanceReload(float duration, float elapsed, double dt, bool inLoadWindow) {
    validateReload(duration, elapsed);
    if (!std::isfinite(dt) || dt < 0) throw std::runtime_error("Invalid reload time delta");
    return inLoadWindow ? static_cast<float>(std::min<double>(duration, double(elapsed) + dt)) : elapsed;
}
inline float rebuildReload(float duration, float elapsed, float rebuiltElapsed) {
    // Crossing the threshold in a native tick creates the loaded projectile.
    if (duration > 0 && elapsed >= duration && rebuiltElapsed < duration)
        return std::nextafter(duration, 0.0f);
    return elapsed;
}
inline float reloadProgress(float duration, float elapsed) {
    validateReload(duration, elapsed);
    return duration > 0 ? std::clamp(elapsed / duration, 0.0f, 1.0f) : 1.0f;
}
}
