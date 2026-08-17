#pragma once

#include <bit>

#include "kstd/assert.hh"
#include "kstd/array.hh"
#include "kstd/basic.hh"


struct Byte_Reader {
    u8*   source;
    usize size;
    usize current_offset;

    Byte_Reader() = default;

    Byte_Reader(void* source, usize size)
        : source(reinterpret_cast<u8*>(source)),
          size(size),
          current_offset(0) {}

    Byte_Reader(Array_View<u8> source)
        : source(source.data),
          size(source.size),
          current_offset(0) {}

    template <typename T>
    struct Read_Result {
        T    result;
        bool ok;
    };

    auto read_u8() -> Read_Result<u8> {
        if (size - current_offset < sizeof(u8)) return { u8{}, false };
        Read_Result<u8> result(source[current_offset], true);
        current_offset += sizeof(u8);
        return result;
    }

    auto read_u16() -> Read_Result<u16> {
        if (size - current_offset < sizeof(u16)) return { u16{}, false };

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
        if (size - current_offset < sizeof(u32)) return { u32{}, false };

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
        if (size - current_offset < sizeof(u64)) return { u64{}, false };

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
