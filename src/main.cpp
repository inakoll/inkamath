#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "inkamath/interpreter.hpp"
#include "inkamath/number.hpp"
#include "inkamath/numeric_interface.hpp"

#include "line_editor.hpp"

// Usage examples live in test/data/*.ink -- they are literal sessions.

using namespace std;

// An unclosed bracket means the line is not finished, which is what makes a
// printed matrix typeable back: it prints over several lines and reads as one.
// Nothing here can hide a bracket -- there are no strings, and '#' comments to
// the end of the line.
static bool unclosed(const string& text)
{
    int depth = 0;
    for(char c : text.substr(0, text.find('#')))
    {
        if(c == '[' || c == '(') ++depth;
        if(c == ']' || c == ')') --depth;
    }
    return depth > 0;
}

using Interp  = Interpreter<Number>;
using Outcome = LineEditor::Outcome;

static const char help[] = R"(Usage: inkamath [options] [file...]

Runs the files in order and exits; with no file, reads standard input.
At a terminal the prompt edits the line and keeps its history.

  -i          read standard input after the files
  --echo      print each input before its answer, as a transcript
  --version   print the version and exit
  --help      print this and exit

A file whose first line that is not blank or a comment starts with '>>'
is a transcript, and only its '>>' lines are read.
)";

static string rstrip(string text) {
    text.erase(text.find_last_not_of(" \t\r\n") + 1);
    return text;
}

// Only spacing, or only a comment: not an input, in a file or from a pipe.
static bool blank(const string& line) {
    const size_t first = line.find_first_not_of(" \t\r");
    return first == string::npos || line[first] == '#';
}

// A line of a file, and whether it is the whole of an input.
struct Queued {
    string line;
    bool   whole;
};

// A file's inputs. In a transcript they are its '>>' lines, each the whole of an
// input as the recorder evaluated it, and the answers it records are left for
// 'diff' to compare.
static vector<Queued> inputs(istream& in) {
    vector<string> lines;
    for (string line; getline(in, line);) lines.push_back(line);
    vector<Queued> queued;
    const auto     first = find_if_not(lines.begin(), lines.end(), blank);
    if (first == lines.end() || first->rfind(">>", 0) != 0) {
        for (const string& line : lines) queued.push_back({line, false});
        return queued;
    }
    for (const string& line : lines) {
        if (line.rfind(">>", 0) != 0) continue;
        const string entry = line.substr(2);
        queued.push_back({rstrip(entry.rfind(' ', 0) == 0 ? entry.substr(1) : entry), true});
    }
    return queued;
}

static string render(const Interp::Result& result) {
    ostringstream out;
    if (const Diagnostic* error = get_if<Diagnostic>(&result)) {
        out << "error: " << error->message;
    } else if (const Echo* echo = get_if<Echo>(&result)) {
        out << echo->text;
    } else {
        out << get<Interp::matrix_type>(result);
    }
    return rstrip(out.str());
}

int main(int argc, char* argv[]) {
    bool           echo = false, then_input = false;
    vector<string> files;
    for (int i = 1; i < argc; ++i) {
        const string arg = argv[i];
        if (arg == "--help") {
            cout << help;
            return 0;
        }
        if (arg == "--version") {
            cout << "inkamath " INKAMATH_VERSION "\n";
            return 0;
        }
        if (arg == "--echo") {
            echo = true;
        } else if (arg == "-i") {
            then_input = true;
        } else if (arg.size() > 1 && arg[0] == '-') {
            cerr << "inkamath: unknown option '" << arg << "'\nTry 'inkamath --help'.\n";
            return 2;
        } else {
            files.push_back(arg);
        }
    }

    // Every file is read before anything runs, so that a command line which
    // cannot be carried out stops before it has done half of it.
    vector<Queued> queued;
    for (const string& name : files) {
        ifstream in(name);
        if (!in) {
            cerr << "inkamath: cannot open '" << name << "'\n";
            return 2;
        }
        const vector<Queued> lines = inputs(in);
        queued.insert(queued.end(), lines.begin(), lines.end());
    }

    const bool     read_input = files.empty() || then_input;
    const bool     terminal   = read_input && RawTerminal::Interactive();
    size_t         next       = 0;
    bool           typed      = false;  // whether the line came from the terminal
    bool           whole      = false;  // whether the line is a transcript's entry
    bool           greeted    = false;
    vector<string> history;

    // The files, then standard input: from a person, edited at the prompt, or
    // from a pipe, where spacing and comments are skipped as they are in files.
    const auto read_line = [&](const string& prompt, string& line) {
        for (;;) {
            typed = whole = false;
            if (next < queued.size()) {
                line  = queued[next].line;
                whole = queued[next++].whole;
            } else if (!read_input) {
                return Outcome::end_of_input;
            } else if (terminal) {
                if (!greeted) cout << "inkamath " INKAMATH_VERSION "\n\n";
                greeted = typed = true;
                return ReadLine(prompt, history, line);
            } else if (!getline(cin, line)) {
                return Outcome::end_of_input;
            }
            if (whole || !blank(line)) return Outcome::done;
        }
    };

    Interp p;
    bool   failed = false;
    for (;;) {
        string  s;
        Outcome read = read_line(">> ", s);
        if (read == Outcome::end_of_input) break;
        if (read == Outcome::cancelled) continue;
        if (s == "q") break;  // quit interpreter

        while (!whole && unclosed(s)) {
            string more;
            read = read_line(".. ", more);
            if (read != Outcome::done) break;  // end of input: let it fail as written
            s += " " + more;
        }
        // Ctrl-C at a continuation drops the whole line, as it does at the first.
        if (read == Outcome::cancelled) continue;

        const Interp::Result result = p.Eval(s);
        const string         answer = render(result);
        if (typed) {
            // The whole of a line that continued, so that recalling it gives
            // back something that reads.
            if (!s.empty() && (history.empty() || history.back() != s)) history.push_back(s);
            cout << answer << "\n\n";
            continue;
        }
        failed = failed || holds_alternative<Diagnostic>(result);
        if (echo) cout << ">> " << s << '\n';
        cout << answer << (echo ? "\n\n" : "\n");
    }
    return failed ? 1 : 0;
}
