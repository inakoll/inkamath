#define BOOST_TEST_MODULE inkamath_test

#include <boost/test/unit_test.hpp>
#include <boost/test/detail/unit_test_parameters.hpp>
#include <boost/test/output_test_stream.hpp>

#include "getlines.hpp"
#include "numeric_interface.hpp"
#include "interpreter.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <stdexcept>
#include <complex>

#include "inkamath_test.hpp"


/**
 ***************************************
 * For real world usage examples see main.cpp file first
 *
 ***************************************
 */


struct InkamathFixture {
    Interpreter<std::complex<double>> interpreter;
};


BOOST_FIXTURE_TEST_SUITE(inkamath_tests, InkamathFixture)

BOOST_AUTO_TEST_CASE( inkamath_1 ) {
    // Pass "record" (false) instead of "match" (true)
    // as 3rd paramter to generate output file instead of matching
    inkamath_test("../inkamath/test/data/input1.txt",
                  "../inkamath/test/data/output1.txt",
                  match);
}

BOOST_AUTO_TEST_CASE( inkamath_2 ) {
    // Pass "record" (false) instead of "match" (true)
    // as 3rd paramter to generate output file instead of matching
    inkamath_test("../inkamath/test/data/input2.txt",
                  "../inkamath/test/data/output2.txt",
                  record);
}

BOOST_AUTO_TEST_SUITE_END()

