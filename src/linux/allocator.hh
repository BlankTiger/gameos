#pragma once

#include "kstd/allocator.hh"

namespace mem {

struct Hosted_Allocator final : mem::Allocator {
    auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* override {
        if (alignment < alignof(std::max_align_t)) alignment = alignof(std::max_align_t);
        if (size == 0) size = 1;
        return ::operator new(size, std::align_val_t{alignment});
    }

    auto free(void* pointer, usize, usize alignment = alignof(std::max_align_t)) -> void override {
        if (pointer == nullptr) return;
        if (alignment < alignof(std::max_align_t)) alignment = alignof(std::max_align_t);
        ::operator delete(pointer, std::align_val_t{alignment});
    }
};

struct Hosted_Thread_Temporary_Storage final : mem::Allocator {
    mem::Allocator* backing = nullptr;
    mem::Temporary_Allocator temporary_allocator{};
    alignas(16) u8 memory[mem::TEMPORARY_STORAGE_SIZE]{};

    auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* override {
        return temporary_allocator.alloc(size, alignment);
    }

    auto free(void*, usize, usize = alignof(std::max_align_t)) -> void override {}
};

inline auto create_thread_temporary_allocator(mem::Allocator* allocator, mem::Allocator* inherited_allocator) -> mem::Allocator* {
    if (inherited_allocator == nullptr || inherited_allocator == &mem::null_allocator) return inherited_allocator;

    kstd_assert(allocator != nullptr);
    auto* storage_memory = allocator->alloc(sizeof(Hosted_Thread_Temporary_Storage), alignof(Hosted_Thread_Temporary_Storage));
    auto* storage = static_cast<Hosted_Thread_Temporary_Storage*>(storage_memory);
    kstd_assert(storage != nullptr, "Thread temporary storage allocation failed");

    new (storage) Hosted_Thread_Temporary_Storage{};
    storage->backing = allocator;
    storage->temporary_allocator.~Temporary_Allocator();
    new (&storage->temporary_allocator) mem::Temporary_Allocator{
        storage->memory,
        mem::TEMPORARY_STORAGE_SIZE,
    };
    return storage;
}

inline auto destroy_thread_temporary_allocator(mem::Allocator* allocator) -> void {
    if (allocator == nullptr || allocator == &mem::null_allocator) return;

    auto* storage = static_cast<Hosted_Thread_Temporary_Storage*>(allocator);
    auto* backing = storage->backing;
    storage->~Hosted_Thread_Temporary_Storage();
    backing->free(storage, sizeof(Hosted_Thread_Temporary_Storage), alignof(Hosted_Thread_Temporary_Storage));
}

#if UNIT_TEST
namespace hidden {
    inline Hosted_Allocator hosted_allocator{};
    constexpr usize LINUX_TEMPORARY_ALLOCATOR_SIZE = 256 * 1024;
    alignas(16) inline u8 linux_temporary_allocator_buffer[LINUX_TEMPORARY_ALLOCATOR_SIZE];

struct Hosted_Allocator_Init {
    Hosted_Allocator_Init() {
        mem::temporary_allocator.~Temporary_Allocator();
        new (&mem::temporary_allocator) mem::Temporary_Allocator{
            linux_temporary_allocator_buffer,
            LINUX_TEMPORARY_ALLOCATOR_SIZE
        };
        context.allocator           = &hosted_allocator;
        context.temporary_allocator = &mem::temporary_allocator;
    }
};

    inline Hosted_Allocator_Init hosted_allocator_init{};
}
#endif

}
