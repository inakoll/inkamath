#ifndef H_DIAGNOSTIC
#define H_DIAGNOSTIC

#include <string>

// What the interpreter reports instead of printing to std::cout and handing
// back a zero. Carries only a message today; a source span arrives with
// positions on tokens (MODERNIZATION.md, phase 3 item 2).
struct Diagnostic {
    std::string message;
};

// The reply when an input is a statement rather than a value: a definition
// reports what it bound, as the user wrote it, and 'frac' the fraction.
struct Echo {
    std::string text;
};

#endif // H_DIAGNOSTIC
