#pragma once

// ARRAY_ITERATOR() injects iterator types and begin/end/cbegin/cend.
// It calls elements() to get a T* to the first live element. Each
// container defines elements() however its backing store requires.
#define ARRAY_ITERATOR()                                                   \
    struct iterator {                                                      \
        using iterator_category = std::random_access_iterator_tag;         \
        using value_type        = T;                                       \
        using difference_type   = usize;                                   \
        using pointer           = T*;                                      \
        using reference         = T&;                                      \
        T* ptr;                                                            \
        iterator(T* p)                                                     \
            : ptr(p) {}                                                    \
        auto operator*() -> T& {                                           \
            return *ptr;                                                   \
        }                                                                  \
        auto operator++() -> iterator& {                                   \
            ++ptr;                                                         \
            return *this;                                                  \
        }                                                                  \
        auto operator++(int) -> iterator {                                 \
            iterator tmp = *this;                                          \
            ++ptr;                                                         \
            return tmp;                                                    \
        }                                                                  \
        auto operator--() -> iterator& {                                   \
            --ptr;                                                         \
            return *this;                                                  \
        }                                                                  \
        auto operator--(int) -> iterator {                                 \
            iterator tmp = *this;                                          \
            --ptr;                                                         \
            return tmp;                                                    \
        }                                                                  \
        auto operator+(usize n) const -> iterator {                        \
            return iterator(ptr + n);                                      \
        }                                                                  \
        auto operator-(usize n) const -> iterator {                        \
            return iterator(ptr - n);                                      \
        }                                                                  \
        auto operator-(const iterator& other) const -> usize {             \
            return ptr - other.ptr;                                        \
        }                                                                  \
        auto operator+=(usize n) -> iterator& {                            \
            ptr += n;                                                      \
            return *this;                                                  \
        }                                                                  \
        auto operator-=(usize n) -> iterator& {                            \
            ptr -= n;                                                      \
            return *this;                                                  \
        }                                                                  \
        auto operator[](usize n) const -> T& {                             \
            return *(ptr + n);                                             \
        }                                                                  \
        auto operator<(const iterator& other) const -> bool {              \
            return ptr < other.ptr;                                        \
        }                                                                  \
        auto operator>(const iterator& other) const -> bool {              \
            return ptr > other.ptr;                                        \
        }                                                                  \
        auto operator<=(const iterator& other) const -> bool {             \
            return ptr <= other.ptr;                                       \
        }                                                                  \
        auto operator>=(const iterator& other) const -> bool {             \
            return ptr >= other.ptr;                                       \
        }                                                                  \
        auto operator==(const iterator& other) const -> bool {             \
            return ptr == other.ptr;                                       \
        }                                                                  \
        auto operator!=(const iterator& other) const -> bool {             \
            return ptr != other.ptr;                                       \
        }                                                                  \
    };                                                                     \
                                                                           \
    struct const_iterator {                                                \
        const T* ptr;                                                      \
        explicit const_iterator(const T* p)                                \
            : ptr(p) {}                                                    \
        auto operator*() const -> const T& {                               \
            return *ptr;                                                   \
        }                                                                  \
        auto operator++() -> const_iterator& {                             \
            ++ptr;                                                         \
            return *this;                                                  \
        }                                                                  \
        auto operator!=(const const_iterator& other) const -> bool {       \
            return ptr != other.ptr;                                       \
        }                                                                  \
                                                                           \
        /* Implicit conversion from iterator to const_iterator */          \
        explicit const_iterator(iterator it)                               \
            : ptr(it.ptr) {}                                               \
    };                                                                     \
                                                                           \
    auto begin() -> iterator {                                             \
        return iterator(elements());                                       \
    }                                                                      \
    auto end() -> iterator {                                               \
        return iterator(elements() + size);                                \
    }                                                                      \
                                                                           \
    auto begin() const noexcept -> const_iterator {                        \
        return const_iterator(elements());                                 \
    }                                                                      \
    auto end() const noexcept -> const_iterator {                          \
        return const_iterator(elements() + size);                          \
    }                                                                      \
                                                                           \
    auto cbegin() const noexcept -> const_iterator {                       \
        return const_iterator(elements());                                 \
    }                                                                      \
    auto cend() const noexcept -> const_iterator {                         \
        return const_iterator(elements() + size);                          \
    }
