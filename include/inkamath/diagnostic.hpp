#ifndef H_DIAGNOSTIC
#define H_DIAGNOSTIC

#include <string>

// What the interpreter reports instead of printing to std::cout and handing
// back a zero. Carries only a message today; a source span arrives with
// positions on tokens (MODERNIZATION.md, phase 3 item 2).
struct Diagnostic {
    std::string message;
};

#endif // H_DIAGNOSTIC
