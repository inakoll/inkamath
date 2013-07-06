#include "pmath.hpp"

double fact(double n)
{
    double i = 1;
    double n1 = 1;
    while (n >= i)
    {
        n1 *= i;
        ++i;
    }
    return n1;
}
