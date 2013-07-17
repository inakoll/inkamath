#include "mapstack.hpp"
#include <iostream>
#include <limits>


#define BOOST_TEST_MODULE inkamath_mapstack
#include <boost/test/unit_test.hpp>

using namespace std;


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
	// Initialy there is no element in the current list
    for(Mapstack<string, int>::current_const_iterator it = mapstack.CurrentBegin();
            it != mapstack.CurrentEnd();
            ++it) {
        BOOST_CHECK_MESSAGE(false, "Current list shouldn't have any element here!");
    }
	
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

	// Mapstack current keys are {"a", "b", "c", "d"}
    string result[4] = {"a", "b", "c", "d"};
    int i = 0;
    for(auto it = mapstack.CurrentBegin();
            i < 4;
            ++it, ++i) {
        BOOST_CHECK_EQUAL(*it, result[i]);
    }

	// After pop mapstack current keys are {"a", "b"}
    mapstack.Pop();
    i = 0;
	auto it = mapstack.CurrentBegin();
    for(;
            i < 2;
            ++it, ++i) {
        BOOST_CHECK_EQUAL(*it, result[i]);
    }
	for(;
            it != mapstack.CurrentEnd();
            ++it) {
		std::cout << *it << std::endl;
		BOOST_CHECK_MESSAGE(false, "Current list shouldn't have any element here!");
    }

	// After pop mapstack keys are empty again
    mapstack.Pop();
    for(Mapstack<string, int>::current_const_iterator it = mapstack.CurrentBegin();
            it != mapstack.CurrentEnd();
            ++it) {
        BOOST_CHECK_MESSAGE(false, "Current list shouldn't have any element here!");
    }
}

BOOST_AUTO_TEST_CASE( iteration )
{
	std::vector<std::pair<const std::string, int>> input 
					{ {"a", 1}
					, {"b", 2}
					, {"c", 3}
					, {"d", 4} };
					
	// initializing the mapstack with input
	for(auto elm : input) {
		mapstack.Set(elm.first, elm.second);
	}
		
	// iterating over the two container to check for equality	
	auto input_it = input.begin();
	for(auto mapstack_it = mapstack.begin(); 
			mapstack_it != mapstack.end();
			++mapstack_it, ++input_it) 
	{
		BOOST_CHECK_MESSAGE(*mapstack_it == *input_it, "mapstack and input should be equel here!");
	}
	
	mapstack.Push();
	
	// Pushing new values
	std::vector<std::pair<const std::string, int>> push  =
					{ {"a", 11}
					, {"b", 12} };
	for(auto elm : push) {
		mapstack.Set(elm.first, elm.second);
	}
	
	// mapstack content should now be equal to :
	std::vector<std::pair<const std::string, int>> output
					{ {"a", 11}
					, {"b", 12}
					, {"c", 3}
					, {"d", 4} };	
	// iterating over the two container to check for equality	
	auto output_it = output.begin();
	for(auto mapstack_it = mapstack.begin(); 
			mapstack_it != mapstack.end();
			++mapstack_it, ++output_it) 
	{
		BOOST_CHECK_MESSAGE(*mapstack_it == *output_it, "mapstack and output should be equel here!");
	}
	
	// After pop, mapstack should retrieve it's previous values
	mapstack.Pop();
	// iterating over the two container to check for equality	
	input_it = input.begin();
	for(auto mapstack_it = mapstack.begin(); 
			mapstack_it != mapstack.end();
			++mapstack_it, ++input_it) 
	{
		BOOST_CHECK_MESSAGE(*mapstack_it == *input_it, "mapstack and input should be equel here!");
	}
	
	
}
BOOST_AUTO_TEST_SUITE_END()
