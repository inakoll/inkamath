#ifndef H_TOKEN
#define H_TOKEN

#include <string>
#include <utility>

enum Type
{
    Add,Mult,Min,Div,Pow,Fact,Equal, Sub,
    Val,Func, Ref,
    LPar,RPar,LBra,RBra,
    Space, Comma, Semico, Query
};

template <typename T>
struct Token
{
    Token(Type t, std::string lexeme, T val = T())
        : type(t), value(val), text(std::move(lexeme)) {}

    Type type;
    T value;           // meaningful for Val
    std::string text;  // exactly what the user typed; the name, for Func
};

#endif
