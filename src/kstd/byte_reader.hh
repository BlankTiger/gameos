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
            result = result | (byte << shift);
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
            result = result | (byte << shift);
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
            result = result | (byte << shift);
        }
        current_offset += sizeof(u64);

        return { result, true };
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
