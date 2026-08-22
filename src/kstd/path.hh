#pragma once

#include "kstd/string.hh"
#include "kstd/string_builder.hh"

namespace path {

constexpr char SEPARATOR = '/';

//
// @NOTE: This always allocates whatever you get back.
//
// @TODO(blanktiger): Make this take arbitrary number of parts.
//
auto join(string part1, string part2, char separator = SEPARATOR) -> string {
    if (part2.size == 0) return copy_string(part1);
    if (part1.size == 0) return copy_string(part2);
    if (part2[0] == separator) return copy_string(part2);

    if (part1[part1.size - 1] == separator)
        return sprint("%1%2", part1, part2);
    else
        return sprint("%1%2%3", part1, separator, part2);
}

#ifdef UNIT_TESTS_KSTD_PATH

TEST(path, join) {
    string result{};

    result = join("gameos", "main.cc");
    ASSERT_STREQ(temp_c_string(result), "gameos/main.cc");

    result = join("gameos/", "main.cc");
    ASSERT_STREQ(temp_c_string(result), "gameos/main.cc");

    result = join("", "main.cc");
    ASSERT_STREQ(temp_c_string(result), "main.cc");

    result = join("gameos", "");
    ASSERT_STREQ(temp_c_string(result), "gameos");

    // We don't care about normalizing.
    result = join("gameos//", "main.cc");
    ASSERT_STREQ(temp_c_string(result), "gameos//main.cc");

    // We don't care about normalizing.
    result = join("gameos//", "//main.cc");
    ASSERT_STREQ(temp_c_string(result), "//main.cc");
}

#endif

} // namespace path

