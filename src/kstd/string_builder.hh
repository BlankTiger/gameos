#pragma once

#include <type_traits>
#include <utility>

#include "basic.hh"
#include "cstring.hh"
#include "assert.hh"
#include "allocator.hh"
#include "format.hh"
#include "string.hh"

inline constexpr s64 STRING_BUILDER_BUFFER_SIZE = 512;

//
// Linked buffers, pointers into prior buffers stay valid. Initial inline
// buffer + heap chain.
//
struct String_Builder {
    struct Buffer {
        s64     count     = 0;
        s64     allocated = 0;
        Buffer* next      = nullptr;
    };

    static_assert(STRING_BUILDER_BUFFER_SIZE > sizeof(Buffer));
    static constexpr s64 INITIAL_DATA_SIZE = STRING_BUILDER_BUFFER_SIZE - sizeof(Buffer);

    mem::Allocator  allocator{};
    Buffer*         current_buffer         = nullptr;
    s64             subsequent_buffer_size = INITIAL_DATA_SIZE;
    bool            failed                 = false;
    alignas(Buffer) u8 initial_bytes[STRING_BUILDER_BUFFER_SIZE]{};

    String_Builder(mem::Allocator allocator = {}, s64 buffer_size = 0)
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

    auto append(const char* bytes, s64 length) -> void {
        if (bytes == nullptr || length == 0) return;

        while (length > 0) {
            Buffer* buffer = get_current_buffer();
            s64 length_max = buffer->allocated - buffer->count;
            if (length_max == 0) {
                if (!expand()) {
                    failed = true;
                    return;
                }
                buffer     = current_buffer;
                length_max = buffer->allocated - buffer->count;
                kstd_assert(length_max > 0, "String_Builder expand produced empty buffer");
            }

            s64 to_copy = length < length_max ? length : length_max;
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

    auto length() const -> s64 {
        s64 bytes = 0;
        for (const Buffer* buffer = base_buffer(); buffer != nullptr; buffer = buffer->next)
            bytes += buffer->count;
        return bytes;
    }

    // Contiguous copy onto destination_allocator (null -> current global).
    // Chain buffers stay on builder's allocator. Non-owning; pair with
    // free_string when not on temp. Resets builder by default.
    auto to_string(mem::Allocator destination_allocator = {}, bool do_reset = true) -> string {
        s64 count = length();
        if (count == 0) {
            if (do_reset) reset();
            return string{};
        }

        auto* out = static_cast<char*>(mem::alloc(count, alignof(char), destination_allocator).memory);
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

    // Contiguous null-terminated copy onto destination_allocator (null -> current
    // global). Chain buffers stay on builder's allocator. free_c_string when not
    // on temp. Resets builder by default.
    auto to_c_string(mem::Allocator destination_allocator = {}, bool do_reset = true) -> const char* {
        s64 count = length();

        auto* out = static_cast<char*>(mem::alloc(count + 1, alignof(char), destination_allocator).memory);
        kstd_assert(out != nullptr, "String_Builder::to_c_string allocation failed");

        char* cursor = out;
        for (Buffer* buffer = base_buffer(); buffer != nullptr; buffer = buffer->next) {
            if (buffer->count > 0) {
                kstd_memcpy(cursor, buffer_data(buffer), buffer->count);
                cursor += buffer->count;
            }
        }
        *cursor = '\0';

        if (do_reset) reset();
        return out;
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
        kstd_assert(allocator.valid(), "String_Builder used without a valid allocator.");
        return base_buffer();
    }

    auto free_buffers() -> void {
        Buffer* base = base_buffer();
        Buffer* buffer = base->next;
        while (buffer != nullptr) {
            Buffer* next = buffer->next;
            s64 block_size = sizeof(Buffer) + buffer->allocated;
            (void)mem::free(buffer, block_size, alignof(Buffer), allocator);
            buffer = next;
        }
        base->next = nullptr;
    }

    auto expand() -> bool {
        kstd_assert(allocator.valid(), "String_Builder expand without a valid allocator.");

            s64 subsequent = subsequent_buffer_size > 0 ? subsequent_buffer_size : INITIAL_DATA_SIZE;
            s64 block_size = sizeof(Buffer) + subsequent;
        auto allocation = mem::alloc(block_size, alignof(Buffer), allocator);
        auto* bytes = static_cast<u8*>(allocation.memory);
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

// Formats into allocator heap (null -> current global). Caller free_string(result) (or defer).
template <typename... Args>
auto sprint(mem::Allocator allocator, Args&&... args) -> string {
    String_Builder builder(allocator);
    builder.print(std::forward<Args>(args)...);
    return builder.to_string(allocator);
}

// Pack form when first arg is not an allocator.
template <typename First, typename... Rest>
    requires (!std::is_convertible_v<First, mem::Allocator>)
auto sprint(First&& first, Rest&&... rest) -> string {
    return sprint(mem::Allocator{},
                  std::forward<First>(first),
                  std::forward<Rest>(rest)...);
}

// Format into context temporary allocator, fire-and-forget until reset().
template <typename... Args>
auto tprint(Args&&... args) -> string {
    return sprint(context.temporary_allocator, std::forward<Args>(args)...);
}

// C string print: formats into allocator heap (null -> current global), null-terminated.
// Caller free_c_string(result) (or defer) when not on temp.
template <typename... Args>
auto csprint(mem::Allocator allocator, Args&&... args) -> const char* {
    String_Builder builder(allocator);
    builder.print(std::forward<Args>(args)...);
    return builder.to_c_string(allocator);
}

    // Pack form when first arg is not an allocator.
template <typename First, typename... Rest>
    requires (!std::is_convertible_v<First, mem::Allocator>)
auto csprint(First&& first, Rest&&... rest) -> const char* {
    return csprint(mem::Allocator{},
                   std::forward<First>(first),
                   std::forward<Rest>(rest)...);
}

// C temporary print: format null-terminated into context temporary allocator.
// Valid until temporary_allocator.reset().
template <typename... Args>
auto ctprint(Args&&... args) -> const char* {
    return csprint(context.temporary_allocator, std::forward<Args>(args)...);
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
    for (s64 i = 0; i < STRING_BUILDER_BUFFER_SIZE * 2; ++i)
        builder.append('x');
    string s = builder.to_string();
    defer(free_string(s));
    EXPECT_EQ(s.size, STRING_BUILDER_BUFFER_SIZE * 2);
    for (s64 i = 0; i < s.size; ++i)
        EXPECT_EQ(s.data[i], 'x');
}

TEST(String_Builder, to_string_uses_current_global) {
    mem::Hosted_Allocator_State allocator_a;
    mem::Hosted_Allocator_State allocator_b;

    mem::Allocator previous = context.allocator;
    defer(mem::set_allocator(previous));

    mem::set_allocator(allocator_a.get_allocator());
    String_Builder builder;
    builder.append("stable");

    mem::set_allocator(allocator_b.get_allocator());
    string s = builder.to_string();
    defer(free_string(s, allocator_b.get_allocator()));
    EXPECT_EQ(s, "stable");
}

TEST(String_Builder, on_temp_survives_until_reset) {
    mem::reset_temporary_allocator();
    defer(mem::reset_temporary_allocator(););
    PUSH_ALLOCATOR(context.temporary_allocator);

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
    mem::Hosted_Allocator_State hosted;
    mem::Debug_Allocator_State  debug{hosted.get_allocator()};

    auto formatted = sprint(debug.get_allocator(), "%, %!", "hello", "world");
    EXPECT_EQ(formatted, "hello, world!");
    free_string(formatted, debug.get_allocator());
    // Debug_Allocator_State destructor asserts no leaks -> alloc + free both hit this heap.
}

TEST(sprint, explicit_allocator_grows_past_inline_buffer) {
    mem::Hosted_Allocator_State hosted;
    mem::Debug_Allocator_State  debug{hosted.get_allocator()};

    char payload[STRING_BUILDER_BUFFER_SIZE * 2];
    kstd_memset(payload, 'x', sizeof(payload));
    auto formatted = sprint(debug.get_allocator(), "%", string(payload, sizeof(payload)));
    EXPECT_EQ(formatted.size, sizeof(payload));
    for (s64 i = 0; i < formatted.size; ++i)
        EXPECT_EQ(formatted.data[i], 'x');
    free_string(formatted, debug.get_allocator());
}

TEST(csprint, can_format_values_into_a_c_string) {
    auto* formatted = csprint("%, %!", "hello", "world");
    defer(free_c_string(formatted));
    EXPECT_STREQ(formatted, "hello, world!");
}

TEST(csprint, numbered_args) {
    auto* formatted = csprint("%2-%1", "a", "b");
    defer(free_c_string(formatted));
    EXPECT_STREQ(formatted, "b-a");
}

TEST(csprint, can_target_explicit_allocator) {
    mem::Hosted_Allocator_State hosted;
    mem::Debug_Allocator_State  debug{hosted.get_allocator()};

    auto* formatted = csprint(debug.get_allocator(), "%, %!", "hello", "world");
    defer(free_c_string(formatted, debug.get_allocator()));
    EXPECT_STREQ(formatted, "hello, world!");
}

TEST(csprint, explicit_allocator_grows_past_inline_buffer) {
    mem::Hosted_Allocator_State hosted;
    mem::Debug_Allocator_State  debug{hosted.get_allocator()};

    char payload[STRING_BUILDER_BUFFER_SIZE * 2];
    kstd_memset(payload, 'x', sizeof(payload));
    auto* formatted = csprint(debug.get_allocator(), "%", string(payload, sizeof(payload)));
    EXPECT_EQ(kstd_strlen(formatted), sizeof(payload));
    for (usize i = 0; i < sizeof(payload); ++i)
        EXPECT_EQ(formatted[i], 'x');
    EXPECT_EQ(formatted[sizeof(payload)], '\0');
    free_c_string(formatted, debug.get_allocator());
}

TEST(csprint, empty_is_null_terminated) {
    auto* formatted = csprint("");
    defer(free_c_string(formatted));
    EXPECT_NE(formatted, nullptr);
    EXPECT_EQ(formatted[0], '\0');
}

TEST(ctprint, formats_into_temporary_allocator) {
    defer(mem::reset_temporary_allocator());

    auto* a = ctprint("%, %!", "hello", "world");
    EXPECT_STREQ(a, "hello, world!");

    auto* b = ctprint("n=%", 42);
    EXPECT_STREQ(b, "n=42");
    EXPECT_STREQ(a, "hello, world!");
}

TEST(String_Builder, to_c_string_null_terminates) {
    String_Builder builder;
    builder.append("hi");
    auto* cstr = builder.to_c_string();
    defer(free_c_string(cstr));
    EXPECT_EQ(cstr[0], 'h');
    EXPECT_EQ(cstr[1], 'i');
    EXPECT_EQ(cstr[2], '\0');
}

TEST(tprint, formats_into_temporary_allocator) {
    defer(mem::reset_temporary_allocator());

    auto a = tprint("%, %!", "hello", "world");
    EXPECT_EQ(a, "hello, world!");

    auto b = tprint("n=%", 42);
    EXPECT_EQ(b, "n=42");
    EXPECT_EQ(a, "hello, world!");
}

TEST(tprint, reset_invalidates_previous_views_memory_reuse) {
    defer(mem::reset_temporary_allocator());
    auto first = tprint("first");
    EXPECT_EQ(first, "first");

    mem::reset_temporary_allocator();
    auto second = tprint("second-longer");
    EXPECT_EQ(second, "second-longer");
}

TEST(tcopy, copies_into_temp) {
    defer(mem::reset_temporary_allocator());
    const char* literal = "abc";
    auto copied = tcopy(string(literal));
    EXPECT_EQ(copied, "abc");
    EXPECT_NE(copied.data, literal);
}

TEST(temp_c_string, null_terminates) {
    defer(mem::reset_temporary_allocator());
    auto* cstr = temp_c_string("hi");
    EXPECT_EQ(cstr[0], 'h');
    EXPECT_EQ(cstr[1], 'i');
    EXPECT_EQ(cstr[2], '\0');
}

TEST(copy_string, can_target_explicit_allocator) {
    mem::Hosted_Allocator_State allocator;
    auto copied = copy_string("xy", allocator.get_allocator());
    defer(free_string(copied, allocator.get_allocator()));
    EXPECT_EQ(copied, "xy");
}

#endif
