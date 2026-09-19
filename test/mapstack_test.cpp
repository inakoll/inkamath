#include <doctest/doctest.h>

#include "mapstack.hpp"
#include <iostream>
#include <limits>
#include <set>
#include <tuple>

TEST_SUITE_BEGIN("mapstack");

using namespace std;

struct MapstackFixture {
    Mapstack<string, int> mapstack;
    int value = 0;
};

TEST_CASE_FIXTURE(MapstackFixture, "single_entry_1")
{
    mapstack.Set("a", 1);
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 1);

    mapstack.Set("a", 2);
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 2);

    mapstack.Pop();
    CHECK_MESSAGE(mapstack.Get("a", value) == false, "mapstack.Get : shouldn't get value!");

    mapstack.Set("a", 1);
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 1);
    mapstack.Push();

    mapstack.Set("a", 2);
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 2);

    mapstack.Pop();
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 1);

    mapstack.Pop();
    CHECK_MESSAGE(mapstack.Get("a", value) == false, "mapstack.Get : shouldn't get value!");
}

TEST_CASE_FIXTURE(MapstackFixture, "single_entry_2")
{
    mapstack.Set("a", 1);
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 1);

    mapstack.Push();
    mapstack.Pop();

    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 1);
    mapstack.Set("a", 2);
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 2);

    mapstack.Push();
    mapstack.Pop();

    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 2);
}

TEST_CASE_FIXTURE(MapstackFixture, "multiple_entry_1")
{
    mapstack.Set("a", 1);
    mapstack.Set("b", 1);
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 1);
    CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 1);

    mapstack.Set("b", 2);
    CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 2);

    mapstack.Pop();
    CHECK_MESSAGE(mapstack.Get("a", value) == false, "mapstack.Get : shouldn't get value!");
    CHECK_MESSAGE(mapstack.Get("b", value) == false, "mapstack.Get : shouldn't get value!");

    mapstack.Clear();
    CHECK_MESSAGE(mapstack.Get("a", value) == false, "mapstack.Get : shouldn't get value!");
    CHECK_MESSAGE(mapstack.Get("b", value) == false, "mapstack.Get : shouldn't get value!");
    CHECK_MESSAGE(mapstack.Get("c", value) == false, "mapstack.Get : shouldn't get value!");
    CHECK_MESSAGE(mapstack.Get("d", value) == false, "mapstack.Get : shouldn't get value!");
}

TEST_CASE_FIXTURE(MapstackFixture, "multiple_entry_2")
{
    mapstack.Set("a", 1);
    mapstack.Set("b", 2);
    mapstack.Push();
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 1);
    CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 2);

    mapstack.Set("c", 3);
    mapstack.Set("d", 4);
    CHECK_MESSAGE(mapstack.Get("c", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 3);
    CHECK_MESSAGE(mapstack.Get("d", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 4);

    mapstack.Pop();
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 1);
    CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 2);
    CHECK_MESSAGE(mapstack.Get("c", value) == false, "mapstack.Get : shouldn't get value!");
    CHECK_MESSAGE(mapstack.Get("d", value) == false, "mapstack.Get : shouldn't get value!");

    mapstack.Clear();
    CHECK_MESSAGE(mapstack.Get("a", value) == false, "mapstack.Get : shouldn't get value!");
    CHECK_MESSAGE(mapstack.Get("b", value) == false, "mapstack.Get : shouldn't get value!");
    CHECK_MESSAGE(mapstack.Get("c", value) == false, "mapstack.Get : shouldn't get value!");
    CHECK_MESSAGE(mapstack.Get("d", value) == false, "mapstack.Get : shouldn't get value!");

}

TEST_CASE_FIXTURE(MapstackFixture, "multiple_entry_3")
{
    mapstack.Set("a", 1);
    mapstack.Set("b", 2);
    mapstack.Push();
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 1);
    CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 2);

    mapstack.Set("a", 11);
    mapstack.Set("b", 12);
    mapstack.Set("c", 3);
    mapstack.Set("d", 4);
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 11);
    CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 12);
    CHECK_MESSAGE(mapstack.Get("c", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 3);
    CHECK_MESSAGE(mapstack.Get("d", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 4);

    mapstack.Pop();
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 1);
    CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 2);
}

