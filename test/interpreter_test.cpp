#include <doctest/doctest.h>

#include "transcript.hpp"

#include "inkamath/interpreter.hpp"

#include <complex>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

// Replays a transcript file through a fresh interpreter, checking each entry.
// With INKAMATH_RECORD=1 the file is rewritten from the observed output
// instead -- use `cmake --build build --target record_goldens`.
void check_transcript(const std::string& name, bool recordable = true) {
    const std::filesystem::path path = data_dir() / name;

    std::ifstream in(path);
    REQUIRE_MESSAGE(in.good(), "cannot open transcript ", path.string());
    std::vector<transcript::Item> items = transcript::parse(in);
    in.close();

    REQUIRE_MESSAGE(!items.empty(), "transcript is empty: ", path.string());

    Interpreter<std::complex<double>> interpreter;
    const bool                        record = recordable && recording();

    for (transcript::Item& item : items) {
        if (!item.is_entry) continue;

        const std::string actual = transcript::eval(interpreter, item.text);
        if (record) {
            item.expected = actual;
        } else {
            INFO(path.filename().string(), ":", item.line, ": >> ", item.text);
            CHECK(actual == item.expected);
        }
    }

    if (record) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        REQUIRE_MESSAGE(out.good(), "cannot rewrite transcript ", path.string());
        out << transcript::render(items);
        MESSAGE("recorded ", path.string());
    }
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

TEST_SUITE_END();

// The language we are building, not the language we have (MODERNIZATION.md,
// phase 2). These fail until the redesign lands, so they are marked may_fail:
// they report the gap on every run without gating CI. They are never
// recordable -- recording a specification from current behaviour would defeat
// its purpose.
TEST_SUITE_BEGIN("spec");

TEST_CASE("definitions" * doctest::may_fail()) {
    check_transcript("spec/definitions.ink", false);
}
TEST_CASE("sequences" * doctest::may_fail()) {
    check_transcript("spec/sequences.ink", false);
}
TEST_CASE("diagnostics" * doctest::may_fail()) {
    check_transcript("spec/diagnostics.ink", false);
}

TEST_CASE("recursion" * doctest::may_fail()) {
    check_transcript("spec/recursion.ink", false);
}

TEST_SUITE_END();
