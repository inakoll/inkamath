#include "mapstack.hpp"
#include <iostream>
#include <limits>

#define BOOST_TEST_MODULE inkamath_mapstack
#include <boost/test/unit_test.hpp>

using namespace std;

#if 0
// legacy test function helpers
void print(const Mapstack<string, int>& ai_mapstack, string ai_key)
{
    int w_value;

    if(!ai_mapstack.Get(ai_key, w_value))
        cout << "Can't get '" << ai_key << "'!" << endl;

    else
        cout << ai_key << " = " << w_value << endl;
}

void printall(const Mapstack<string, int>& ai_mapstack)
{
    for(Mapstack<string, int>::const_iterator it = ai_mapstack.begin(); it != ai_mapstack.end() ; ++it) {
        print(ai_mapstack, it->first);
    }
}
# endif

struct MapstackFixture {
    Mapstack<string, int> mapstack;
    int value;
};

BOOST_FIXTURE_TEST_SUITE(mapstack_tests, MapstackFixture)

BOOST_AUTO_TEST_CASE( single_entry )
{
    mapstack.Set("a", 1);
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 1);

    mapstack.Set("a", 2);
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 2);

    mapstack.Pop();
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == false, "mapstack.Get : shouldn't get value!");

    mapstack.Set("a", 1);
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 1);
    mapstack.Push();

    mapstack.Set("a", 2);
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 2);

    mapstack.Pop();
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 1);

    mapstack.Pop();
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == false, "mapstack.Get : shouldn't get value!");
}

BOOST_AUTO_TEST_CASE( multiple_entry_1 )
{
    mapstack.Set("a", 1);
    mapstack.Set("b", 1);
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 1);
    BOOST_CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 1);

    mapstack.Set("b", 2);
    BOOST_CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 2);

    mapstack.Pop();
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == false, "mapstack.Get : shouldn't get value!");
    BOOST_CHECK_MESSAGE(mapstack.Get("b", value) == false, "mapstack.Get : shouldn't get value!");

    mapstack.Clear();
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == false, "mapstack.Get : shouldn't get value!");
    BOOST_CHECK_MESSAGE(mapstack.Get("b", value) == false, "mapstack.Get : shouldn't get value!");
    BOOST_CHECK_MESSAGE(mapstack.Get("c", value) == false, "mapstack.Get : shouldn't get value!");
    BOOST_CHECK_MESSAGE(mapstack.Get("d", value) == false, "mapstack.Get : shouldn't get value!");
}

BOOST_AUTO_TEST_CASE( multiple_entry_2 )
{
    mapstack.Set("a", 1);
    mapstack.Set("b", 2);
    mapstack.Push();
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 1);
    BOOST_CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 2);

    mapstack.Set("c", 3);
    mapstack.Set("d", 4);
    BOOST_CHECK_MESSAGE(mapstack.Get("c", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 3);
    BOOST_CHECK_MESSAGE(mapstack.Get("d", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 4);

    mapstack.Pop();
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 1);
    BOOST_CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 2);
    BOOST_CHECK_MESSAGE(mapstack.Get("c", value) == false, "mapstack.Get : shouldn't get value!");
    BOOST_CHECK_MESSAGE(mapstack.Get("d", value) == false, "mapstack.Get : shouldn't get value!");

    mapstack.Clear();
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == false, "mapstack.Get : shouldn't get value!");
    BOOST_CHECK_MESSAGE(mapstack.Get("b", value) == false, "mapstack.Get : shouldn't get value!");
    BOOST_CHECK_MESSAGE(mapstack.Get("c", value) == false, "mapstack.Get : shouldn't get value!");
    BOOST_CHECK_MESSAGE(mapstack.Get("d", value) == false, "mapstack.Get : shouldn't get value!");

}

BOOST_AUTO_TEST_CASE( multiple_entry_3 )
{
    mapstack.Set("a", 1);
    mapstack.Set("b", 2);
    mapstack.Push();
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 1);
    BOOST_CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 2);


    mapstack.Set("a", 11);
    mapstack.Set("b", 12);
    mapstack.Set("c", 3);
    mapstack.Set("d", 4);
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 11);
    BOOST_CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 12);
    BOOST_CHECK_MESSAGE(mapstack.Get("c", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 3);
    BOOST_CHECK_MESSAGE(mapstack.Get("d", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 4);

    mapstack.Pop();
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 1);
    BOOST_CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 2);
}

BOOST_AUTO_TEST_CASE( multiple_entry_4 )
{
    mapstack.Set("a", 1);
    mapstack.Set("b", 2);
    mapstack.Push();
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 1);
    BOOST_CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 2);

    mapstack.Set("a", 11);
    mapstack.Set("b", 12);
    mapstack.Set("c", 3);
    mapstack.Set("d", 4);
    BOOST_CHECK_MESSAGE(mapstack.Get("a", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 11);
    BOOST_CHECK_MESSAGE(mapstack.Get("b", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 12);
    BOOST_CHECK_MESSAGE(mapstack.Get("c", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 3);
    BOOST_CHECK_MESSAGE(mapstack.Get("d", value) == true, "mapstack.Get : can't get value!");
    BOOST_CHECK_EQUAL(value, 4);

    string result[4] = {"a", "b", "c", "d"};
    int i = 0;

    for(Mapstack<string, int>::current_const_iterator it = mapstack.CurrentBegin();
            it != mapstack.CurrentEnd();
            ++it) {
        BOOST_CHECK_EQUAL(*it, result[i]);
        ++i;
    }

    mapstack.Pop();
    i = 0;

    for(Mapstack<string, int>::current_const_iterator it = mapstack.CurrentBegin();
            it != mapstack.CurrentEnd();
            ++it) {
        BOOST_CHECK_EQUAL(*it, result[i]);
        ++i;
    }

    mapstack.Pop();

    for(Mapstack<string, int>::current_const_iterator it = mapstack.CurrentBegin();
            it != mapstack.CurrentEnd();
            ++it) {
        BOOST_CHECK_MESSAGE(false, "Current list shouldn't have any element here!");
    }
}

BOOST_AUTO_TEST_SUITE_END()
