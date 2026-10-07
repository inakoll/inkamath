#ifndef H_TOKEN
#define H_TOKEN

#include <string>
#include <utility>

enum Type {
    Add,
    Mult,
    Min,
    Div,
    Pow,
    Fact,
    Equal,
    Sub,
    Compare,
    Guard,
    Val,
    Func,
    Ref,
    LPar,
    RPar,
    LBra,
    RBra,
    Comma,
    Semico,
    Query,
    Approx,
    Quote,
    Dot
};

template <typename T>
struct Token
{
    Token(Type t, std::string lexeme, T val = T())
        : type(t), value(val), text(std::move(lexeme)) {}

    Type type;
    T value;           // meaningful for Val
    std::string text;  // exactly what the user typed; the name, for Func
    bool spaced = false;  // whether a space came before it; brackets index only what they touch
    // Spaced, and so beginning the next element of a list (C159).
    bool apart = false;
};

#endif
