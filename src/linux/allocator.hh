#pragma once

#include "kstd/allocator.hh"
#include "kstd/math.hh"

namespace mem {

struct Hosted_Allocator_State {
    using enum Allocator_Features;
    static constexpr Allocator_Features FEATURES = FREE | THREADSAFE | INFO | GENERAL_HEAP_ALLOCATOR;

    struct Allocation_Header {
        void* raw;
        usize size;
        usize alignment;
        usize raw_alignment;
        u64   magic;
    };

    static constexpr u64 HEADER_MAGIC = 0x676767676767;

    auto get_allocator() -> Allocator {
        return { .proc = proc, .data = this };
    }

    static auto proc(Allocator_Mode mode, s64 size, s64 alignment, s64, void* old_memory, void*) -> Allocator_Result {
        switch (mode) {
            case Allocator_Mode::ALLOCATE: {
                if (size < 0 || alignment <= 0 || !math::is_power_of_two(alignment))
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                if (size == 0) return result(nullptr);

                const usize requested_alignment = static_cast<usize>(alignment);
                const usize raw_alignment       = requested_alignment < alignof(std::max_align_t) ? alignof(std::max_align_t) : requested_alignment;
                const usize requested_size      = static_cast<usize>(size);
                if (requested_size > USIZE_MAX - sizeof(Allocation_Header) - requested_alignment + 1)
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                const usize total_size = requested_size + sizeof(Allocation_Header) + requested_alignment - 1;

                void* raw = ::operator new(total_size, std::align_val_t{raw_alignment});
                auto* aligned = reinterpret_cast<u8*>(align_up(ptr_addr(raw) + sizeof(Allocation_Header), requested_alignment));
                auto* header  = aligned - sizeof(Allocation_Header);
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

                auto* header = reinterpret_cast<Allocation_Header*>(static_cast<u8*>(old_memory) - sizeof(Allocation_Header));
                if (header->magic != HEADER_MAGIC)
                    return result(nullptr, Allocator_Error::INVALID_POINTER);

                ::operator delete(header->raw, std::align_val_t{header->raw_alignment});
                return result(nullptr);
            } break;

            case Allocator_Mode::FEATURES: {
                auto* features = static_cast<Allocator_Features*>(old_memory);
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
                auto* info = static_cast<Allocator_Info*>(old_memory);
                if (info == nullptr || info->pointer == nullptr)
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                auto* header = reinterpret_cast<Allocation_Header*>(static_cast<u8*>(info->pointer) - sizeof(Allocation_Header));
                if (header->magic != HEADER_MAGIC)
                    return result(nullptr, Allocator_Error::INVALID_POINTER);

                info->size      = static_cast<s64>(header->size);
                info->alignment = static_cast<s64>(header->alignment);

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
    constexpr usize LINUX_TEMPORARY_ALLOCATOR_SIZE = 256 * 1024;
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
