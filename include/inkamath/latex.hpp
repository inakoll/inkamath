#ifndef INKAMATH_LATEX_HPP
#define INKAMATH_LATEX_HPP

#include "inkamath/expression.hpp"
#include "inkamath/parameters.hpp"
#include "inkamath/reference.hpp"

#include <algorithm>
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
        const std::string& name = definition.Name();
        // A clause for one index or one cell is a line of its own, the
        // clauses for every index or every cell one definition in cases; for
        // a definition by cells, the clauses written whole come first, as
        // what the cells override. A term written after a guard is tried
        // after it (C45), so it is a case in its place (C85).
        std::vector<const Clause<T>*>              cases, cells;
        std::vector<std::vector<const Clause<T>*>> lines;
        bool                                       guarded = false;
        for (const Clause<T>& clause : definition.Clauses()) {
            const ParametersDefinition<T>& p    = clause.parameters;
            const bool                     base = p.indexed() && !p.general();
            if (p.cells() && !p.row_name().empty() && !base) {
                cells.push_back(&clause);
            } else if (!p.cells() && (!base || guarded)) {
                guarded = guarded || p.guarded();
                cases.push_back(&clause);
            } else {
                // A base term by its cells is one line however many clauses.
                const auto term = std::find_if(lines.begin(), lines.end(), [&](const auto& line) {
                    const ParametersDefinition<T>& q = line.front()->parameters;
                    return p.cells() && !p.row_name().empty() && q.cells() &&
                           !q.row_name().empty() && q.index() == p.index();
                });
                if (term == lines.end())
                    lines.push_back({&clause});
                else
                    term->push_back(&clause);
            }
        }
        const bool  bycells = std::any_of(definition.Clauses().begin(), definition.Clauses().end(),
                                          [](const Clause<T>& c) { return c.parameters.cells(); });
        std::string out;
        const auto  whole = [&] {
            if (!cases.empty())
                out += Left(name, cases.front()->parameters) + " = " + Cases(cases) + "\n";
        };
        if (bycells) whole();
        for (const auto& line : lines) {
            const ParametersDefinition<T>& p = line.front()->parameters;
            if (!p.cells())
                out += Left(name, p) + " = " + Of(*line.front()->expression).text + "\n";
            else
                out += Entry(name, p) + " = " + Cases(line) + Bounds(p) + "\n";
        }
        if (!bycells) whole();
        if (!cells.empty())
            out += Entry(name, cells.front()->parameters) + " = " + Cases(cells) +
                   Bounds(cells.front()->parameters);
        return Trimmed(out);
    }

    // A model: its head, then its definitions a line each, indented as '?'
    // shows them, each as it is set alone. 'instance' is the model with its
    // defaults, where its definitions are made.
    static std::string System(const Reference<T>& definition, const Scope<T>& instance) {
        const auto& model = *definition.model;
        std::string out   = Operator(definition.Name()) + "(";
        for (std::size_t k = 0; k < model.parameters.size(); ++k) {
            const auto& parameter = model.parameters[k];
            out += (k ? ", " : "") + Name(parameter.name);
            if (!parameter.index.empty()) out += "_" + Braced(parameter.index);
            for (std::size_t b = 0; b < parameter.bounds.size(); ++b)
                out += (b ? " \\times " : " \\in \\mathbb{R}^{") + Of(*parameter.bounds[b]).text;
            if (!parameter.bounds.empty()) out += "}";
            if (parameter.fallback) out += " = " + Of(*parameter.fallback).text;
        }
        out += "):";
        std::set<std::string> done;
        for (const auto& statement : model.body) {
            if (statement.model)
                throw std::runtime_error("tex cannot show a model inside a model yet");
            if (!done.insert(statement.name).second) continue;
            const std::string text = Definition(*instance.names.at(statement.name));
            for (std::size_t start = 0; start < text.size();) {
                const std::size_t end = std::min(text.find('\n', start), text.size());
                out += "\n    " + text.substr(start, end - start);
                start = end + 1;
            }
        }
        return out;
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

    static std::string Braced(const std::string& text) {
        return text.size() == 1 ? text : "{" + text + "}";
    }

    // The clauses for every call, or for every cell: the one expression, or
    // the guarded in the order written, then the one that always applies, as
    // they are tried.
    static std::string Cases(const std::vector<const Clause<T>*>& cases) {
        if (cases.size() == 1 && !cases.front()->parameters.guarded())
            return Of(*cases.front()->expression).text;
        // A term among the cases is named by the index the others take.
        std::string index;
        for (const Clause<T>* clause : cases)
            if (clause->parameters.general()) index = clause->parameters.index_name();
        const auto term = [](const ParametersDefinition<T>& p) {
            return p.indexed() && !p.general() && !p.cells();
        };
        std::vector<std::string> lines;
        for (const bool guarded : {true, false})
            for (const Clause<T>* clause : cases) {
                const ParametersDefinition<T>& p = clause->parameters;
                if ((p.guarded() || term(p)) != guarded) continue;
                const std::string condition =
                    p.guarded() ? Of(*p.guard()).text : index + " = " + std::to_string(p.index());
                lines.push_back(Of(*clause->expression).text + " & \\text{" +
                                (guarded ? "if } " + condition : std::string("otherwise}")));
            }
        std::string right = "\\begin{cases} ";
        for (std::size_t k = 0; k < lines.size(); ++k) right += (k ? " \\\\ " : "") + lines[k];
        return right + " \\end{cases}";
    }

    // An entry of a definition by cells, 'M_{j,k}', or a term's, 'x_{n,j}':
    // one subscript, as a term's cell is read.
    static std::string Entry(const std::string& name, const ParametersDefinition<T>& p) {
        std::string subscript;
        if (p.indexed())
            subscript = (p.general() ? p.index_name() : std::to_string(p.index())) + ",";
        if (p.tensor())
            subscript +=
                (p.slice_name().empty() ? std::to_string(p.slice()) : p.slice_name()) + ",";
        subscript += p.row_name().empty() ? std::to_string(p.row()) : p.row_name();
        if (!p.column())
            subscript += "," + (p.col_name().empty() ? std::to_string(p.col()) : p.col_name());
        return Head(name, p) + "_" + Braced(subscript);
    }

    // Where the names of a clause for every cell range, those bounded as
    // written: a size inferred is a call's, not the definition's.
    static std::string Bounds(const ParametersDefinition<T>& p) {
        std::string out;
        for (const auto& [name, bound] : {std::pair(&p.slice_name(), &p.slices()),
                                          {&p.row_name(), &p.rows()},
                                          {&p.col_name(), p.column() ? nullptr : &p.cols()}})
            if (bound && *bound)
                out += (out.empty() ? ", \\quad " : ",\\ ") + std::string("1 \\le ") + Name(*name) +
                       " \\le " + Of(**bound).text;
        return out;
    }

    static std::string Head(const std::string& name, const ParametersDefinition<T>& p) {
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
        return left;
    }

    static std::string Left(const std::string& name, const ParametersDefinition<T>& p) {
        if (!p.indexed()) return Head(name, p);
        return Head(name, p) + "_" +
               Braced(p.general() ? p.index_name() : std::to_string(p.index()));
    }

    static std::string Wrapped(const Text& text, int level) {
        return text.level < level ? "(" + text.text + ")" : text.text;
    }

    // As written, in their order: a keyword's name with the index an input's
    // is written with, 'x_n = n'.
    static std::string Arguments(const ParametersCall<T>& call) {
        std::vector<PExpression<T>> given;
        if (const auto* list = dynamic_cast<const MatExpression<T>*>(call.arguments().get()))
            given = list->Children();
        else if (call.arguments())
            given = {call.arguments()};
        std::string out;
        for (const PExpression<T>& argument : given) {
            out += out.empty() ? "" : ", ";
            if (const auto* named = dynamic_cast<const EqualExpression<T>*>(argument.get()))
                out += Of(*named->m_e1()).text + " = " + Of(*named->m_e2()).text;
            else
                out += Of(*argument).text;
        }
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
            return {
                Wrapped(Of(*power->m_e1(), tight), primary) + "^" + Braced(Of(*power->m_e2()).text),
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
            std::string place = cell->Slice() ? Of(*cell->Slice(), true).text + "," : "";
            place += Of(*cell->Row(), true).text;
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
        if (dynamic_cast<const TensorExpression<T>*>(&e))
            throw std::runtime_error("tex cannot show ';;', which has no form on paper");
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
            // An index is set tight, as a subscript is: 's_{n-1}'.
            if (call->m_e2()) out += "_" + Braced(Of(*call->m_e2(), true).text);
            return {out};
        }
        if (dynamic_cast<const EqualExpression<T>*>(&e))
            throw std::runtime_error("tex cannot show a local definition");
        return {Name(e.Name())};
    }
};

#endif  // INKAMATH_LATEX_HPP
