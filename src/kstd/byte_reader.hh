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
        if (current_offset + sizeof(u8) > size) return { u8{}, false };
        Read_Result<u8> result(source[current_offset], true);
        current_offset += sizeof(u8);
        return result;
    }

    auto read_u16() -> Read_Result<u16> {
        if (current_offset + sizeof(u16) > size) return { u16{}, false };
        auto [low_bytes,  ok1] = read_u8();
        auto [high_bytes, ok2] = read_u8();
        kstd_assert(ok1);
        kstd_assert(ok2);

        u16 result =
            static_cast<u16>(high_bytes) << 8 |
            static_cast<u16>(low_bytes);
        return { result, true };
    }

    auto read_u32() -> Read_Result<u32> {
        if (current_offset + sizeof(u32) > size) return { u32{}, false };
        auto [bytes1, ok1] = read_u8();
        auto [bytes2, ok2] = read_u8();
        auto [bytes3, ok3] = read_u8();
        auto [bytes4, ok4] = read_u8();
        kstd_assert(ok1);
        kstd_assert(ok2);
        kstd_assert(ok3);
        kstd_assert(ok4);

        u32 result =
            static_cast<u32>(bytes4) << 24 |
            static_cast<u32>(bytes3) << 16 |
            static_cast<u32>(bytes2) << 8  |
            static_cast<u32>(bytes1);
        return { result, true };
    }

    auto read_u64() -> Read_Result<u64> {
        if (current_offset + sizeof(u64) > size) return { u64{}, false };
        auto [bytes1, ok1] = read_u8();
        auto [bytes2, ok2] = read_u8();
        auto [bytes3, ok3] = read_u8();
        auto [bytes4, ok4] = read_u8();
        auto [bytes5, ok5] = read_u8();
        auto [bytes6, ok6] = read_u8();
        auto [bytes7, ok7] = read_u8();
        auto [bytes8, ok8] = read_u8();
        kstd_assert(ok1);
        kstd_assert(ok2);
        kstd_assert(ok3);
        kstd_assert(ok4);
        kstd_assert(ok5);
        kstd_assert(ok6);
        kstd_assert(ok7);
        kstd_assert(ok8);

        u64 result =
            static_cast<u64>(bytes8) << 56 |
            static_cast<u64>(bytes7) << 48 |
            static_cast<u64>(bytes6) << 40 |
            static_cast<u64>(bytes5) << 32 |
            static_cast<u64>(bytes4) << 24 |
            static_cast<u64>(bytes3) << 16 |
            static_cast<u64>(bytes2) << 8  |
            static_cast<u64>(bytes1);
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
