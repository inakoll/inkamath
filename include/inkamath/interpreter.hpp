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
#include <variant>

#include "inkamath/diagnostic.hpp"
#include "inkamath/expression.hpp"
#include "inkamath/expression_visitor.hpp"
#include "inkamath/pexpression.hpp"
#include "inkamath/matrix.hpp"
#include "inkamath/token.hpp"
#include "inkamath/numeric_interface.hpp"
#include "inkamath/reference_stack.hpp"

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
    void PrintTokens(void);

    void ResetInterpreter(void);

private:
    void Lexer(const std::string& s);
    void Number_Lexer(const std::string& s, size_t& i);
    void Reference_Lexer(const std::string& s, size_t& i);

    PExpression<U> ParseAll();
    PExpression<U> Parse();
    PExpression<U> ParseEqualExpr();
    PExpression<U> ParseAddExpr();
    PExpression<U> ParseMultExpr();
    PExpression<U> ParsePowExpr();
    PExpression<U> ParseMatrix();
    PExpression<U> ParseSimpleExpr();
    PExpression<U> ParseParameters();
    PExpression<U> ParseSubExpr();
    PExpression<U> ParseLimit();
    std::string ParseQuery();

    // The one reserved word. A limit is a property of a definition, so 'lim'
    // takes a name rather than an expression.
    static bool IsLimit(const Token<T>& token) {return token.type == Func && token.text == "lim";}

    // One line cannot be allowed to exhaust the C++ stack. Token count bounds
    // every recursion that a line can provoke -- the parser's, the evaluator's
    // and the destructor's -- because the tree has at most one node per token.
    // Measured: the sanitizer build overflows at about 2000 nested
    // parentheses and the release build at about 8000, so 1000 tokens cannot
    // reach either.
    static constexpr size_t max_tokens = 1000;

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
            m_tokens.push_back(Token<T>(Equal, std::string(1, s[i])));
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
        case ' ':
            break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
            this->Number_Lexer(s,i);
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
            if(std::isalpha(s[i]))
                    Reference_Lexer(s,i);
            else
            {
                Fail("unexpected character '", s[i], "'");
            }
        }
    }

    if (m_tokens.empty()) Fail("empty expression");
    if (m_tokens.size() > max_tokens)
    {
        Fail("expression is longer than ", max_tokens, " tokens");
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
    while ((i < s.length()) && isalpha(s[i]))
    {
        ++i;
    }
    if (i!=s_i)
    {
        while (i < s.length() && isdigit(s[i]))
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
PExpression<U> Interpreter<T,U>::ParseAll()
{
    m_i = 0;
    PExpression<U> e = Parse();
    if (!AtEnd())
    {
        if (Peek().type == LPar)
        {
            Fail("unexpected '(' -- the operator '*' is probably missing");
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
    const size_t m_s = m_i;
    if (!AtEnd() && Peek().type == Func && !IsLimit(Peek()))
    {
        std::string name = m_tokens[m_i++].text;
        ref = PExpression<U>(new RefExpression<U>(name));
        params = ParseParameters();
        sub = ParseSubExpr();
        if (!AtEnd() && m_tokens[m_i++].type == Equal)
        {
            expr = Parse();
            if(params || sub) {
                e.reset(new EqualExpression<U>(PExpression<U>(new FuncExpression<U>(ref, params, sub)),expr));
            }
            else {
                e.reset(new EqualExpression<U>(ref, expr));
            }
        }
        else {
            m_i = m_s;
            e = ParseAddExpr();
        }
    }
    else
    {
        e = ParseAddExpr();
    }
    return e;
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::ParseAddExpr()
{
    PExpression<U> e = ParseMultExpr();
    while (!AtEnd() && (Peek().type == Add || Peek().type == Min) )
    {
        if (m_tokens[m_i++].type == Add)
        {
            e.reset(new AddExpression<U>(e,ParseMultExpr()));
        }
        else
        {
            PExpression<U> tmp;
            tmp.reset(new NegExpression<U>(ParseMultExpr()));
            e.reset(new AddExpression<U>(e,tmp));
        }
    }
    return e;
}

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::ParseMultExpr()
{
    PExpression<U> e = ParsePowExpr();
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
PExpression<U> Interpreter<T,U>::ParsePowExpr()
{
    PExpression<U> e = ParseSimpleExpr();
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
PExpression<U>  Interpreter<T,U>::ParseSimpleExpr()
{
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
            ref.reset(new RefExpression<U>(m_tokens[m_i++].text));
            param = ParseParameters();
            sub = ParseSubExpr();
            if(param || sub) {
                e.reset(new FuncExpression<U>(ref,param,sub));
            }
            else {
                e = ref;
            }
			break;

        case Min:
            ++m_i;
            e.reset(new NegExpression<U>(ParseMultExpr()));
			break;

        case Fact:
            ++m_i;
            e.reset(new FactExpression<U>(ParsePowExpr()));
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
			break;

        default:
        case RPar:
            Fail("unexpected '", Peek().text, "'");
            break;
        }
    }
    else if(m_i != 0)
    {
        Fail("unexpected end of input after '", m_tokens[--m_i].text, "'");
    }
    return e;
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
    if (Peek().type != Func)
    {
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

template <Parsable T, Numeric U>
PExpression<U> Interpreter<T,U>::ParseSubExpr()
{
    PExpression<U> e;
    const size_t m_s = m_i;
    if (!AtEnd() && m_tokens[m_i++].type == Sub)
    {
        e = ParseSimpleExpr();
    }
    else
    {
        m_i=m_s;
    }
    return e;
}


template <Parsable T, Numeric U>
typename Interpreter<T,U>::Result Interpreter<T,U>::Eval(const std::string& s)
{
    Result result{U()};
    try
    {
        /* the following functions might throw some evaluation errors */
        stack_.BeginEvaluation();
        Lexer(s);
        if(m_tokens[0].type == Query)
        {
            result = Echo{ParseQuery()};
        }
        else
        {
            m_E = ParseAll();
            EvaluationVisitor<U> evaluator(stack_);
            if(EqualExpression<U>* definition = dynamic_cast<EqualExpression<U>*>(m_E.get()))
            {
                evaluator.Bind(definition, AsWritten(s));
                result = Echo{AsWritten(s)};
            }
            else
            {
                result = m_E->accept(evaluator);
            }
        }
    }
    catch (const std::exception& e)
    {
        // Deliberately not catch(...): an exception that is not std::exception
        // is our bug, and laundering it into a diagnostic would hide it.
        result = Diagnostic{e.what()};
    }
    ResetInterpreter(); // reset whatever happens and forgive the user
    return result;
}

template <Parsable T, Numeric U>
void Interpreter<T,U>::PrintTokens(void)
{
    for (const Token<T>& token : m_tokens)
    {
        std::cout << token.text;
    }
    std::cout << std::endl;
}

#endif
