#pragma once
// Compatibility helpers for older compilers (GCC < 7)

template<typename T>
inline T clampVal(T val, T lo, T hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}
