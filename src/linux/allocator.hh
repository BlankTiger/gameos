#pragma once

#include "kstd/allocator.hh"
#include "kstd/math.hh"

namespace mem {

struct Hosted_Allocator_State {
    using enum Allocator_Features;
    static constexpr Allocator_Features FEATURES = FREE | THREADSAFE | INFO | GENERAL_HEAP_ALLOCATOR;

    struct Allocation_Header {
        void* raw;
        ssize size;
        ssize alignment;
        ssize raw_alignment;
        u64   magic;
    };

    static constexpr u64 HEADER_MAGIC = 0x676767676767;

    auto get_allocator() -> Allocator {
        return { .proc = proc, .data = this };
    }

    static auto proc(Allocator_Mode mode, ssize size, ssize alignment, ssize, void* old_memory, void*) -> Allocator_Result {
        switch (mode) {
            case Allocator_Mode::ALLOCATE: {
                if (size < 0 || !math::is_power_of_two(alignment))
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                if (size == 0) return result(nullptr);

                const ssize requested_alignment = alignment;
                const ssize raw_alignment       = requested_alignment < MAX_ALIGN ? MAX_ALIGN : requested_alignment;
                const ssize requested_size      = size;
                if (requested_size > SSIZE_MAX_VALUE - size_of(Allocation_Header) - requested_alignment + 1)
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                const ssize total_size = requested_size + size_of(Allocation_Header) + requested_alignment - 1;

                void* raw = ::operator new(cast(usize)total_size, cast(std::align_val_t)raw_alignment);
                auto* aligned = cast(u8*)(align_up(ptr_addr(raw) + size_of(Allocation_Header), cast(psize)requested_alignment));
                auto* header  = aligned - size_of(Allocation_Header);
                new (header) Allocation_Header {
                    .raw = raw,
                    .size = requested_size,
                    .alignment = requested_alignment,
                    .raw_alignment = raw_alignment,
                    .magic = HEADER_MAGIC
                };

                return result(aligned);
            } break;

            case Allocator_Mode::FREE: {
                if (old_memory == nullptr) return result(nullptr);

                auto* header = cast(Allocation_Header*)(cast(u8*)old_memory - size_of(Allocation_Header));
                if (header->magic != HEADER_MAGIC)
                    return result(nullptr, Allocator_Error::INVALID_POINTER);

                ::operator delete(header->raw, cast(std::align_val_t)header->raw_alignment);
                return result(nullptr);
            } break;

            case Allocator_Mode::FEATURES: {
                auto* features = cast(Allocator_Features*)old_memory;
                if (features != nullptr) {
                    *features = FEATURES;
                } else {
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);
                }

                return result(nullptr);
            } break;

            // @TODO(blanktiger): This can and should be implemented because everything has headers.
            case Allocator_Mode::IS_THIS_YOURS: return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);

            case Allocator_Mode::INFO: {
                auto* info = cast(Allocator_Info*)old_memory;
                if (info == nullptr || info->pointer == nullptr)
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                auto* header = cast(Allocation_Header*)(cast(u8*)info->pointer - size_of(Allocation_Header));
                if (header->magic != HEADER_MAGIC)
                    return result(nullptr, Allocator_Error::INVALID_POINTER);

                info->requested_size      = header->size;
                info->requested_alignment = header->alignment;

                return result(nullptr);
            } break;

            default: return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);
        }

        unreachable();
    }
};

#if UNIT_TEST
namespace hidden {
    inline Hosted_Allocator_State hosted_allocator_state{};
    constexpr int LINUX_TEMPORARY_ALLOCATOR_SIZE = 256 * 1024;
    alignas(16) inline u8 linux_temporary_allocator_buffer[LINUX_TEMPORARY_ALLOCATOR_SIZE];

    struct Hosted_Allocator_Init {
        Hosted_Allocator_Init() {
            set_allocator(hosted_allocator_state.get_allocator());
            new (&temporary_allocator_state) Temporary_Allocator_State{
                linux_temporary_allocator_buffer,
                LINUX_TEMPORARY_ALLOCATOR_SIZE
            };
            set_temporary_allocator(&temporary_allocator_state);
        }
    };

    inline Hosted_Allocator_Init hosted_allocator_init{};
}
#endif

}
