#pragma once

#include "kstd/allocator.hh"
#include "kstd/array.hh"
#include "kstd/assert.hh"
#include "kstd/basic.hh"
#include "kstd/hash.hh"
#include "kstd/math.hh"

// @TODO(blanktiger): implement iterators.
template <typename Key_Type, typename Value_Type, u32 LOAD_FACTOR_PERCENT = 70, bool REUSE_TOMBSTONES = true>
struct Hash_Table {
    static_assert(LOAD_FACTOR_PERCENT < 100);

    struct Entry {
        u32        hash;
        Key_Type   key;
        Value_Type value;
    };

    usize count;         // Valid entry count.
    usize slots_filled;  // Count of slots that are unusable for new entries.

    mem::Allocator*   allocator;
    Array_View<Entry> entries;

    static constexpr auto MIN_SIZE = 32;

    static constexpr auto EMPTY_HASH       = 0;
    static constexpr auto TOMBSTONE_HASH   = 1;
    static constexpr auto FIRST_VALID_HASH = 2;

    Hash_Table(usize initial_size = MIN_SIZE, mem::Allocator* backing_allocator = nullptr)
        : count(0),
          slots_filled(0),
          allocator(mem::resolve_allocator(backing_allocator)),
          entries({}) {
        auto null = _resize_entries(initial_size);
        kstd_debug_assert(null.data == nullptr);
        kstd_assert(entries.data    != nullptr);
    }

    Hash_Table(const Hash_Table&) = delete;

    Hash_Table(Hash_Table&& from) noexcept
        : count(from.count),
          slots_filled(from.slots_filled),
          allocator(from.allocator),
          entries(from.entries) {
        from.count        = 0;
        from.slots_filled = 0;
        from.allocator    = nullptr;
        from.entries      = {};
    }

    auto operator = (Hash_Table&& from) noexcept -> Hash_Table& {
        if (this == &from) return *this;

        _destroy_entries(entries);

        count        = from.count;
        slots_filled = from.slots_filled;
        allocator    = from.allocator;
        entries      = from.entries;

        from.count        = 0;
        from.slots_filled = 0;
        from.allocator    = nullptr;
        from.entries      = {};

        return *this;
    }

    explicit Hash_Table(mem::Allocator* backing_allocator) : Hash_Table(MIN_SIZE, backing_allocator) {}

    ~Hash_Table() {
        _destroy_entries(entries);
    }

    auto find(Key_Type key) -> Value_Type* {
        auto* entry = _find_entry(*this, key);
        return entry == nullptr ? nullptr : &entry->value;
    }

    auto find(Key_Type key) const -> const Value_Type* {
        auto* entry = _find_entry(*this, key);
        return entry == nullptr ? nullptr : &entry->value;
    }

    template <typename Table_Type>
    static auto _find_entry(Table_Type& table, Key_Type key) -> decltype(&table.entries[0]) {
        u32 mask = table.entries.size - 1;
        u32 hash = hash::compute(key);
        if (hash < FIRST_VALID_HASH) hash += FIRST_VALID_HASH;

        usize index = hash & mask;
        u32 probe_increment = 1;

        while (table.entries[index].hash != EMPTY_HASH) {
            if (table.entries[index].hash == hash && table.entries[index].key == key)
                return &table.entries[index];

            index = (index + probe_increment) & mask;
            probe_increment += 1;
        }

        return nullptr;
    }

    //
    // Adds key/value pair and returns a pointer to the inserted value.
    // Duplicate keys are allowed. Use set to replace.
    //
    auto add(Key_Type key, Value_Type value) -> Value_Type* {
        // filled / allocated >= 70/100
        if ((slots_filled + 1) * 100 >= entries.size * LOAD_FACTOR_PERCENT) {
            expand();
        }

        kstd_assert(slots_filled < entries.size);

        u32 mask = entries.size - 1;
        u32 hash = hash::compute(key);
        if (hash < FIRST_VALID_HASH) hash += FIRST_VALID_HASH;

        usize index = hash & mask;
        u32 probe_increment = 1;

        while (entries[index].hash != EMPTY_HASH) {
            if constexpr (REUSE_TOMBSTONES) {
                if (entries[index].hash == TOMBSTONE_HASH) {
                    slots_filled -= 1;
                    break;
                }
            }

            // @TODO(blanktiger): Count collisions here if we want to.

            index = (index + probe_increment) & mask;
            probe_increment += 1;
        }

        count        += 1;
        slots_filled += 1;

        auto& entry = entries[index];
        entry.hash  = hash;
        entry.key   = key;
        entry.value = value;

        return &entry.value;
    }

    //
    // Adds key/value pair, or replaces value for an existing key and returns a
    // pointer to the inserted/replaced value.
    //
    auto set(Key_Type key, Value_Type value) -> Value_Type* {
        if (auto* existing = find(key)) {
            *existing = value;
            return existing;
        }

        return add(key, value);
    }

    struct Remove_Result {
        bool       success;
        Value_Type value;
    };

    auto remove(Key_Type key) -> Remove_Result {
        if (entries.size == 0) {
            return { false, Value_Type{} };
        }

        auto* entry = _find_entry(*this, key);
        if (entry) {
            entry->hash = TOMBSTONE_HASH;
            count -= 1;
            return { true, entry->value };
        }

        return { false, Value_Type{} };
    }

    force_inline auto contains(Key_Type key) const -> bool {
        return find(key) != nullptr;
    }

