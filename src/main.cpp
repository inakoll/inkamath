#include <iostream>
#include <iomanip>
#include <complex>
#include <variant>
#include "inkamath/interpreter.hpp"
#include "inkamath/numeric_interface.hpp"

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

int main(void) {
    cout << "inkamath 0.8\n" << endl;
    using Interp = Interpreter<complex<double>>;
    Interp p;

    for (;;) {
        string s;

        cout << ">> ";
        if (!getline(cin, s)) break;  // end of input
        if (s == "q") break;          // quit interpreter

        while (unclosed(s)) {
            string more;
            cout << ".. ";
            if (!getline(cin, more)) break;  // end of input: let it fail as written
            s += " " + more;
        }

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
