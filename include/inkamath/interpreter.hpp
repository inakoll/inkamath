#ifndef H_PARSER
#define H_PARSER

#include <algorithm>
#include <array>
#include <cctype>  // isalpha
#include <concepts>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "inkamath/diagnostic.hpp"
#include "inkamath/expression.hpp"
#include "inkamath/expression_visitor.hpp"
#include "inkamath/inkamath_prelude.h"
#include "inkamath/latex.hpp"
#include "inkamath/matrix.hpp"
#include "inkamath/number.hpp"
#include "inkamath/numeric_interface.hpp"
#include "inkamath/pexpression.hpp"
#include "inkamath/reference_stack.hpp"
#include "inkamath/token.hpp"

// What the interpreter needs of the type it evaluates to. Stating it is the
// point of C9: sqrt was missing for complex and threw for Matrix, and nothing
// said so because nothing asked. Notably absent are zero() and one(), which
// Matrix has never had.
template <typename T>
concept Numeric =
    std::default_initializable<T>
    && requires(const T& a, const T& b, const typename T::value_type& cell,
                typename T::value_type& accumulator) {
    typename T::value_type;
    // The product accumulates into a cell, which no other requirement implies:
    // a number type that satisfied all of them still failed to compile, deep
    // inside Matrix rather than here (DESIGN.md, C44).
    { accumulator += cell } -> std::same_as<typename T::value_type&>;
    { T(cell) } -> std::same_as<T>;
    { T(Extent()) } -> std::same_as<T>;
    { a.Size() } -> std::convertible_to<Extent>;
    { a(size_t(1), size_t(1)) } -> std::convertible_to<typename T::value_type>;
    { a + b } -> std::convertible_to<T>;
    { a - b } -> std::convertible_to<T>;
    { a * b } -> std::convertible_to<T>;
    { a / b } -> std::convertible_to<T>;
    { -a } -> std::convertible_to<T>;
    { numeric_interface<T>::pow(a, b) } -> std::convertible_to<T>;
    { T(numeric_interface<T>::fact(a)) } -> std::same_as<T>;
    { numeric_interface<T>::abs(a) > 1.0 } -> std::convertible_to<bool>;
    { numeric_interface<T>::toInt(a) } -> std::convertible_to<int>;
    { numeric_interface<T>::toString(a) } -> std::convertible_to<std::string>;
    { numeric_interface<T>::inexact(a) } -> std::convertible_to<T>;
};

// What the lexer needs of the type it reads numbers into.
template <typename T>
concept Parsable = numeric_interface_parses<T>
    && requires(T& num, const char* begin, char*& end) {
    { numeric_interface<T>::parse(num, begin, end) } -> std::convertible_to<bool>;
};

// How many brackets, parentheses and braces a text leaves open: a line that
// leaves one open is not finished. A comment runs to the end of its line.
inline int Unclosed(const std::string& text, std::string_view open = "([{",
                    std::string_view close = ")]}") {
    int  depth   = 0;
    bool comment = false;
    for (const char c : text) {
        if (c == '\n') comment = false;
        if (c == '#') comment = true;
        if (comment) continue;
        if (open.find(c) != std::string_view::npos) ++depth;
        if (close.find(c) != std::string_view::npos) --depth;
    }
    return depth;
}

template <Parsable T, Numeric U = Matrix<T> >
class Interpreter
{
public:
    Interpreter();

    typedef typename U::value_type value_type;
    typedef U matrix_type;

    // A value, or why there isn't one. std::expected is C++23; this becomes
    // one mechanically if the project ever moves.
    using Result = std::variant<U, Echo, Diagnostic>;

    Result Eval(const std::string& s);

    // An answer as this session shows it.
    [[nodiscard]] std::string Show(const U& value) const {
        return U::toString(
            value, [this](const T& x) { return numeric_interface<T>::toString(x, digits_); });
    }

    // An answer, and a comment that reading it back ignores where it was
    // approximated past the bound (DESIGN.md, phase 13).
    [[nodiscard]] std::string Answer(const U& value) const {
        return Show(value) + (numeric_interface<U>::approximated(value)
                                  ? "  # approximated past a thousand digits"
                                  : "");
    }

    void ResetInterpreter(void);

    // What the session has defined, for the compiler to read.
    [[nodiscard]] const ReferenceStack<U>& Definitions() const { return stack_; }
    ReferenceStack<U>&                     Definitions() { return stack_; }

    // A file's names, read as 'use' reads them, from beside the directory set.
    std::shared_ptr<const Scope<U>> Read(const std::string& stem) { return Load(stem); }

    // Where 'use' looks for a file named at the prompt: beside the file being
    // run, or the working directory.
    void Directory(std::filesystem::path directory) { directory_ = std::move(directory); }

private:
    Result                                            Run(const std::string& s);
    std::string                                       Use(const std::string& s);
    std::shared_ptr<Scope<U>>                         Load(const std::string& name);
    std::string                                       DefineModel(const std::string& text);
    std::pair<std::string, std::shared_ptr<Model<U>>> ParseModel(const std::string& text);
    PExpression<U>                                    ParseMembers(PExpression<U> object);

    void Lexer(const std::string& s);
    void Number_Lexer(const std::string& s, size_t& i);
    void Reference_Lexer(const std::string& s, size_t& i);

    PExpression<U> ParseAll(size_t first = 0);
    PExpression<U> Parse();
    PExpression<U> ParseEqualExpr();
    // `lead`, where given, is a leading operand the caller has already parsed.
    // Without it ParseEqualExpr has to rewind and parse its speculative
    // left-hand side a second time, which nests into O(2^depth).
    PExpression<U> ParseOrExpr(PExpression<U> lead = PExpression<U>());
    PExpression<U> ParseAndExpr(PExpression<U> lead = PExpression<U>());
    PExpression<U> ParseCompareExpr(PExpression<U> lead = PExpression<U>());
    PExpression<U> ParseAddExpr(PExpression<U> lead = PExpression<U>());
    PExpression<U> ParseMultExpr(PExpression<U> lead = PExpression<U>());
    PExpression<U> ParsePowExpr(PExpression<U> lead = PExpression<U>());
    PExpression<U> ParseMatrix();
    PExpression<U> MatrixLiteral(std::vector<PExpression<U>>& mat, std::vector<size_t>& size);
    PExpression<U> ParseSimpleExpr(bool postfix = true);
    PExpression<U> ParseCell(PExpression<U> matrix);
    PExpression<U> ParseQuotes(PExpression<U> e);
    PExpression<U> ParsePostfix(PExpression<U> e);

    // Inside a matrix literal, and inside an argument list, a space between
    // two expressions separates them. Everywhere else it means nothing, which
    // is what lets '[1 2;3 4][2,1]' be an index rather than two blocks.
    PExpression<U> ParseParameters();
    PExpression<U> ParseSubExpr();
    PExpression<U> ParseLimit();
    PExpression<U> ParseSeries();
    PExpression<U> ParseGrad();
    std::string ParseQuery();
    std::string    Tex();

    // The reserved words. A limit is a property of a definition, so 'lim'
    // takes a name rather than an expression; 'sum' and 'prod' begin a series.
    static bool IsLimit(const Token<T>& token) {return token.type == Func && token.text == "lim";}
    static bool IsSeries(const Token<T>& token) {
        return token.type == Func && (token.text == "sum" || token.text == "prod");
    }
    static bool IsWord(const Token<T>& token, const char* word) {
        return token.type == Func && token.text == word;
    }
    static bool IsGrad(const Token<T>& token) { return IsWord(token, "grad"); }
    static bool IsLogic(const Token<T>& token) {
        return IsWord(token, "and") || IsWord(token, "or");
    }
    // 'frac', 'digits' and 'tex' are about the whole line, so they begin it.
    static bool BeginsLine(const Token<T>& token) {
        return IsWord(token, "frac") || IsWord(token, "digits") || IsWord(token, "tex");
    }
    bool   DefinesReserved() const;
    Result Digits(const std::string& s);

