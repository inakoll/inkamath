#include <doctest/doctest.h>

#include "line_editor.hpp"

#include <string>
#include <vector>

// The keys a terminal sends, fed as bytes: no terminal needed. What the
// terminal itself does is not tested here, and on Windows not tested at all.

TEST_SUITE_BEGIN("line_editor");

namespace {

const std::string left = "\x1b[D", right = "\x1b[C", up = "\x1b[A", down = "\x1b[B";
const std::string home = "\x1b[H", end = "\x1b[F", del = "\x1b[3~";

struct Typed {
    std::string         line;
    LineEditor::Outcome outcome;
};

Typed type(const std::string& keys, const std::vector<std::string>& history = {}) {
    LineEditor editor(history);
    for (char c : keys) {
        const LineEditor::Outcome outcome = editor.Feed(c);
        if (outcome != LineEditor::Outcome::editing) return {editor.Line(), outcome};
    }
    return {editor.Line(), LineEditor::Outcome::editing};
}

}  // namespace

// C60: an arrow at the old prompt arrived as "\x1b[D", and its '[' opened a
// continuation that swallowed the lines after it.
TEST_CASE("an arrow moves the cursor and leaves nothing in the line") {
    CHECK(type("1+2" + left + left + "0\r").line == "10+2");
    CHECK(type("12" + left + right + "3\r").line == "123");
}

TEST_CASE("a key it does not know is swallowed whole") {
    CHECK(type("1\x1bOP\x1b[5~\x1b[15;2~+2\r").line == "1+2");  // F1, PageUp, Shift-F5
    CHECK(type("1\x1bx+2\r").line == "1+2");                    // Alt-x
}

TEST_CASE("the cursor stops at either end") {
    CHECK(type(left + "1\r").line == "1");
    CHECK(type("1" + right + right + "2\r").line == "12");
}

TEST_CASE("home and end, in both spellings") {
    CHECK(type("2+3" + home + "10*" + end + "+1\r").line == "10*2+3+1");
    CHECK(type("2+3\x01"
               "10*\x05+1\r")
              .line == "10*2+3+1");
    CHECK(type("2+3\x1b[1~10*\x1b[4~+1\r").line == "10*2+3+1");
}

TEST_CASE("backspace, delete and Ctrl-U") {
    CHECK(type("123\x7f\r").line == "12");
    CHECK(type("123\x08\r").line == "12");
    CHECK(type("\x7f"
               "1\r")
              .line == "1");
    CHECK(type("123" + left + left + del + "\r").line == "13");
    CHECK(type("123" + del + "\r").line == "123");
    CHECK(type("12" + left +
               "\x15"
               "5\r")
              .line == "52");
}

TEST_CASE("up and down walk the history and keep the line being written") {
    const std::vector<std::string> history = {"1", "2"};
    CHECK(type(up + "\r", history).line == "2");
    CHECK(type(up + up + up + "\r", history).line == "1");
    CHECK(type(up + up + down + "\r", history).line == "2");
    CHECK(type("3*" + up + down + "4\r", history).line == "3*4");
    CHECK(type(down + "5\r", history).line == "5");
    CHECK(type(up + "\x7f"
                    "7\r",
               history)
              .line == "7");
    CHECK(type(up + "\r").line == "");
}

TEST_CASE("a tab is a space, and what the language cannot read stays out") {
    CHECK(type("\xc3\xa9+1\r").line == "+1");  // é
    CHECK(type("[1\t2]\r").line == "[1 2]");
}

TEST_CASE("Enter, Ctrl-C and Ctrl-D end a line differently") {
    CHECK(type("1+1\r").outcome == LineEditor::Outcome::done);
    CHECK(type("1+1\n").outcome == LineEditor::Outcome::done);
    CHECK(type("1+\x03").outcome == LineEditor::Outcome::cancelled);
    CHECK(type("\x04").outcome == LineEditor::Outcome::end_of_input);
    const Typed deleted = type("12" + left + left + "\x04\r");
    CHECK(deleted.outcome == LineEditor::Outcome::done);
    CHECK(deleted.line == "2");
}

// A ten-column terminal and a three-column prompt, so that seven characters
// fill the first row.
TEST_CASE("a line that fits is drawn on its row") {
    size_t row = 0;
    CHECK(Render(">> ", "abc", 3, 10, row) == "\r\x1b[J>> abc");
    CHECK(row == 0);
    CHECK(Render(">> ", "abc", 1, 10, row) == "\r\x1b[J>> abc\r\x1b[4C");
    CHECK(row == 0);
}

TEST_CASE("a line that wraps is redrawn from its first row") {
    size_t row = 0;
    CHECK(Render(">> ", "abcdefghij", 10, 10, row) == "\r\x1b[J>> abcdefghij");
    CHECK(row == 1);
    CHECK(Render(">> ", "abcdefghijk", 5, 10, row) ==
          "\x1b[1A\r\x1b[J>> abcdefghijk\x1b[1A\r\x1b[8C");
    CHECK(row == 0);
    CHECK(Render(">> ", "abcdefghijk", 11, 10, row) == "\r\x1b[J>> abcdefghijk");
    CHECK(row == 1);
}

TEST_CASE("a row filled to its last column moves on to the next") {
    size_t row = 0;
    CHECK(Render(">> ", "abcdefg", 7, 10, row) == "\r\x1b[J>> abcdefg\r\n");
    CHECK(row == 1);
    CHECK(Render(">> ", "abcdefg", 0, 10, row) == "\x1b[1A\r\x1b[J>> abcdefg\r\n\x1b[1A\r\x1b[3C");
    CHECK(row == 0);
}

TEST_SUITE_END();
