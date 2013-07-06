#ifndef H_BEST_PROMOTION
#define H_BEST_PROMOTION

template <typename T>
struct best_promotion
{
    typedef T type;
};

template <>
struct best_promotion<char>
{
    typedef long type;
};

template <>
struct best_promotion<unsigned char>
{
    typedef unsigned long type;
};

template <>
struct best_promotion<short int>
{
    typedef long type;
};

template <>
struct best_promotion<unsigned short int>
{
    typedef unsigned long type;
};

template <>
struct best_promotion<int>
{
    typedef long type;
};

template <>
struct best_promotion<unsigned int>
{
    typedef unsigned long type;
};

template <>
struct best_promotion<float>
{
    typedef double type;
};

template <>
struct best_promotion<long double>
{
    typedef double type;
};

#endif
