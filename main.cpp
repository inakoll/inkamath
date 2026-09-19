#include <iostream>
#include <iomanip>
#include <complex>
#include "interpreter.hpp"
#include "numeric_interface.hpp"

// Usage examples live in test/data/*.ink -- they are literal sessions.

using namespace std;

int main(void)
{
    cout << "inkamath 0.8\n" << endl;
    Interpreter<complex<double>> p;
	
	for(;;)
    {
		string s;
		
		cout << ">> ";
		getline(cin,s);

        if(s=="q") break; // quit interpreter

        cout << p.Eval(s) << endl << endl;
    }
	return 0;
}
