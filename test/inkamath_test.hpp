#ifndef INKAMATH_TEST_HPP
#define INKAMATH_TEST_HPP

// set this global to false to generate test reference for all the tests
// prefer using the third parameter of inkamath_test macro
// or use with caution
static bool MATCHING = true;

// use this helper class instead to generate test reference test by test.
struct Record {
    Record() : prev_matching(MATCHING) {
        using boost::unit_test::log_warnings;
        if( boost::unit_test::runtime_config::log_level() < log_warnings )
            boost::unit_test::unit_test_log.set_threshold_level( log_warnings );
        MATCHING=false;
        BOOST_WARN_MESSAGE(true, "Generation of test reference.");
    }
    ~Record() {MATCHING=prev_matching;}
private:
    bool prev_matching;
};


#define inkamath_test_imp(infilepath, outfilepath)          \
    using boost::test_toolbox::output_test_stream;          \
    std::ifstream input{infilepath};                        \
    std::ifstream output{outfilepath};                          \
    BOOST_CHECK_MESSAGE(input, "Failed to open test data.");    \
    if(MATCHING)                                                \
       BOOST_CHECK_MESSAGE(output, "Failed to open test data.");\
    output_test_stream test_stream{outfilepath, MATCHING};      \
    size_t line_number = 1;                                     \
    for(auto& line : getlines(input)) {                         \
        auto before = test_stream.tellp();                      \
        test_stream << interpreter.Eval(line);                  \
        std::string out;                                        \
        auto eval_size = test_stream.tellp()-before;            \
        out.resize(eval_size);                                  \
        output.read(&out[0], eval_size);                        \
        if(MATCHING)                                           \
            BOOST_WARN_MESSAGE(eval_size == output.gcount(), "Failed to keep up the output reference file.");\
        BOOST_CHECK_MESSAGE(test_stream.match_pattern(),                                                     \
                                "in file : " << infilepath << "(" << std::to_string(line_number) << ")\n" << \
                                "Failed to match the evaluation of the expression : " << line << "\n" <<     \
                                "Evaluation : \n" << toString(interpreter.Eval(line)) <<                     \
                                "Expecting : \n" << out);                                                    \
        ++line_number;                                                                                       \
    }

#define inkamath_test(infilepath, outfilepath, mode)    \
    if(mode) {                                          \
        inkamath_test_imp(infilepath, outfilepath)      \
    }                                                   \
    else {                                              \
        Record record{}; (void)record;                  \
        inkamath_test_imp(infilepath, outfilepath)      \
    }                                                   \

// inkamath_test 3rd parameter alias for convenience
static bool match = true;
static bool record = false;


#endif // INKAMATH_TEST_HPP
