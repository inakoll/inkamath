#include <doctest/doctest.h>

#include "inkamath/interpreter.hpp"
#include "inkamath/number.hpp"
#include "inkamath/transcript.hpp"

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
    Interpreter<Number> interpreter;
    interpreter.Directory(data_dir());

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
TEST_CASE("locals") {
    check_transcript("locals.ink");
}
TEST_CASE("conditional") {
    check_transcript("conditional.ink");
}
TEST_CASE("sequences") {
    check_transcript("sequences.ink");
}
TEST_CASE("series") {
    check_transcript("series.ink");
}
TEST_CASE("exact") {
    check_transcript("exact.ink");
}
TEST_CASE("decimals") {
    check_transcript("decimals.ink");
}
TEST_CASE("bignum") {
    check_transcript("bignum.ink");
}
TEST_CASE("floor") {
    check_transcript("floor.ink");
}
TEST_CASE("logic") {
    check_transcript("logic.ink");
}
TEST_CASE("terms") {
    check_transcript("terms.ink");
}
TEST_CASE("vectors") {
    check_transcript("vectors.ink");
}
TEST_CASE("latex") {
    check_transcript("latex.ink");
}
TEST_CASE("limits") {
    check_transcript("limits.ink");
}
TEST_CASE("imaginary") {
    check_transcript("imaginary.ink");
}
TEST_CASE("grad") {
    check_transcript("grad.ink");
}
TEST_CASE("tex") {
    check_transcript("tex.ink");
}
TEST_CASE("sign") {
    check_transcript("sign.ink");
}
TEST_CASE("prelude") {
    check_transcript("prelude.ink");
}
TEST_CASE("models") {
    check_transcript("models.ink");
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
TEST_CASE("tensor") {
    check_transcript("tensor.ink");
}
TEST_CASE("gradcells") {
    check_transcript("gradcells.ink");
}
TEST_CASE("sizes") {
    check_transcript("sizes.ink");
}
TEST_CASE("history") {
    check_transcript("history.ink");
}
TEST_CASE("readme") {
    check_readme();
}

// Not a transcript entry: the transcript format strips a trailing '\r', so
// the one case that matters -- a file written on Windows, piped to the REPL,
// whose every line ends with one -- cannot be written as one
// (DESIGN.md, C58).
TEST_CASE("a carriage return is whitespace") {
    Interpreter<Number> interpreter;
    CHECK(transcript::eval(interpreter, "1+1\r") == "2");
    CHECK(transcript::eval(interpreter, "f(x)=x+1\r") == "f(x)=x+1");
}

// Not a transcript entry: the inputs are thousands of characters wide. Both
// of these used to exhaust the C++ stack and kill the process, so before the
// limit this case took the whole suite with it (DESIGN.md, C20). Depth is
// bounded, not length: a flat literal is as deep as one of its cells.
TEST_CASE("depth limit") {
    Interpreter<Number> interpreter;
    const std::string   deep = "error: expression nests more than 1000 deep";

    const std::string nested = std::string(8000, '(') + "1" + std::string(8000, ')');
    CHECK(transcript::eval(interpreter, nested) == deep);

    std::string flat = "1";
    for (int i = 0; i < 40000; ++i) flat += "+1";
    CHECK(transcript::eval(interpreter, flat) == deep);

    CHECK(transcript::eval(interpreter, std::string(5000, '-') + "1") == deep);
    CHECK(transcript::eval(interpreter, std::string(999, '(') + "1" + std::string(999, ')')) ==
          "1");

    // Lines the limit must not reject.
    CHECK(transcript::eval(interpreter, std::string(400, '(') + "1" + std::string(400, ')')) ==
          "1");
    std::string column = "[1";
    for (int i = 2; i <= 20000; ++i) column += "; " + std::to_string(i);
    CHECK(transcript::eval(interpreter, column + "][20000]") == "20000");

    // Twenty million of them, refused for their length before they cost
    // memory: the answer was always this, the cost was 1.8 GB (DESIGN.md, C57).
    CHECK(transcript::eval(interpreter, std::string(20000000, '(')) ==
          "error: expression is longer than 100000 tokens");
}

// ParseEqualExpr used to rewind and re-parse its speculative left-hand side,
// which nested into O(2^depth): this took nine seconds at depth 24 and did not
// finish at 40 (DESIGN.md, C32). Also not a transcript entry -- the
// assertion is that it returns at all.
TEST_CASE("nested calls parse in linear time") {
    Interpreter<Number> interpreter;
    CHECK(transcript::eval(interpreter, "f(x)=x") == "f(x)=x");

    std::string nested = "1";
    for (int i = 0; i < 200; ++i) nested = "f(" + nested + ")";
    CHECK(transcript::eval(interpreter, nested) == "1");
}

TEST_SUITE_END();

// A model's input of more than one cell, which the interpreter does not have
// yet. Marked may_fail so the gap is reported on every run without gating
// CI, and never recorded: a specification taken from the code it judges is
// worth nothing.
TEST_SUITE_BEGIN("spec");

TEST_CASE("inputs" * doctest::may_fail()) {
    check_spec("spec/inputs.ink");
}

TEST_SUITE_END();
