#pragma once

#include <bit>

#include "kstd/assert.hh"
#include "kstd/array.hh"
#include "kstd/basic.hh"

struct Byte_Reader {
    u8*   source;
    s64 size;
    s64 current_offset;

    Byte_Reader() = default;

    Byte_Reader(void* source, s64 size)
        : source(reinterpret_cast<u8*>(source)),
          size(size),
          current_offset(0) {}

    Byte_Reader(Array_View<u8> source)
        : source(source.data),
          size(source.size),
          current_offset(0) {}

    // @TODO(blanktiger): Make source const u8*
    Byte_Reader(Array_View<const u8> source)
        : source(const_cast<u8*>(source.data)),
          size(source.size),
          current_offset(0) {}

    template <typename T>
    struct Read_Result {
        T    result;
        bool ok;
    };

    auto remaining() -> s64 {
        return size - current_offset;
    }

    auto read_u8() -> Read_Result<u8> {
        if (size - current_offset < static_cast<s64>(sizeof(u8))) return { u8{}, false };
        Read_Result<u8> result(source[current_offset], true);
        current_offset += sizeof(u8);
        return result;
    }

    force_inline auto read_bool() -> Read_Result<bool> {
        auto [value, value_ok] = read_u8();
        return { static_cast<bool>(value), value_ok };
    }

    force_inline auto read_s8() -> Read_Result<s8> {
        auto [value, value_ok] = read_u8();
        return { std::bit_cast<s8>(value), value_ok };
    }

    auto read_u16() -> Read_Result<u16> {
        if (size - current_offset < static_cast<s64>(sizeof(u16))) return { u16{}, false };

        u16 result = 0;
        @for (int i = 0; i < 2; ++i) {
            u16 byte = source[current_offset + i];

            constexpr u32 shift = i * 8;
            result |= (byte << shift);
        }
        current_offset += sizeof(u16);

        return { result, true };
    }

    auto read_u32() -> Read_Result<u32> {
        if (size - current_offset < static_cast<s64>(sizeof(u32))) return { u32{}, false };

        u32 result = 0;
        @for (int i = 0; i < 4; ++i) {
            u32 byte = source[current_offset + i];

            constexpr u32 shift = i * 8;
            result |= (byte << shift);
        }
        current_offset += sizeof(u32);

        return { result, true };
    }

    auto read_u64() -> Read_Result<u64> {
        if (size - current_offset < static_cast<s64>(sizeof(u64))) return { u64{}, false };

        u64 result = 0;
        @for (int i = 0; i < 8; ++i) {
            u64 byte = source[current_offset + i];

            constexpr u32 shift = i * 8;
            result |= (byte << shift);
        }
        current_offset += sizeof(u64);

        return { result, true };
    }

    static constexpr u8 LEB_VALUE_BIT_COUNT = 7;
    static constexpr u8 LEB_STOP_VALUE      = 0;

    static constexpr u8 LEB_VALUE_MASK    = 0b01111111;
    static constexpr u8 LEB_CONTINUE_MASK = 0b10000000;
    static constexpr u8 LEB_SIGN_MASK     = 0b01000000;

    auto read_uleb128() -> Read_Result<u64> {
        u64 result     = 0;
        u32 byte_shift = 0;
        for (;;) {
            if (size - current_offset <= byte_shift)
                return { u64{}, false };

            // @NOTE: 10 is the maximum shifts you need to take to encode
            // U64_MAX, so if we go over that something went wrong.
            if (byte_shift >= 10)
                return { u64{}, false };

            u8 byte = source[current_offset + byte_shift];
            result |= static_cast<u64>(byte & LEB_VALUE_MASK) << (byte_shift * LEB_VALUE_BIT_COUNT);
            if ((byte & LEB_CONTINUE_MASK) == LEB_STOP_VALUE) break;

            ++byte_shift;
        }
        current_offset += byte_shift + 1;

        return { result, true };
    }

