#ifndef INKAMATH_LATEX_HPP
#define INKAMATH_LATEX_HPP

#include "inkamath/expression.hpp"
#include "inkamath/parameters.hpp"
#include "inkamath/reference.hpp"

#include <cctype>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// A definition as LaTeX (DESIGN.md, next in line): what was parsed, set as a
// paper sets it, so that a transcription can be read against its page.
template <typename T>
class Latex {
public:
    static std::string Definition(const Reference<T>& definition) {
        if (definition.model) throw std::runtime_error("tex cannot show a model yet");
        std::vector<const Clause<T>*> cases;
        std::string                   out;
        for (const Clause<T>& clause : definition.Clauses()) {
            const ParametersDefinition<T>& p = clause.parameters;
            if (p.cells()) throw std::runtime_error("tex cannot show a definition by cells yet");
            if (p.indexed() && !p.general())
                out += Left(definition.Name(), p) + " = " + Of(*clause.expression).text + "\n";
            else
                cases.push_back(&clause);
        }
        if (cases.empty()) return Trimmed(out);
        std::string right;
        if (cases.size() == 1 && !cases.front()->parameters.guarded()) {
            right = Of(*cases.front()->expression).text;
        } else {
            // Guarded in the order written, then the one that always applies,
            // as they are tried.
            std::vector<std::string> lines;
            for (const bool guarded : {true, false})
                for (const Clause<T>* clause : cases)
                    if (clause->parameters.guarded() == guarded)
                        lines.push_back(Of(*clause->expression).text + " & \\text{" +
                                        (guarded ? "if } " + Of(*clause->parameters.guard()).text
                                                 : std::string("otherwise}")));
            right = "\\begin{cases} ";
            for (std::size_t k = 0; k < lines.size(); ++k) right += (k ? " \\\\ " : "") + lines[k];
            right += " \\end{cases}";
        }
        return out + Left(definition.Name(), cases.front()->parameters) + " = " + right;
    }

private:
    // How tightly a rendering binds, so that an operand is parenthesised only
    // where it binds more loosely than its place.
    enum Level { relation = 1, sum, product, unary, power, primary };

    struct Text {
        std::string text;
        int         level = primary;
    };

    static std::string Trimmed(std::string text) {
        while (!text.empty() && text.back() == '\n') text.pop_back();
        return text;
    }

    // A name of one letter is itself, a Greek one its letter, any other an
    // italic word.
    static std::string Name(const std::string& name) {
        static const std::set<std::string> greek = {
            "alpha",   "beta",   "gamma", "delta", "epsilon", "zeta",  "eta",   "theta", "iota",
            "kappa",   "lambda", "mu",    "nu",    "xi",      "pi",    "rho",   "sigma", "tau",
            "upsilon", "phi",    "chi",   "psi",   "omega",   "Gamma", "Delta", "Theta", "Lambda",
            "Xi",      "Pi",     "Sigma", "Phi",   "Psi",     "Omega"};
        if (name.size() == 1) return name;
        if (greek.count(name)) return "\\" + name;
        return "\\mathit{" + name + "}";
    }

    static std::string Operator(const std::string& name) {
        return name.size() == 1 ? name : "\\operatorname{" + name + "}";
    }

    // An index set tight, as a subscript is: 's_{n-1}'.
    static std::string Index(const Expression<T>& index) {
        std::string text = Of(index, true).text;
        return text.size() == 1 ? text : "{" + text + "}";
    }

    static std::string Left(const std::string& name, const ParametersDefinition<T>& p) {
        std::string left = p.parameters_names().empty() ? Name(name) : Operator(name);
        if (!p.parameters_names().empty()) {
            left += "(";
            for (std::size_t k = 0; k < p.parameters_names().size(); ++k) {
                const std::string& parameter = p.parameters_names()[k];
                left += (k ? ", " : "") + Name(parameter);
                const auto fallback = p.parameters_dict().find(parameter);
                if (fallback != p.parameters_dict().end())
                    left += " = " + Of(*fallback->second).text;
            }
            left += ")";
        }
        if (p.indexed()) {
            const std::string index = p.general() ? p.index_name() : std::to_string(p.index());
            left += "_" + (index.size() == 1 ? index : "{" + index + "}");
        }
        return left;
    }

