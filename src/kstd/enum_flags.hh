#pragma once

#include <type_traits>

#include "basic.hh"

template <typename Enum, typename... Flags>
    requires(std::is_enum_v<Enum> && (std::is_same_v<Enum, Flags> && ...))
force_inline auto has_flag(Enum value, Flags... flags) -> bool {
    static_assert(sizeof...(Flags) > 0);

    using Underlying = std::underlying_type_t<Enum>;
    const Underlying mask = (cast(Underlying)flags | ...);
    return (cast(Underlying)value & mask) == mask;
}
