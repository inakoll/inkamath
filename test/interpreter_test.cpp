#include <doctest/doctest.h>

#include "transcript.hpp"

#include "inkamath/interpreter.hpp"

#include <complex>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

TEST_SUITE_BEGIN("interpreter");

namespace {

std::filesystem::path data_dir() {
    return std::filesystem::path(INKAMATH_TEST_DATA_DIR);
}

bool recording() {
    const char* value = std::getenv("INKAMATH_RECORD");
    return value != nullptr && *value != '\0' && std::string(value) != "0";
}

// Replays the entries through a fresh interpreter. When recording, the
// observed output replaces the expectation instead of being checked.
void replay(std::vector<transcript::Item>& items, const std::string& label, bool record) {
    Interpreter<std::complex<double>> interpreter;

    for (transcript::Item& item : items) {
        if (!item.is_entry) continue;

        const std::string actual = transcript::eval(interpreter, item.text);
        if (record) {
            item.expected = actual;
        } else {
            INFO(label, ":", item.line, ": >> ", item.text);
            CHECK(actual == item.expected);
        }
    }
}

// With INKAMATH_RECORD=1 the file is rewritten from the observed output --
// use `cmake --build build --target record_goldens`.
void check_transcript(const std::string& name) {
    const std::filesystem::path path = data_dir() / name;

    std::ifstream in(path);
    REQUIRE_MESSAGE(in.good(), "cannot open transcript ", path.string());
    std::vector<transcript::Item> items = transcript::parse(in);
    in.close();

    REQUIRE_MESSAGE(!items.empty(), "transcript is empty: ", path.string());

    const bool record = recording();
    replay(items, path.filename().string(), record);

    if (record) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        REQUIRE_MESSAGE(out.good(), "cannot rewrite transcript ", path.string());
        out << transcript::render(items);
        MESSAGE("recorded ", path.string());
    }
}

// A specification, replayed but never recorded (CLAUDE.md, section 3).
void check_spec(const std::string& name) {
    const std::filesystem::path path = data_dir() / name;

    std::ifstream in(path);
    REQUIRE_MESSAGE(in.good(), "cannot open transcript ", path.string());
    std::vector<transcript::Item> items = transcript::parse(in);
    in.close();

    REQUIRE_MESSAGE(!items.empty(), "transcript is empty: ", path.string());
    replay(items, path.filename().string(), false);
}

// Every fenced block in README.md is a session, and they run as one. The
// documentation cannot drift from the interpreter without failing here. It is
// never recorded: prose is not ours to rewrite.
void check_readme() {
    const std::filesystem::path path = std::filesystem::path(INKAMATH_SOURCE_DIR) / "README.md";

    std::ifstream in(path);
    REQUIRE_MESSAGE(in.good(), "cannot open ", path.string());
    std::istringstream fenced(transcript::fenced_lines(in));
    std::vector<transcript::Item> items = transcript::parse(fenced);

    REQUIRE_MESSAGE(!items.empty(), "no fenced blocks in ", path.string());
    replay(items, path.filename().string(), false);
}

}  // namespace

TEST_CASE("basics") {
    check_transcript("basics.ink");
}
TEST_CASE("matrices") {
    check_transcript("matrices.ink");
}
TEST_CASE("references") {
    check_transcript("references.ink");
}
TEST_CASE("sequences") {
    check_transcript("sequences.ink");
}
TEST_CASE("errors") {
    check_transcript("errors.ink");
}
TEST_CASE("queries") {
    check_transcript("queries.ink");
}
TEST_CASE("recursion") {
    check_transcript("recursion.ink");
}
TEST_CASE("readme") {
    check_readme();
}

// Not a transcript entry: the inputs are thousands of characters wide. Both
// of these used to exhaust the C++ stack and kill the process, so before the
// token limit this case took the whole suite with it (MODERNIZATION.md, C20).
TEST_CASE("token limit") {
    Interpreter<std::complex<double>> interpreter;
    const std::string expected = "error: expression is longer than 1000 tokens";

    const std::string nested = std::string(8000, '(') + "1" + std::string(8000, ')');
    CHECK(transcript::eval(interpreter, nested) == expected);

    std::string flat = "1";
    for (int i = 0; i < 40000; ++i) flat += "+1";
    CHECK(transcript::eval(interpreter, flat) == expected);

    // A line the limit must not reject.
    CHECK(transcript::eval(interpreter, std::string(400, '(') + "1" + std::string(400, ')')) == "1");
}

// ParseEqualExpr used to rewind and re-parse its speculative left-hand side,
// which nested into O(2^depth): this took nine seconds at depth 24 and did not
// finish at 40 (MODERNIZATION.md, C32). Also not a transcript entry -- the
// assertion is that it returns at all.
TEST_CASE("nested calls parse in linear time") {
    Interpreter<std::complex<double>> interpreter;
    CHECK(transcript::eval(interpreter, "f(x)=x") == "f(x)=x");

    std::string nested = "1";
    for (int i = 0; i < 200; ++i) nested = "f(" + nested + ")";
    CHECK(transcript::eval(interpreter, nested) == "1");
}

TEST_SUITE_END();

// The language phase 8 is designing, not the language we have. Marked
// may_fail so the gap is reported on every run without gating CI, and never
// recorded: a specification taken from the code it judges is worth nothing.
TEST_SUITE_BEGIN("spec");

TEST_CASE("locals" * doctest::may_fail()) {
    check_spec("spec/locals.ink");
}
TEST_CASE("laziness" * doctest::may_fail()) {
    check_spec("spec/laziness.ink");
}

TEST_SUITE_END();
