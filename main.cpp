#include <iostream>
#include <iomanip>
#include <complex>
#include <queue>
#include "interpreter.hpp"
#include "numeric_interface.hpp"

using namespace std;

int main(void)
{
    cout << "\nInkaMath 0.7\n" << endl;
	Interpreter<complex<double> > p;
	
	for(;;)
    {
		string s;
		
		cout << ">> ";
		getline(cin,s);

        if(s=="q") break;

        cout << p.Eval(s) << endl << endl;
    }
	return 0;
}

void inkamath_test()
{
	Interpreter<complex<double> > p;
    queue<string> s;
	s.push("[pi, e]");
	s.push("A=[a a; a a]");
	s.push("a=[1 2;3 4]");
	s.push("A");
	s.push("exp(x)_n=exp(x)_(n-1)+x^n/!n");
	s.push("exp(1)");
	s.push("cos(x)=(exp(i*x)+exp(-i*x))/2");
	s.push("sin(x)=(exp(i*x)-exp(-i*x))/(2*i)");
	s.push("sin(pi/6)");

    while(!s.empty())
    {
		cout << ">> " << s.front() << endl;

        if(s.front()=="q") break;

        cout << p.Eval(s.front()) << endl << endl;
		s.pop();
    }
}