    auto read_sleb128() -> Read_Result<s64> {
        u64 result     = 0;
        u32 byte_shift = 0;
        u8  byte       = 0;
        for (;;) {
            if (size - current_offset <= byte_shift)
                return { s64{}, false };

            // @NOTE: 10 is the maximum shifts you need to take to encode
            // S64_MAX, so if we go over that something went wrong.
            if (byte_shift >= 10)
                return { s64{}, false };

            byte = source[current_offset + byte_shift];
            result |= static_cast<u64>(byte & LEB_VALUE_MASK) << (byte_shift * LEB_VALUE_BIT_COUNT);
            if ((byte & LEB_CONTINUE_MASK) == LEB_STOP_VALUE) break;

            ++byte_shift;
        }

        u32 value_bit_count = (byte_shift + 1) * LEB_VALUE_BIT_COUNT;
        if (value_bit_count < 64 && (byte & LEB_SIGN_MASK) != 0) {
            result |= (~u64{0} << value_bit_count);
        }

        current_offset += byte_shift + 1;
        return { static_cast<s64>(result), true };
    }

    // Returns a non-owning view into the source bytes.
    auto read_bytes(s64 count) -> Read_Result<Array_View<const u8>> {
        if (size - current_offset < count) return { {}, false };

        Array_View<const u8> result{ count, source + current_offset };
        current_offset += count;
        return { result, true };
    }

    // Returns a non-owning string view from the source bytes.
    auto read_cstring() -> Read_Result<string> {
        s64 string_size = 0;
        while (true) {
            if (size - current_offset <= string_size) return { {}, false };

            if (source[current_offset + string_size] == '\0') break;
            ++string_size;
        }

        defer(current_offset += string_size + 1);
        return { { reinterpret_cast<const char*>(source + current_offset), string_size }, true };
    }

    // Returns true if there was enough bytes left to skip over them.
    auto skip(s64 count) -> bool {
        if (size - current_offset < count) return false;
        current_offset += count;
        return true;
    }
};


#ifdef UNIT_TESTS_KSTD_BYTE_READER

TEST(Byte_Reader, read_u8) {
    Static_Array<u8, 3> source{{1, 2, 3}};
    Byte_Reader reader(source);

    auto [byte, ok] = reader.read_u8();
    ASSERT_TRUE(ok);
    ASSERT_EQ(byte, 1);
    ASSERT_EQ(reader.current_offset, 1);
}

TEST(Byte_Reader, read_u16) {
    Static_Array<u8, 3> source{{1, 2, 3}};
    Byte_Reader reader(source);

    auto [word, ok] = reader.read_u16();
    ASSERT_TRUE(ok);
    ASSERT_EQ(word, 0x201);
    ASSERT_EQ(reader.current_offset, 2);
}

TEST(Byte_Reader, read_u32) {
    Static_Array<u8, 4> source{{1, 2, 3, 4}};
    Byte_Reader reader(source);

    auto [word, ok] = reader.read_u32();
    ASSERT_TRUE(ok);
    ASSERT_EQ(word, 0x4030201);
    ASSERT_EQ(reader.current_offset, 4);
}

TEST(Byte_Reader, read_u64) {
    Static_Array<u8, 8> source{{1, 2, 3, 4, 1, 2, 3, 4}};
    Byte_Reader reader(source);

    auto [word, ok] = reader.read_u64();
    ASSERT_TRUE(ok);
    ASSERT_EQ(word, 0x403020104030201);
    ASSERT_EQ(reader.current_offset, 8);
}

TEST(Byte_Reader, read_uleb128) {
    Static_Array<u8, 6> source{{0x00, 0x7f, 0x80, 0x01, 0xac, 0x02}};
    Byte_Reader reader(source);

    {
        auto [value, ok] = reader.read_uleb128();
        ASSERT_TRUE(ok);
        ASSERT_EQ(value, 0);
        ASSERT_EQ(reader.current_offset, 1);
    }

    {
        auto [value, ok] = reader.read_uleb128();
        ASSERT_TRUE(ok);
        ASSERT_EQ(value, 127);
        ASSERT_EQ(reader.current_offset, 2);
    }

    {
        auto [value, ok] = reader.read_uleb128();
        ASSERT_TRUE(ok);
        ASSERT_EQ(value, 128);
        ASSERT_EQ(reader.current_offset, 4);
    }

    {
        auto [value, ok] = reader.read_uleb128();
        ASSERT_TRUE(ok);
        ASSERT_EQ(value, 300);
        ASSERT_EQ(reader.current_offset, 6);
    }
}

