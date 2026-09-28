#ifndef H_PARSER
#define H_PARSER

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <vector>
#include <utility>
#include <cctype> // isalpha
#include <map>
#include <stdexcept>
#include <concepts>
#include <variant>

#include "inkamath/diagnostic.hpp"
#include "inkamath/expression.hpp"
#include "inkamath/expression_visitor.hpp"
#include "inkamath/pexpression.hpp"
#include "inkamath/matrix.hpp"
#include "inkamath/token.hpp"
#include "inkamath/numeric_interface.hpp"
#include "inkamath/reference_stack.hpp"

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
    // inside Matrix rather than here (MODERNIZATION.md, C44).
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

template <Parsable T, Numeric U = Matrix<T> >
class Interpreter
{
public:
    Interpreter();
    ~Interpreter();

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
    // approximated past the bound (MODERNIZATION.md, phase 13).
    [[nodiscard]] std::string Answer(const U& value) const {
        return Show(value) + (numeric_interface<U>::approximated(value)
                                  ? "  # approximated past a thousand digits"
                                  : "");
    }

    void ResetInterpreter(void);

    // What the session has defined, for the compiler to read.
    [[nodiscard]] const ReferenceStack<U>& Definitions() const { return stack_; }

private:
    void Lexer(const std::string& s);
    void Number_Lexer(const std::string& s, size_t& i);
    void Reference_Lexer(const std::string& s, size_t& i);

    PExpression<U> ParseAll(size_t first = 0);
    PExpression<U> Parse();
    PExpression<U> ParseEqualExpr();
    // `lead`, where given, is a leading operand the caller has already parsed.
    // Without it ParseEqualExpr has to rewind and parse its speculative
    // left-hand side a second time, which nests into O(2^depth).
    PExpression<U> ParseCompareExpr(PExpression<U> lead = PExpression<U>());
    PExpression<U> ParseAddExpr(PExpression<U> lead = PExpression<U>());
    PExpression<U> ParseMultExpr(PExpression<U> lead = PExpression<U>());
    PExpression<U> ParsePowExpr(PExpression<U> lead = PExpression<U>());
    PExpression<U> ParseMatrix();
    PExpression<U> ParseSimpleExpr(bool postfix = true);
    PExpression<U> ParseCell(PExpression<U> matrix, bool named);
    PExpression<U> ParseQuotes(PExpression<U> e);

    // Inside a matrix literal, and inside an argument list, a space between
    // two expressions separates them. Everywhere else it means nothing, which
    // is what lets '[1 2;3 4][2,1]' be an index rather than two blocks.
    size_t juxtaposed_ = 0;
    struct Juxtaposed {
        explicit Juxtaposed(size_t& depth) : depth_(depth) {++depth_;}
        ~Juxtaposed() {--depth_;}
        Juxtaposed(const Juxtaposed&) = delete;
        Juxtaposed& operator=(const Juxtaposed&) = delete;
    private:
        size_t& depth_;
    };
    PExpression<U> ParseParameters();
    PExpression<U> ParseSubExpr();
    PExpression<U> ParseLimit();
    PExpression<U> ParseSeries();
    std::string ParseQuery();

    // The reserved words. A limit is a property of a definition, so 'lim'
    // takes a name rather than an expression; 'sum' and 'prod' begin a series.
    static bool IsLimit(const Token<T>& token) {return token.type == Func && token.text == "lim";}
    static bool IsSeries(const Token<T>& token) {
        return token.type == Func && (token.text == "sum" || token.text == "prod");
    }
    static bool IsWord(const Token<T>& token, const char* word) {
        return token.type == Func && token.text == word;
    }
    // 'frac' and 'digits' are about the whole answer, so they begin a line.
    static bool BeginsLine(const Token<T>& token) {
        return IsWord(token, "frac") || IsWord(token, "digits");
    }
    bool   DefinesReserved() const;
    Result Digits(const std::string& s);

    // One line cannot be allowed to exhaust the C++ stack. Token count bounds
    // every recursion that a line can provoke -- the parser's, the evaluator's
    // and the destructor's -- because the tree has at most one node per token.
    // Measured: the sanitizer build overflows at about 2000 nested
    // parentheses and the release build at about 8000, so 1000 tokens cannot
    // reach either -- on an 8 MB stack, which CMakeLists.txt gives Windows too.
    static constexpr size_t max_tokens = 1000;

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

