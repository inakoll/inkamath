#include <doctest/doctest.h>

#include "inkamath/dynarraylike.hpp"
#include <iostream>
#include <limits>
#include <set>
#include <tuple>
#include <stdexcept>

#include <algorithm>

TEST_SUITE_BEGIN("dynarray");

struct DynarrayFixture {
    dynarray<int> a;
    dynarray<int> b;
};

struct TestType {
    static size_t desctructor_count;
    TestType() = default;
    ~TestType() {
        ++desctructor_count;
    }
};

size_t TestType::desctructor_count=0;

TEST_CASE_FIXTURE(DynarrayFixture, "dynarray_1")
{
    a = dynarray<int>();
    CHECK(a.empty());
    CHECK(b.empty());
    CHECK_EQ(a.size(), 0);
    CHECK_EQ(b.size(), 0);
    CHECK(a == b);
    a = dynarray<int>(10);
    CHECK_EQ(a.size(), 10);
    a = make_dynarray({1,2,3,4});
    CHECK_EQ(a.size(), 4);
    auto c(a);
    CHECK(std::equal(a.cbegin(), a.cend(), c.begin(), c.end()));
    CHECK(a == c);
    int i = 1;
    for(auto va : a) {
        CHECK_EQ(va, i);
        ++i;
    }
    for(auto j : {1,2,3}) {
        CHECK_EQ(a[j-1], j);
    }
    auto d(make_dynarray({3,2,1}));
    CHECK_EQ(d.size(), 3);
    i = 3;
    for(auto vd : d) {
        CHECK_EQ(vd, i);
        --i;
    }
    CHECK(a != d);
    a.swap(d);
    CHECK_EQ(d.size(), 4);
    CHECK_EQ(a.size(), 3);
    CHECK(std::equal(d.cbegin(), d.cend(), c.begin(), c.end()));
    i = 3;
    for(auto va : a) {
        CHECK_EQ(va, i);
        --i;
    }
}

TEST_CASE_FIXTURE(DynarrayFixture, "dynarray_2")
{
    a = {1,2,3,4,5,6};
    CHECK_EQ(a.size(), 6);
    int i = 1;
    for(auto it = a.begin(); it != a.end(); ++it) {
        CHECK_EQ(*it, i);
        ++i;
    }
    i = 1;
    for(auto it = a.cbegin(); it != a.cend(); ++it) {
        CHECK_EQ(*it, i);
        ++i;
    }
    i = 6;
    for(auto it = a.rbegin(); it != a.rend(); ++it) {
        CHECK_EQ(*it, i);
        --i;
    }
    i = 6;
    for(auto it = a.crbegin(); it != a.crend(); ++it) {
        CHECK_EQ(*it, i);
        --i;
    }

    CHECK_EQ(a.front(), 1);
    CHECK_EQ(a.back(), 6);
    CHECK_EQ(*a.data(), 1);
    CHECK_EQ(*(a.data()+5), 6);
    CHECK_EQ(a.at(0), 1);
    CHECK_EQ(a.at(a.size()-1), 6);
    try {
        a.at(a.size());
        CHECK_MESSAGE(false, "a[6] should be out of range here.");
    }
    catch(std::out_of_range&) {
        CHECK_MESSAGE(true, "a[6] should be out of range here.");
    }
    a.fill(0);
    CHECK_EQ(a.size(), 6);
    for(auto v : a) {
        CHECK_EQ(v, 0);
    }
}

TEST_CASE_FIXTURE(DynarrayFixture, "dynarray_3")
{
    {
        dynarray<TestType>(10);
    }
    CHECK_EQ(TestType::desctructor_count, 10);
}

TEST_SUITE_END();