    // One line cannot be allowed to exhaust the C++ stack. What a line
    // provokes is bounded by its depth, not its length: the parser's
    // recursion by its nesting, below, and every walk of the tree by the
    // tree's (Expression::max_depth). Measured: the sanitizer build overflows
    // at about 2000 nested parentheses and the release build at about 8000,
    // so 1000 reaches neither -- on an 8 MB stack, which CMakeLists.txt gives
    // Windows too. The length is bounded only so that a line refused costs
    // little memory (DESIGN.md, C57).
    static constexpr size_t max_tokens = 100000;
    size_t                  nesting_   = 0;

    // Printing costs some 25 ns and 20 bytes a digit in every cell, measured:
    // a thousand keeps an answer to a page and a 100x100 matrix to a second.
    static constexpr int max_digits = 1000;

    bool AtEnd() const {return m_i >= m_tokens.size();}
    const Token<T>& Peek() const {return m_tokens[m_i];}

    // What the user typed, without the trailing comment the lexer stops at.
    static std::string AsWritten(const std::string& source)
    {
        std::string text = source.substr(0, source.find('#'));
        while(!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();
        return text;
    }

    // Messages are lower case, unpunctuated and quote what the user typed;
    // the caller adds the "error: " prefix.
    template <typename... Parts>
    [[noreturn]] static void Fail(const Parts&... parts)
    {
        std::ostringstream message;
        (message << ... << parts);
        throw std::runtime_error(message.str());
    }

    std::vector< Token<T> > m_tokens;
    size_t m_i = 0;

    // Whether the innermost bracket open is a list's, a matrix literal's or
    // an argument list's, where a sign with a space before it and none after
    // it begins the next element.
    bool listed_ = false;

    PExpression<U> m_E;
    ReferenceStack<U> stack_;
    int               digits_ = numeric_interface_precision;

    std::filesystem::path directory_ = ".";
    // The files used, each read once; null while one is being read.
    std::map<std::string, std::shared_ptr<Scope<U>>> files_;
};

// Included bare beneath the session, as the built-ins are: seen from every
// scope, and replaced by a session for itself alone (DESIGN.md, phase
// 15). Not 'round', whose rule is the model's to choose.
//
// exp, log and tanh by the operations a compiled step performs on the same
// doubles (DESIGN.md): reduced by a power of two exactly, k ln 2 taken as
// 355/512 less a correction (Cody and Waite), rounded once where the
// reduction ends, then a polynomial of fixed degree. exp takes 2^k in two
// halves, as 2^k overflows or vanishes where e^x does not; log reduces by
// ilogb, thirteen guarded steps from 2^-4096, and folds its mantissa into
// [sqrt(2)/2, sqrt(2)); tanh is -m/(m + 2), m = e^(-2x) - 1, which does not
// cancel near 0. sin and cos reduce by k pi/2, pi/2 in five parts so that
// every product is exact for |k| < 2^20 and every subtraction that cancels
// is too, and pick sin r, cos r or a negative by k + c mod 4, x + pi/2 being
// inexact; refused past 2^20, where the products round. abs, max and min are
// README's, a guard each: at a tie the first argument's slope.
inline constexpr const char* prelude[] = {
    "ceil(x) = -floor(-x)",
    "mod(a, b) = a - b*floor(a/b)",
    "exp(x) = expk(x, floor(x*1.4426950408889634 + 1/2))",
    "exp(x) | x > 1000 = exp(1000)",
    "exp(x) | x < -1000 = exp(-1000)",
    "expk(x, k) = (1 + expp(~(x - k*355/512 + k*2.1219444005469057e-4)))"
    "*2^(k - floor(k/2))*2^floor(k/2)",
    "expp(r) = r*(1 + r*(1/2 + r*(1/6 + r*(1/24 + r*(1/120 + r*(1/720 + r*(1/5040 "
    "+ r*(1/40320 + r*(1/362880 + r*(1/3628800 + r*(1/39916800 + r*(1/479001600 "
    "+ r*(1/6227020800 + r/87178291200)))))))))))))",
    "ilogb(x) = ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, "
    "ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, ilogbs(x, -4096, 4096), 2048), "
    "1024), 512), 256), 128), 64), 32), 16), 8), 4), 2), 1)",
    "ilogb(x) | x <= 0 = 1/0",
    "ilogbs(x, k, s) = k",
    "ilogbs(x, k, s) | k + s > 3321 = k",
    "ilogbs(x, k, s) | x >= 2^(k + s) = k + s",
    "log(x) = logk(x, ilogb(x))",
    "log(x) | x <= 0 = 1/0",
    "log(x) | 2*x == x = x",
    "logk(x, k) = logm(x/2^k, k)",
    "logm(m, k) = logs(~((m - 1)/(m + 1)), k)",
    "logm(m, k) | m*m > 2 = logs(~((m/2 - 1)/(m/2 + 1)), k + 1)",
    "logs(s, k) = k*355/512 + (2*s*logp(s*s) - k*~2.1219444005469057e-4)",
    "logp(z) = 1 + z*(1/3 + z*(1/5 + z*(1/7 + z*(1/9 + z*(1/11 + z*(1/13 + z*(1/15 "
    "+ z*(1/17 + z*(1/19 + z/21)))))))))",
    "tanh(x) = tanhp(x)",
    "tanh(x) | x < 0 = -tanhp(-x)",
    "tanhp(x) = tanhk(-2*x, floor(-2*x*1.4426950408889634 + 1/2))",
    "tanhp(x) | x > 20 = ~1",
    "tanhk(y, k) = tanhe(2^k - 1 + 2^k*expp(~(y - k*355/512 + k*2.1219444005469057e-4)))",
    "tanhe(m) = -m/(m + 2)",
    "abs(x) | x < 0 = -x",
    "abs(x) | x >= 0 = x",
    "max(a, b) = a",
    "max(a, b) | a < b = b",
    "min(a, b) = a",
    "min(a, b) | b < a = b",
    "sin(x) = sinr(x, floor(x*0.6366197723675814 + 1/2), 0)",
    "sin(x) | abs(x) > 2^20 = 1/0",
    "cos(x) = sinr(x, floor(x*0.6366197723675814 + 1/2), 1)",
    "cos(x) | abs(x) > 2^20 = 1/0",
    "sinr(x, k, c) = sink(~(x - k*3217/2048 + k*2391/2^29 + k*8029421003/2^63 "
    "+ k*1987263209/2^96 - k*7744522442262977/2^156), mod(k + c, 4))",
    "sink(r, j) = sinp(r)",
    "sink(r, j) | j > 2 = -cosp(r)",
    "sink(r, j) | j > 1 = -sinp(r)",
    "sink(r, j) | j > 0 = cosp(r)",
    "sinp(r) = r - r*sins(r*r)",
    "sins(z) = z*(1/6 - z*(1/120 - z*(1/5040 - z*(1/362880 - z*(1/39916800 - z*(1/6227020800 "
    "- z*(1/1307674368000 - z/355687428096000)))))))",
    "cosp(r) = cosw(r*r)",
    "cosw(z) = 1 - z/2 + z*z*(1/24 - z*(1/720 - z*(1/40320 - z*(1/3628800 - z*(1/479001600 "
    "- z*(1/87178291200 - z*(1/20922789888000 - z/6402373705728000)))))))",
};

template <Parsable T, Numeric U>
Interpreter<T, U>::Interpreter() {
    const typename ReferenceStack<U>::Into builtins(stack_, stack_.Builtins());
    for (const char* line : prelude) (void)Run(line);
    // Each function of the header is its definition's operations on the same
    // doubles (DESIGN.md), so it is called where the definition would answer
    // a double too.
    if constexpr (std::is_same_v<T, Number>) {
        const auto& names = stack_.Builtins().names;
        const std::array<std::pair<const Reference<U>*, double (*)(double)>, 4> functions{{
            {names.at("exp").get(), inkamath_prelude_exp},
            {names.at("tanh").get(), inkamath_prelude_tanh},
            {names.at("log").get(), inkamath_prelude_log},
            {names.at("ilogb").get(), inkamath_prelude_ilogb},
        }};
        stack_.compiled = [this, functions](const Reference<U>& f, const U& x) -> std::optional<U> {
            const auto found = std::find_if(functions.begin(), functions.end(),
                                            [&](const auto& each) { return each.first == &f; });
            // Every run of --check walks: the one its guards listen to, and
            // the disturbed ones.
            if (found == functions.end() || !x.IsScalar() || stack_.guards || Number::disturbed)
                return {};
            const auto    c = found->second;
            const Number& a = x(1, 1);
            const auto    z = a.Inexact();
            if (a.exact() || Number::approximated(a) || z.imag() != 0 || !std::isfinite(z.real()) ||
                ((c == inkamath_prelude_log || c == inkamath_prelude_ilogb) && !(z.real() > 0)))
                return {};
            const double y = c(z.real());
            if (c == inkamath_prelude_ilogb) return U(Number(static_cast<long long>(y)));
            return U(Number(y));
        };
    }
    ResetInterpreter();
}

template <Parsable T, Numeric U>
void Interpreter<T,U>::ResetInterpreter()
{
    m_E.reset();
    m_tokens.clear();
    m_i = 0;
}

template <Parsable T, Numeric U>
void Interpreter<T,U>::Lexer(const std::string& s)
{
    size_t i = 0;
    for (i=0; i < s.length(); i++)
    {
        const size_t start = i, before = m_tokens.size();
        switch (s[i])
        {
        case '(':
            m_tokens.push_back(Token<T>(LPar, std::string(1, s[i])));
            break;
        case ')':
            m_tokens.push_back(Token<T>(RPar, std::string(1, s[i])));
            break;
        case '[':
            m_tokens.push_back(Token<T>(LBra, std::string(1, s[i])));
            break;
        case ']':
            m_tokens.push_back(Token<T>(RBra, std::string(1, s[i])));
            break;
        case ',':
            m_tokens.push_back(Token<T>(Comma, std::string(1, s[i])));
            break;
        case ';':
            // A touching ';;' stacks slices: one semicolon more than separates
            // rows, as Julia counts semicolons, one more for each axis.
            if (s.compare(i, 3, ";;;") == 0)
                Fail("a tensor has at most three indices, and ';;;' would give it a fourth");
            if (s.compare(i, 2, ";;") == 0) {
                m_tokens.push_back(Token<T>(Semico, ";;"));
                ++i;
            } else {
                m_tokens.push_back(Token<T>(Semico, std::string(1, s[i])));
            }
            break;
        case '+':
            m_tokens.push_back(Token<T>(Add, std::string(1, s[i])));
            break;
        case '-':
            m_tokens.push_back(Token<T>(Min, std::string(1, s[i])));
            break;
        case '*':
            m_tokens.push_back(Token<T>(Mult, std::string(1, s[i])));
            break;
        case '=':
            // '==' asks, '=' tells. One symbol for both is what made
            // 'f_n | n = 0 = 1' unreadable (DESIGN.md, phase 10).
            if(i + 1 < s.length() && s[i+1] == '=')
            {
                m_tokens.push_back(Token<T>(Compare, "=="));
                ++i;
            }
            else
            {
                m_tokens.push_back(Token<T>(Equal, std::string(1, s[i])));
            }
            break;
        case '<':
            // '<>' and not '!=': '!' is the prefix factorial.
            if(i + 1 < s.length() && (s[i+1] == '=' || s[i+1] == '>'))
            {
                m_tokens.push_back(Token<T>(Compare, std::string(1, s[i]) + s[i+1]));
                ++i;
            }
            else
            {
                m_tokens.push_back(Token<T>(Compare, std::string(1, s[i])));
            }
            break;
        case '>':
            if(i + 1 < s.length() && s[i+1] == '=')
            {
                m_tokens.push_back(Token<T>(Compare, ">="));
                ++i;
            }
            else
            {
                m_tokens.push_back(Token<T>(Compare, std::string(1, s[i])));
            }
            break;
        case '|':
            m_tokens.push_back(Token<T>(Guard, std::string(1, s[i])));
            break;
        case '/':
            m_tokens.push_back(Token<T>(Div, std::string(1, s[i])));
            break;
        case '^':
            m_tokens.push_back(Token<T>(Pow, std::string(1, s[i])));
            break;
        case '!':
            m_tokens.push_back(Token<T>(Fact, std::string(1, s[i])));
            break;
        case '_':
            m_tokens.push_back(Token<T>(Sub, std::string(1, s[i])));
            break;
        case '?':
            m_tokens.push_back(Token<T>(Query, std::string(1, s[i])));
            break;
        case '~':
            m_tokens.push_back(Token<T>(Approx, std::string(1, s[i])));
            break;
        case '\'':
            m_tokens.push_back(Token<T>(Quote, std::string(1, s[i])));
            break;
        case ' ':
        // A tab is what a pasted line is indented with, and a '\r' is what a
        // line written on Windows ends with. Neither was typed to be read. A
        // line continued inside a model's braces keeps its break.
        case '\t':
        case '\r':
        case '\n':
            break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
            this->Number_Lexer(s,i);
            break;
        case '.':
            // '.5' is a number, and 'g.y' reads y of g; a point anywhere else
            // is neither.
            if(i + 1 < s.length() && std::isdigit(static_cast<unsigned char>(s[i+1])))
            {
                this->Number_Lexer(s,i);
            } else if (i + 1 < s.length() && std::isalpha(static_cast<unsigned char>(s[i + 1]))) {
                m_tokens.push_back(Token<T>(Dot, "."));
            } else {
                Fail("unexpected character '", s[i], "'");
            }
            break;
        case '#': // inkamath comments
            // Not a return: a line that is only a comment must still reach
            // the empty check below, or the parser starts on no tokens.
            i = std::min(s.find('\n', i), s.length());
            break;
		default:
            if(std::isalpha(static_cast<unsigned char>(s[i])))
                    Reference_Lexer(s,i);
            else
            {
                Fail("unexpected character '", s[i], "'");
            }
        }
        if (m_tokens.size() > before)
            m_tokens[before].spaced =
                start > 0 && std::isspace(static_cast<unsigned char>(s[start - 1]));

        // Checked here and not after the loop: a line of twenty million
        // brackets cost 1.8 GB before the limit got a word in.
        if (m_tokens.size() > max_tokens)
        {
            Fail("expression is longer than ", max_tokens, " tokens");
        }
    }

    if (m_tokens.empty()) Fail("empty expression");
    for (size_t token = 1; token < m_tokens.size(); ++token) {
        if (BeginsLine(m_tokens[token])) Fail(m_tokens[token].text, " can only begin a line");
    }
}

template <Parsable T, Numeric U>
void Interpreter<T,U>::Number_Lexer(const std::string& s, size_t& i)
{
    T num;
    char* end = NULL;
    const size_t start = i;
    if(numeric_interface<T>::parse(num,&s[i],end))
    {
        i = end - &s[0] - 1;
        m_tokens.push_back(Token<T>(Val, s.substr(start, i + 1 - start), num));
    }
    else
    {
        Fail("cannot parse a number at '", s.substr(i), "'");
    }
}

template <Parsable T, Numeric U>
void Interpreter<T,U>::Reference_Lexer(const std::string &s, size_t& i)
{
    size_t s_i = i;
    while ((i < s.length()) && std::isalpha(static_cast<unsigned char>(s[i])))
    {
        ++i;
    }
    if (i!=s_i)
    {
        while (i < s.length() && std::isdigit(static_cast<unsigned char>(s[i])))
        {
            ++i;
        }
        m_tokens.push_back(Token<T>(Func, s.substr(s_i, i - s_i)));
        --i;
    }
    else
    {
        Fail("unexpected character '", s[i], "'");
    }

}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T, U>::ParseAll(size_t first) {
    m_i              = first;
    PExpression<U> e = Parse();
    if (!AtEnd())
    {
        // Two operands with nothing between them: '3(4)', '2pi', '1 2'.
        // Mathematics writes the multiplication by juxtaposition and this
        // language does not, so say which operator is missing rather than
        // only where the parse stopped.
        const Type next = Peek().type;
        if (next == LPar || next == LBra || next == Val || next == Ref || next == Func ||
            next == Approx || next == Fact) {
            Fail("unexpected '", Peek().text, "' -- the operator '*' is probably missing");
        }
        Fail("unexpected '", Peek().text, "'");
    }
    if (e == 0)
    {
        if(!m_tokens.empty())
            throw(std::logic_error("internal error: null expression"));
        else
            e = std::make_shared<ValExpression<U>>(U{});
    }
    return e;
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::Parse()
{
    if(!AtEnd() && Peek().type == Comma)
        ++m_i;
    return ParseEqualExpr();
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::ParseEqualExpr()
{
    PExpression<U> e,ref,params,expr,sub;
    if (!AtEnd() && Peek().type == Func && !IsLimit(Peek()) && !IsSeries(Peek()) &&
        !IsGrad(Peek()) && !IsLogic(Peek())) {
        const size_t signature_begin = m_i;
        std::string name = m_tokens[m_i++].text;
        ref = PExpression<U>(new RefExpression<U>(name));
        params = ParseParameters();
        sub = ParseSubExpr();
        if (!AtEnd() && Peek().type == Dot) {
            PExpression<U> member = ParseMembers(
                params || sub ? std::make_shared<FuncExpression<U>>(ref, params, sub) : ref);
            if (!AtEnd() && (Peek().type == Equal || Peek().type == Guard))
                stack_.Outside(dynamic_cast<const MemberExpression<U>&>(*member));
            return ParseOrExpr(ParsePostfix(member));
        }
        // On the left of a definition the brackets define cells, 'M[j<=2,
        // k<=2]' or 'M[1,2]'; anywhere else they read one.
        const PExpression<U> cell  = ParseCell(ref);
        const auto*          place = dynamic_cast<CellExpression<U>*>(cell.get());
        PExpression<U> guard;
        if (!AtEnd() && Peek().type == Guard)
        {
            ++m_i;
            guard = ParseOrExpr();
        }
        if (!AtEnd() && Peek().type == Equal)
        {
            // What names the clause, so that writing it again replaces that
            // clause rather than adding one: the left-hand side, as the
            // tokens spell it, which is the same however it was spaced.
            std::string signature;
            for(size_t token = signature_begin; token < m_i; ++token) {
                // Kept apart: joined, '[1 2]' and '[12]' are the same string,
                // and the two clauses become one (DESIGN.md, C55).
                signature += '\x1f';
                signature += m_tokens[token].text;
            }
            ++m_i;
            expr = Parse();
            if (params || sub || guard || place) {
                e.reset(new EqualExpression<U>(PExpression<U>(new FuncExpression<U>(
                                                   ref, params, sub, false, guard, signature,
                                                   place ? place->Row() : PExpression<U>(),
                                                   place ? place->Col() : PExpression<U>(),
                                                   place ? place->Slice() : PExpression<U>())),
                                               expr));
            } else {
                e.reset(new EqualExpression<U>(ref, expr));
            }
        }
        else {
            if(guard) {
                Fail("a guard belongs to a definition, as 'name | condition = value'");
            }
            // Not a definition after all. The left-hand side is a perfectly
            // good leading operand, so hand it on rather than rewinding: the
            // rewind re-parsed the parameters, and nesting squared the cost.
            if(params || sub) {
                ref.reset(new FuncExpression<U>(ref, params, sub));
            }
            // A name at the head of a line is parsed here, not in
            // ParseSimpleExpr, so the cell brackets and quotes are read here too.
            if (place) {
                ref = std::make_shared<CellExpression<U>>(ref, place->Row(), place->Col(),
                                                          place->Slice());
            }
            e = ParseOrExpr(ParsePostfix(ref));
        }
    } else {
        e = ParseOrExpr();
    }
    return e;
}

// Looser than a comparison, 'or' looser than 'and', as everywhere.
template <Parsable T, Numeric U>
PExpression<U> Interpreter<T, U>::ParseOrExpr(PExpression<U> lead) {
    PExpression<U> e = ParseAndExpr(lead);
    while (!AtEnd() && IsWord(Peek(), "or")) {
        ++m_i;
        e = std::make_shared<LogicExpression<U>>(false, e, ParseAndExpr());
    }
    return e;
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T, U>::ParseAndExpr(PExpression<U> lead) {
    PExpression<U> e = ParseCompareExpr(lead);
    while (!AtEnd() && IsWord(Peek(), "and")) {
        ++m_i;
        e = std::make_shared<LogicExpression<U>>(true, e, ParseCompareExpr());
    }
    return e;
}

// Looser than addition, tighter than a definition: 'a < b+c' compares a with
// the sum, and 'f(x) | x < 0 = ...' guards on the comparison.
inline Comparison AsComparison(const std::string& op)
{
    if(op == "<")  return Comparison::Less;
    if(op == ">")  return Comparison::Greater;
    if(op == "<=") return Comparison::LessEqual;
    if(op == ">=") return Comparison::GreaterEqual;
    if(op == "==") return Comparison::Equal;
    return Comparison::NotEqual;
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::ParseCompareExpr(PExpression<U> lead)
{
    PExpression<U> e = ParseAddExpr(lead);
    while (!AtEnd() && Peek().type == Compare)
    {
        const std::string& op = m_tokens[m_i++].text;
        e.reset(new CompareExpression<U>(AsComparison(op), e, ParseAddExpr()));
    }
    return e;
}

// A sign or a tilde on a literal is applied here, once, rather than at every
// evaluation: the 1 of `n-1` in a recurrence would be negated at each term.
template <typename Node, typename U, typename Apply>
PExpression<U> Unary(PExpression<U> operand, Apply apply) {
    if (const auto* literal = dynamic_cast<const ValExpression<U>*>(operand.get()))
        return std::make_shared<ValExpression<U>>(apply(literal->value));
    return std::make_shared<Node>(std::move(operand));
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::ParseAddExpr(PExpression<U> lead)
{
    PExpression<U> e = ParseMultExpr(lead);
    while (!AtEnd() && (Peek().type == Add || Peek().type == Min) )
    {
        if (listed_ && Peek().spaced && m_i + 1 < m_tokens.size() && !m_tokens[m_i + 1].spaced)
            break;
        if (m_tokens[m_i++].type == Add)
        {
            e.reset(new AddExpression<U>(e,ParseMultExpr()));
        }
        else
        {
            PExpression<U> tmp =
                Unary<NegExpression<U>>(ParseMultExpr(), [](const U& value) { return -value; });
            e.reset(new AddExpression<U>(e,tmp));
        }
    }
    return e;
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::ParseMultExpr(PExpression<U> lead)
{
    PExpression<U> e = ParsePowExpr(lead);
    while (!AtEnd() && (Peek().type == Mult || Peek().type == Div) )
    {
        if (m_tokens[m_i++].type == Mult)
        {
            e.reset(new MultExpression<U>(e,ParsePowExpr()));
        }
        else
        {
            e.reset(new DivExpression<U>(e,ParsePowExpr()));
        }
    }
    return e;
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::ParsePowExpr(PExpression<U> lead)
{
    PExpression<U> e = lead ? lead : ParseSimpleExpr();
    if (!AtEnd() && Peek().type == Pow)
    {
        ++m_i;
        e.reset(new PowExpression<U>(e,ParsePowExpr()));
    }
    return e;
}

template <typename T>
std::vector<PExpression<T>> make_matrix_array_from_vector(size_t n, size_t m,
                                                          std::vector<PExpression<T>>& mat,
                                                          std::vector<size_t>& size);

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T, U>::ParseMatrix() {
    std::vector<PExpression<U>> mat, slices;
    std::vector<size_t> size(1, 0);
    PExpression<U> e;

    while ((!AtEnd()) && (Peek().type != RBra) && (Peek().type != RPar))
    {
        switch (Peek().type)
        {
        case Semico :
            if (Peek().text == ";;") {
                slices.push_back(MatrixLiteral(mat, size));
                mat.clear();
                size.assign(1, 0);
            } else {
                size.push_back(0);
            }
            ++m_i;
            break;

        default: {
            const Setting<bool> listing(listed_, true);
            e = Parse();
            mat.push_back(e);
            ++size.back();
        }
        }
    }
    if (slices.empty()) return MatrixLiteral(mat, size);
    // A trailing ';;' ends the last slice rather than starting an empty one.
    if (size.size() > 1 || size[0] > 0) slices.push_back(MatrixLiteral(mat, size));
    return std::make_shared<TensorExpression<U>>(std::move(slices));
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T, U>::MatrixLiteral(std::vector<PExpression<U>>& mat,
                                                std::vector<size_t>&         size) {
    size_t n = size.size();
    size_t m = *std::max_element(size.begin(), size.end());
    if (m == 0)
    {
        // A matrix with no elements at all has no extent to give, and the
        // evaluator reads one per column.
        Fail("a matrix needs at least one element");
    }
    return std::make_shared<MatExpression<U>>(n, m, make_matrix_array_from_vector(n, m, mat, size));
}

template <typename T>
std::vector<PExpression<T>>
 make_matrix_array_from_vector(size_t n, size_t m, std::vector<PExpression<T>>& mat,
                           std::vector<size_t>& size)
{
    auto exprs = std::vector<PExpression<T>>(n*m);
    size_t prev = 0;
    for(size_t i = 0; i < n; ++i) {
        std::move(mat.begin()+prev, mat.begin()+prev+size[i], exprs.begin()+i*m);
        for(size_t j = size[i]; j <m; ++j) {
            exprs[i*m+j] = PExpression<T>(new ValExpression<T>(T(0)));
        }
        prev += size[i];
    }
    return exprs;
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T, U>::ParseSimpleExpr(bool postfix) {
    // Every recursion of the parser passes here: a bracket, a sign, a power.
    struct Nested {
        explicit Nested(size_t& nesting) : nesting_(nesting) {
            // Undone here, as no destructor runs for a constructor that throws.
            if (++nesting_ > Expression<U>::max_depth) {
                --nesting_;
                Fail("expression nests more than ", Expression<U>::max_depth, " deep");
            }
        }
        ~Nested() { --nesting_; }
        Nested(const Nested&)            = delete;
        Nested& operator=(const Nested&) = delete;

    private:
        size_t& nesting_;
    } nested(nesting_);
    PExpression<U> e,ref,param,sub;
    bool           indexable = false;  // a name, a call or a bracketed value, not a number
    std::string name;
    if (!AtEnd())
    {
        switch (Peek().type)
        {
        case Val:
            // The explicit conversion is D9 in the flesh: every literal
            // becomes a 1x1 matrix on the heap.
            e.reset(new ValExpression<U>(U(m_tokens[m_i++].value)));
			break;

        case Func:
            if (IsLogic(Peek())) Fail("expected a value before '", Peek().text, "'");
            if (IsLimit(Peek()))
            {
                ++m_i;
                e = ParseLimit();
                break;
            }
            if (IsSeries(Peek())) {
                e = ParseSeries();
                break;
            }
            if (IsGrad(Peek())) {
                e = ParseGrad();
                break;
            }
            ref.reset(new RefExpression<U>(m_tokens[m_i++].text));
            param = ParseParameters();
            sub = ParseSubExpr();
            if(param || sub) {
                e.reset(new FuncExpression<U>(ref,param,sub));
            }
            else {
                e = ref;
            }
            if (postfix) e = ParseMembers(e);
            indexable = true;
            break;

        case Add:
            // Unary plus is the identity, and binds as unary minus does.
            ++m_i;
            e = ParsePowExpr();
            break;

        case Min:
            // A power, not a product: the sign belongs to what follows it, so
            // '6/-2/3' is '(6/-2)/3' and '-2^2' is still -4. Binding the whole
            // multiplicative chain made the first of those -9 (C48).
            ++m_i;
            e = Unary<NegExpression<U>>(ParsePowExpr(), [](const U& value) { return -value; });
            break;

        case Fact:
            ++m_i;
            e.reset(new FactExpression<U>(ParsePowExpr()));
			break;

        case Approx:
            ++m_i;
            e = Unary<InexactExpression<U>>(ParsePowExpr(), [](const U& value) {
                return numeric_interface<U>::inexact(value);
            });
            break;

        case LPar: {
            ++m_i;
            const Setting<bool> grouped(listed_, false);
            e = Parse();
            if (!AtEnd() && Peek().type == RPar)
            {
                ++m_i;
            }
            else
            {
                Fail("missing ')' after '", m_tokens[--m_i].text, "'");
            }
            indexable = true;
            break;
        }

        case LBra:
            ++m_i;
            e = ParseMatrix();
            if (!AtEnd() && Peek().type == RBra)
            {
                ++m_i;
            }
            else
            {
                Fail("missing ']' after '", m_tokens[--m_i].text, "'");
            }
            indexable = true;
            break;

        default:
        case RPar:
            Fail("unexpected '", Peek().text, "'");
            break;
        }
        if (postfix) e = indexable ? ParsePostfix(e) : ParseQuotes(e);
    }
    else if(m_i != 0)
    {
        Fail("unexpected end of input after '", m_tokens[--m_i].text, "'");
    }
    return e;
}

// 'g.y_3', 'filters.lowpass(a = 1/2).v_3': names read in an instance or a file.
// Only a name or a call names one; a term is a value, and has no names.
template <Parsable T, Numeric U>
PExpression<U> Interpreter<T, U>::ParseMembers(PExpression<U> object) {
    while (!AtEnd() && Peek().type == Dot) {
        const Expression<U>* last = object.get();
        if (const auto* member = dynamic_cast<const MemberExpression<U>*>(last))
            last = member->Member().get();
        if (const auto* term = dynamic_cast<const FuncExpression<U>*>(last); term && term->m_e2())
            Fail("a term has no names; an instance or a file has, as 'g.y_3'");
        ++m_i;
        if (AtEnd() || Peek().type != Func) Fail("expected a name after '.'");
        const auto     name   = std::make_shared<RefExpression<U>>(m_tokens[m_i++].text);
        PExpression<U> params = ParseParameters();
        PExpression<U> sub    = ParseSubExpr();
        PExpression<U> member = name;
        if (params || sub) member = std::make_shared<FuncExpression<U>>(name, params, sub);
        object = std::make_shared<MemberExpression<U>>(object, member);
    }
    return object;
}

// Brackets that touch a value and quotes, in any order, 'r'[2]' or 'a[1]'',
// all before any operator.
template <Parsable T, Numeric U>
PExpression<U> Interpreter<T, U>::ParsePostfix(PExpression<U> e) {
    for (PExpression<U> before; before != e;) {
        before = e;
        e      = ParseQuotes(ParseCell(e));
    }
    return e;
}

// A quote binds to what it follows before any operator does, as Julia's does:
// 'a^2'' is 'a^(2')'.
template <Parsable T, Numeric U>
PExpression<U> Interpreter<T, U>::ParseQuotes(PExpression<U> e) {
    while (!AtEnd() && Peek().type == Quote) {
        ++m_i;
        e = std::make_shared<TransposeExpression<U>>(e);
    }
    return e;
}

// 'm[i,j]', or 'm[i]', a row. Brackets index only what they touch: a space
// before them separates blocks in a literal, as in '[a [3 4]]', and is a
// missing operator anywhere else.
template <Parsable T, Numeric U>
PExpression<U> Interpreter<T, U>::ParseCell(PExpression<U> matrix) {
    if (AtEnd() || Peek().type != LBra || Peek().spaced) {
        return matrix;
    }
    ++m_i;
    const Setting<bool>         index(listed_, false);
    std::vector<PExpression<U>> at{Parse()};
    while (!AtEnd() && Peek().type == Comma) {
        if (at.size() == 3)
            Fail("a tensor has at most three indices, and ",
                 matrix->Name().empty() ? "this" : matrix->Name(), " names four");
        ++m_i;
        at.push_back(Parse());
    }
    if (AtEnd() || Peek().type != RBra)
    {
        Fail("missing ']' after '", m_tokens[--m_i].text, "'");
    }
    ++m_i;
    if (at.size() == 3) return std::make_shared<CellExpression<U>>(matrix, at[1], at[2], at[0]);
    return std::make_shared<CellExpression<U>>(matrix, at[0],
                                               at.size() > 1 ? at[1] : PExpression<U>());
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::ParseParameters()
{
    PExpression<U> e;
    const size_t m_s = m_i;
    // 'gain()' is 'gain', an instance with every default.
    if (m_i + 1 < m_tokens.size() && Peek().type == LPar && m_tokens[m_i + 1].type == RPar) {
        m_i += 2;
        return e;
    }
    if (!AtEnd() && m_tokens[m_i++].type == LPar && !AtEnd() && Peek().type != RPar)
    {
        e = ParseMatrix();
        if (dynamic_cast<TensorExpression<U>*>(e.get()))
            Fail("arguments are separated by ',', not ';;'");
        if (AtEnd() || Peek().type != RPar)
            Fail("missing ')' after function parameters");
        ++m_i;
    }
    else
    {
        m_i=m_s;
    }
    return e;
}

// '?name' prints a definition back as it was written. It is a statement, not
// an expression: there is nothing to do with the answer but read it.
template <Parsable T, Numeric U>
std::string Interpreter<T,U>::ParseQuery()
{
    m_i = 1;
    if (AtEnd())
    {
        Fail("expected a name after '?'");
    }
    if (Peek().type != Func)
    {
        Fail("expected a name after '?', not '", Peek().text, "'");
    }
    const std::string name = m_tokens[m_i++].text;
    if (!AtEnd() && Peek().type == LPar)
    {
        Fail("'?' takes a name, not a call");
    }
    PExpression<U> sub = ParseSubExpr();
    if (!AtEnd())
    {
        Fail("unexpected '", Peek().text, "'");
    }
    return stack_.Describe(name, ParametersCall<U>(PExpression<U>(), sub));
}

// 'tex ?name': a definition as LaTeX (DESIGN.md, next in line).
template <Parsable T, Numeric U>
std::string Interpreter<T, U>::Tex() {
    if (m_tokens.size() < 2 || m_tokens[1].type != Query)
        Fail("tex shows a definition, as 'tex ?name'");
    if (m_tokens.size() < 3 || m_tokens[2].type != Func) Fail("expected a name after '?'");
    if (m_tokens.size() > 3) Fail("tex shows a whole definition, as 'tex ?name'");
    const auto definition = stack_.Find(m_tokens[2].text);
    if (!definition) Fail(m_tokens[2].text, " is not defined");
    if (definition->model) return Latex<U>::System(*definition, *stack_.Defaults(*definition));
    return Latex<U>::Definition(*definition);
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::ParseLimit()
{
    if (AtEnd())
    {
        Fail("expected a sequence name after 'lim'");
    }
    if (Peek().type != Func || IsSeries(Peek())) {
        Fail("expected a sequence name after 'lim', not '", Peek().text, "'");
    }
    PExpression<U> ref(new RefExpression<U>(m_tokens[m_i++].text));
    PExpression<U> param = ParseParameters();
    if (ParseSubExpr())
    {
        Fail("'lim' takes a sequence, not one of its terms");
    }
    return PExpression<U>(new FuncExpression<U>(ref, param, PExpression<U>(), true));
}

// 'sum_(k=1)^n body', as it is written on paper. The body is a term: it runs to
// the next '+' or '-', and a leading sign is part of it.
template <Parsable T, Numeric U>
PExpression<U> Interpreter<T, U>::ParseSeries() {
    const std::string word    = m_tokens[m_i++].text;
    const auto        example = word + "_(k=1)^n";
    const auto        expect  = [&](Type type) -> const std::string& {
        if (AtEnd()) Fail("expected an index after '", word, "', as in ", example);
        if (Peek().type != type || IsLimit(Peek()) || IsSeries(Peek()))
            Fail("expected an index after '", word, "', as in ", example, ", not '", Peek().text,
                 "'");
        return m_tokens[m_i++].text;
    };
    expect(Sub);
    expect(LPar);
    const std::string index = expect(Func);
    expect(Equal);
    const PExpression<U> lower = [&] {
        const Setting<bool> bound(listed_, false);
        return ParseCompareExpr();
    }();
    if (AtEnd() || Peek().type != RPar) Fail("missing ')' after '", m_tokens[m_i - 1].text, "'");
    ++m_i;

    // A number, a name or parentheses, and a name here is never a call:
    // 'n (k+1)' is the bound n and the body (k+1).
    PExpression<U> upper;
    if (!AtEnd() && Peek().type == Pow) {
        ++m_i;
        if (AtEnd()) Fail("expected the last index after '^', as in ", example);
        if (Peek().type == Val) {
            upper = std::make_shared<ValExpression<U>>(U(m_tokens[m_i++].value));
        } else if (Peek().type == Func && !IsLimit(Peek()) && !IsSeries(Peek())) {
            upper = std::make_shared<RefExpression<U>>(m_tokens[m_i++].text);
        } else if (Peek().type == LPar) {
            ++m_i;
            const Setting<bool> bound(listed_, false);
            upper = ParseCompareExpr();
            if (AtEnd() || Peek().type != RPar)
                Fail("missing ')' after '", m_tokens[m_i - 1].text, "'");
            ++m_i;
        } else {
            Fail("expected the last index after '^', as in ", example, ", not '", Peek().text, "'");
        }
    }
    return std::make_shared<SeriesExpression<U>>(word == "prod", index, lower, upper,
                                                 ParseMultExpr());
}

// 'grad_(x = a) body', bound as a sum is, its body a term as a sum's is.
template <Parsable T, Numeric U>
PExpression<U> Interpreter<T, U>::ParseGrad() {
    const auto expect = [&](Type type) -> const std::string& {
        if (AtEnd() || Peek().type != type)
            Fail("grad takes a name at a point, as 'grad_(x = 2) x^3'");
        return m_tokens[m_i++].text;
    };
    ++m_i;
    expect(Sub);
    expect(LPar);
    const std::string variable = expect(Func);
    expect(Equal);
    const PExpression<U> point = [&] {
        const Setting<bool> at(listed_, false);
        return ParseCompareExpr();
    }();
    expect(RPar);
    return std::make_shared<GradExpression<U>>(variable, point, ParseMultExpr());
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::ParseSubExpr()
{
    PExpression<U> e;
    const size_t m_s = m_i;
    if (!AtEnd() && m_tokens[m_i++].type == Sub)
    {
        // A quote or cell brackets after the index are the term's: 'x_(n-1)''
        // transposes x_(n-1), and 'x_n[1,1]' is its first cell.
        e = ParseSimpleExpr(false);
    }
    else
    {
        m_i=m_s;
    }
    return e;
}

// 'frac(x) = x' and the like: a definition of a word that was a name before it
// was reserved. An '=' outside brackets is what makes a line a definition.
template <Parsable T, Numeric U>
bool Interpreter<T, U>::DefinesReserved() const {
    const Type next = m_tokens.size() > 1 ? m_tokens[1].type : Val;
    if (next != LPar && next != Sub && next != Guard &&
        !(next == Equal &&
          (IsWord(m_tokens[0], "frac") || IsWord(m_tokens[0], "tex") || IsGrad(m_tokens[0]))))
        return false;
    int depth = 0;
    for (size_t token = 1; token < m_tokens.size(); ++token) {
        const Type type = m_tokens[token].type;
        depth += (type == LPar || type == LBra) - (type == RPar || type == RBra);
        if (type == Equal && depth == 0) return true;
    }
    return false;
}

// 'digits' reads how many significant digits an answer shows, and
// 'digits = n' sets it.
template <Parsable T, Numeric U>
typename Interpreter<T, U>::Result Interpreter<T, U>::Digits(const std::string& s) {
    if (m_tokens.size() == 1) return U(T(digits_));
    if (m_tokens[1].type != Equal) Fail("unexpected '", m_tokens[1].text, "'");
    const PExpression<U> e = ParseAll(2);
    if (dynamic_cast<EqualExpression<U>*>(e.get())) {
        Fail("digits takes a number, not a definition");
    }
    EvaluationVisitor<U>              evaluator(stack_);
    typename ReferenceStack<U>::Frame line(stack_);
    const U                           value = e->accept(evaluator);
    const bool                        scalar = value.Size() == Extent{1, 1};
    if (scalar && value(1, 1) > T(max_digits)) {
        Fail("digits can be ", max_digits, " at most, not ", Show(value));
    }
    const int digits = scalar ? numeric_interface<U>::toInt(value) : 0;
    if (digits < 1 || !(value(1, 1) == T(digits))) {
        Fail("digits must be a whole number of at least 1, not ", Show(value));
    }
    digits_ = digits;
    return Echo{AsWritten(s)};
}

template <Parsable T, Numeric U>
typename Interpreter<T, U>::Result Interpreter<T, U>::Eval(const std::string& s) {
    Result result{U()};
    try {
        /* the following functions might throw some evaluation errors */
        stack_.BeginEvaluation();
        result = Run(s);
    } catch (const std::exception& e) {
        // Deliberately not catch(...): an exception that is not std::exception
        // is our bug, and laundering it into a diagnostic would hide it.
        result = Diagnostic{e.what()};
    }
    ResetInterpreter();  // reset whatever happens and forgive the user
    return result;
}

// Without comments, which may hide a brace.
inline std::string Uncommented(const std::string& text) {
    std::string kept;
    for (size_t start = 0; start <= text.size();) {
        const size_t end  = std::min(text.find('\n', start), text.size());
        std::string  line = text.substr(start, end - start);
        line              = line.substr(0, line.find('#'));
        line.erase(line.find_last_not_of(" \t\r") + 1);
        if (start != 0) kept += '\n';
        kept += line;
        start = end + 1;
    }
    return kept;
}

inline std::string Trimmed(const std::string& text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return std::string();
    return text.substr(first, text.find_last_not_of(" \t\r\n") + 1 - first);
}

// A text of several definitions, a model's body or a file, one a line and
// continued while a bracket or a brace is open, each with the line it
// starts on.
inline std::vector<std::pair<int, std::string>> Statements(std::istream& in) {
    std::vector<std::pair<int, std::string>> statements;
    std::string                              line, statement;
    int                                      first = 0, depth = 0;
    for (int number = 1; std::getline(in, line); ++number) {
        if (statement.empty()) {
            if (Trimmed(Uncommented(line)).empty()) continue;
            first     = number;
            statement = line;
            depth     = 0;
        } else {
            statement += '\n' + line;
        }
        // Counted line by line, as a comment ends with its line: recounting
        // the whole statement made reading a literal quadratic in its rows.
        depth += Unclosed(line);
        if (depth > 0) continue;
        statements.emplace_back(first, statement);
        statement.clear();
    }
    if (!statement.empty()) statements.emplace_back(first, statement);
    return statements;
}

template <Parsable T, Numeric U>
typename Interpreter<T, U>::Result Interpreter<T, U>::Run(const std::string& s) {
    ResetInterpreter();
    if (const std::string text = Uncommented(s); text.find('{') != std::string::npos)
        return Echo{DefineModel(text)};
    Lexer(s);
    if (IsWord(m_tokens[0], "use") && m_tokens.size() > 1 && m_tokens[1].type == Func)
        return Echo{Use(s)};
    const bool fraction = IsWord(m_tokens[0], "frac");
    if ((BeginsLine(m_tokens[0]) || IsGrad(m_tokens[0])) && DefinesReserved()) {
        Fail(m_tokens[0].text, " is reserved, so it cannot be defined");
    }
    if (m_tokens[0].type == Query) return Echo{ParseQuery()};
    if (IsWord(m_tokens[0], "tex")) return Echo{Tex()};
    if (IsWord(m_tokens[0], "digits")) return Digits(s);
    m_E = ParseAll(fraction ? 1 : 0);
    EvaluationVisitor<U> evaluator(stack_);
    if (EqualExpression<U>* definition = dynamic_cast<EqualExpression<U>*>(m_E.get())) {
        if (fraction) Fail("frac shows an answer, not a definition");
        evaluator.Bind(definition, AsWritten(s));
        return Echo{AsWritten(s)};
    }
    // A line being evaluated opens a scope, so a local lives exactly as long
    // as the line that wrote it. A line that is only a definition is a
    // definition, parentheses or not, and takes the branch above.
    typename ReferenceStack<U>::Frame line(stack_);
    const U                           value = m_E->accept(evaluator);
    if (!fraction) return value;
    return Echo{U::toString(
        value, [this](const T& x) { return numeric_interface<T>::fraction(x, digits_); })};
}

// 'use filters' and 'use filters (lowpass)': the file's names reached
// qualified, and those listed unqualified too.
template <Parsable T, Numeric U>
std::string Interpreter<T, U>::Use(const std::string& s) {
    const std::string        name = m_tokens[1].text;
    std::vector<std::string> listed;
    size_t                   i = 2;
    if (i < m_tokens.size()) {
        if (m_tokens[i].type != LPar) Fail("unexpected '", m_tokens[i].text, "'");
        do {
            if (++i >= m_tokens.size() || m_tokens[i].type != Func)
                Fail("expected a name to bring in, as 'use ", name, " (a, b)'");
            listed.push_back(m_tokens[i++].text);
        } while (i < m_tokens.size() && m_tokens[i].type == Comma);
        if (i >= m_tokens.size() || m_tokens[i].type != RPar)
            Fail("missing ')' after '", m_tokens[i - 1].text, "'");
        if (++i < m_tokens.size()) Fail("unexpected '", m_tokens[i].text, "'");
    }
    const std::shared_ptr<Scope<U>> file = Load(name);
    for (const std::string& brought : listed)
        if (file->names.count(brought) == 0) Fail(file->file, " defines no ", brought);
    auto used  = std::make_shared<Reference<U>>(name);
    used->file = file;
    used->home = &stack_.Target();
    stack_.Put(name, used);
    for (const std::string& brought : listed) stack_.Put(brought, file->names.at(brought));
    return AsWritten(s);
}

// A file's definitions, run into a scope of their own, beside the file that
// names it. One that fails loads nothing.
template <Parsable T, Numeric U>
std::shared_ptr<Scope<U>> Interpreter<T, U>::Load(const std::string& name) {
    const std::string           file = name + ".ink";
    const std::filesystem::path path = directory_ / file;
    const std::string           key  = std::filesystem::weakly_canonical(path).string();
    if (const auto loaded = files_.find(key); loaded != files_.end()) {
        if (!loaded->second) Fail(file, " uses itself");
        return loaded->second;
    }
    std::ifstream in(path);
    if (!in) Fail("cannot read ", file);
    const auto statements = Statements(in);

    auto scope                            = std::make_shared<Scope<U>>();
    scope->parent                         = &stack_.Builtins();
    scope->label                          = name;
    scope->file                           = file;
    files_[key]                           = nullptr;
    const std::filesystem::path directory = directory_;
    directory_                            = path.parent_path();
    try {
        const typename ReferenceStack<U>::Into into(stack_, *scope);
        for (const auto& [line, statement] : statements) {
            try {
                if (std::holds_alternative<U>(Run(statement)))
                    Fail("a file used holds definitions, not answers");
            } catch (const std::exception& e) {
                throw std::runtime_error(file + ", line " + std::to_string(line) + ": " + e.what());
            }
        }
    } catch (...) {
        files_.erase(key);
        directory_ = directory;
        throw;
    }
    directory_  = directory;
    files_[key] = scope;
    return scope;
}

template <Parsable T, Numeric U>
std::string Interpreter<T, U>::DefineModel(const std::string& text) {
    auto [name, model] = ParseModel(text);
    model->scope       = &stack_.Target();
    auto definition    = std::make_shared<Reference<U>>(name);
    definition->model  = model;
    definition->home   = &stack_.Target();
    stack_.Put(name, definition);
    return model->header + " = { ... }";
}

// 'gain(k = 2, b = 1, x_n) = { ... }', its text without comments.
template <Parsable T, Numeric U>
std::pair<std::string, std::shared_ptr<Model<U>>> Interpreter<T, U>::ParseModel(
    const std::string& text) {
    const char*  form  = "a model is written name(parameters) = { definitions }";
    const size_t open  = text.find('{');
    const size_t close = text.rfind('}');
    if (close == std::string::npos || close < open) Fail("missing '}' after the body of a model");
    if (const std::string rest = Trimmed(text.substr(close + 1)); !rest.empty())
        Fail("unexpected '", rest, "' after '}'");
    std::string header = Trimmed(text.substr(0, open));
    if (header.empty() || header.back() != '=') Fail(form);
    header = Trimmed(header.substr(0, header.size() - 1));

    ResetInterpreter();
    if (header.empty()) Fail(form);
    Lexer(header);
    const Token<T>& head = m_tokens[0];
    if (head.type != Func) Fail(form);
    if (IsLimit(head) || IsSeries(head) || IsGrad(head) || IsLogic(head) || BeginsLine(head) ||
        IsWord(head, "use"))
        Fail(head.text, " is reserved, so it cannot be defined");
    const std::string name      = head.text;
    m_i                         = 1;
    const PExpression<U> params = ParseParameters();
    if (!AtEnd()) Fail(form);

    auto model    = std::make_shared<Model<U>>();
    model->header = header;
    std::vector<PExpression<U>> given;
    if (params) given = params->Children();
    for (const PExpression<U>& written : given) {
        typename Model<U>::Parameter parameter;
        PExpression<U>               left = written;
        if (const auto* equal = dynamic_cast<const EqualExpression<U>*>(written.get())) {
            left               = equal->m_e1();
            parameter.fallback = equal->m_e2();
        }
        // Its places are a cell's, or with a default the left side's.
        std::vector<PExpression<U>> places;
        if (const auto* cell = dynamic_cast<const CellExpression<U>*>(left.get())) {
            places = {cell->Slice(), cell->Row(), cell->Col()};
            left   = cell->Matrix();
        }
        const auto* term = dynamic_cast<const FuncExpression<U>*>(left.get());
        if (term && places.empty())
            places = {term->Children()[5], term->Children()[3], term->Children()[4]};
        bool bounded = true;  // each place, as 'j<=2'
        for (const PExpression<U>& place : places) {
            const auto* compare = dynamic_cast<const CompareExpression<U>*>(place.get());
            if (compare && compare->Op() == Comparison::LessEqual &&
                dynamic_cast<const RefExpression<U>*>(compare->m_e1().get()))
                parameter.bounds.push_back(compare->m_e2());
            else
                bounded = bounded && !place;
        }
        const auto* index =
            term ? dynamic_cast<const RefExpression<U>*>(term->m_e2().get()) : nullptr;
        if (dynamic_cast<const RefExpression<U>*>(left.get()) && bounded &&
            parameter.bounds.empty()) {
            parameter.name = left->Name();
        } else if (index && bounded && !term->m_e1() && !term->Children()[2]) {
            parameter.name  = term->Name();
            parameter.index = index->Name();
        } else {
            Fail("a model's parameter is a name, as 'k = 2', or an input, as 'x_n' or 'x_n[j<=2]'");
        }
        // A size that moved from term to term could not be compiled.
        for (const PExpression<U>& bound : parameter.bounds)
            if (Mentions(*bound, parameter.index))
                Fail(parameter.name, " is an input of ", name,
                     ", so its size cannot read the index ", parameter.index);
        for (const auto& other : model->parameters)
            if (other.name == parameter.name)
                Fail(name, " has two parameters named ", parameter.name);
        model->parameters.push_back(std::move(parameter));
    }

    std::istringstream body(text.substr(open + 1, close - open - 1));
    for (const auto& [line, written] : Statements(body)) {
        typename Model<U>::Statement statement;
        statement.written = Trimmed(written);
        if (statement.written.find('{') != std::string::npos) {
            std::tie(statement.name, statement.model) = ParseModel(statement.written);
            statement.written.clear();
        } else {
            ResetInterpreter();
            Lexer(statement.written);
            statement.definition = ParseAll();
            if (!dynamic_cast<const EqualExpression<U>*>(statement.definition.get()))
                Fail("a model's body holds definitions, not '", statement.written, "'");
            statement.name = statement.definition->Name();
        }
        for (const auto& parameter : model->parameters) {
            if (parameter.name != statement.name) continue;
            if (parameter.index.empty())
                Fail(statement.name, " is a parameter of ", name, ", so its body cannot define it");
            const auto& left = statement.definition
                                   ? statement.definition->Children()[0]->Children()
                                   : std::vector<PExpression<U>>();
            // A history by cells chooses cells, not terms, so gives every term (C89).
            if (left.empty() || !left[1] || left[3] ||
                (!left[2] && dynamic_cast<const RefExpression<U>*>(left[1].get())))
                Fail(statement.name, " is an input of ", name,
                     ", so its body can give it only a history: a term or a guarded clause");
        }
        model->body.push_back(std::move(statement));
    }
    return {name, model};
}

#endif
