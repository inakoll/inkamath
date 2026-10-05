#ifndef INKAMATH_TRANSCRIPT_HPP
#define INKAMATH_TRANSCRIPT_HPP

// A golden-file format that is literally an interpreter session:
//
//     # a comment
//     >> 1+1
//     2
//
//     >> [pi, e]
//     3.14159265 2.71828183
//
// Lines starting with ">> " are fed to the interpreter, with the ".. " lines
// that follow as their continuation; everything up to the next ">> " (or EOF)
// is the expected output. Lines starting with '#' at column 0 are comments
// and are preserved when re-recording.
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

// Keeps only the contents of ``` fenced blocks, so a Markdown file can be
// read as a transcript. A '#' between blocks ends the preceding entry, which
// otherwise swallows the prose that follows it.
inline std::string fenced_lines(std::istream& in) {
    std::ostringstream out;
    std::string        line;
    bool               inside = false;

    while (std::getline(in, line)) {
        if (line.rfind("```", 0) == 0) {
            if (inside) out << "#\n";
            inside = !inside;
            continue;
        }
        if (inside) out << line << '\n';
    }
    return out.str();
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
        } else if (line.rfind("..", 0) == 0 && !items.empty() && items.back().is_entry &&
                   items.back().expected.empty()) {
            std::string more = rstrip(line.substr(2));
            if (!more.empty() && more.front() == ' ') more.erase(0, 1);
            items.back().text += '\n' + more;
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
        // A blank line ends an entry before what follows it, not the file.
        if (&item != &items.front() && items[&item - &items.front() - 1].is_entry) out << '\n';
        if (!item.is_entry) {
            out << item.text << '\n';
            continue;
        }
        out << ">> ";
        for (const char c : item.text) {
            if (c == '\n') {
                out << "\n.. ";
            } else {
                out << c;
            }
        }
        out << '\n';
        if (!item.expected.empty()) out << item.expected << '\n';
    }
    return out.str();
}

// An answer as a session prints it.
template <typename Interpreter>
std::string answer(const Interpreter& interpreter, const typename Interpreter::Result& result) {
    std::ostringstream out;
    if (const Diagnostic* error = std::get_if<Diagnostic>(&result)) {
        out << "error: " << error->message;
    } else if (const Echo* echo = std::get_if<Echo>(&result)) {
        out << echo->text;
    } else {
        out << interpreter.Answer(std::get<typename Interpreter::matrix_type>(result));
    }
    return rstrip(out.str());
}

template <typename Interpreter>
std::string eval(Interpreter& interpreter, const std::string& expression) {
    return answer(interpreter, interpreter.Eval(expression));
}

}  // namespace transcript

#endif  // INKAMATH_TRANSCRIPT_HPP
