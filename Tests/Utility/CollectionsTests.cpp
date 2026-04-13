#include <map>
#include <string>
#include <unordered_map>

#include <doctest/doctest.h>

#include <Lucky/Collections.hpp>

using namespace Lucky;

TEST_CASE("Contains finds present keys in a std::map") {
    std::map<int, int> m = {{1, 10}, {2, 20}, {3, 30}};
    CHECK(Contains(m, 1));
    CHECK(Contains(m, 2));
    CHECK(Contains(m, 3));
}

TEST_CASE("Contains rejects missing keys in a std::map") {
    std::map<int, int> m = {{1, 10}, {2, 20}};
    CHECK_FALSE(Contains(m, 99));
    CHECK_FALSE(Contains(m, 0));
}

TEST_CASE("Contains works with std::unordered_map") {
    std::unordered_map<std::string, int> m = {{"a", 1}, {"b", 2}};
    CHECK(Contains(m, std::string{"a"}));
    CHECK_FALSE(Contains(m, std::string{"missing"}));
}

TEST_CASE("Contains works on an empty map") {
    std::map<int, int> m;
    CHECK_FALSE(Contains(m, 0));
}
