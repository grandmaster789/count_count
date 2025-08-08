#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <string>

#include "../src/util/flat_map.h"

// Test basic insert functionality
TEST_CASE("insert basic", "[FlatMap]") {
    cc::FlatMap<int, std::string> map;

    map.insert(1,"one");
    map.insert(2,"two");

    REQUIRE(map.get_num_entries() == 2);
    REQUIRE(map.contains(1));
    REQUIRE(map.contains(2));
    REQUIRE(map[1] == "one");
    REQUIRE(map[2] == "two");
}

// Test insert with overwrite
TEST_CASE("insert with overwrite", "[FlatMap]") {
    cc::FlatMap<int, std::string> map;

    map.insert(1, "one");
    map.insert(1, "ONE");

    REQUIRE(map.get_num_entries() == 1);
    REQUIRE(map[1] == "ONE");
}

// Test contains functionality
TEST_CASE("contains", "[FlatMap]") {
    cc::FlatMap<int, std::string> map;

    REQUIRE(!map.contains(1));

    map.insert(1, "one");
    REQUIRE( map.contains(1));
    REQUIRE(!map.contains(2));
}

// Test remove functionality
TEST_CASE("remove", "[FlatMap]") {
    cc::FlatMap<int, std::string> map;

    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");

    REQUIRE(map.get_num_entries() == 3);

    map.remove(2);
    REQUIRE( map.get_num_entries() == 2);

    REQUIRE(!map.contains(2));

    REQUIRE( map.contains(1));
    REQUIRE( map.contains(3));
}

// Test remove non-existent key (should be silently ignored)
TEST_CASE("remove non-existent", "[FlatMap]") {
    cc::FlatMap<int, std::string> map;

    map.insert(1, "one");
    REQUIRE(map.get_num_entries() == 1);

    // Should not crash or throw, should silently ignore
    map.remove(99);
    REQUIRE(map.get_num_entries() == 1);
    REQUIRE(map.contains(1));
}

// Test operator[] access
TEST_CASE("operator[] access", "[FlatMap]") {
    cc::FlatMap<std::string, int> map;

    map.insert("hello", 42);
    map.insert("world", 24);

    REQUIRE(map["hello"] == 42);
    REQUIRE(map["world"] == 24);
}

// Test operator[] with non-existent key (should throw)
TEST_CASE("operator[] throw", "[FlatMap]") {
    cc::FlatMap<std::string, int> map;

    map.insert("hello", 42);

    REQUIRE_THROWS_AS(map["nonexistent"], std::runtime_error);
}

// Test get_num_entries
TEST_CASE("get_num_entries", "[FlatMap]") {
    cc::FlatMap<int, std::string> map;

    REQUIRE(map.get_num_entries() == 0);

    map.insert(1, std::string("one"));
    REQUIRE(map.get_num_entries() == 1);

    map.insert(2, std::string("two"));
    REQUIRE(map.get_num_entries() == 2);

    map.remove(1);
    REQUIRE(map.get_num_entries() == 1);
}

// Test clear functionality
TEST_CASE("clear", "[FlatMap]") {
    cc::FlatMap<int, std::string> map;

    map.insert(1, std::string("one"));
    map.insert(2, std::string("two"));
    map.insert(3, std::string("three"));

    REQUIRE(map.get_num_entries() == 3);

    map.clear();

    REQUIRE(map.get_num_entries() == 0);
    REQUIRE(!map.contains(1));
    REQUIRE(!map.contains(2));
    REQUIRE(!map.contains(3));
}

// Test with different types
TEST_CASE("different types", "[FlatMap]") {
    cc::FlatMap<std::string, double> map;

    map.insert("pi", 3.14159);
    map.insert("e", 2.71828);

    REQUIRE(map.get_num_entries() == 2);

    REQUIRE(map["pi"] == Catch::Approx(3.14159));
    REQUIRE(map["e"]  == Catch::Approx(2.71828));
}

// Test empty map operations
TEST_CASE("empty map operations", "[FlatMap]") {
    cc::FlatMap<int, std::string> map;

    REQUIRE( map.get_num_entries() == 0);
    REQUIRE(!map.contains(1));

    // Should not crash
    map.remove(1);
    map.clear();

    REQUIRE(map.get_num_entries() == 0);
}

TEST_CASE("multiple insertions and removals", "[FlatMap]") {
    cc::FlatMap<int, std::string> map;

    for (int i = 0; i < 10; ++i)
        map.insert(i, std::string("value") + std::to_string(i));

    REQUIRE(map.get_num_entries() == 10);

    for (int i = 0; i < 5; ++i)
        map.remove(i);

    REQUIRE(map.get_num_entries() == 5);

    // Check remaining items
    for (int i = 5; i < 10; ++i) {
        REQUIRE(map.contains(i));
        REQUIRE(map[i] == std::string("value") + std::to_string(i));
    }
}