    auto reset() -> void {
        count        = 0;
        slots_filled = 0;
        for (auto& entry: entries) entry.hash = 0;
    }

    auto ensure_has_space_for(usize item_count) {
        if ((slots_filled + item_count) * 100 < entries.size * LOAD_FACTOR_PERCENT) return;

        //
        // @NOTE: We check slots_filled above, because there is a possibility
        // that we can insert the new entries in the currently allocated space,
        // including space occupied by tombstones. But when calculating
        // new_slot_count, we use count instead of slots_filled, because
        // resizing the table will remove the tombstones.
        //
        auto new_slot_count = ((count + item_count) * 100) / LOAD_FACTOR_PERCENT;
        new_slot_count = math::next_power_of_two(new_slot_count);
        expand(new_slot_count);
    }

    auto expand(usize new_slot_count = USIZE_MAX) {
        usize to_allocate = new_slot_count;
        if (to_allocate == USIZE_MAX) {
            if ((count * 2 + 1) * 100 < entries.size * LOAD_FACTOR_PERCENT) {
                // If we have a lot of tombstones then we don't need to necessarily grow.
                to_allocate = entries.size;
            } else {
                to_allocate = entries.size * 2;
            }
        } else {
            kstd_assert(
                math::is_power_of_two(to_allocate),
                ctprint("If you pass in a value yourself it must be a power of 2. You gave: %", to_allocate)
            );
        }

        if (to_allocate < MIN_SIZE) to_allocate = MIN_SIZE;

        auto old_entries = _resize_entries(to_allocate);
        defer(_destroy_entries(old_entries));

        count        = 0;
        slots_filled = 0;

        for (auto& entry: old_entries) {
            if (entry.hash >= FIRST_VALID_HASH) add(entry.key, entry.value);
        }
    }

    //
    // Returns the old entries. Caller is responsible for freeing them.
    //
    [[nodiscard]] auto _resize_entries(usize new_slot_count = MIN_SIZE) -> Array_View<Entry> {
        auto slot_count_to_allocate = new_slot_count;
        if (slot_count_to_allocate < MIN_SIZE) slot_count_to_allocate = MIN_SIZE;

        slot_count_to_allocate = math::next_power_of_two(slot_count_to_allocate);
        auto new_entries = mem::alloc_array<Entry>(slot_count_to_allocate, allocator);
        for (usize i = 0; i < new_entries.size; ++i)
            ::new (static_cast<void*>(new_entries.data + i)) Entry{};

        auto old_entries = entries;
        entries = new_entries;
        return old_entries;
    }

    auto _destroy_entries(Array_View<Entry> entries_to_destroy) -> void {
        for (auto& entry: entries_to_destroy) entry.~Entry();
        mem::free_array(entries_to_destroy, allocator);
    }
};

#ifdef UNIT_TESTS_KSTD_HASH_TABLE

TEST(Hash_Table, init) {
    Hash_Table<u32, u32> table;

    ASSERT_EQ(table.count,        0);
    ASSERT_EQ(table.slots_filled, 0);
    ASSERT_EQ(table.entries.size, table.MIN_SIZE);
}

TEST(Hash_Table, add) {
    Hash_Table<u32, u32> table;

    table.add(6, 7);

    auto* value = table.find(6);
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(*value,             7);
    ASSERT_EQ(table.count,        1);
    ASSERT_EQ(table.slots_filled, 1);
    ASSERT_EQ(table.entries.size, table.MIN_SIZE);
}

TEST(Hash_Table, set_replaces_existing_value) {
    Hash_Table<u32, u32> table;

    table.add(6, 7);
    table.set(6, 8);

    auto* value = table.find(6);
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(*value,             8);
    ASSERT_EQ(table.count,        1);
    ASSERT_EQ(table.slots_filled, 1);
}

TEST(Hash_Table, expansion_preserves_hash_two) {
    Hash_Table<u32, u32> table;

    table.add(0, 0);
    for (u32 key = 1; key <= 22; ++key) table.add(key, key);

    ASSERT_TRUE(table.entries.size > table.MIN_SIZE);
    auto* value = table.find(0);
    ASSERT_TRUE(value != nullptr);
    ASSERT_EQ(*value, 0);
}

TEST(Hash_Table, contains) {
    Hash_Table<u32, u32> table;
    ASSERT_FALSE(table.contains(6));

    table.add(6, 7);
    ASSERT_TRUE(table.contains(6));
    ASSERT_EQ(table.count,        1);
    ASSERT_EQ(table.slots_filled, 1);
}

TEST(Hash_Table, remove) {
    Hash_Table<u32, u32> table;

    table.add(6, 7);
    table.add(7, 7);

    ASSERT_EQ(table.count,        2);
    ASSERT_EQ(table.slots_filled, 2);

    auto [success, value] = table.remove(6);

    ASSERT_TRUE(success);
    ASSERT_EQ(value, 7);
    ASSERT_FALSE(table.contains(6));
    ASSERT_TRUE(table.contains(7));
    ASSERT_EQ(table.count,        1);
    ASSERT_EQ(table.slots_filled, 2);
}

TEST(Hash_Table, indeed_67_maps_to_67) {
    Hash_Table<string, u32> table;
    table.add("67", 67);

    auto* value_68 = table.find("68");
    ASSERT_EQ(value_68, nullptr);

    auto* value_67 = table.find("67");
    ASSERT_EQ(*value_67, 67);
}

#endif
