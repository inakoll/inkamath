#include <iostream>
#include <iomanip>
#include <complex>
#include <variant>
#include "inkamath/interpreter.hpp"
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

static LineEditor::Outcome read_line(bool interactive, const string& prompt,
                                     const vector<string>& history, string& line) {
    if (interactive) return ReadLine(prompt, history, line);
    cout << prompt;
    return getline(cin, line) ? LineEditor::Outcome::done : LineEditor::Outcome::end_of_input;
}

int main(void) {
    cout << "inkamath 0.8\n" << endl;
    using Interp = Interpreter<complex<double>>;
    Interp p;

    const bool     interactive = RawTerminal::Interactive();
    vector<string> history;

    for (;;) {
        string s;

        LineEditor::Outcome read = read_line(interactive, ">> ", history, s);
        if (read == LineEditor::Outcome::end_of_input) break;
        if (read == LineEditor::Outcome::cancelled) continue;
        if (s == "q") break;  // quit interpreter

        while (unclosed(s)) {
            string more;
            read = read_line(interactive, ".. ", history, more);
            if (read != LineEditor::Outcome::done) break;  // end of input: let it fail as written
            s += " " + more;
        }
        // Ctrl-C at a continuation drops the whole line, as it does at the first.
        if (read == LineEditor::Outcome::cancelled) continue;
        // The whole of a line that continued, so that recalling it gives back
        // something that reads.
        if (interactive && !s.empty() && (history.empty() || history.back() != s))
            history.push_back(s);

        Interp::Result result = p.Eval(s);
        if (const Diagnostic* error = get_if<Diagnostic>(&result)) {
            cout << "error: " << error->message;
        } else if (const Echo* echo = get_if<Echo>(&result)) {
            cout << echo->text << endl;
        } else {
            cout << get<Interp::matrix_type>(result);
        }
        cout << endl << endl;
    }
    return 0;
}