TEST(Byte_Reader, read_uleb128_source_exhausted) {
    Static_Array<u8, 1> source{{0x80}};
    Byte_Reader reader(source);

    auto [value, ok] = reader.read_uleb128();
    ASSERT_FALSE(ok);
    ASSERT_EQ(value, u64{});
    ASSERT_EQ(reader.current_offset, 0);
}

TEST(Byte_Reader, read_sleb128) {
    Static_Array<u8, 6> source{{0x00, 0x01, 0x7f, 0x7e, 0xc0, 0x00}};
    Byte_Reader reader(source);

    {
        auto [value, ok] = reader.read_sleb128();
        ASSERT_TRUE(ok);
        ASSERT_EQ(value, 0);
        ASSERT_EQ(reader.current_offset, 1);
    }

    {
        auto [value, ok] = reader.read_sleb128();
        ASSERT_TRUE(ok);
        ASSERT_EQ(value, 1);
        ASSERT_EQ(reader.current_offset, 2);
    }

    {
        auto [value, ok] = reader.read_sleb128();
        ASSERT_TRUE(ok);
        ASSERT_EQ(value, -1);
        ASSERT_EQ(reader.current_offset, 3);
    }

    {
        auto [value, ok] = reader.read_sleb128();
        ASSERT_TRUE(ok);
        ASSERT_EQ(value, -2);
        ASSERT_EQ(reader.current_offset, 4);
    }

    {
        auto [value, ok] = reader.read_sleb128();
        ASSERT_TRUE(ok);
        ASSERT_EQ(value, 64);
        ASSERT_EQ(reader.current_offset, 6);
    }

    // DWARF example: -624485 is encoded as 0x9b 0xf1 0x59.
    Static_Array<u8, 3> negative_source{{0x9b, 0xf1, 0x59}};
    Byte_Reader negative_reader(negative_source);
    auto [negative_value, negative_ok] = negative_reader.read_sleb128();
    ASSERT_TRUE(negative_ok);
    ASSERT_EQ(negative_value, -624485);
    ASSERT_EQ(negative_reader.current_offset, 3);
}

TEST(Byte_Reader, read_sleb128_source_exhausted) {
    Static_Array<u8, 1> source{{0x80}};
    Byte_Reader reader(source);

    auto [value, ok] = reader.read_sleb128();
    ASSERT_FALSE(ok);
    ASSERT_EQ(value, s64{});
    ASSERT_EQ(reader.current_offset, 0);
}

TEST(Byte_Reader, read_bytes) {
    Static_Array<u8, 4> source{{1, 2, 3, 4}};
    Byte_Reader reader(source);

    auto [bytes, ok] = reader.read_bytes(2);
    ASSERT_TRUE(ok);
    ASSERT_EQ(bytes.size, 2);
    ASSERT_EQ(bytes[0], 1);
    ASSERT_EQ(bytes[1], 2);
    ASSERT_EQ(reader.current_offset, 2);

    auto [remaining, remaining_ok] = reader.read_bytes(2);
    ASSERT_TRUE(remaining_ok);
    ASSERT_EQ(remaining.size, 2);
    ASSERT_EQ(remaining[0], 3);
    ASSERT_EQ(remaining[1], 4);
    ASSERT_EQ(reader.current_offset, 4);
}

TEST(Byte_Reader, read_bytes_zero_count) {
    Static_Array<u8, 1> source{{1}};
    Byte_Reader reader(source);

    auto [bytes, ok] = reader.read_bytes(0);
    ASSERT_TRUE(ok);
    ASSERT_EQ(bytes.size, 0);
    ASSERT_EQ(reader.current_offset, 0);
}

TEST(Byte_Reader, read_bytes_source_exhausted) {
    Static_Array<u8, 2> source{{1, 2}};
    Byte_Reader reader(source);

    auto [bytes, ok] = reader.read_bytes(3);
    ASSERT_FALSE(ok);
    ASSERT_EQ(bytes.size, 0);
    ASSERT_EQ(reader.current_offset, 0);
}

