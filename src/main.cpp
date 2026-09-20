#include <iostream>
#include <iomanip>
#include <complex>
#include <variant>
#include "inkamath/interpreter.hpp"
#include "inkamath/numeric_interface.hpp"

// Usage examples live in test/data/*.ink -- they are literal sessions.

using namespace std;

int main(void)
{
    cout << "inkamath 0.8\n" << endl;
    using Interp = Interpreter<complex<double>>;
    Interp p;
	
	for(;;)
    {
		string s;
		
		cout << ">> ";
        if(!getline(cin,s)) break; // end of input
        if(s=="q") break; // quit interpreter

        Interp::Result result = p.Eval(s);
        if (const Diagnostic* error = get_if<Diagnostic>(&result))
        {
            cout << "error: " << error->message;
        }
        else if (const Echo* echo = get_if<Echo>(&result))
        {
            cout << echo->text << endl;
        }
        else
        {
            cout << get<Interp::matrix_type>(result);
        }
        cout << endl << endl;
    }
	return 0;
}