    PExpression<U> m_E;
    ReferenceStack<U> stack_;
    int               digits_ = numeric_interface_precision;
};

template <Parsable T, Numeric U>
Interpreter<T,U>::Interpreter()
{}

template <Parsable T, Numeric U>
Interpreter<T,U>::~Interpreter()
{

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
            m_tokens.push_back(Token<T>(Semico, std::string(1, s[i])));
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
            // 'f_n | n = 0 = 1' unreadable (MODERNIZATION.md, phase 10).
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
        // line written on Windows ends with. Neither was typed to be read.
        case '\t':
        case '\r':
            break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
            this->Number_Lexer(s,i);
            break;
        case '.':
            // '.5' is a number; a point anywhere else is not.
            if(i + 1 < s.length() && std::isdigit(static_cast<unsigned char>(s[i+1])))
            {
                this->Number_Lexer(s,i);
            }
            else
            {
                Fail("unexpected character '", s[i], "'");
            }
            break;
        case 'i':
            // The imaginary unit only when it is not the start of a longer
            // name: 'i*2' is imaginary, 'ii' and 'index' are identifiers.
            if(i + 1 < s.length() && std::isalnum(static_cast<unsigned char>(s[i+1])))
                Reference_Lexer(s,i);
            else
                this->Number_Lexer(s,i);
            break;
        case '#': // inkamath comments
            // Not a return: a line that is only a comment must still reach
            // the empty check below, or the parser starts on no tokens.
            i = s.length();
            break;
		default:
            if(std::isalpha(static_cast<unsigned char>(s[i])))
                    Reference_Lexer(s,i);
            else
            {
                Fail("unexpected character '", s[i], "'");
            }
        }

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
    if (!AtEnd() && Peek().type == Func && !IsLimit(Peek()) && !IsSeries(Peek())) {
        const size_t signature_begin = m_i;
        std::string name = m_tokens[m_i++].text;
        ref = PExpression<U>(new RefExpression<U>(name));
        params = ParseParameters();
        sub = ParseSubExpr();
        // On the left of a definition the brackets define cells, 'M[j<=2,
        // k<=2]' or 'M[1,2]'; anywhere else they read one.
        const PExpression<U> cell  = ParseCell(ref, true);
        const auto*          place = dynamic_cast<CellExpression<U>*>(cell.get());
        PExpression<U> guard;
        if (!AtEnd() && Peek().type == Guard)
        {
            ++m_i;
            guard = ParseCompareExpr();
        }
        if (!AtEnd() && Peek().type == Equal)
        {
            // What names the clause, so that writing it again replaces that
            // clause rather than adding one: the left-hand side, as the
            // tokens spell it, which is the same however it was spaced.
            std::string signature;
            for(size_t token = signature_begin; token < m_i; ++token) {
                // Kept apart: joined, '[1 2]' and '[12]' are the same string,
                // and the two clauses become one (MODERNIZATION.md, C55).
                signature += '\x1f';
                signature += m_tokens[token].text;
            }
            ++m_i;
            expr = Parse();
            if (params || sub || guard || place) {
                e.reset(new EqualExpression<U>(
                    PExpression<U>(new FuncExpression<U>(ref, params, sub, false, guard, signature,
                                                         place ? place->Row() : PExpression<U>(),
                                                         place ? place->Col() : PExpression<U>())),
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
                ref = std::make_shared<CellExpression<U>>(ref, place->Row(), place->Col());
            }
            e = ParseCompareExpr(ParseQuotes(ref));
        }
    } else {
        e = ParseCompareExpr();
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
PExpression<U> Interpreter<T,U>::ParseMatrix()
{
    const Juxtaposed juxtaposed(juxtaposed_);
    std::vector<PExpression<U>> mat;
    std::vector<size_t> size(1, 0);
    PExpression<U> e;

    while ((!AtEnd()) && (Peek().type != RBra) && (Peek().type != RPar))
    {
        switch (Peek().type)
        {
        case Semico :
            size.push_back(0);
            ++m_i;
            break;

        default:
            e = Parse();
            mat.push_back(e);
            ++size.back();
        }
    }
    size_t n = size.size();
    size_t m = *std::max_element(size.begin(), size.end());
    if (m == 0)
    {
        // A matrix with no elements at all has no extent to give, and the
        // evaluator reads one per column.
        Fail("a matrix needs at least one element");
    }
    e.reset(new MatExpression<U>(n, m, make_matrix_array_from_vector(n, m, mat, size)));
    return e;
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
    PExpression<U> e,ref,param,sub;
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
            ref.reset(new RefExpression<U>(m_tokens[m_i++].text));
            param = ParseParameters();
            sub = ParseSubExpr();
            if(param || sub) {
                e.reset(new FuncExpression<U>(ref,param,sub));
            }
            else {
                e = ref;
            }
            if (postfix) e = ParseCell(e, true);
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

        case LPar:
            ++m_i;
            e = Parse();
            if (!AtEnd() && Peek().type == RPar)
            {
                ++m_i;
            }
            else
            {
                Fail("missing ')' after '", m_tokens[--m_i].text, "'");
            }
            if (postfix) e = ParseCell(e, false);
            break;

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
            if (postfix) e = ParseCell(e, false);
            break;

        default:
        case RPar:
            Fail("unexpected '", Peek().text, "'");
            break;
        }
        if (postfix) e = ParseQuotes(e);
    }
    else if(m_i != 0)
    {
        Fail("unexpected end of input after '", m_tokens[--m_i].text, "'");
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

// 'm[i,j]'. Inside a matrix literal only a name takes an index, because there
// a space between two blocks already means something: '[[1 2] [3 4]]' is one
// row of two blocks, while '[a [3 4]]' reads as an index of 'a'.
template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::ParseCell(PExpression<U> matrix, bool named)
{
    if (AtEnd() || Peek().type != LBra)
    {
        return matrix;
    }
    // A name carries its brackets everywhere; anything else does so only
    // where juxtaposition is not already separating expressions.
    if (!named && juxtaposed_ != 0)
    {
        return matrix;
    }
    ++m_i;
    PExpression<U> row = Parse();
    if (AtEnd() || Peek().type != Comma)
    {
        Fail("a cell needs a row and a column, as 'm[1,2]'");
    }
    ++m_i;
    PExpression<U> col = Parse();
    if (AtEnd() || Peek().type != RBra)
    {
        Fail("missing ']' after '", m_tokens[--m_i].text, "'");
    }
    ++m_i;
    return PExpression<U>(new CellExpression<U>(matrix, row, col));
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::ParseParameters()
{
    PExpression<U> e;
    const size_t m_s = m_i;
    if (!AtEnd() && m_tokens[m_i++].type == LPar && !AtEnd() && Peek().type != RPar)
    {
        e = ParseMatrix();
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
    const PExpression<U> lower = ParseCompareExpr();
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
        !(next == Equal && IsWord(m_tokens[0], "frac")))
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
        Lexer(s);
        const bool fraction = IsWord(m_tokens[0], "frac");
        if (BeginsLine(m_tokens[0]) && DefinesReserved()) {
            Fail(m_tokens[0].text, " is reserved, so it cannot be defined");
        }
        if (m_tokens[0].type == Query) {
            result = Echo{ParseQuery()};
        } else if (IsWord(m_tokens[0], "digits")) {
            result = Digits(s);
        } else {
            m_E = ParseAll(fraction ? 1 : 0);
            EvaluationVisitor<U> evaluator(stack_);
            if (EqualExpression<U>* definition = dynamic_cast<EqualExpression<U>*>(m_E.get())) {
                if (fraction) Fail("frac shows an answer, not a definition");
                evaluator.Bind(definition, AsWritten(s));
                result = Echo{AsWritten(s)};
            } else {
                // A line being evaluated opens a scope, so a local lives
                // exactly as long as the line that wrote it. A line that is
                // only a definition is a definition, parentheses or not, and
                // takes the branch above.
                typename ReferenceStack<U>::Frame line(stack_);
                const U                           value = m_E->accept(evaluator);
                if (fraction) {
                    result = Echo{U::toString(value, [this](const T& x) {
                        return numeric_interface<T>::fraction(x, digits_);
                    })};
                } else {
                    result = value;
                }
            }
        }
    } catch (const std::exception& e) {
        // Deliberately not catch(...): an exception that is not std::exception
        // is our bug, and laundering it into a diagnostic would hide it.
        result = Diagnostic{e.what()};
    }
    ResetInterpreter();  // reset whatever happens and forgive the user
    return result;
}

#endif
