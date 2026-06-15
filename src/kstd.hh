#pragma once

namespace kstd {
template <typename T>
struct Array {
    uint64_t size;
    T* data;

    T& operator[](int index) {
        return data[index];
    }
};

}  // namespace kstd
