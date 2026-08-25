#pragma once

#include <thread>
#include <tuple>
#include <utility>

#include "kstd/allocator.hh"
#include "kstd/context.hh"

namespace kstd {

template <typename F, typename... Args>
auto spawn_thread(F&& function, Args&&... args) -> std::thread {
    Context inherited_context = context;

    return std::thread(
        [inherited_context,
         function = std::forward<F>(function),
         args = std::make_tuple(std::forward<Args>(args)...)
        ] mutable {
            context = inherited_context;

            void* temporary_storage = nullptr;
            Temporary_Allocator_State temporary_state{};
            if (inherited_context.temporary_state != nullptr && inherited_context.temporary_state->base != nullptr) {
                auto allocation = mem::alloc(mem::TEMPORARY_STORAGE_SIZE, align_of(std::max_align_t), inherited_context.allocator);
                kstd_assert(allocation.memory != nullptr, "Thread temporary storage allocation failed.");

                temporary_storage = allocation.memory;
                temporary_state   = Temporary_Allocator_State{ temporary_storage, mem::TEMPORARY_STORAGE_SIZE };
                mem::set_temporary_allocator(&temporary_state);
            }

            std::apply(std::move(function), std::move(args));

            if (temporary_storage != nullptr)
                (void)mem::free(temporary_storage, mem::TEMPORARY_STORAGE_SIZE, align_of(std::max_align_t), inherited_context.allocator);
        }
    );
}

}
