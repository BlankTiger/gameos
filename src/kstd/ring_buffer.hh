#pragma once

#include "array.hh"
#include "basic.hh"

template <typename T, usize N>
struct Ring_Buffer {
    static_assert(N > 0);

    Static_Array<T, N> data;
    usize              head = 0;
    usize              tail = 0;
    usize              size = 0;

    auto push_back(const T& element) -> void {
        kstd_assert(size < N, "push_back on full Ring_Buffer");

        data[tail] = element;
        tail = (tail + 1) % N;
        ++size;
    }

    auto pop_front() -> T {
        kstd_assert(size > 0, "pop_front on empty Ring_Buffer");

        auto element = data[head];
        head = (head + 1) % N;
        --size;
        return element;
    }

    auto empty() const -> bool {
        return size == 0;
    }

    auto full() const -> bool {
        return size == N;
    }

    auto clear() -> void {
        head = 0;
        tail = 0;
        size = 0;
    }
};

#ifdef UNIT_TESTS_KSTD_RING_BUFFER

TEST(Ring_Buffer, default_is_empty) {
    Ring_Buffer<int, 3> buffer;

    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_EQ(buffer.size, 0);
}

TEST(Ring_Buffer, preserves_fifo_order_across_wraparound) {
    Ring_Buffer<int, 3> buffer;

    buffer.push_back(1);
    buffer.push_back(2);
    EXPECT_EQ(buffer.pop_front(), 1);

    buffer.push_back(3);
    buffer.push_back(4);

    EXPECT_EQ(buffer.pop_front(), 2);
    EXPECT_EQ(buffer.pop_front(), 3);
    EXPECT_EQ(buffer.pop_front(), 4);
    EXPECT_TRUE(buffer.empty());
}

TEST(Ring_Buffer, detects_full_buffer) {
    Ring_Buffer<int, 2> buffer;

    buffer.push_back(1);
    buffer.push_back(2);

    EXPECT_TRUE(buffer.full());
    EXPECT_DEATH(buffer.push_back(3), "push_back on full Ring_Buffer");
}

TEST(Ring_Buffer, detects_empty_buffer) {
    Ring_Buffer<int, 2> buffer;

    EXPECT_DEATH(buffer.pop_front(), "pop_front on empty Ring_Buffer");
}

TEST(Ring_Buffer, clear_resets_buffer) {
    Ring_Buffer<int, 2> buffer;

    buffer.push_back(1);
    buffer.push_back(2);
    buffer.clear();

    EXPECT_TRUE(buffer.empty());
    buffer.push_back(3);
    EXPECT_EQ(buffer.pop_front(), 3);
}

#endif