TEST(Byte_Reader, read_cstring) {
    Static_Array<u8, 13> source{{'f', 'i', 'r', 's', 't', 0, 's', 'e', 'c', 'o', 'n', 'd', 0}};
    Byte_Reader reader(source);

    auto [first, first_ok] = reader.read_cstring();
    ASSERT_TRUE(first_ok);
    ASSERT_EQ(first, "first");
    ASSERT_EQ(reader.current_offset, 6);

    auto [second, second_ok] = reader.read_cstring();
    ASSERT_TRUE(second_ok);
    ASSERT_EQ(second, "second");
    ASSERT_EQ(reader.current_offset, 13);
}

TEST(Byte_Reader, read_cstring_empty) {
    Static_Array<u8, 1> source{{0}};
    Byte_Reader reader(source);

    auto [value, ok] = reader.read_cstring();
    ASSERT_TRUE(ok);
    ASSERT_EQ(value.size, 0);
    ASSERT_EQ(reader.current_offset, 1);
}

TEST(Byte_Reader, read_cstring_source_exhausted) {
    Static_Array<u8, 3> source{{'a', 'b', 'c'}};
    Byte_Reader reader(source);

    auto [value, ok] = reader.read_cstring();
    ASSERT_FALSE(ok);
    ASSERT_EQ(value.size, 0);
    ASSERT_EQ(reader.current_offset, 0);
}

TEST(Byte_Reader, skip) {
    Static_Array<u8, 4> source{{1, 2, 3, 4}};
    Byte_Reader reader(source);

    ASSERT_TRUE(reader.skip(1));
    ASSERT_EQ(reader.current_offset, 1);

    auto [byte, ok] = reader.read_u8();
    ASSERT_TRUE(ok);
    ASSERT_EQ(byte, 2);
    ASSERT_EQ(reader.current_offset, 2);

    ASSERT_TRUE(reader.skip(2));
    ASSERT_EQ(reader.current_offset, 4);

    ASSERT_FALSE(reader.skip(1));
    ASSERT_EQ(reader.current_offset, 4);
}

TEST(Byte_Reader, skip_zero_count) {
    Static_Array<u8, 1> source{{1}};
    Byte_Reader reader(source);

    ASSERT_TRUE(reader.skip(0));
    ASSERT_EQ(reader.current_offset, 0);
}

TEST(Byte_Reader, remaining) {
    Static_Array<u8, 4> source{{1, 2, 3, 4}};
    Byte_Reader reader(source);

    ASSERT_EQ(reader.remaining(), 4);

    auto [byte, ok] = reader.read_u8();
    ASSERT_TRUE(ok);
    ASSERT_EQ(byte, 1);
    ASSERT_EQ(reader.remaining(), 3);

    ASSERT_TRUE(reader.skip(2));
    ASSERT_EQ(reader.remaining(), 1);

    auto [bytes, bytes_ok] = reader.read_bytes(1);
    ASSERT_TRUE(bytes_ok);
    ASSERT_EQ(bytes[0], 4);
    ASSERT_EQ(reader.remaining(), 0);
}

TEST(Byte_Reader, remaining_empty_source) {
    Byte_Reader reader(nullptr, 0);

    ASSERT_EQ(reader.remaining(), 0);
}

TEST(Byte_Reader, source_empty) {
    Byte_Reader reader(nullptr, 0);

    auto [byte, ok] = reader.read_u8();
    ASSERT_FALSE(ok);
    ASSERT_EQ(byte, u8{});
    ASSERT_EQ(reader.current_offset, 0);
}

TEST(Byte_Reader, source_exhausted) {
    Static_Array<u8, 3> source{{1, 2, 3}};
    Byte_Reader reader(source);

    {
        auto [byte, ok] = reader.read_u8();
        ASSERT_TRUE(ok);
        ASSERT_EQ(byte, 1);
        ASSERT_EQ(reader.current_offset, 1);
    }

    {
        auto [byte, ok] = reader.read_u8();
        ASSERT_TRUE(ok);
        ASSERT_EQ(byte, 2);
        ASSERT_EQ(reader.current_offset, 2);
    }

    {
        auto [byte, ok] = reader.read_u8();
        ASSERT_TRUE(ok);
        ASSERT_EQ(byte, 3);
        ASSERT_EQ(reader.current_offset, 3);
    }

    {
        auto [byte, ok] = reader.read_u8();
        ASSERT_FALSE(ok);
        ASSERT_EQ(byte, u8{});
        ASSERT_EQ(reader.current_offset, 3);
    }
}

#endif
