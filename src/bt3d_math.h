#ifndef BT3D_MATH_H
#define BT3D_MATH_H

#define ARRAY_COUNT(values) ((int)(sizeof(values) / sizeof((values)[0])))

static inline int bt3d_min_i(int a, int b) {
    return a < b ? a : b;
}

static inline int bt3d_max_i(int a, int b) {
    return a > b ? a : b;
}

static inline int bt3d_clamp_i(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static inline float bt3d_clamp_f(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static inline float bt3d_clamp01(float value) {
    return bt3d_clamp_f(value, 0.0f, 1.0f);
}

#endif
