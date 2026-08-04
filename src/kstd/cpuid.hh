#pragma once

#include "basic.hh"

namespace cpu {

// Intel SDM Vol. 2 / AMD APM Vol. 3: EAX leaf selectors for CPUID.
enum struct Cpuid_Leaf : u32 {
    VENDOR_AND_MAX_BASIC           = 0x00000000,
    FEATURE_BITS                   = 0x00000001,
    CACHE_PARAMETERS               = 0x00000004,
    STRUCTURED_EXTENDED_FEATURES   = 0x00000007,
    MAX_EXTENDED                   = 0x80000000,
    EXTENDED_FEATURE_BITS          = 0x80000001,
    EXTENDED_L2_CACHE_FEATURES     = 0x80000006,
    ADVANCED_POWER_MANAGEMENT      = 0x80000007,
};

// ECX subleaf. Only some leaves read it (7, 4, 0xB, 0xD, ...).
// Leaf 7: subleaf 0 = main feature bits in EBX/ECX/EDX; higher = more.
enum struct Cpuid_Subleaf : u32 {
    NONE = 0,
    STRUCTURED_EXTENDED_FEATURES_MAIN = 0,
};

struct Cpuid_Result {
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
};

struct Features {
    bool fpu;
    bool tsc;
    bool apic;
    bool x2apic;
    bool fxsr;
    bool sse;
    bool sse2;
    bool xsave;
    bool avx;
    bool fsgsbase;
    bool nx;
    bool rdrand;
    bool invariant_tsc;
    u32  max_basic_leaf;
    u32  max_extended_leaf;
    u32  initial_apic_id;
    u32  cache_line_size;
    char vendor[13];
};

namespace hidden {
    inline Features features {};
}

force_inline auto cpuid(u32 leaf, u32 subleaf = 0) -> Cpuid_Result {
    Cpuid_Result r;
    asm volatile("cpuid"
                 : "=a"(r.eax), "=b"(r.ebx), "=c"(r.ecx), "=d"(r.edx)
                 : "a"(leaf), "c"(subleaf));
    return r;
}

force_inline auto cpuid(Cpuid_Leaf leaf, Cpuid_Subleaf subleaf = Cpuid_Subleaf::NONE) -> Cpuid_Result {
    return cpuid(static_cast<u32>(leaf), static_cast<u32>(subleaf));
}

force_inline auto bit(u32 value, u32 n) -> bool {
    return (value >> n) & 1;
}

inline auto detect_features() -> void {
    using enum Cpuid_Leaf;

    Features f {};

    const auto leaf0 = cpuid(VENDOR_AND_MAX_BASIC);
    f.max_basic_leaf = leaf0.eax;

    // Vendor string is packed EBX, EDX, ECX.
    const u32 parts[3] = { leaf0.ebx, leaf0.edx, leaf0.ecx };
    for (usize i = 0; i < 3; i++) {
        f.vendor[i * 4 + 0] = static_cast<char>( parts[i]        & 0xFF);
        f.vendor[i * 4 + 1] = static_cast<char>((parts[i] >>  8) & 0xFF);
        f.vendor[i * 4 + 2] = static_cast<char>((parts[i] >> 16) & 0xFF);
        f.vendor[i * 4 + 3] = static_cast<char>((parts[i] >> 24) & 0xFF);
    }
    f.vendor[12] = '\0';

    if (f.max_basic_leaf >= static_cast<u32>(FEATURE_BITS)) {
        const auto leaf1 = cpuid(FEATURE_BITS);
        f.fpu             = bit(leaf1.edx, 0);
        f.tsc             = bit(leaf1.edx, 4);
        f.apic            = bit(leaf1.edx, 9);
        f.fxsr            = bit(leaf1.edx, 24);
        f.sse             = bit(leaf1.edx, 25);
        f.sse2            = bit(leaf1.edx, 26);
        f.x2apic          = bit(leaf1.ecx, 21);
        f.xsave           = bit(leaf1.ecx, 26);
        f.avx             = bit(leaf1.ecx, 28);
        f.rdrand          = bit(leaf1.ecx, 30);
        f.initial_apic_id = (leaf1.ebx >> 24) & 0xFF;
    }

    if (f.max_basic_leaf >= static_cast<u32>(STRUCTURED_EXTENDED_FEATURES)) {
        const auto leaf7 = cpuid(
            STRUCTURED_EXTENDED_FEATURES,
            Cpuid_Subleaf::STRUCTURED_EXTENDED_FEATURES_MAIN
        );
        f.fsgsbase = bit(leaf7.ebx, 0);
    }

    // Leaf 4: EBX[11:0]+1 = system coherency line size. Walk subleaves until type=0.
    if (f.max_basic_leaf >= static_cast<u32>(CACHE_PARAMETERS)) {
        for (u32 i = 0; ; i++) {
            const auto c = cpuid(static_cast<u32>(CACHE_PARAMETERS), i);
            const u32 type = c.eax & 0x1F;
            if (type == 0) {
                break;
            }
            f.cache_line_size = (c.ebx & 0xFFF) + 1;
            break;
        }
    }

    const auto ext0 = cpuid(MAX_EXTENDED);
    f.max_extended_leaf = ext0.eax;

    if (f.max_extended_leaf >= static_cast<u32>(EXTENDED_FEATURE_BITS)) {
        const auto ext1 = cpuid(EXTENDED_FEATURE_BITS);
        f.nx = bit(ext1.edx, 20);
    }

    // Leaf 0x80000006: ECX[7:0] = L2 cache line size. Fallback if leaf 4 missing.
    if (f.cache_line_size == 0 && f.max_extended_leaf >= static_cast<u32>(EXTENDED_L2_CACHE_FEATURES)) {
        const auto l2 = cpuid(EXTENDED_L2_CACHE_FEATURES);
        f.cache_line_size = l2.ecx & 0xFF;
    }

    if (f.cache_line_size == 0) {
        f.cache_line_size = 64;
    }

    if (f.max_extended_leaf >= static_cast<u32>(ADVANCED_POWER_MANAGEMENT)) {
        const auto ext7 = cpuid(ADVANCED_POWER_MANAGEMENT);
        f.invariant_tsc = bit(ext7.edx, 8);
    }

    hidden::features = f;
}

force_inline auto features() -> const Features& {
    return hidden::features;
}

force_inline auto initial_apic_id() -> u32 {
    return hidden::features.initial_apic_id;
}

}
