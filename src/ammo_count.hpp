#pragma once
#include <cstdint>
#include <limits>

namespace dualsp {
// 仅用于原生有符号32位弹药计数：负数为无限、零为空弹、正数为有限库存。
// 数组长度、装填窗口、配置/武器编号等字段仍使用各自的非负边界检查。
inline constexpr bool validAmmoCount(std::int64_t value) {
    return value>=std::numeric_limits<std::int32_t>::min() && value<=100000;
}
}
