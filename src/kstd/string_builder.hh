#pragma once

#include <type_traits>
#include <utility>

#include "basic.hh"
#include "cstring.hh"
#include "assert.hh"
#include "allocator.hh"
#include "format.hh"
#include "string.hh"

inline constexpr usize STRING_BUILDER_BUFFER_SIZE = 512;

//
// Linked buffers, pointers into prior buffers stay valid. Initial inline
// buffer + heap chain.
//
struct String_Builder {
    struct Buffer {
        usize   count     = 0;
        usize   allocated = 0;
        Buffer* next      = nullptr;
    };

    static_assert(STRING_BUILDER_BUFFER_SIZE > sizeof(Buffer));
    static constexpr usize INITIAL_DATA_SIZE = STRING_BUILDER_BUFFER_SIZE - sizeof(Buffer);

    mem::Allocator* allocator              = nullptr;
    Buffer*         current_buffer         = nullptr;
    usize           subsequent_buffer_size = INITIAL_DATA_SIZE;
    bool            failed                 = false;
    alignas(Buffer) u8 initial_bytes[STRING_BUILDER_BUFFER_SIZE]{};

    String_Builder(mem::Allocator* allocator = nullptr, usize buffer_size = 0)
        : allocator(mem::resolve_allocator(allocator)),
          current_buffer(nullptr),
          subsequent_buffer_size(buffer_size > 0 ? buffer_size : INITIAL_DATA_SIZE),
          failed(false) {
        Buffer* base = base_buffer();
        base->count     = 0;
        base->allocated = INITIAL_DATA_SIZE;
        base->next      = nullptr;
    }

    ~String_Builder() {
        free_buffers();
    }

    String_Builder(const String_Builder&)                      = delete;
    auto operator = (const String_Builder&) -> String_Builder& = delete;
    String_Builder(String_Builder&&)                           = delete;
    auto operator = (String_Builder&&) -> String_Builder&      = delete;

    auto reset() -> void {
        free_buffers();
        Buffer* base = base_buffer();
        base->count = 0;
        current_buffer = nullptr;
        failed = false;
    }

    auto append(const char* bytes, usize length) -> void {
        if (bytes == nullptr || length == 0) return;

        while (length > 0) {
            Buffer* buffer = get_current_buffer();
            usize length_max = buffer->allocated - buffer->count;
            if (length_max == 0) {
                if (!expand()) {
                    failed = true;
                    return;
                }
                buffer     = current_buffer;
                length_max = buffer->allocated - buffer->count;
                kstd_assert(length_max > 0, "String_Builder expand produced empty buffer");
            }

            usize to_copy = length < length_max ? length : length_max;
            kstd_memcpy(buffer_data(buffer) + buffer->count, bytes, to_copy);
            buffer->count += to_copy;
            length        -= to_copy;
            bytes         += to_copy;
        }
    }

    auto append(string s) -> void {
        append(s.data, s.size);
    }

    auto append(char c) -> void {
        append(&c, 1);
    }

    auto put_char(char c) -> void {
        append(c);
    }

    auto new_line() -> void {
        append('\n');
    }