    static std::string Wrapped(const Text& text, int level) {
        return text.level < level ? "(" + text.text + ")" : text.text;
    }

    static std::string Arguments(const ParametersCall<T>& call) {
        std::string out;
        for (const auto& argument : call.parameters_expression())
            out += (out.empty() ? "" : ", ") + Of(*argument).text;
        for (const auto& [name, argument] : call.parameters_dict())
            out += (out.empty() ? "" : ", ") + Name(name) + " = " + Of(*argument).text;
        return out;
    }

    // An expression, its operators spaced unless it is an index, where
    // they are set tight.
    static Text Of(const Expression<T>& e, bool tight = false) {
        const std::string plus = tight ? "+" : " + ", minus = tight ? "-" : " - ";
        if (const auto* value = dynamic_cast<const ValExpression<T>*>(&e)) {
            const std::string text = numeric_interface<T>::toString(value->value);
            return {text, text.front() == '-' ? unary : primary};
        }
        if (const auto* sum = dynamic_cast<const AddExpression<T>*>(&e)) {
            const Text  left  = Of(*sum->m_e1(), tight);
            const auto& right = *sum->m_e2();
            if (const auto* negative = dynamic_cast<const NegExpression<T>*>(&right))
                return {Wrapped(left, Level::sum) + minus +
                            Wrapped(Of(*negative->m_e(), tight), product),
                        Level::sum};
            const Text term = Of(right, tight);
            if (term.text.front() == '-')
                return {Wrapped(left, Level::sum) + minus + term.text.substr(1), Level::sum};
            return {Wrapped(left, Level::sum) + plus + Wrapped(term, product), Level::sum};
        }
        if (const auto* times = dynamic_cast<const MultExpression<T>*>(&e)) {
            const std::string left  = Wrapped(Of(*times->m_e1(), tight), product);
            // A sum that ends a product reaches to its end, as on paper.
            const bool        last  = dynamic_cast<const SeriesExpression<T>*>(times->m_e2().get());
            const std::string right = last ? Of(*times->m_e2(), tight).text
                                           : Wrapped(Of(*times->m_e2(), tight), unary + 1);
            const bool        digit = std::isdigit(static_cast<unsigned char>(right.front()));
            return {left + (digit ? " \\cdot " : "\\,") + right, product};
        }
        if (const auto* ratio = dynamic_cast<const DivExpression<T>*>(&e))
            return {"\\frac{" + Of(*ratio->m_e1()).text + "}{" + Of(*ratio->m_e2()).text + "}"};
        if (const auto* power = dynamic_cast<const PowExpression<T>*>(&e)) {
            const std::string exponent = Of(*power->m_e2()).text;
            return {Wrapped(Of(*power->m_e1(), tight), primary) + "^" +
                        (exponent.size() == 1 ? exponent : "{" + exponent + "}"),
                    Level::power};
        }
        if (const auto* negative = dynamic_cast<const NegExpression<T>*>(&e))
            return {"-" + Wrapped(Of(*negative->m_e(), tight), product), unary};
        if (const auto* factorial = dynamic_cast<const FactExpression<T>*>(&e))
            return {Wrapped(Of(*factorial->m_e(), tight), primary) + "!", Level::power};
        if (const auto* transpose = dynamic_cast<const TransposeExpression<T>*>(&e))
            return {Wrapped(Of(*transpose->m_e(), tight), primary) + "^\\mathsf{T}", Level::power};
        if (const auto* floor = dynamic_cast<const FloorExpression<T>*>(&e))
            return {"\\lfloor " + Of(*floor->m_e()).text + " \\rfloor"};
        if (dynamic_cast<const InexactExpression<T>*>(&e))
            throw std::runtime_error("tex cannot show '~', which has no form on paper");
        if (const auto* compare = dynamic_cast<const CompareExpression<T>*>(&e)) {
            static const char* const signs[] = {" < ", " > ", " \\le ", " \\ge ", " = ", " \\ne "};
            return {Of(*compare->m_e1(), tight).text + signs[static_cast<int>(compare->Op())] +
                        Of(*compare->m_e2(), tight).text,
                    relation};
        }
        if (const auto* logic = dynamic_cast<const LogicExpression<T>*>(&e))
            return {Of(*logic->m_e1()).text + (logic->Conjunction() ? " \\land " : " \\lor ") +
                        Of(*logic->m_e2()).text,
                    relation - 1};
        if (const auto* cell = dynamic_cast<const CellExpression<T>*>(&e)) {
            std::string place = Of(*cell->Row(), true).text;
            if (cell->Col()) place += "," + Of(*cell->Col(), true).text;
            // A term's cell shares its subscript, 'x_{n,j}': LaTeX takes one.
            if (const auto* term = dynamic_cast<const FuncExpression<T>*>(cell->Matrix().get());
                term && term->m_e2() && !term->limit()) {
                const std::string name =
                    term->m_e1() ? Operator(term->Name()) + "(" + Arguments(term->Call()) + ")"
                                 : Name(term->Name());
                return {name + "_{" + Of(*term->m_e2(), true).text + "," + place + "}"};
            }
            return {Wrapped(Of(*cell->Matrix(), tight), primary) + "_{" + place + "}"};
        }
        if (const auto* series = dynamic_cast<const SeriesExpression<T>*>(&e)) {
            const std::string upper = series->Upper() ? Of(*series->Upper()).text : "\\infty";
            return {std::string(series->Product() ? "\\prod" : "\\sum") + "_{" + series->Index() +
                        "=" + Of(*series->Lower(), true).text + "}^{" + upper + "} " +
                        Wrapped(Of(*series->Body()), product),
                    Level::sum};
        }
        if (const auto* grad = dynamic_cast<const GradExpression<T>*>(&e)) {
            const std::string x = Name(grad->Variable());
            return {"\\left.\\frac{\\partial}{\\partial " + x + "} " + Of(*grad->Body()).text +
                        "\\right|_{" + x + "=" + Of(*grad->Point(), true).text + "}",
                    Level::sum};
        }
        if (const auto* matrix = dynamic_cast<const MatExpression<T>*>(&e)) {
            const Extent size = matrix->Size();
            std::string  out  = "\\begin{bmatrix} ";
            for (std::size_t i = 0; i < size.rows; ++i)
                for (std::size_t j = 0; j < size.cols; ++j)
                    out += (j   ? " & "
                            : i ? " \\\\ "
                                : "") +
                           Of(*matrix->Children()[i * size.cols + j]).text;
            return {out + " \\end{bmatrix}"};
        }
        if (const auto* member = dynamic_cast<const MemberExpression<T>*>(&e))
            return {Of(*member->Object()).text + "." + Of(*member->Member()).text};
        if (const auto* call = dynamic_cast<const FuncExpression<T>*>(&e)) {
            const std::string& name = call->Name();
            if (call->limit()) return {"\\lim_{n \\to \\infty} " + Name(name) + "_n", Level::sum};
            const ParametersCall<T>& arguments = call->Call();
            if (name == "floor" && arguments.parameters_expression().size() == 1)
                return {"\\lfloor " + Of(*arguments.parameters_expression().front()).text +
                        " \\rfloor"};
            std::string out =
                call->m_e1() ? Operator(name) + "(" + Arguments(arguments) + ")" : Name(name);
            if (call->m_e2()) out += "_" + Index(*call->m_e2());
            return {out};
        }
        if (dynamic_cast<const EqualExpression<T>*>(&e))
            throw std::runtime_error("tex cannot show a local definition");
        return {Name(e.Name())};
    }
};

#endif  // INKAMATH_LATEX_HPP