TEST_CASE_FIXTURE(MapstackFixture, "multiple_entry_4")
{
    // Initialy there is no element in the current list
    for(Mapstack<string, int>::current_const_iterator it = mapstack.CurrentBegin();
            it != mapstack.CurrentEnd();
            ++it) {
        CHECK_MESSAGE(false, "Current list shouldn't have any element here!");
    }

    mapstack.Set("a", 1);
    mapstack.Set("b", 2);
    mapstack.Push();
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 1);
    CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 2);

    mapstack.Set("a", 11);
    mapstack.Set("b", 12);
    mapstack.Set("c", 3);
    mapstack.Set("d", 4);
    CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 11);
    CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 12);
    CHECK_MESSAGE(mapstack.Get("c", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 3);
    CHECK_MESSAGE(mapstack.Get("d", value) == true, "mapstack.Get : can't get value!");
    CHECK_EQ(value, 4);

    // Mapstack current keys are {"a", "b", "c", "d"}
    string result[4] = {"a", "b", "c", "d"};
    int i = 0;
    for(auto it = mapstack.CurrentBegin();
            i < 4;
            ++it, ++i) {
        CHECK_EQ(*it, result[i]);
    }

    // After pop mapstack current keys are {"a", "b"}
    mapstack.Pop();
    i = 0;
    auto it = mapstack.CurrentBegin();
    for(;
            i < 2;
            ++it, ++i) {
        CHECK_EQ(*it, result[i]);
    }
    for(;
            it != mapstack.CurrentEnd();
            ++it) {
        std::cout << *it << std::endl;
        CHECK_MESSAGE(false, "Current list shouldn't have any element here!");
    }

    // After pop mapstack keys are empty again
    mapstack.Pop();
    for(Mapstack<string, int>::current_const_iterator it = mapstack.CurrentBegin();
            it != mapstack.CurrentEnd();
            ++it) {
        CHECK_MESSAGE(false, "Current list shouldn't have any element here!");
    }
}

TEST_CASE_FIXTURE(MapstackFixture, "const_iteration")
{
    std::vector<std::tuple<const std::string, int>> input
                    { std::make_tuple("a", 1)
                    , std::make_tuple("b", 2)
                    , std::make_tuple("c", 3)
                    , std::make_tuple("d", 4) };

    // initializing the mapstack with input
    for(auto elm : input) {
        mapstack.Set(std::get<0>(elm), std::get<1>(elm));
    }

    // check for equality of the two containers using std::set
    std::set<std::tuple<const std::string, int>> set1(mapstack.begin(), mapstack.end());
    std::set<std::tuple<const std::string, int>> set2(input.begin(),input.end());
    CHECK_MESSAGE(set1 == set2, "mapstack and input should be equal here!");

    mapstack.Push();

    // Pushing new values
    std::vector<std::tuple<const std::string, int>> push  =
    {  std::make_tuple("a", 11)
     , std::make_tuple("b", 12)};
    for(auto elm : push) {
        mapstack.Set(std::get<0>(elm), std::get<1>(elm));
    }

    // mapstack content should now be equal to :
    std::set<std::tuple<const std::string, int>> set3
                    { std::make_tuple("a", 11)
                    , std::make_tuple("b", 12)
                    , std::make_tuple("c", 3)
                    , std::make_tuple("d", 4) };
    set1.clear();
    set1.insert(mapstack.begin(), mapstack.end());
    CHECK_MESSAGE(set1 == set3, "mapstack and input should be equal here!");

    // After pop, mapstack should retrieve it's previous values
    mapstack.Pop();
    set1.clear();
    set1.insert(mapstack.begin(), mapstack.end());
    CHECK_MESSAGE(set1 == set2, "mapstack and input should be equal here!");

}

TEST_CASE_FIXTURE(MapstackFixture, "iteration")
{
    std::vector<std::tuple<const std::string, int>> input
                    { std::make_tuple("a", 1)
                    , std::make_tuple("b", 2)
                    , std::make_tuple("c", 3)
                    , std::make_tuple("d", 4) };

    // initializing the mapstack with input
    for(auto elm : input) {
        mapstack.Set(std::get<0>(elm), std::get<1>(elm));
    }

    // check for equality of the two containers using std::set
    std::set<std::tuple<const std::string, int>> set1(mapstack.begin(), mapstack.end());
    std::set<std::tuple<const std::string, int>> set2(input.begin(),input.end());
    CHECK_MESSAGE(set1 == set2, "mapstack and input should be equal here!");

    // mutate the mapstack from an iterator
    for(auto elem : mapstack) {
        std::get<1>(elem) -= 1;
    }

    // mapstack content should now be equal to :
    std::vector<std::tuple<const std::string, int>> output
                    { std::make_tuple("a", 0)
                    , std::make_tuple("b", 1)
                    , std::make_tuple("c", 2)
                    , std::make_tuple("d", 3) };

    set1.insert(mapstack.begin(), mapstack.end());
    set2.insert(output.begin(),output.end());
    CHECK_MESSAGE(set1 == set2, "mapstack and output should be equal here!");

}

TEST_SUITE_END();