    template <typename... Args>
    auto print(Args&&... args) -> int {
        return fmt::print(*this, std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto println(Args&&... args) -> int {
        return fmt::println(*this, std::forward<Args>(args)...);
    }

    auto length() const -> usize {
        usize bytes = 0;
        for (const Buffer* buffer = base_buffer(); buffer != nullptr; buffer = buffer->next)
            bytes += buffer->count;
        return bytes;
    }

    // Contiguous copy onto destination_allocator (null → current global).
    // Chain buffers stay on builder's allocator. Non-owning; pair with
    // free_string when not on temp. Resets builder by default.
    auto to_string(mem::Allocator* destination_allocator = nullptr, bool do_reset = true) -> string {
        usize count = length();
        if (count == 0) {
            if (do_reset) reset();
            return string{};
        }

        auto* destination = mem::resolve_allocator(destination_allocator);
        auto* out = static_cast<char*>(destination->alloc(count, alignof(char)));
        kstd_assert(out != nullptr, "String_Builder::to_string allocation failed");

        char* cursor = out;
        for (Buffer* buffer = base_buffer(); buffer != nullptr; buffer = buffer->next) {
            if (buffer->count > 0) {
                kstd_memcpy(cursor, buffer_data(buffer), buffer->count);
                cursor += buffer->count;
            }
        }

        if (do_reset) reset();
        return string(out, count);
    }

private:
    auto base_buffer() -> Buffer* {
        return reinterpret_cast<Buffer*>(initial_bytes);
    }

    auto base_buffer() const -> const Buffer* {
        return reinterpret_cast<const Buffer*>(initial_bytes);
    }

    static auto buffer_data(Buffer* buffer) -> char* {
        return reinterpret_cast<char*>(buffer) + sizeof(Buffer);
    }

    auto get_current_buffer() -> Buffer* {
        if (current_buffer != nullptr) return current_buffer;
        kstd_assert(allocator != nullptr, "String_Builder used without allocator");
        return base_buffer();
    }

    auto free_buffers() -> void {
        Buffer* base = base_buffer();
        Buffer* buffer = base->next;
        while (buffer != nullptr) {
            Buffer* next = buffer->next;
            usize block_size = sizeof(Buffer) + buffer->allocated;
            allocator->free(buffer, block_size, alignof(Buffer));
            buffer = next;
        }
        base->next = nullptr;
    }

    auto expand() -> bool {
        kstd_assert(allocator != nullptr, "String_Builder expand without allocator");

        usize subsequent = subsequent_buffer_size > 0 ? subsequent_buffer_size : INITIAL_DATA_SIZE;
        usize block_size = sizeof(Buffer) + subsequent;
        auto* bytes = static_cast<u8*>(allocator->alloc(block_size, alignof(Buffer)));
        if (bytes == nullptr) return false;

        auto* buffer = reinterpret_cast<Buffer*>(bytes);
        buffer->next      = nullptr;
        buffer->count     = 0;
        buffer->allocated = subsequent;

        get_current_buffer()->next = buffer;
        current_buffer = buffer;
        return true;
    }
};

// Free heap bytes from copy_string / String_Builder::to_string / sprint.
// allocator null → current global allocator at free time (must match alloc heap).
inline auto free_string(string s, mem::Allocator* allocator = nullptr) -> void {
    if (s.data == nullptr || s.size == 0) return;
    mem::resolve_allocator(allocator)->free(s.data, s.size, alignof(char));
}

inline auto copy_string(string s, mem::Allocator* allocator = nullptr) -> string {
    if (s.size == 0) return string{};

    auto* destination_allocator = mem::resolve_allocator(allocator);
    auto* data = static_cast<char*>(destination_allocator->alloc(s.size, alignof(char)));
    kstd_assert(data != nullptr, "copy_string allocation failed");
    kstd_memcpy(data, s.data, s.size);
    return string(data, s.size);
}

force_inline auto tcopy(string s) -> string {
    return copy_string(s, &mem::temporary_allocator);
}

// Null-terminated copy in temp (for C APIs). Not counted in the string length.
inline auto temp_c_string(string s) -> const char* {
    auto* data = static_cast<char*>(mem::talloc(s.size + 1, alignof(char)));
    if (s.size > 0) kstd_memcpy(data, s.data, s.size);
    data[s.size] = '\0';
    return data;
}

// Formats into allocator heap (null → current global). Caller free_string(result) (or defer).
template <typename... Args>
auto sprint(mem::Allocator* allocator, Args&&... args) -> string {
    String_Builder builder(allocator);
    builder.print(std::forward<Args>(args)...);
    return builder.to_string(allocator);
}

// Pack form when first arg is not an allocator (else derived Allocator* would prefer pack).
template <typename First, typename... Rest>
    requires (!std::is_convertible_v<First, mem::Allocator*>)
auto sprint(First&& first, Rest&&... rest) -> string {
    return sprint(static_cast<mem::Allocator*>(nullptr),
                  std::forward<First>(first),
                  std::forward<Rest>(rest)...);
}

// Format into mem::temporary_allocator, fire-and-forget until temporary_allocator.reset().
template <typename... Args>
auto tprint(Args&&... args) -> string {
    return sprint(&mem::temporary_allocator, std::forward<Args>(args)...);
}

#ifdef UNIT_TESTS_KSTD_STRING_BUILDER

TEST(String_Builder, append_and_to_string) {
    String_Builder builder;
    builder.append("hello ");
    builder.append("world");
    string s = builder.to_string();
    defer(free_string(s));
    EXPECT_EQ(s, "hello world");
}

TEST(String_Builder, grows_past_initial_buffer) {
    String_Builder builder;
    for (usize i = 0; i < STRING_BUILDER_BUFFER_SIZE * 2; ++i)
        builder.append('x');
    string s = builder.to_string();
    defer(free_string(s));
    EXPECT_EQ(s.size, STRING_BUILDER_BUFFER_SIZE * 2);
    for (usize i = 0; i < s.size; ++i)
        EXPECT_EQ(s.data[i], 'x');
}

TEST(String_Builder, to_string_uses_current_global) {
    mem::Hosted_Allocator allocator_a;
    mem::Hosted_Allocator allocator_b;

    mem::Allocator* previous = mem::resolve_allocator();
    defer(mem::set_global_allocator(previous));

    mem::set_global_allocator(&allocator_a);
    String_Builder builder;
    builder.append("stable");

    mem::set_global_allocator(&allocator_b);
    string s = builder.to_string();
    defer(free_string(s, &allocator_b));
    EXPECT_EQ(s, "stable");
}

TEST(String_Builder, on_temp_survives_until_reset) {
    mem::temporary_allocator.reset();
    defer(mem::temporary_allocator.reset());
    PUSH_ALLOCATOR(&mem::temporary_allocator);

    String_Builder builder;
    builder.append("frame");
    string s = builder.to_string();
    EXPECT_EQ(s, "frame");
}

TEST(String_Builder, print_formats_into_builder) {
    String_Builder builder;
    builder.print("%, %!", "hello", "world");
    string s = builder.to_string();
    defer(free_string(s));
    EXPECT_EQ(s, "hello, world!");
}

TEST(sprint, can_format_values_into_a_string) {
    auto formatted = sprint("%, %!", "hello", "world");
    defer(free_string(formatted));
    EXPECT_EQ(formatted, "hello, world!");
}

TEST(sprint, numbered_args) {
    auto formatted = sprint("%2-%1", "a", "b");
    defer(free_string(formatted));
    EXPECT_EQ(formatted, "b-a");
}

TEST(sprint, can_target_explicit_allocator) {
    mem::Hosted_Allocator hosted;
    mem::Debug_Allocator  allocator{&hosted};

    auto formatted = sprint(&allocator, "%, %!", "hello", "world");
    EXPECT_EQ(formatted, "hello, world!");
    free_string(formatted, &allocator);
    // Debug_Allocator dtor asserts no leaks -> alloc + free both hit this heap.
}

TEST(sprint, explicit_allocator_grows_past_inline_buffer) {
    mem::Hosted_Allocator hosted;
    mem::Debug_Allocator  allocator{&hosted};

    char payload[STRING_BUILDER_BUFFER_SIZE * 2];
    kstd_memset(payload, 'x', sizeof(payload));
    auto formatted = sprint(&allocator, "%", string(payload, sizeof(payload)));
    EXPECT_EQ(formatted.size, sizeof(payload));
    for (usize i = 0; i < formatted.size; ++i)
        EXPECT_EQ(formatted.data[i], 'x');
    free_string(formatted, &allocator);
}

TEST(tprint, formats_into_temporary_allocator) {
    mem::temporary_allocator.reset();
    defer(mem::temporary_allocator.reset());

    auto a = tprint("%, %!", "hello", "world");
    EXPECT_EQ(a, "hello, world!");

    auto b = tprint("n=%", 42);
    EXPECT_EQ(b, "n=42");
    EXPECT_EQ(a, "hello, world!");
}

TEST(tprint, reset_invalidates_previous_views_memory_reuse) {
    mem::temporary_allocator.reset();
    defer(mem::temporary_allocator.reset());
    auto first = tprint("first");
    EXPECT_EQ(first, "first");

    mem::temporary_allocator.reset();
    auto second = tprint("second-longer");
    EXPECT_EQ(second, "second-longer");
}

TEST(tcopy, copies_into_temp) {
    mem::temporary_allocator.reset();
    defer(mem::temporary_allocator.reset());
    const char* literal = "abc";
    auto copied = tcopy(string(literal));
    EXPECT_EQ(copied, "abc");
    EXPECT_NE(copied.data, literal);
}

TEST(temp_c_string, null_terminates) {
    mem::temporary_allocator.reset();
    defer(mem::temporary_allocator.reset());
    auto* cstr = temp_c_string("hi");
    EXPECT_EQ(cstr[0], 'h');
    EXPECT_EQ(cstr[1], 'i');
    EXPECT_EQ(cstr[2], '\0');
}

TEST(copy_string, can_target_explicit_allocator) {
    mem::Hosted_Allocator allocator;
    auto copied = copy_string("xy", &allocator);
    defer(free_string(copied, &allocator));
    EXPECT_EQ(copied, "xy");
}

#endif
