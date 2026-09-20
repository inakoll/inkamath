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

TEST_SUITE_END();
