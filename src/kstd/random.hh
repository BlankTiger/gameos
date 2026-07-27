#pragma once

#include "kstd/assert.hh"
#include "kstd/basic.hh"

namespace rand {

// Adapted from the code included on Sebastiano Vigna's website
struct Xoshiro_256pp_State {
    u64 s[4];
};

force_inline u64 rol64(u64 x, int k) {
    return (x << k) | (x >> (64 - k));
}

u64 xoshiro256pp(Xoshiro_256pp_State* state) {
	u64* s = state->s;
	const u64 result = rol64(s[0] + s[3], 23) + s[0];
	const u64 t = s[1] << 17;

	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];

	s[2] ^= t;
	s[3] = rol64(s[3], 45);

	return result;
}

inline Xoshiro_256pp_State xoshiro256pp_state;

auto initialize() -> void {
    // @TODO: Get random seeds here later.
    xoshiro256pp_state = { 67, 420, 2137, 1337 };
}

force_inline auto generate() -> u64 {
    return xoshiro256pp(&xoshiro256pp_state);
}

// [from; to)
force_inline auto generate(u64 from, u64 to) -> u64 {
    kstd_assert(from < to);

    const u64 range = to - from;
    const u64 limit = U64_MAX - (U64_MAX % range);

    u64 random;
    do {
        random = xoshiro256pp(&xoshiro256pp_state);
    } while (random >= limit);

    return from + (random % range);
}

}
