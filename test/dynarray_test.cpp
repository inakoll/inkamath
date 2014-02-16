#include "dynarraylike.hpp"
#include <iostream>
#include <limits>
#include <set>
#include <tuple>
#include <stdexcept>

#include <boost/test/unit_test.hpp>

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

BOOST_FIXTURE_TEST_SUITE(dynarray_tests, DynarrayFixture)

BOOST_AUTO_TEST_CASE( dynarray_1 )
{
    a = dynarray<int>();
    BOOST_CHECK(a.empty());
    BOOST_CHECK(b.empty());
    BOOST_CHECK_EQUAL(a.size(), 0);
    BOOST_CHECK_EQUAL(b.size(), 0);
    BOOST_CHECK(a == b);
    a = dynarray<int>(10);
    BOOST_CHECK_EQUAL(a.size(), 10);
    a = make_dynarray({1,2,3,4});
    BOOST_CHECK_EQUAL(a.size(), 4);
    auto c(a);
    BOOST_CHECK_EQUAL_COLLECTIONS(a.cbegin(), a.cend(), c.begin(), c.end());
    BOOST_CHECK(a == c);
    int i = 1;
    for(auto va : a) {
        BOOST_CHECK_EQUAL(va, i);
        ++i;
    }
    for(auto j : {1,2,3}) {
        BOOST_CHECK_EQUAL(a[j-1], j);
    }
    auto d(make_dynarray({3,2,1}));
    BOOST_CHECK_EQUAL(d.size(), 3);
    i = 3;
    for(auto vd : d) {
        BOOST_CHECK_EQUAL(vd, i);
        --i;
    }
    BOOST_CHECK(a != d);
    a.swap(d);
    BOOST_CHECK_EQUAL(d.size(), 4);
    BOOST_CHECK_EQUAL(a.size(), 3);
    BOOST_CHECK_EQUAL_COLLECTIONS(d.cbegin(), d.cend(), c.begin(), c.end());
    i = 3;
    for(auto va : a) {
        BOOST_CHECK_EQUAL(va, i);
        --i;
    }
}

BOOST_AUTO_TEST_CASE( dynarray_2 )
{
    a = {1,2,3,4,5,6};
    BOOST_CHECK_EQUAL(a.size(), 6);
    int i = 1;
    for(auto it = a.begin(); it != a.end(); ++it) {
        BOOST_CHECK_EQUAL(*it, i);
        ++i;
    }
    i = 1;
    for(auto it = a.cbegin(); it != a.cend(); ++it) {
        BOOST_CHECK_EQUAL(*it, i);
        ++i;
    }
    i = 6;
    for(auto it = a.rbegin(); it != a.rend(); ++it) {
        BOOST_CHECK_EQUAL(*it, i);
        --i;
    }
    i = 6;
    for(auto it = a.crbegin(); it != a.crend(); ++it) {
        BOOST_CHECK_EQUAL(*it, i);
        --i;
    }

    BOOST_CHECK_EQUAL(a.front(), 1);
    BOOST_CHECK_EQUAL(a.back(), 6);
    BOOST_CHECK_EQUAL(*a.data(), 1);
    BOOST_CHECK_EQUAL(*(a.data()+5), 6);
    BOOST_CHECK_EQUAL(a.at(0), 1);
    BOOST_CHECK_EQUAL(a.at(a.size()-1), 6);
    try {
        a.at(a.size());
        BOOST_CHECK_MESSAGE(false, "a[6] should be out of range here.");
    }
    catch(std::out_of_range&) {
        BOOST_CHECK_MESSAGE(true, "a[6] should be out of range here.");
    }
    a.fill(0);
    BOOST_CHECK_EQUAL(a.size(), 6);
    for(auto v : a) {
        BOOST_CHECK_EQUAL(v, 0);
    }
}

BOOST_AUTO_TEST_CASE( dynarray_3 )
{
    {
        dynarray<TestType>(10);
    }
    BOOST_CHECK_EQUAL(TestType::desctructor_count, 10);
}

BOOST_AUTO_TEST_SUITE_END()
