#pragma once

#define ARRAY_ITERATOR()                                             \
    struct iterator {                                                \
        T* ptr;                                                      \
        iterator(T* p)                                               \
            : ptr(p) {}                                              \
        auto operator*() -> T& {                                     \
            return *ptr;                                             \
        }                                                            \
        auto operator++() -> iterator& {                             \
            ++ptr;                                                   \
            return *this;                                            \
        }                                                            \
        auto operator!=(const iterator& other) const -> bool {       \
            return ptr != other.ptr;                                 \
        }                                                            \
    };                                                               \
                                                                     \
    struct const_iterator {                                          \
        const T* ptr;                                                \
        explicit const_iterator(const T* p)                          \
            : ptr(p) {}                                              \
        auto operator*() const -> const T& {                         \
            return *ptr;                                             \
        }                                                            \
        auto operator++() -> const_iterator& {                       \
            ++ptr;                                                   \
            return *this;                                            \
        }                                                            \
        auto operator!=(const const_iterator& other) const -> bool { \
            return ptr != other.ptr;                                 \
        }                                                            \
                                                                     \
        /* Implicit conversion from iterator to const_iterator */    \
        explicit const_iterator(iterator it)                         \
            : ptr(it.ptr) {}                                         \
    };                                                               \
                                                                     \
    auto begin() -> iterator {                                       \
        return iterator(data);                                       \
    }                                                                \
    auto end() -> iterator {                                         \
        return iterator(data + size);                                \
    }                                                                \
                                                                     \
    auto begin() const noexcept -> const_iterator {                  \
        return const_iterator(data);                                 \
    }                                                                \
    auto end() const noexcept -> const_iterator {                    \
        return const_iterator(data + size);                          \
    }                                                                \
                                                                     \
    auto cbegin() const noexcept -> const_iterator {                 \
        return const_iterator(data);                                 \
    }                                                                \
    auto cend() const noexcept -> const_iterator {                   \
        return const_iterator(data + size);                          \
    }
