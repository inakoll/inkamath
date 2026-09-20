#ifndef INKAMATH_TEST_TRANSCRIPT_HPP
#define INKAMATH_TEST_TRANSCRIPT_HPP

// A golden-file format that is literally an interpreter session:
//
//     # a comment
//     >> 1+1
//     2
//
//     >> [pi, e]
//     3.14159265 2.71828183
//
// Lines starting with ">> " are fed to the interpreter; everything up to the
// next ">> " (or EOF) is the expected output. Lines starting with '#' at
// column 0 are comments and are preserved when re-recording.
//
// One interpreter instance runs a whole file, so definitions persist between
// entries exactly as they do in a real session.

#include "inkamath/diagnostic.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace transcript {

struct Item {
    bool        is_entry = false;  // false => verbatim comment/blank line
    int         line     = 0;      // 1-based line of the ">> " in the source file
    std::string text;              // comment text, or the expression for an entry
    std::string expected;          // expected output, trailing whitespace stripped
};

inline std::string rstrip(std::string s) {
    while (!s.empty() &&
           (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    return s;
}

inline std::vector<Item> parse(std::istream& in) {
    std::vector<Item> items;
    std::string       line;
    int               lineno = 0;

    while (std::getline(in, line)) {
        ++lineno;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.rfind(">>", 0) == 0) {
            Item item;
            item.is_entry = true;
            item.line     = lineno;
            item.text     = rstrip(line.substr(2));
            // leading space after ">>" is cosmetic
            if (!item.text.empty() && item.text.front() == ' ') item.text.erase(0, 1);
            items.push_back(std::move(item));
        } else if (!items.empty() && items.back().is_entry && (line.empty() || line[0] != '#')) {
            items.back().expected += line;
            items.back().expected += '\n';
        } else {
            Item item;
            item.line = lineno;
            item.text = line;
            items.push_back(std::move(item));
        }
    }

    for (Item& item : items) {
        if (item.is_entry) item.expected = rstrip(std::move(item.expected));
    }
    return items;
}

inline std::string render(const std::vector<Item>& items) {
    std::ostringstream out;
    for (const Item& item : items) {
        if (!item.is_entry) {
            out << item.text << '\n';
            continue;
        }
        out << ">> " << item.text << '\n';
        if (!item.expected.empty()) out << item.expected << '\n';
        out << '\n';
    }
    return out.str();
}

// Complex pow is NaN at 0^0, which poisons any series whose first term is x^0
// (MODERNIZATION.md, C5). The sign of that NaN is unspecified and differs
// between GCC and Clang, so it is normalised here to keep the goldens
// portable. Delete this once C5 is fixed.
inline std::string normalize(std::string s) {
    for (std::string::size_type i = s.find("-nan"); i != std::string::npos;
         i                        = s.find("-nan", i + 3)) {
        s.erase(i, 1);
    }
    return s;
}

template <typename Interpreter>
std::string eval(Interpreter& interpreter, const std::string& expression) {
    std::ostringstream out;
    typename Interpreter::Result result = interpreter.Eval(expression);
    if (const Diagnostic* error = std::get_if<Diagnostic>(&result)) {
        out << "error: " << error->message;
    } else {
        out << std::get<typename Interpreter::matrix_type>(result);
    }
    return normalize(rstrip(out.str()));
}

}  // namespace transcript

#endif  // INKAMATH_TEST_TRANSCRIPT_HPP
