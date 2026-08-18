#pragma once

#include <utility>

#include "basic.hh"

#if HOSTED
#include <thread>
#include <tuple>
#endif

namespace mem {
struct Allocator;

#if HOSTED
auto create_thread_temporary_allocator(Allocator* allocator, Allocator* inherited_allocator) -> Allocator*;
auto destroy_thread_temporary_allocator(Allocator* allocator) -> void;
#endif
}

namespace kstd {

struct Context {
    mem::Allocator* allocator;
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
            auto* temporary_allocator = mem::create_thread_temporary_allocator(
                inherited_context.allocator,
                inherited_context.temporary_allocator
            );
            context = inherited_context;
            context.temporary_allocator = temporary_allocator;
            std::apply(std::move(function), std::move(args));
            mem::destroy_thread_temporary_allocator(temporary_allocator);
        }
    );
}

#endif

} // namespace kstd
