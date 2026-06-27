#pragma once

#include <type_traits>

template <typename Enum, typename... Flags>
    requires(std::is_enum_v<Enum> && (std::is_same_v<Enum, Flags> && ...))
force_inline auto has_flag(Enum value, Flags... flags) -> bool {
    if constexpr (sizeof...(Flags) == 0) {
        return false;
    }

    using Underlying = std::underlying_type_t<Enum>;
    const Underlying mask = (static_cast<Underlying>(flags) | ...);
    return (static_cast<Underlying>(value) & mask) == mask;
}
