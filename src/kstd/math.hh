#pragma once

namespace math {

force_inline auto abs(u32 x) -> u32 {
    return (x ^ (x >> 31)) - (x >> 31);
}

force_inline auto lerp(u8 a, u8 b, int pos, int max) -> u8 {
    return a + ((b - a) * pos) / max;
}

struct Rect {
    u32 x1, x2;
    u32 y1, y2;

    static force_inline auto create(u32 x, u32 y, u32 w, u32 h) -> Rect {
        return {
            .x1 = x,
            .x2 = x + w,
            .y1 = y,
            .y2 = y + h,
        };
    }

    force_inline void clip(u32 screen_width, u32 screen_height) {
        x1 = (x1 >= screen_width) ? screen_width - 1 : x1;
        x2 = (x2 >= screen_width) ? screen_width - 1 : x2;
        y1 = (y1 >= screen_height) ? screen_height - 1 : y1;
        y2 = (y2 >= screen_height) ? screen_height - 1 : y2;
    }
};

}  // namespace math
