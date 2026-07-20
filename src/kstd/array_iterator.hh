#pragma once

// ARRAY_ITERATOR() injects iterator types and begin/end/cbegin/cend.
// It calls elements() to get a T* to the first live element. Each
// container defines elements() however its backing store requires.
#define ARRAY_ITERATOR()                                                  \
    struct iterator {                                                     \
        T* ptr;                                                           \
        iterator(T* p)                                                    \
            : ptr(p) {}                                                   \
        auto operator*() -> T& {                                          \
            return *ptr;                                                  \
        }                                                                 \
        auto operator++() -> iterator& {                                  \
            ++ptr;                                                        \
            return *this;                                                 \
        }                                                                 \
        auto operator!=(const iterator& other) const -> bool {            \
            return ptr != other.ptr;                                      \
        }                                                                 \
    };                                                                    \
                                                                          \
    struct const_iterator {                                               \
        const T* ptr;                                                     \
        explicit const_iterator(const T* p)                               \
            : ptr(p) {}                                                   \
        auto operator*() const -> const T& {                              \
            return *ptr;                                                  \
        }                                                                 \
        auto operator++() -> const_iterator& {                            \
            ++ptr;                                                        \
            return *this;                                                 \
        }                                                                 \
        auto operator!=(const const_iterator& other) const -> bool {      \
            return ptr != other.ptr;                                      \
        }                                                                 \
                                                                          \
        /* Implicit conversion from iterator to const_iterator */         \
        explicit const_iterator(iterator it)                              \
            : ptr(it.ptr) {}                                              \
    };                                                                    \
                                                                          \
    auto begin() -> iterator {                                            \
        return iterator(elements());                                      \
    }                                                                     \
    auto end() -> iterator {                                              \
        return iterator(elements() + size);                               \
    }                                                                     \
                                                                          \
    auto begin() const noexcept -> const_iterator {                       \
        return const_iterator(elements());                                \
    }                                                                     \
    auto end() const noexcept -> const_iterator {                         \
        return const_iterator(elements() + size);                         \
    }                                                                     \
                                                                          \
    auto cbegin() const noexcept -> const_iterator {                      \
        return const_iterator(elements());                                \
    }                                                                     \
    auto cend() const noexcept -> const_iterator {                        \
        return const_iterator(elements() + size);                         \
    }
