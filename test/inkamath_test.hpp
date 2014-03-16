#ifndef INKAMATH_TEST_HPP
#define INKAMATH_TEST_HPP

#include "getlines.hpp"
#include "numeric_interface.hpp"
#include "interpreter.hpp"

#include <boost/test/detail/unit_test_parameters.hpp>
#include <boost/test/output_test_stream.hpp>
#include <boost/test/unit_test_monitor.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <complex>
#include <future>
#include <thread>
#include <chrono>
#include <exception>
#include <cstdlib> // std::_Exit

namespace bt = boost::unit_test;
namespace bttools = boost::test_toolbox;

void onTerminate() noexcept
{
    std::cout << "Unrecoverable error.\nThe test runner has stopped durring the tests." << std::endl;
    std::_Exit( EXIT_FAILURE );
}

const auto installed{ std::set_terminate(&onTerminate) };

struct InkamathFixture {
    Interpreter<std::complex<double>> interpreter;
    // inkamath_test 3rd parameter alias for convenience
    const bool match = true;
    const bool record = false;

    void inkamath_test(const std::string& infilepath,
                       const std::string& outfilepath,
                       bool matching) {
        std::ifstream input{infilepath};
        std::ifstream output{outfilepath};
        BOOST_CHECK_MESSAGE(input, "Failed to open test data.");

        bttools::output_test_stream test_stream{outfilepath, matching};
        size_t line_number = 1;
        for(auto& line : getlines(input)) {
            if(!line.empty() && line[0] != '#') {
                auto before = test_stream.tellp();
                auto asynceval = std::async(std::launch::async, [&]() {
                    test_stream << interpreter.Eval(line);
                });
                if(asynceval.wait_for(std::chrono::seconds(1))
                    != std::future_status::ready) {
                    BOOST_CHECK_MESSAGE(false,
                    "in file : " << infilepath << "(" << std::to_string(line_number) << ")\n" <<
                    "The interpreter timed out for :\n" << line);
                    // We can't terminate this non-cooperative thread
                    // and pass to the next test.
                    // Terminate the process instead of waiting death (stack overflow or whatever..)
                    std::terminate();
                    // Todo : find a way to stop a cooperative interpreter thread.
                }
                std::string out;
                auto eval_size = test_stream.tellp()-before;
                out.resize(eval_size);
                if(matching) {
                    BOOST_CHECK_MESSAGE(output, "Failed to open test data.");
                    output.read(&out[0], eval_size);
                    BOOST_WARN_MESSAGE(eval_size == output.gcount(), "Failed to keep up the output reference file.");
                }

                BOOST_CHECK_MESSAGE(test_stream.match_pattern(),
                                        "in file : " << infilepath << "(" << std::to_string(line_number) << ")\n" <<
                                        "Failed to match the evaluation of the expression : " << line << "\n" <<
                                        "Evaluation : \n" << toString(interpreter.Eval(line)) <<
                                        "Expecting : \n" << out);

            }
            ++line_number;
        }
    }
};

#endif // INKAMATH_TEST_HPP
