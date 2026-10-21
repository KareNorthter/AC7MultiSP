#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dualsp {
// 计时不是百分比。-1为停用、-2为等待原生实体，不能用离开时长自动推进。
// 其他有符号偏移和完成后的超量值仍保留；HUD才归一化到[0,1]。
inline void validateReload(float duration, float elapsed) {
    if (!std::isfinite(duration) || duration < 0 || duration > 300)
        throw std::runtime_error("Invalid reload duration");
    if (!std::isfinite(elapsed))
        throw std::runtime_error("Non-finite reload elapsed time");
}
inline float advanceReload(float duration, float elapsed, double dt, bool inLoadWindow) {
    validateReload(duration, elapsed);
    if (!std::isfinite(dt) || dt < 0) throw std::runtime_error("Invalid reload time delta");
    if (elapsed == -1.f || elapsed == -2.f) return elapsed;
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
