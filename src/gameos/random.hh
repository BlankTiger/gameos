#pragma once

#include "kstd/assert.hh"
#include "kstd/basic.hh"

#include "gameos/cpuid.hh"

namespace krand {

// Adapted from the code included on Sebastiano Vigna's website
struct Xoshiro_256pp_State {
    u64 s[4];
};

force_inline auto rol64(u64 x, int k) -> u64 {
    return (x << k) | (x >> (64 - k));
}

auto xoshiro256pp(Xoshiro_256pp_State* state) -> u64 {
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

// Vigna's recommended way to turn a single seed into well-distributed state.
force_inline auto splitmix64(u64* state) -> u64 {
    u64 z = (*state += 0x9E3779B97F4A7C15);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EB;
    return z ^ (z >> 31);
}

force_inline auto rdtsc() -> u64 {
    u32 lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (u64(hi) << 32) | lo;
}

force_inline auto cpu_has_rdrand() -> bool {
    return cpu::features().rdrand;
}

// Intel's recommended retry loop; a handful of failures in a row means the
// hardware RNG is (temporarily) out of entropy, not that it's unsupported.
force_inline auto rdrand32(u32* out) -> bool {
    for (int attempt = 0; attempt < 10; attempt++) {
        u8 ok;
        asm volatile("rdrand %0; setc %1" : "=r"(*out), "=qm"(ok));
        if (ok) return true;
    }
    return false;
}

force_inline auto hardware_seed() -> u64 {
    if (cpu_has_rdrand()) {
        u32 lo, hi;
        if (rdrand32(&lo) && rdrand32(&hi)) {
            return (u64(hi) << 32) | lo;
        }
    }

    // No RDRAND (or it's out of entropy): fall back to the timestamp
    // counter, which by this point has accumulated jitter from every
    // interrupt handled during boot (PIT, PS/2, ...).
    return rdtsc();
}

auto initialize() -> void {
    u64 seed = hardware_seed();

    xoshiro256pp_state.s[0] = splitmix64(&seed);
    xoshiro256pp_state.s[1] = splitmix64(&seed);
    xoshiro256pp_state.s[2] = splitmix64(&seed);
    xoshiro256pp_state.s[3] = splitmix64(&seed);
}

force_inline auto generate() -> u64 {
    return xoshiro256pp(&xoshiro256pp_state);
}

// [from; to)
force_inline auto generate(u64 from, u64 to) -> u64 {
    kstd_assert(from < to);

    const u64 range     = to - from;
    const u64 threshold = -range % range;

    u64 random;
    do {
        random = xoshiro256pp(&xoshiro256pp_state);
    } while (random < threshold);

    return from + (random % range);
}

}
