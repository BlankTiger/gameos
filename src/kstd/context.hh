#pragma once

#include <utility>

#include "basic.hh"

#if HOSTED
#include <thread>
#include <tuple>
#endif

namespace mem {
struct Allocator;
}

namespace kstd {

struct Context {
    mem::Allocator* global_allocator;
    mem::Allocator* temporary_allocator;
};

inline thread_local Context context{};

#if HOSTED

template <typename F, typename... Args>
auto spawn_thread(F&& function, Args&&... args) -> std::thread {
    Context inherited_context = context;

    return std::thread(
        [inherited_context,
         function = std::forward<F>(function),
         args = std::make_tuple(std::forward<Args>(args)...)
        ] mutable {
            context = inherited_context;
            std::apply(std::move(function), std::move(args));
        }
    );
}

#endif

} // namespace kstd
