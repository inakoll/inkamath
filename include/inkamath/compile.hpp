#ifndef INKAMATH_COMPILE_HPP
#define INKAMATH_COMPILE_HPP

#include "inkamath/convergence.hpp"
#include "inkamath/matrix.hpp"
#include "inkamath/number.hpp"
#include "inkamath/reference_stack.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// What the compiler will not compile, and in which definition.
struct Refusal : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// DESIGN.md, phase 14, step 2: the sequences a session defines, as a C
// header over doubles. Each sequence keeps a window of its latest terms, and a
// step computes the next index's terms in the order they need each other. A
// value of any shape is its cells, each one C expression.
class CompileC : public TransformationVisitor<Matrix<Number>> {
public:
    using Value = Matrix<Number>;
    using TransformationVisitor<Value>::visit;

    // A header, and what stepping it takes and keeps.
    struct Compiled {
        struct Sequence {
            std::string name;  // as the struct reaches it, 'h.low.v'
            std::size_t rows, cols;
            int         period = 1, phase = 0;  // term m at step period*m + phase
            int         start = 0;              // the step of its first term, at another rate
            bool        unnamed = false;          // of an instance a model writes unnamed
        };
        std::string              header;
        int                      first;      // the index of the first step
        std::vector<std::string> inputs;     // the step's arguments, in order
        std::vector<Sequence>    sequences;  // those it computes, in the order it does
        std::vector<std::string> guarded;    // those whose clause is kept, 'v_clause_'
    };

    // The session's sequences, or with a model, those of its instance with
    // every default and no input given (DESIGN.md, phase 15).
    static std::string Header(ReferenceStack<Value>& definitions, const std::string& module,
                              const std::string& source, const Model<Value>* model = nullptr,
                              const Scope<Value>* instance = nullptr) {
        return Build(definitions, module, source, model, instance).header;
    }

    // With 'clauses', each guarded sequence also keeps in 'name_clause_' the
    // clause its latest term took: one more than its place among the
    // definition's, or 0 where no guard decided.
    static Compiled Build(ReferenceStack<Value>& definitions, const std::string& module,
                          const std::string& source, const Model<Value>* model = nullptr,
                          const Scope<Value>* instance = nullptr, bool clauses = false) {
        return Run(definitions, module, source, model, instance, {}, clauses);
    }

    // Every reason the header cannot be made, not the first: a definition
    // refused is set aside and the rest compiled again, and one that reads it
    // is refused in turn, saying so.
    static std::vector<std::string> Refusals(ReferenceStack<Value>& definitions,
                                             const std::string&     source,
                                             const Model<Value>*    model    = nullptr,
                                             const Scope<Value>*    instance = nullptr) {
        std::vector<std::string> refusals;
        std::set<std::string>    aside;
        for (;;) {
            try {
                (void)Run(definitions, "check", source, model, instance, aside);
                return refusals;
            } catch (const Refusal& refusal) {
                refusals.emplace_back(refusal.what());
                // 'cannot compile NAME: why', or no name where no one
                // definition is to blame, which ends it.
                const std::string text = refusal.what(), head = "cannot compile ";
                const std::size_t colon = text.find(": ", head.size());
                if (text.rfind(head, 0) != 0 || colon == std::string::npos ||
                    !aside.insert(text.substr(head.size(), colon - head.size())).second)
                    return refusals;
            }
        }
    }

private:
    static Compiled Run(ReferenceStack<Value>& definitions, const std::string& module,
                        const std::string& source, const Model<Value>* model,
                        const Scope<Value>* instance, const std::set<std::string>& aside,
                        bool clauses = false) {
        std::set<std::string> fixed;
        for (;;) {
            CompileC compiler(definitions, model, instance ? *instance : definitions.Session());
            compiler.module_ = module;
            compiler.fixed_  = fixed;
            compiler.aside_  = aside;
            compiler.clauses_ = clauses;
            try {
                compiler.Define(compiler.root_);
                if (model) compiler.Signature();
                // An unnamed instance compiled on the way defines more.
                for (bool more = true; more;) {
                    more = false;
                    for (const auto& [name, sequence] : Sorted(compiler.sequences_)) {
                        if (!sequence.definition || sequence.compiled) continue;
                        compiler.Compile(name);
                        more = true;
                    }
                }
                return compiler.Print(module, source);
            } catch (const Fix& fix) {
                fixed.insert(fix.names.begin(), fix.names.end());
            }
        }
    }

    // Parameters read where the compiled code needs a constant -- a size, a
    // bound, a lag -- which the struct cannot let the host change. The file is
    // compiled again with them fixed, and so a constant everywhere they are
    // read. A parameter is never read once fixed, so this ends.
    struct Fix {
        std::set<std::string> names;
    };

    // Why a definition cannot be compiled; Compile names the definition.
    struct Reason : std::runtime_error {
        using std::runtime_error::runtime_error;
    };
    // The interpreter's own error, folding a value that reads no name.
    struct Undefined : Reason {
        using Reason::Reason;
    };
    // The interpreter's own refusal, as a reason the compiler gives.
    template <typename F>
    static auto Reasoned(F f) {
        try {
            return f();
        } catch (const Reason&) {
            throw;
        } catch (const std::runtime_error& error) {
            throw Reason(error.what());
        }
    }

    // A primary, a unary, a product, a sum: an operand is parenthesised only
    // where C would otherwise read it differently.
    enum Level { sum = 1, product, unary, primary };

    struct Cell {
        Cell() = default;
        Cell(std::string written, int precedence) : text(std::move(written)), level(precedence) {}

        std::string text;
        int         level = primary;
        std::string magnitude;  // what it is the negation of, if it is one
        int         magnitude_level = primary;
        bool        atom            = false;  // a name or a number, as cheap to repeat as to store
    };

    static Cell Atom(std::string text) {
        Cell cell(std::move(text), primary);
        cell.atom = true;
        return cell;
    }

    struct Code {
        std::size_t          rows = 1, cols = 1;
        std::vector<Cell>    cells;     // row by row
        std::optional<Value> constant;  // its exact value, where it reads no name

        bool Scalar() const { return cells.size() == 1; }
        // A single value stretches to any shape, as it does in arithmetic.
        const Cell& At(std::size_t i, std::size_t j) const {
            return Scalar() ? cells[0] : cells[i * cols + j];
        }
    };

    // A value a step computes once, before the sequence that reads it: its
    // name, what it computes, and the lines that declare it.
    struct Temporary {
        std::string              name, value;
        std::vector<std::string> lines;
    };

    // A value that reads parameters, computed where they are set rather than
    // at every step.
    struct Derived {
        std::string            name;
        Code                   code;
        std::vector<Temporary> temporaries;
        bool                   read = false;  // from its field, by some cell
        std::set<std::string>  parameters;    // that it reads
    };

    using Reads = std::map<std::string, std::set<int>>;  // lags, by the sequence read

    // A guarded clause, and what its guard and its value each read: its guard
    // is evaluated wherever the chain reaches it, its value only where it holds,
    // so each has its own index from which it can be.
    // A term a base clause reads: 'x_at' in the base clause of 'index'.
    struct Seed {
        std::string read;
        int         index, at, lag = 0;
    };

    struct Guarded {
        std::string              condition;
        std::vector<std::string> cells;
        Reads                    guard, value;
        int                      guard_from = 0, value_from = 0;
        int                      clause = 0;  // its place among the definition's
    };

    struct Sequence {
        const Reference<Value>*                 definition = nullptr;  // none for an input
        std::map<int, std::vector<std::string>> bases;                 // cells, by index
        std::vector<Guarded>                    guarded;               // in the order tried
        int general_clause = -1;  // the place of the clause that always applies, if one does
        Reads                                   general_reads;
        std::vector<std::string>                general;
        std::size_t                             rows = 0, cols = 0;  // 0 until known
        bool                                    based = false, compiling = false, compiled = false;
        std::map<std::string, std::set<int>>    reads;  // lags, by the sequence read
        Reads deferred;  // read only where the left of an 'and' or 'or' has not decided
        int                                     depth = 1;
        int                                     start = 0;
        std::vector<Temporary>                  temporaries;  // in the order declared
        // Of a term chosen cell by cell with a guard, the clause each cell
        // takes, as the guarded clause's 'name_clause_' is.
        std::vector<std::string> taken;
        // At another rate (DESIGN.md, several rates): a term every 'period'
        // steps, term m at step period*m + phase; the input-rate sequences it
        // samples, by the b of 'x_(period*m + b)'; its own terms read back, in
        // its own terms; and the holds it reads, by place in holds_.
        int                                  period = 1, phase = 0, first = 0;
        int filled = 0;  // its terms before the first that init folds from a history
        std::set<std::string> parameters;  // that its clauses read
        std::map<std::string, std::set<int>> samples;
        std::set<int>                        own;
        std::vector<std::size_t>             holds;
        // The terms its base clauses read, 'x_k' in the base clause of index
        // m, each a lag once the step computing that base is known.
        std::vector<Seed> seeds;
        Reads             seeded;
    };

    // A read of the latest term of a sequence at another rate, 'y_(floor((n -
    // b)/a) + d)': from where, how far back in its own terms it reads, and
    // the term computed again it is in, if any.
    struct Hold {
        std::string read;
        int         a, b, d;
        int         from = 0, least = 0, most = 0;
        int         early = -1;
    };

    // A term read back, by its mark: what it is, who reads it, what computing
    // it again reads, and the one it is computed again inside, if any.
    struct Early {
        std::string name;
        int         lag;
        Sequence*   reader;
        Reads       reads;
        int         outer;
    };

    // What a deferred right side reads, and the left that keeps it, read
    // with the scope, index, places and shift given.
    struct Check {
        Reads                        reads;
        PExpression<Value>           left;
        bool                         conjunction;
        const Scope<Value>*          scope;
        std::string                  index;
        std::map<std::string, Value> places;
        int                          shift;
    };

    struct Parameter {
        std::size_t         rows, cols;
        std::vector<double> initial;
    };

    CompileC(ReferenceStack<Value>& definitions, const Model<Value>* model,
             const Scope<Value>& root)
        : definitions_(definitions), model_(model), root_(root), scope_(&root) {}

    template <typename Map>
    static std::map<std::string, typename Map::mapped_type> Sorted(const Map& map) {
        return {map.begin(), map.end()};
    }

    // Each scope's definitions, and those of the instances it names, each
    // under the name that says where it is, 'fast.v' (DESIGN.md, phase
    // 15), which is also how C reaches it in the struct.
    void Define(const Scope<Value>& scope) {
        for (const auto& [name, definition] : Sorted(scope.names)) {
            const std::string key = scope.Qualified(name);
            if (!definition->Value() || aside_.count(key)) continue;
            if (definition->argument) {
                History(key, *definition);
                if (!definition->argument->input) Define(key, *definition->argument);
                continue;
            }
            Define(key, *definition);
            const Scope<Value>* instance = nullptr;
            try {
                instance = definitions_.InstanceScope(definition);
            } catch (const std::runtime_error& error) {
                throw Refusal("cannot compile " + key + ": " + error.what());
            }
            if (instance) Define(*instance);
        }
    }

    // A call compiled where it is made (DESIGN.md, phase 15): the names
    // of a function, or of an instance of a model without memory, bound to
    // what the caller gives them, and its own computed when first read. A
    // constant folds through, and a recursion unrolls where its guards fold.
    struct Expansion {
        std::map<std::string, Code>                    values;  // given, or computed
        std::map<std::string, const Reference<Value>*> own;     // defined here
        const Scope<Value>*                            scope;   // where the rest are sought
        Expansion*                                     outer;   // the model a function is in
        std::set<std::string>                          computing;
    };

    // What an expansion gives a name, if one does.
    std::optional<Code> Expanded(const std::string& name) {
        for (Expansion* expansion = expansion_; expansion; expansion = expansion->outer) {
            if (const auto value = expansion->values.find(name); value != expansion->values.end())
                return value->second;
            const auto own = expansion->own.find(name);
            if (own == expansion->own.end()) continue;
            const Reference<Value>& definition = *own->second;
            if (definition.input) throw Reason(name + " is an input nothing gives");
            if (!definition.Value()) return std::nullopt;
            if (!definition.Clauses().front().parameters.parameters_names().empty())
                throw Reason(name + ", a function, read as a value");
            if (!expansion->computing.insert(name).second)
                throw Reason(name + " is defined by itself");
            const Code code = Inside(*expansion, [&] { return Chained(name, definition); });
            expansion->computing.erase(name);
            return expansion->values.emplace(name, code).first->second;
        }
        return std::nullopt;
    }

    // Where a function a call names is defined inside the expansion, if it is.
    std::pair<const Reference<Value>*, Expansion*> Local(const std::string& name) const {
        for (Expansion* expansion = expansion_; expansion; expansion = expansion->outer) {
            const auto own = expansion->own.find(name);
            if (own != expansion->own.end()) return {own->second, expansion};
            if (expansion->values.count(name)) break;
        }
        return {nullptr, nullptr};
    }

    // The places an unnamed instance's argument was given, as constants.
    static std::map<std::string, Value> Captured(const Reference<Value>* definition) {
        std::map<std::string, Value> places;
        if (definition)
            for (const auto& [name, value] : definition->captured) places.emplace(name, value);
        return places;
    }

    // One with memory is kept between steps, so it is an instance of its own,
    // made once for each place it is written and for each value it reads of
    // the cell there, and named after them: 'bank_smooth_1' is the one the
    // first row of bank makes. One made anew at each step would run from its
    // base at every one.
    Code Kept(const Reference<Value>& model, const ParametersCall<Value>& call,
              const PExpression<Value>& member) {
        if (expansion_)
            throw Reason("an instance of " + model.Name() + ", which keeps a history, in a call");
        // What each argument reads, but its own index, which a positional
        // one takes from the signature.
        const auto bound = Reasoned([&] { return model.model->Bind(model.Name(), call); });
        std::set<std::string> read;
        for (const auto& argument : bound) {
            if (!argument) continue;
            std::set<std::string> names;
            Named(argument->expression, names);
            names.erase(argument->index);
            read.insert(names.begin(), names.end());
        }
        std::vector<std::pair<std::string, Value>> captured;
        std::string                                values;
        for (const std::string& name : read) {
            if (!index_.empty() && name == index_)
                throw Reason("an instance of " + model.Name() +
                             ", which keeps a history, made anew at each step");
            const auto place = places_.find(name);
            if (place == places_.end()) continue;
            captured.emplace_back(name, place->second);
            values += "_" + std::to_string(numeric_interface<Value>::toInt(place->second));
        }
        const Scope<Value>*& kept = kept_[{member.get(), values}];
        if (!kept) {
            // Two written in one sequence are told apart by a number.
            std::string label = within_ + "_" + model.Name() + values;
            for (int other = 2; !labels_.insert(label).second; ++other)
                label = within_ + "_" + model.Name() + std::to_string(other) + values;
            held_.push_back(definitions_.Detached(model, call, *scope_, label, captured));
            kept = held_.back().get();
            Define(*kept);
        }
        own_ = kept;
        return Emit(member);
    }

    // The names an expression reads, but not those after a point.
    static void Named(const PExpression<Value>& expression, std::set<std::string>& names) {
        if (!expression) return;
        if (!expression->Name().empty()) names.insert(expression->Name());
        const auto* member = dynamic_cast<const MemberExpression<Value>*>(expression.get());
        for (const PExpression<Value>& child : expression->Children())
            if (!member || child != member->Member()) Named(child, names);
    }

    // The model an object names, 'conv(lap, x)' or 'gain', if it names one.
    const Reference<Value>* ModelOf(const Expression<Value>& object) const {
        const auto* ref  = dynamic_cast<const RefExpression<Value>*>(&object);
        const auto* call = dynamic_cast<const FuncExpression<Value>*>(&object);
        if ((!ref && !call) || (call && (call->m_e2() || call->limit()))) return nullptr;
        if (const auto [local, in] = Local(object.Name()); local)
            return local->model ? local : nullptr;
        for (const Scope<Value>* scope = scope_; scope; scope = scope->parent) {
            const auto found = scope->names.find(object.Name());
            if (found != scope->names.end())
                return found->second->model ? found->second.get() : nullptr;
        }
        return nullptr;
    }

    static const ParametersCall<Value>& Call(const Expression<Value>& expression) {
        static const ParametersCall<Value> plain;
        const auto* call = dynamic_cast<const FuncExpression<Value>*>(&expression);
        return call ? call->Call() : plain;
    }

    // An expansion's names are its own: the caller's index, cells and names
    // are not seen inside it.
    template <typename Body>
    Code Inside(Expansion& expansion, Body body) {
        struct Restore {
            CompileC&                    compiler;
            Expansion*                   expansion;
            const Scope<Value>*          scope;
            std::string                  index;
            std::map<std::string, Value> places;
            ~Restore() {
                compiler.expansion_ = expansion;
                compiler.scope_     = scope;
                compiler.index_     = index;
                compiler.places_    = places;
                --compiler.expanded_;
            }
        } restore{*this, std::exchange(expansion_, &expansion),
                  std::exchange(scope_, expansion.scope), std::exchange(index_, std::string()),
                  std::exchange(places_, {})};
        if (++expanded_ > max_expanded)
            throw Reason("calls nested " + std::to_string(max_expanded) +
                         " deep, which a recursion its guards do not end would pass");
        return body();
    }
    static constexpr int max_expanded = 64;

    // A guarded value the interpreter refuses wherever it is taken, as log's
    // '| x <= 0 = 1/0', is what a step says there: NaN, as where no clause
    // applies (C86).
    Code Taken(const PExpression<Value>& expression) {
        try {
            return Emit(expression);
        } catch (const Undefined&) {
            Code nan;
            nan.cells.push_back(Atom("NAN"));
            return nan;
        }
    }

    // A value that is not a sequence, its clauses tried as the interpreter
    // tries them: the guarded in the order written, then the one that always
    // applies.
    Code Chained(const std::string& name, const Reference<Value>& definition) {
        if (IsSequence(definition)) throw Reason(name + " is a sequence; index it");
        const auto cells = [](const Clause<Value>& c) { return c.parameters.cells(); };
        if (std::any_of(definition.Clauses().begin(), definition.Clauses().end(), cells))
            return Cells(name, definition);
        std::vector<std::pair<std::string, Code>> guarded;
        std::optional<Code>                       otherwise;
        for (const bool guard : {true, false}) {
            for (const Clause<Value>& clause : definition.Clauses()) {
                const ParametersDefinition<Value>& p = clause.parameters;
                if (otherwise || p.guarded() != guard) continue;
                const std::optional<std::string> condition =
                    guard ? Condition(p.guard()) : std::optional<std::string>("");
                if (!condition) continue;
                if (condition->empty())
                    otherwise = Emit(clause.expression);
                else
                    guarded.emplace_back(*condition, Taken(clause.expression));
            }
        }
        if (guarded.empty()) {
            if (!otherwise) throw Reason("no clause of " + name + " applies");
            return *otherwise;
        }
        std::size_t rows = 1, cols = 1;
        for (const auto& [condition, value] : guarded) {
            rows = std::max(rows, value.rows);
            cols = std::max(cols, value.cols);
        }
        return Chain(guarded, otherwise, rows, cols);
    }

    // Each cell as the step tests it: the guarded values in order, then the
    // one that always applies, else NaN, as where no clause applies (C86).
    static Code Chain(const std::vector<std::pair<std::string, Code>>& guarded,
                      const std::optional<Code>& otherwise, std::size_t rows, std::size_t cols) {
        Code chain;
        chain.rows = rows;
        chain.cols = cols;
        for (std::size_t i = 0; i < rows; ++i) {
            for (std::size_t j = 0; j < cols; ++j) {
                if (guarded.empty() && otherwise) {
                    chain.cells.push_back(otherwise->At(i, j));
                    continue;
                }
                std::string cell;
                for (const auto& [condition, value] : guarded)
                    cell += condition + " ? " + value.At(i, j).text + " : ";
                cell += otherwise ? otherwise->At(i, j).text : "NAN";
                chain.cells.emplace_back(cell, 0);  // a conditional, below every operator
            }
        }
        return chain;
    }

    // A function called: its parameters bound to the arguments, read where the
    // call is, and its defaults to what they read inside it.
    Code Call(const std::string& name, const Reference<Value>& function,
              const ParametersCall<Value>& call, Expansion* outer) {
        const ParametersDefinition<Value>& p = function.Clauses().front().parameters;
        Reasoned([&] { p.CheckArity(name, call); });
        Expansion expansion{{}, {}, outer ? outer->scope : function.home, outer, {}};
        const std::vector<std::string>& names = p.parameters_names();
        for (std::size_t i = 0; i < call.parameters_expression().size(); ++i)
            expansion.values.emplace(names[i], Emit(call.parameters_expression()[i]));
        for (const auto& [given, argument] : call.parameters_dict())
            expansion.values.emplace(given, Emit(argument));
        Reasoned([&] {
            for (const auto& [given, value] : expansion.values)
                function.Divides(given, Value(Extent{value.rows, value.cols}), definitions_);
        });
        return Inside(expansion, [&] {
            for (const std::string& parameter : names) {
                if (expansion.values.count(parameter)) continue;
                expansion.values.emplace(parameter, Emit(p.parameters_dict().at(parameter)));
            }
            return Chained(name, function);
        });
    }

    // A name of an instance written where it is read, 'conv(lap, u_(n-1)).out':
    // one of a model without memory is a call.
    Code Instance(const Reference<Value>& model, const ParametersCall<Value>& call,
                  const PExpression<Value>& member) {
        const Model<Value>& m         = *model.model;
        const auto          remembers = [](const typename Model<Value>::Statement& s) {
            return s.definition && !s.definition->Children()[0]->Children().empty() &&
                   s.definition->Children()[0]->Children()[1];
        };
        if (std::any_of(m.parameters.begin(), m.parameters.end(),
                        [](const auto& q) { return !q.index.empty(); }) ||
            std::any_of(m.body.begin(), m.body.end(), remembers))
            return Kept(model, call, member);
        const auto bound = Reasoned([&] { return m.Bind(model.Name(), call); });
        held_.push_back(definitions_.Defaults(model));
        Expansion expansion{{}, {}, m.scope, nullptr, {}};
        for (std::size_t i = 0; i < m.parameters.size(); ++i)
            if (bound[i])
                expansion.values.emplace(m.parameters[i].name, Emit(bound[i]->expression));
        for (const auto& [name, definition] : held_.back()->names)
            if (!expansion.values.count(name)) expansion.own.emplace(name, definition.get());
        return Inside(expansion, [&] { return Emit(member); });
    }

    // A definition, where it was found, and its name there.
    struct Found {
        const Reference<Value>* definition;  // null for an input the step is given
        const Scope<Value>*     where;
        std::string             key;
    };

    // A name where the scope being compiled reads it, or, just after a point,
    // in the scope before the point alone. An input is the step's where the
    // header's own scope declares it, or where the session reads a name
    // nothing defines; anywhere else a name nothing gives is a mistake.
    Found Lookup(const std::string& name) {
        const Scope<Value>* const own = std::exchange(own_, nullptr);
        for (const Scope<Value>* scope = own ? own : scope_; scope;
             scope                     = own ? nullptr : scope->parent) {
            const auto found = scope->names.find(name);
            if (found == scope->names.end()) continue;
            const Reference<Value>& definition =
                found->second->argument ? *found->second->argument : *found->second;
            const std::string key = scope->Qualified(name);
            if (definition.input) {
                if (scope == &root_) return {nullptr, scope, key};
                throw Reason(key + " is an input nothing gives");
            }
            if (definition.model) throw Reason("an instance of " + key + " read as a value");
            if (definition.file) throw Reason(key + ", a file, read as a value");
            if (aside_.count(key)) throw Reason(key + ", which cannot be compiled");
            return {&definition, scope, key};
        }
        if (!own && scope_ == &root_ && !root_.model) return {nullptr, &root_, name};
        throw Reason((own ? own : scope_)->Qualified(name) + " is not defined");
    }

    // Names in a definition are sought where it was written.
    struct Home : Setting<const Scope<Value>*> {
        Home(CompileC& compiler, const Reference<Value>* definition)
            : Setting(compiler.scope_,
                      definition && definition->home ? definition->home : compiler.scope_) {}
    };

    // Every parameter the signature gives is in the header, read or not: a
    // value as a field, an input as an argument of the step.
    void Signature() {
        for (const typename Model<Value>::Parameter& parameter : model_->parameters) {
            if (aside_.count(parameter.name)) continue;
            if (!parameter.index.empty() && !parameter.fallback) {
                Unreserved(parameter.name);
                Sequence& input = sequences_[parameter.name];
                input.rows = input.cols = 1;
            } else if (parameter.index.empty()) {
                try {
                    (void)Emit(std::make_shared<RefExpression<Value>>(parameter.name));
                } catch (const Reason& reason) {
                    throw Refusal("cannot compile " + parameter.name + ": " + reason.what());
                }
            }
        }
    }

    // An instance's parameters are its fields, whatever their defaults read,
    // and nothing else it reads is; a file used has none; in the session every
    // value that reads none is one, as a built-in is.
    bool Settable(const Scope<Value>& where, const std::string& name, bool reads) const {
        if (!root_.model && (&where == &root_ || &where == &definitions_.Builtins())) return !reads;
        if (!where.model) return false;
        return std::any_of(where.model->parameters.begin(), where.model->parameters.end(),
                           [&](const auto& p) { return p.name == name && p.index.empty(); });
    }

    static bool IsSequence(const Reference<Value>& definition) {
        return definition.Clauses().front().parameters.indexed();
    }

    // Every definition is looked at, so that one the target cannot express is
    // refused rather than left out; a plain one is compiled where it is read.
    void Define(const std::string& name, const Reference<Value>& definition) {
        for (const Clause<Value>& clause : definition.Clauses()) {
            if (clause.parameters.tensor()) throw Refusal("cannot compile " + name + ": a tensor");
            // A function is compiled where it is called, and a sequence with
            // parameters where a limit walks it.
            if (!clause.parameters.parameters_names().empty()) return;
            const ParametersDefinition<Value>& p = clause.parameters;
            if (p.guarded() && !p.cells() && !p.general())
                throw Refusal("cannot compile " + name + ": a guarded " +
                              (p.indexed() ? "base clause" : "value"));
        }
        // A base clause written after a guarded one is reached only if the guard
        // fails, which the chain a step computes would not say. A term by its
        // cells is the exception: its base comes first, however it was written.
        bool guarded = false;
        for (const Clause<Value>& clause : definition.Clauses()) {
            if (ByCells(definition)) break;
            guarded = guarded || (clause.parameters.guarded() && clause.parameters.general());
            if (guarded && clause.parameters.indexed() && !clause.parameters.general())
                throw Refusal("cannot compile " + name + ": a base clause after a guarded one");
        }
        if (IsSequence(definition)) sequences_[name].definition = &definition;
    }

    // A history is folded by init, so it may read only its index and constants,
    // and must give nothing in the stream: a guard is its index below a
    // constant, which shows where it stops.
    void History(const std::string& key, const Reference<Value>& history) {
        histories_[key] = &history;
        const Home        home(*this, &history);
        const std::string outer = index_;
        try {
            for (const Clause<Value>& clause : history.Clauses()) {
                const ParametersDefinition<Value>& p = clause.parameters;
                index_                               = p.general() ? p.index_name() : std::string();
                int top = p.general() ? std::numeric_limits<int>::min() : p.index();
                if (p.guarded()) {
                    const auto* compare = dynamic_cast<CompareExpression<Value>*>(p.guard().get());
                    const Comparison op = compare ? compare->Op() : Comparison::Equal;
                    const bool  flip = op == Comparison::Greater || op == Comparison::GreaterEqual;
                    const auto* at   = compare ? dynamic_cast<const RefExpression<Value>*>(
                                                   (flip ? compare->m_e2() : compare->m_e1()).get())
                                               : nullptr;
                    const auto  k =
                        op < Comparison::Equal && at && p.general() && at->Name() == index_
                             ? Constant(flip ? compare->m_e1() : compare->m_e2())
                             : std::nullopt;
                    if (!k) throw Reason("a history whose guard is not its index below a constant");
                    top = *k - (op == Comparison::Less || op == Comparison::Greater);
                }
                const auto [code, reads] = Reading(clause.expression);
                if (!reads.empty()) throw Reason("a history that reads " + *reads.begin());
                if (!code.Scalar()) throw Reason("a history that is not a single value");
                int& reach = reach_.try_emplace(key, top).first->second;
                reach      = std::max(reach, top);
            }
        } catch (const Reason& reason) {
            throw Refusal("cannot compile " + key + ": " + reason.what());
        }
        index_ = outer;
    }

    // A clause is compiled in a scope of its own, and a refusal inside it
    // names its sequence.
    template <typename Body>
    void Within(const std::string& name, Sequence* reading, const std::string& index, Body body) {
        const Home        home(*this, sequences_.at(name).definition);
        Sequence* const   outer_reading = std::exchange(reading_, reading);
        const std::string outer_index   = std::exchange(index_, index);
        const std::string outer_within  = std::exchange(within_, name);
        auto* const       outer_temporaries =
            std::exchange(temporaries_, &sequences_.at(name).temporaries);
        // A cell's names are its own clause's: another sequence compiled on
        // the way, whose cells may use the same names, must not see or undo them.
        const auto outer_places = std::exchange(places_, Captured(sequences_.at(name).definition));
        const int  outer_shift  = std::exchange(shift_, 0);
        auto       outer_parameters = std::exchange(read_parameters_, {});
        try {
            body();
        } catch (const Reason& reason) {
            throw Refusal("cannot compile " + name + ": " + reason.what());
        }
        sequences_.at(name).parameters.insert(read_parameters_.begin(), read_parameters_.end());
        read_parameters_.insert(outer_parameters.begin(), outer_parameters.end());
        reading_     = outer_reading;
        index_       = outer_index;
        within_      = outer_within;
        temporaries_ = outer_temporaries;
        places_      = outer_places;
        shift_       = outer_shift;
    }

    // A cell the compiler would write out more than once -- an operand of a
    // matrix product, a single value stretched over a matrix -- is computed once
    // per step instead, into a temporary: the same value, in fewer lines.
    Cell Shared(const Cell& cell) {
        if (cell.atom || !temporaries_) return cell;
        return Atom(Declare(cell.text, [&](const std::string& name) {
            return std::vector<std::string>{"const double " + name + " = " + cell.text + ";"};
        }));
    }

    // The same value is the same temporary within one sequence's step.
    template <typename Lines>
    std::string Declare(const std::string& value, Lines lines) {
        if (!temporaries_) throw Reason("a matrix inverse outside a sequence");
        for (const Temporary& temporary : *temporaries_)
            if (temporary.value == value) return temporary.name;
        const std::string name = "t" + std::to_string(temporary_count_++) + "_";
        temporaries_->push_back({name, value, lines(name)});
        return name;
    }
    Code Shared(Code code) {
        for (Cell& cell : code.cells) cell = Shared(cell);
        return code;
    }

    template <typename Combine>
    Code Broadcast(Code left, Code right, Combine combine) {
        if (left.Scalar() && !right.Scalar()) left = Shared(left);
        if (right.Scalar() && !left.Scalar()) right = Shared(right);
        return Cellwise(left, right, combine);
    }

    void Shape(Sequence& sequence, const Code& code) {
        if (sequence.rows && (code.rows != sequence.rows || code.cols != sequence.cols))
            throw Reason("its clauses have different shapes");
        sequence.rows = code.rows;
        sequence.cols = code.cols;
    }

    static std::vector<std::string> Texts(const Code& code) {
        std::vector<std::string> texts;
        for (const Cell& cell : code.cells) texts.push_back(cell.text);
        return texts;
    }

    // The base clauses give a sequence its shape without its general clause,
    // which may read the very sequences that need the shape.
    static bool ByCells(const Reference<Value>& definition) {
        const auto& clauses = definition.Clauses();
        return std::any_of(clauses.begin(), clauses.end(), [](const Clause<Value>& c) {
            return c.parameters.indexed() && c.parameters.cells();
        });
    }

    void Bases(const std::string& name) {
        Sequence& sequence = sequences_.at(name);
        if (sequence.based || !sequence.definition) return;
        sequence.based = true;
        Within(name, nullptr, std::string(), [&] {
            Unreserved(name);
            if (ByCells(*sequence.definition)) return BaseTerms(name, sequence);
            for (const Clause<Value>& clause : sequence.definition->Clauses()) {
                if (clause.parameters.general()) continue;
                Sequence* const outer = std::exchange(basing_, &sequence);
                const int       base  = std::exchange(base_, clause.parameters.index());
                const Code      code  = Emit(clause.expression);
                basing_               = outer;
                base_                 = base;
                Shape(sequence, code);
                sequence.bases[clause.parameters.index()] = Texts(code);
            }
        });
    }

    // In turn, or earlier where a reader needs a shape no base clause gives.
    void Compile(const std::string& name) {
        Sequence& sequence = sequences_.at(name);
        if (sequence.compiled || !sequence.definition) return;
        const Home home(*this, sequence.definition);
        if (sequence.compiling)
            throw Refusal("cannot compile " + name + ": its shape depends on itself");
        sequence.compiling = true;
        Bases(name);
        if (ByCells(*sequence.definition)) {
            CompileTerms(name, sequence);
            sequence.compiled = true;
            return;
        }
        Reads* const outer_reads = clause_reads_;
        // The guarded clauses in the order written, then the unguarded one, as
        // the interpreter tries them; a guard that always holds ends the chain.
        bool settled = false;
        for (const bool guarded : {true, false}) {
            for (const Clause<Value>& clause : sequence.definition->Clauses()) {
                const ParametersDefinition<Value>& p = clause.parameters;
                if (settled || !p.general() || p.guarded() != guarded) continue;
                Within(name, &sequence, p.index_name(), [&] {
                    // The index as the step has it, known once the rate is.
                    const std::string outer_text =
                        std::exchange(index_text_, "(double)\x12" + name + "\x13");
                    Reads guard_reads, value_reads;
                    clause_reads_ = &guard_reads;
                    const std::optional<std::string> condition =
                        guarded ? Condition(p.guard()) : std::optional<std::string>("");
                    if (!condition) return;
                    clause_reads_   = &value_reads;
                    const Code code = Emit(clause.expression);
                    clause_reads_   = nullptr;
                    Shape(sequence, code);
                    const int place =
                        static_cast<int>(&clause - sequence.definition->Clauses().data());
                    if (condition->empty()) {
                        sequence.general        = Texts(code);
                        sequence.general_reads  = value_reads;
                        sequence.general_clause = place;
                        settled                 = true;
                    } else {
                        sequence.guarded.push_back(
                            {*condition, Texts(code), guard_reads, value_reads, 0, 0, place});
                    }
                    index_text_ = outer_text;
                });
            }
        }
        if (!settled && sequence.guarded.empty())
            throw Refusal("cannot compile " + name + ": a sequence with no general clause");
        // Where no guard holds the interpreter says so; a step can only say NaN.
        if (!settled) sequence.general.assign(sequence.rows * sequence.cols, "NAN");
        clause_reads_     = outer_reads;
        sequence.compiled = true;
    }

    Code Emit(const PExpression<Value>& expression) {
        expression->accept(*this);
        return code_;
    }

    // A value, and the parameters it reads.
    std::pair<Code, std::set<std::string>> Reading(const PExpression<Value>& expression) {
        auto       outer = std::exchange(read_parameters_, {});
        const Code code  = Emit(expression);
        auto       reads = std::exchange(read_parameters_, std::move(outer));
        read_parameters_.insert(reads.begin(), reads.end());
        return {code, reads};
    }

    // A value the compiled code needs as a constant; see Fix.
    Code Known(const PExpression<Value>& expression, const std::string& refusal) {
        auto [code, reads] = Reading(expression);
        if (code.constant) return code;
        if (!reads.empty()) throw Fix{std::move(reads)};
        throw Reason(refusal);
    }

    static std::string Wrap(const std::string& text, int level, int needed) {
        return level < needed ? "(" + text + ")" : text;
    }
    static std::string Wrap(const Cell& cell, int needed) {
        return Wrap(cell.text, cell.level, needed);
    }

    PExpression<Value> Answer(Code code) {
        code_ = std::move(code);
        return PExpression<Value>();
    }
    PExpression<Value> Answer(Cell cell) {
        Code code;
        code.cells = {std::move(cell)};
        return Answer(std::move(code));
    }

    // Exactly, as the interpreter would, and rounded once: 0.1 + 0.2 is 3/10
    // here, which a C compiler folding the doubles would not find.
    // By the interpreter, where the expression is read: a cell's names and
    // what a call gives are locals there, and a function it calls is its own.
    PExpression<Value> Fold(Expression<Value>* expression) {
        definitions_.BeginEvaluation();
        const typename ReferenceStack<Value>::Within within(definitions_, scope_);
        const typename ReferenceStack<Value>::Frame  frame(definitions_);
        std::vector<const Expansion*>                calls;
        for (const Expansion* call = expansion_; call; call = call->outer) calls.push_back(call);
        for (auto call = calls.rbegin(); call != calls.rend(); ++call)
            for (const auto& [name, code] : (*call)->values)
                if (code.constant) definitions_.BindValue(name, *code.constant);
        for (const auto& [name, value] : places_) definitions_.BindValue(name, value);
        EvaluationVisitor<Value> evaluator(definitions_);
        try {
            return Answer(Literal(expression->accept(evaluator)));
        } catch (const Reason&) {
            throw;
        } catch (const std::runtime_error& error) {
            throw Undefined(error.what());
        }
    }

    static std::vector<double> Doubles(const Value& value) {
        // The interpreter is the reference a model is held to first; compiling
        // attention per batch is an entry of its own (DESIGN.md).
        if (value.Size().slices) throw Reason("a tensor");
        std::vector<double> doubles;
        for (std::size_t i = 1; i <= value.Size().rows; ++i) {
            for (std::size_t j = 1; j <= value.Size().cols; ++j) {
                const auto number = value(i, j).Inexact();
                if (number.imag() != 0) throw Reason("a complex number");
                doubles.push_back(number.real());
            }
        }
        return doubles;
    }

    static Code Literal(const Value& value) {
        Code code;
        code.rows     = value.Size().rows;
        code.cols     = value.Size().cols;
        code.constant = value;
        for (const double x : Doubles(value)) {
            Cell cell = Atom(Double(std::abs(x)));
            if (std::signbit(x)) {
                cell.magnitude = cell.text;
                cell.text      = "-" + cell.text;
                cell.level     = unary;
            }
            code.cells.push_back(cell);
        }
        return code;
    }

    static std::string Double(double x) {
        if (std::isnan(x)) return "NAN";
        if (std::isinf(x)) return "INFINITY";
        char        text[32];
        const auto  end = std::to_chars(text, text + sizeof text, x).ptr;
        std::string written(text, end);
        if (written.find_first_of(".e") == std::string::npos) written += ".0";
        return written;
    }

    // Cell by cell, a single value stretching to the other's shape, as the
    // interpreter's arithmetic does.
    template <typename Combine>
    static Code Cellwise(const Code& left, const Code& right, Combine combine) {
        if (!left.Scalar() && !right.Scalar() &&
            (left.rows != right.rows || left.cols != right.cols))
            throw Reason("these matrices have different sizes");
        Code code;
        code.rows = left.Scalar() ? right.rows : left.rows;
        code.cols = left.Scalar() ? right.cols : left.cols;
        for (std::size_t i = 0; i < code.rows; ++i)
            for (std::size_t j = 0; j < code.cols; ++j)
                code.cells.push_back(combine(left.At(i, j), right.At(i, j)));
        return code;
    }

    // a + (-b) is a - b in IEEE arithmetic, and reads as what was written.
    static Cell Added(const Cell& left, const Cell& right) {
        if (!right.magnitude.empty())
            return {Wrap(left, sum) + " - " + Wrap(right.magnitude, right.magnitude_level, product),
                    sum};
        return {Wrap(left, sum) + " + " + Wrap(right, product), sum};
    }
    static Cell Multiplied(const Cell& left, const Cell& right) {
        return {Wrap(left, product) + " * " + Wrap(right, unary), product};
    }
    static Cell Divided(const Cell& left, const Cell& right) {
        return {Wrap(left, product) + " / " + Wrap(right, unary), product};
    }

    PExpression<Value> visit(ValExpression<Value>* expression) override {
        return Answer(Literal(expression->value));
    }

    PExpression<Value> visit(AddExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        return Answer(Broadcast(left, right, Added));
    }

    PExpression<Value> visit(DivExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        return Answer(Broadcast(left, right, Divided));
    }

    // A matrix product is a sum over the inner dimension, in the interpreter's
    // order.
    PExpression<Value> visit(MultExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        return Answer(Product(left, right));
    }

    // Each left cell is read once for each right column, each right cell once
    // for each left row.
    Code Product(Code left, Code right) {
        if (left.Scalar() || right.Scalar()) return Broadcast(left, right, Multiplied);
        if (right.cols > 1) left = Shared(left);
        if (left.rows > 1) right = Shared(right);
        if (left.cols != right.rows)
            throw Reason("a matrix product needs as many columns on the left as rows on the right");
        Code code;
        code.rows = left.rows;
        code.cols = right.cols;
        for (std::size_t i = 0; i < code.rows; ++i) {
            for (std::size_t j = 0; j < code.cols; ++j) {
                Cell cell = Multiplied(left.At(i, 0), right.At(0, j));
                for (std::size_t k = 1; k < left.cols; ++k)
                    cell = Added(cell, Multiplied(left.At(i, k), right.At(k, j)));
                code.cells.push_back(cell);
            }
        }
        return code;
    }

    PExpression<Value> visit(NegExpression<Value>* expression) override {
        Code code = Emit(expression->m_e());
        if (code.constant) return Fold(expression);
        for (Cell& cell : code.cells) {
            Cell negation(
                "-" + (cell.magnitude.empty() ? Wrap(cell, unary) : "(" + cell.text + ")"), unary);
            negation.magnitude       = cell.text;
            negation.magnitude_level = cell.level;
            negation.atom            = cell.atom;
            cell                     = negation;
        }
        return Answer(code);
    }

    PExpression<Value> visit(InexactExpression<Value>* expression) override {
        const Code operand = Emit(expression->m_e());
        return operand.constant ? Fold(expression) : Answer(operand);
    }

    PExpression<Value> visit(TransposeExpression<Value>* expression) override {
        const Code operand = Emit(expression->m_e());
        if (operand.constant) return Fold(expression);
        Code code;
        code.rows = operand.cols;
        code.cols = operand.rows;
        for (std::size_t i = 0; i < code.rows; ++i)
            for (std::size_t j = 0; j < code.cols; ++j) code.cells.push_back(operand.At(j, i));
        return Answer(code);
    }

    PExpression<Value> visit(PowExpression<Value>* expression) override {
        const Code base = Emit(expression->m_e1());
        const Code exponent =
            base.Scalar()
                ? Emit(expression->m_e2())
                : Known(expression->m_e2(), "a matrix power whose exponent is not a constant");
        if (base.constant && exponent.constant) return Fold(expression);
        if (!exponent.Scalar()) throw Reason("a matrix cannot be an exponent");
        if (!base.Scalar()) return Answer(Power(base, exponent));
        return Answer(
            Cell("pow(" + base.cells[0].text + ", " + exponent.cells[0].text + ")", primary));
    }

    // By squaring, as the interpreter does it, of the inverse for a negative
    // exponent. The interpreter starts from the identity and multiplies it in;
    // starting from the first factor instead changes only a cell that is
    // infinite, where the identity's zeros would make it NaN.
    Code Power(Code base, const Code& exponent) {
        if (!exponent.constant) throw Reason("a matrix power whose exponent is not a constant");
        const auto power = (*exponent.constant)(1, 1).Inexact();
        if (power.imag() != 0 || power.real() != std::floor(power.real()) ||
            std::abs(power.real()) > 2147483647.0)
            throw Reason("a matrix power must be a whole number");
        if (base.rows != base.cols) throw Reason("only a square matrix has a power");
        const long long whole = static_cast<long long>(power.real());
        if (whole == 0) {
            // The identity whatever the base, but not a constant: a constant is
            // what reads no name, and an enclosing fold would read this one's.
            Code identity = Literal(Value::Identity(Extent{base.rows, base.cols}));
            identity.constant.reset();
            return identity;
        }
        if (whole < 0) base = Inverse(base);
        std::optional<Code> result;
        for (unsigned long long n = static_cast<unsigned long long>(whole < 0 ? -whole : whole);
             n != 0; n >>= 1) {
            if (n & 1) result = result ? Product(*result, base) : base;
            if (n > 1) base = Product(base, base);
        }
        return *result;
    }

    // In place, by the helper the header defines for its size, which pivots as
    // the interpreter does: that depends on the values, so it is done as the
    // step runs.
    Code Inverse(const Code& matrix) {
        const std::size_t n     = matrix.rows;
        std::string       value = "inverse";
        std::string       rows;
        for (std::size_t i = 0; i < n; ++i) {
            std::string row;
            for (std::size_t j = 0; j < n; ++j) row += (j ? ", " : "") + matrix.At(i, j).text;
            rows += (i ? ", {" : "{") + row + "}";
            value += "\x1f" + row;
        }
        inverses_.insert(n);
        const std::string name = Declare(value, [&](const std::string& t) {
            return std::vector<std::string>{
                "double " + t + Subscript(n, n) + " = {" + rows + "};",
                module_ + "_inverse" + std::to_string(n) + "_(" + t + ");"};
        });
        return Array(name, n, n);
    }

    static const char* Operator(Comparison op) {
        static const char* const ops[] = {" < ", " > ", " <= ", " >= ", " == ", " != "};
        return ops[static_cast<int>(op)];
    }

    PExpression<Value> visit(CompareExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (left.constant && right.constant) return Fold(expression);
        if (!left.Scalar() || !right.Scalar()) throw Reason("a comparison of matrices");
        return Answer(Cell("(" + Wrap(left.cells[0], sum) + Operator(expression->Op()) +
                               Wrap(right.cells[0], sum) + " ? 1.0 : 0.0)",
                           primary));
    }

    // A guard as C tests it. Empty where it always holds, and nothing where
    // it never does.
    std::optional<std::string> Condition(const PExpression<Value>& guard) {
        const Code code = Quiet(guard);
        if (!code.Scalar()) throw Reason("a guard that is a matrix");
        if (code.constant) {
            if (!Value::truth(*code.constant)) return std::nullopt;
            return std::string();
        }
        if (!Defers(guard)) return Test(guard);
        const std::string truth = Shared(Cell(Truth(guard), primary)).text;
        return "isnan(" + truth + ") ? NAN : " + truth + " != 0.0";
    }

    // A truth as C tests it: a comparison as itself, 'and' and 'or' as C's,
    // which read their right side only when they must, as the interpreter's
    // do; anything else against zero, which is the interpreter's truth, NaN
    // holding. For what does not defer (see Right).
    std::string Test(const PExpression<Value>& expression) {
        if (const auto* logic = dynamic_cast<LogicExpression<Value>*>(expression.get())) {
            const auto operand = [this](const PExpression<Value>& side) {
                const std::string test = Test(side);
                return dynamic_cast<LogicExpression<Value>*>(side.get()) ? "(" + test + ")" : test;
            };
            const std::string left = operand(logic->m_e1());
            if (left == (logic->Conjunction() ? "0" : "1")) return left;
            return left + (logic->Conjunction() ? " && " : " || ") + Right(*logic, operand).first;
        }
        const Code code = Emit(expression);
        if (code.constant) return Value::truth(*code.constant) ? "1" : "0";
        if (const auto* compare = dynamic_cast<CompareExpression<Value>*>(expression.get())) {
            const Code left = Emit(compare->m_e1()), right = Emit(compare->m_e2());
            return Wrap(left.cells[0], sum) + Operator(compare->Op()) + Wrap(right.cells[0], sum);
        }
        return Wrap(code.cells[0], sum) + " != 0.0";
    }

    // The right of an 'and' or 'or' is read only where its left has not
    // decided, so the terms it reads cannot say where its clause starts.
    // Those the clause has already read exist wherever it is evaluated; any
    // other defers, and is checked where it is read: before it exists the
    // interpreter reports it, and the answer is NaN. The check, when there is
    // one, is named by a mark that Print resolves once the starts are known.
    template <typename Text>
    std::pair<std::string, std::optional<std::string>> Right(const LogicExpression<Value>& logic,
                                                             Text                          text) {
        Reads             reads;
        Reads* const      outer   = std::exchange(clause_reads_, &reads);
        const bool        was     = std::exchange(deferring_, true);
        const std::string written = text(logic.m_e2());
        deferring_                = was;
        clause_reads_             = outer;
        bool read                 = true;
        for (const auto& [name, lags] : reads) {
            const auto found = outer ? outer->find(name) : Reads::const_iterator();
            read =
                read && outer && found != outer->end() && *found->second.rbegin() >= *lags.rbegin();
        }
        if (read) {
            for (const auto& [name, lags] : reads) {
                if (outer) (*outer)[name].insert(lags.begin(), lags.end());
                if (reading_)
                    (shift_ && early_ >= 0 ? earlies_[static_cast<std::size_t>(early_)].reads
                                           : reading_->reads)[name]
                        .insert(lags.begin(), lags.end());
            }
            return {written, std::nullopt};
        }
        checks_.push_back(
            {reads, logic.m_e1(), logic.Conjunction(), scope_, index_, places_, shift_});
        return {written, "\x02" + std::to_string(checks_.size() - 1) + "\x03"};
    }

    // What the right side reads, emitted without saying where its clause
    // starts.
    Code Quiet(const PExpression<Value>& expression) {
        Reads        reads;
        Reads* const outer = std::exchange(clause_reads_, &reads);
        const bool   was   = std::exchange(deferring_, true);
        const Code   code  = Emit(expression);
        deferring_         = was;
        clause_reads_      = outer;
        return code;
    }

    // Whether some 'and' or 'or' in it defers its right side. What is always
    // read on the way is recorded, as it bounds what the right needs to check.
    bool Defers(const PExpression<Value>& expression) {
        const auto* logic = dynamic_cast<LogicExpression<Value>*>(expression.get());
        if (!logic) {
            Emit(expression);
            return false;
        }
        if (Defers(logic->m_e1())) return true;
        const std::size_t checks = checks_.size();
        bool              nested = false;
        const bool        defers = Right(*logic, [&](const PExpression<Value>& side) {
                                nested = Defers(side);
                                return std::string();
                            }).second.has_value();
        checks_.resize(checks);
        return defers || nested;
    }

    // A truth as a double: 1, 0, or NaN where a term it needed did not exist.
    std::string Truth(const PExpression<Value>& expression) {
        const auto* logic = dynamic_cast<LogicExpression<Value>*>(expression.get());
        if (!logic) return "(" + Test(expression) + " ? 1.0 : 0.0)";
        const std::string left = Shared(Cell(Truth(logic->m_e1()), primary)).text;
        auto [right, check] =
            Right(*logic, [this](const PExpression<Value>& side) { return Truth(side); });
        if (check) right = "(" + *check + right + ")";
        return "(" + left + " == 0.0 ? " + (logic->Conjunction() ? "0.0" : right) + " : " + left +
               " != " + left + " ? NAN : " + (logic->Conjunction() ? right : "1.0") + ")";
    }

    PExpression<Value> visit(LogicExpression<Value>* expression) override {
        const auto truth = [expression](const Code& code) {
            if (!code.Scalar()) throw Reason(std::string(expression->Word()) + " of a matrix");
            return code.constant ? std::optional<bool>(Value::truth(*code.constant)) : std::nullopt;
        };
        // A left side that decides is the answer, and the right is not read.
        const std::optional<bool> left = truth(Emit(expression->m_e1()));
        if (left && *left != expression->Conjunction())
            return Answer(Literal(Value(Number(*left ? 1 : 0))));
        const std::optional<bool> right = truth(Quiet(expression->m_e2()));
        if (left && right) return Answer(Literal(Value(Number(*right ? 1 : 0))));
        const PExpression<Value> self = expression->self();
        if (Defers(self)) return Answer(Cell(Truth(self), primary));
        return Answer(Cell("(" + Test(self) + " ? 1.0 : 0.0)", primary));
    }

    // The built-in, as C's; exactly, where what it is given is a constant.
    PExpression<Value> Floor(FuncExpression<Value>* expression) {
        const ParametersCall<Value>& call = expression->Call();
        if (call.parameters_expression().size() != 1 || !call.parameters_dict().empty())
            throw Reason("floor expects 1 argument");
        Code code = Emit(call.parameters_expression()[0]);
        if (code.constant) return Fold(expression);
        for (Cell& cell : code.cells) cell = Cell("floor(" + cell.text + ")", primary);
        return Answer(code);
    }

    PExpression<Value> visit(MatExpression<Value>* expression) override {
        Code code;
        code.rows     = expression->Size().rows;
        code.cols     = expression->Size().cols;
        bool constant = true;
        for (const PExpression<Value>& child : expression->Children()) {
            const Code cell = Emit(child);
            if (!cell.Scalar()) throw Reason("a matrix built from matrices");
            constant = constant && cell.constant;
            code.cells.push_back(cell.cells[0]);
        }
        return constant ? Fold(expression) : Answer(code);
    }

    PExpression<Value> visit(RefExpression<Value>* expression) override {
        const std::string& name = expression->Name();
        if (!own_) {
            if (const auto place = places_.find(name); place != places_.end())
                return Answer(Literal(place->second));
            if (!index_.empty() && name == index_) return Answer(Atom(index_text_));
            if (const auto expanded = Expanded(name)) return Answer(*expanded);
        }
        const Found             found      = Lookup(name);
        const std::string&      key        = found.key;
        const Reference<Value>* definition = found.definition;
        if (!definition) {
            if (fixed_.count(key)) throw Reason(key + " is not defined");
            return Answer(Field(key, Value(Number(NAN))));
        }
        if (IsSequence(*definition)) throw Reason(key + " is a sequence; index it");
        read_global_ = true;
        if (const auto known = known_.find(key); known != known_.end())
            return Answer(Literal(known->second));
        for (Derived& derived : derived_) {
            if (derived.name != key) continue;
            read_parameters_.insert(derived.parameters.begin(), derived.parameters.end());
            return Answer(Read(derived));
        }
        if (!reading_plain_.insert(key).second) throw Reason(key + " is defined by itself");
        // A global is evaluated in a scope of its own, where no index or place
        // is seen.
        Sequence* const   reading = std::exchange(reading_, nullptr);
        const std::string index   = std::exchange(index_, std::string());
        const auto             places  = std::exchange(places_, Captured(definition));
        std::vector<Temporary> temporaries;
        auto* const            outer_temporaries = std::exchange(temporaries_, &temporaries);
        auto                   outer_parameters  = std::exchange(read_parameters_, {});
        read_global_                             = false;
        const auto cells = [](const Clause<Value>& c) { return c.parameters.cells(); };
        Code       code;
        {
            const Home home(*this, definition);
            code = std::any_of(definition->Clauses().begin(), definition->Clauses().end(), cells)
                       ? Cells(key, *definition)
                       : Emit(definition->Clauses().front().expression);
        }
        reading_                  = reading;
        index_                    = index;
        places_                   = places;
        temporaries_              = outer_temporaries;
        const bool reads          = std::exchange(read_global_, true);
        const auto parameters     = std::exchange(read_parameters_, std::move(outer_parameters));
        read_parameters_.insert(parameters.begin(), parameters.end());
        reading_plain_.erase(key);
        // A value that reads no other is a parameter the host may change; one
        // that reads only what is fixed is a constant; one that reads a
        // parameter derives from it, and is computed where it is set.
        if (code.constant && (!Settable(*found.where, name, reads) || fixed_.count(key))) {
            known_.emplace(key, *code.constant);
            return Answer(Literal(*code.constant));
        }
        if (code.constant) return Answer(Field(key, *code.constant));
        derived_.push_back({key, code, std::move(temporaries), false, parameters});
        return Answer(Read(derived_.back()));
    }

    // A term's cells from the clauses that fit, as Reference::EvaluateTerm
    // chooses them: one for this cell, then those for all cells, guarded ones
    // in the order written and the unguarded one last, then the term written
    // whole's, else 0. Each cell is the chain the step tests in that order,
    // with its row and column bound as constants, so that a guard reading only
    // them folds away and one reading a term is tested where the cell is.
    // With 'taken', each cell's clause too: one more than its place, or 0
    // where none of the clauses for every cell gives it.
    template <typename Fits>
    std::pair<Code, std::vector<std::string>> TermCells(const std::string&      name,
                                                        const Reference<Value>& definition,
                                                        Fits fits, const std::optional<Code>& whole,
                                                        std::vector<std::string>* taken = nullptr) {
        const std::optional<Extent> extent = Measured(name, definition, whole, fits);
        if (!extent) throw Reason(name + " has no size");
        Code shape;
        shape.rows = extent->rows;
        shape.cols = extent->cols;
        std::vector<std::string> cells;
        for (std::size_t row = 1; row <= shape.rows; ++row) {
            for (std::size_t col = 1; col <= shape.cols; ++col) {
                std::string                chain, pick;
                std::optional<std::string> last;
                int                        picked  = 0;
                const auto                 settles = [&](const Clause<Value>& clause) {
                    const int place = static_cast<int>(&clause - definition.Clauses().data()) + 1;
                    const ParametersDefinition<Value>& p = clause.parameters;
                    const std::string                  outer =
                        std::exchange(index_, p.general() ? p.index_name() : "");
                    if (!p.row_name().empty()) {
                        places_[p.row_name()] = Value(Number(static_cast<int>(row)));
                        places_[p.col_name()] = Value(Number(static_cast<int>(col)));
                    }
                    const std::optional<std::string> condition =
                        p.guarded() ? Condition(p.guard()) : std::optional<std::string>("");
                    const std::optional<Code> value =
                        condition ? std::optional<Code>(Emit(clause.expression)) : std::nullopt;
                    places_.erase(p.row_name());
                    places_.erase(p.col_name());
                    index_ = outer;
                    if (!value) return false;
                    if (!value->Scalar())
                        throw Reason("a cell of " + name + " must be a single value");
                    if (condition->empty()) {
                        last   = value->cells[0].text;
                        picked = place;
                    } else {
                        chain += *condition + " ? " + value->cells[0].text + " : ";
                        pick += *condition + " ? " + std::to_string(place) + " : ";
                    }
                    return condition->empty();
                };
                bool settled = false;
                for (const Clause<Value>& clause : definition.Clauses()) {
                    const ParametersDefinition<Value>& p = clause.parameters;
                    if (settled || !p.cells() || !p.row_name().empty() || !fits(clause)) continue;
                    if (p.row() < 1 || static_cast<std::size_t>(p.row()) > shape.rows ||
                        p.col() < 1 || static_cast<std::size_t>(p.col()) > shape.cols)
                        throw Reason("row " + std::to_string(p.row()) + ", column " +
                                     std::to_string(p.col()) + " is outside a " +
                                     std::to_string(shape.rows) + "x" + std::to_string(shape.cols) +
                                     " matrix");
                    if (static_cast<std::size_t>(p.row()) == row &&
                        static_cast<std::size_t>(p.col()) == col)
                        settled = settles(clause);
                }
                for (const bool guarded : {true, false})
                    for (const Clause<Value>& clause : definition.Clauses()) {
                        const ParametersDefinition<Value>& p = clause.parameters;
                        if (settled || p.row_name().empty() || p.guarded() != guarded ||
                            !fits(clause))
                            continue;
                        settled = settles(clause);
                    }
                if (!last) last = whole ? whole->At(row - 1, col - 1).text : "0.0";
                cells.push_back(chain + *last);
                if (taken) taken->push_back(pick + std::to_string(picked));
            }
        }
        return {shape, cells};
    }

    // The base terms of a sequence defined by its cells: those written whole
    // or by their cells, each with its own cells beating them. A cell of every
    // term that would also give one of a base term's is refused, as the
    // interpreter asks which is meant.
    void BaseTerms(const std::string& name, Sequence& sequence) {
        const Reference<Value>& definition = *sequence.definition;
        std::set<int>           indices;
        for (const Clause<Value>& clause : definition.Clauses()) {
            const ParametersDefinition<Value>& p = clause.parameters;
            if (p.indexed() && !p.general() && (p.cells() ? !p.row_name().empty() : !p.guarded()))
                indices.insert(p.index());
        }
        for (const int index : indices) {
            const auto at = [index](const Clause<Value>& c) {
                return c.parameters.indexed() && !c.parameters.general() &&
                       c.parameters.index() == index;
            };
            Sequence* const     outer = std::exchange(basing_, &sequence);
            const int           base  = std::exchange(base_, index);
            std::optional<Code> whole;
            for (const Clause<Value>& clause : definition.Clauses())
                if (at(clause) && !clause.parameters.cells()) whole = Emit(clause.expression);
            const auto [shape, cells] = TermCells(name, definition, at, whole);
            basing_                   = outer;
            base_                     = base;
            for (const Clause<Value>& clause : definition.Clauses()) {
                const ParametersDefinition<Value>& every = clause.parameters;
                if (!every.general() || !every.cells() || !every.row_name().empty()) continue;
                const bool own = std::any_of(
                    definition.Clauses().begin(), definition.Clauses().end(),
                    [&](const Clause<Value>& c) {
                        return at(c) && c.parameters.cells() && c.parameters.row_name().empty() &&
                               c.parameters.row() == every.row() &&
                               c.parameters.col() == every.col();
                    });
                const std::string term = name + "_" + std::to_string(index);
                const std::string cell =
                    "[" + std::to_string(every.row()) + "," + std::to_string(every.col()) + "]";
                if (!own)
                    throw Reason(term + " and " + name + "_" + every.index_name() + cell +
                                 " both give a cell of " + term + "; write " + term + cell +
                                 " to say which");
            }
            Shape(sequence, shape);
            sequence.bases[index] = cells;
        }
    }

    // The general terms of a sequence defined by its cells, every term's
    // cells and the term written whole, and in front of them the cells of one
    // term where no base term stands.
    void CompileTerms(const std::string& name, Sequence& sequence) {
        const Reference<Value>& definition = *sequence.definition;
        Within(name, &sequence, std::string(), [&] {
            const std::string outer_text =
                std::exchange(index_text_, "(double)\x12" + name + "\x13");
            clause_reads_ = &sequence.general_reads;
            std::optional<Code> whole;
            for (const Clause<Value>& clause : definition.Clauses()) {
                const ParametersDefinition<Value>& p = clause.parameters;
                if (!p.general() || p.cells()) continue;
                if (p.guarded()) throw Reason("a guarded term written whole beside its cells");
                const std::string outer = std::exchange(index_, p.index_name());
                whole                   = Emit(clause.expression);
                index_                  = outer;
            }
            const auto every = [](const Clause<Value>& c) { return c.parameters.general(); };
            const bool guarded =
                clauses_ && std::any_of(definition.Clauses().begin(), definition.Clauses().end(),
                                        [](const Clause<Value>& c) {
                                            return c.parameters.general() && c.parameters.cells() &&
                                                   c.parameters.guarded();
                                        });
            auto [shape, cells] =
                TermCells(name, definition, every, whole, guarded ? &sequence.taken : nullptr);
            clause_reads_       = nullptr;
            Shape(sequence, shape);
            // One term's own cells, where no base term gives the rest: read
            // like a base clause, from nothing but constants and parameters.
            Sequence* const reading = std::exchange(reading_, nullptr);
            for (const Clause<Value>& clause : definition.Clauses()) {
                const ParametersDefinition<Value>& p = clause.parameters;
                if (!p.indexed() || p.general() || !p.cells() || !p.row_name().empty() ||
                    sequence.bases.count(p.index()))
                    continue;
                if (p.guarded()) throw Reason("a guarded cell of one term");
                const Code value = Emit(clause.expression);
                if (!value.Scalar()) throw Reason("a cell of " + name + " must be a single value");
                std::string& cell = cells[static_cast<std::size_t>(p.row() - 1) * shape.cols +
                                          static_cast<std::size_t>(p.col() - 1)];
                cell              = "\x12" + name + "\x13 == " + std::to_string(p.index()) + " ? " +
                       value.cells[0].text + " : " + cell;
            }
            reading_         = reading;
            index_text_      = outer_text;
            sequence.general = cells;
        });
    }

    // A matrix defined by its cells, one cell at a time with its names bound to
    // the cell's place. They are constants, so the size, every guard and which
    // clause gives the cell are decided here, as the interpreter would decide
    // them; one that cannot be is refused.
    Code Cells(const std::string& name, const Reference<Value>& definition) {
        std::optional<Code> whole;  // the matrix written whole, if it is
        for (const Clause<Value>& clause : definition.Clauses())
            if (!clause.parameters.cells()) whole = Emit(clause.expression);
        const std::optional<Extent> extent =
            Measured(name, definition, whole, [](const Clause<Value>&) { return true; });
        if (!extent) throw Reason(name + " has no size");
        for (const Clause<Value>& clause : definition.Clauses()) {
            const ParametersDefinition<Value>& p = clause.parameters;
            if (!p.cells() || !p.row_name().empty()) continue;
            if (p.row() < 1 || static_cast<std::size_t>(p.row()) > extent->rows || p.col() < 1 ||
                static_cast<std::size_t>(p.col()) > extent->cols)
                throw Reason("row " + std::to_string(p.row()) + ", column " +
                             std::to_string(p.col()) + " is outside a " +
                             std::to_string(extent->rows) + "x" + std::to_string(extent->cols) +
                             " matrix");
        }
        Code  code;
        Value exact(*extent);
        bool  constant = true;
        code.rows      = extent->rows;
        code.cols      = extent->cols;
        for (std::size_t row = 1; row <= code.rows; ++row) {
            for (std::size_t col = 1; col <= code.cols; ++col) {
                const std::optional<Code> cell =
                    CellOf(definition, static_cast<int>(row), static_cast<int>(col));
                Code given = cell ? *cell : Literal(Value(Number(0)));
                if (!cell && whole) {
                    given       = Code();
                    given.cells = {whole->At(row - 1, col - 1)};
                    if (whole->constant) given.constant = Value((*whole->constant)(row, col));
                }
                if (!given.Scalar())
                    throw Reason("a cell of " + name + " must be a single value, not a " +
                                 std::to_string(given.rows) + "x" + std::to_string(given.cols) +
                                 " matrix");
                code.cells.push_back(given.cells[0]);
                if (given.constant) exact(row, col) = (*given.constant)(1, 1);
                constant = constant && given.constant;
            }
        }
        if (constant) code.constant = exact;
        return code;
    }

    // The size the interpreter measures (Reference::Measured), its bounds and
    // its reads compiled where the definition is: a function's where it is
    // called, whose shapes are known.
    template <typename Fits>
    std::optional<Extent> Measured(const std::string& name, const Reference<Value>& definition,
                                   const std::optional<Code>& whole, Fits fits) {
        return Reasoned([&] {
            return definition.Measured(
                whole ? std::optional<Extent>(Extent{whole->rows, whole->cols}) : std::nullopt,
                fits, name,
                [&](const Clause<Value>& clause, auto measure) {
                    const ParametersDefinition<Value>& p = clause.parameters;
                    const std::string                  outer =
                        std::exchange(index_, p.general() ? p.index_name() : "");
                    measure();
                    index_ = outer;
                },
                [&](const PExpression<Value>& e) { return Size(e); },
                [&](const PExpression<Value>& e) {
                    const Code read = Emit(e);
                    return Extent{read.rows, read.cols};
                });
        });
    }

    std::optional<Code> CellOf(const Reference<Value>& definition, int row, int col) {
        for (const Clause<Value>& clause : definition.Clauses()) {
            const ParametersDefinition<Value>& p = clause.parameters;
            if (!p.cells() || !p.row_name().empty() || p.row() != row || p.col() != col) continue;
            if (p.guarded() && !Holds(p.guard())) continue;
            return Emit(clause.expression);
        }
        for (const bool guarded : {true, false}) {
            for (const Clause<Value>& clause : definition.Clauses()) {
                const ParametersDefinition<Value>& p = clause.parameters;
                if (p.row_name().empty() || p.guarded() != guarded) continue;
                places_[p.row_name()]           = Value(Number(row));
                places_[p.col_name()]           = Value(Number(col));
                const bool                holds = !p.guarded() || Holds(p.guard());
                const std::optional<Code> cell =
                    holds ? std::optional<Code>(Emit(clause.expression)) : std::nullopt;
                places_.erase(p.row_name());
                places_.erase(p.col_name());
                if (cell) return cell;
            }
        }
        return std::nullopt;
    }

    bool Holds(const PExpression<Value>& guard) {
        return Value::truth(*Known(guard, "a guard on cells that is not a constant").constant);
    }

    std::size_t Size(const PExpression<Value>& expression) {
        const int size = Whole(Known(expression, "a matrix whose size is not a constant"));
        if (size < 1) throw Reason("a size must be at least 1, not " + std::to_string(size));
        return static_cast<std::size_t>(size);
    }

    // 'lim s', 'lim nw(x_(n-1), u_n)': folded where it reads only constants,
    // and otherwise a function of its arguments that walks the terms as the
    // interpreter does, stopping by its rule (Convergence), and NaN where the
    // interpreter would say it did not converge, since a step cannot. A
    // limit of matrices fills an array, its steps the largest cell's.
    PExpression<Value> Limit(FuncExpression<Value>* expression, const std::string& name,
                             const ParametersCall<Value>& call) {
        const Found found = Lookup(name);
        if (!found.definition || !IsSequence(*found.definition))
            throw Reason(found.key + " is not a sequence, so it has no limit");
        const Reference<Value>&            sequence = *found.definition;
        const ParametersDefinition<Value>& p        = sequence.Clauses().front().parameters;
        Reasoned([&] { p.CheckArity(name, call); });
        // The arguments, where the limit is read; a default, where it is not
        // given, reads the others.
        Expansion                       given{{}, {}, scope_, expansion_, {}};
        const std::vector<std::string>& names = p.parameters_names();
        for (std::size_t i = 0; i < call.parameters_expression().size(); ++i)
            given.values.emplace(names[i], Emit(call.parameters_expression()[i]));
        for (const auto& [parameter, argument] : call.parameters_dict())
            given.values.emplace(parameter, Emit(argument));
        for (const std::string& parameter : names)
            if (!given.values.count(parameter))
                given.values.emplace(parameter, Inside(given, [&] {
                                         return Emit(p.parameters_dict().at(parameter));
                                     }));
        bool constant = true;
        for (const auto& [parameter, code] : given.values) constant = constant && code.constant;
        // Its terms, as locals of a function of their own. 'arg_x': no name
        // of the language has a '_', and no name of the function's own
        // begins so.
        Walked    walked{name, names, 0, 0, 0};
        Expansion inside{{}, {}, sequence.home ? sequence.home : scope_, nullptr, {}};
        for (const std::string& parameter : names) {
            const Code& argument = given.values.at(parameter);
            inside.values.emplace(parameter,
                                  Array("arg_" + parameter, argument.rows, argument.cols));
        }
        Walked* const              outer_limit       = std::exchange(limit_, &walked);
        auto* const                outer_temporaries = std::exchange(temporaries_, nullptr);
        auto                       outer_reads       = std::exchange(read_parameters_, {});
        std::map<int, Code>        bases;
        std::vector<std::string>   general;
        const auto                 shaped = [&](const Code& code) {
            if (walked.rows && (code.rows != walked.rows || code.cols != walked.cols))
                throw Reason("a limit of " + name + ", whose terms change size");
            walked.rows = code.rows;
            walked.cols = code.cols;
            return code;
        };
        try {
            Inside(inside, [&] {
                for (const Clause<Value>& clause : sequence.Clauses()) {
                    const ParametersDefinition<Value>& c = clause.parameters;
                    if (!c.indexed() || c.general()) continue;
                    if (c.guarded()) throw Reason("a guarded base clause in a limit's terms");
                    bases[c.index()] = shaped(Emit(clause.expression));
                }
                for (const Clause<Value>& clause : sequence.Clauses())
                    if (clause.parameters.general()) index_ = clause.parameters.index_name();
                if (index_.empty())
                    throw Reason(name + " has no general clause, so it has no limit");
                const std::string outer_text = std::exchange(index_text_, "(double)k_");
                std::vector<std::pair<std::string, Code>> guarded;
                std::optional<Code>                       otherwise;
                for (const bool guard : {true, false})
                    for (const Clause<Value>& clause : sequence.Clauses()) {
                        const ParametersDefinition<Value>& c = clause.parameters;
                        if (!c.general() || c.guarded() != guard || otherwise) continue;
                        const std::optional<std::string> condition =
                            guard ? Condition(c.guard()) : std::optional<std::string>("");
                        if (!condition) continue;
                        const Code value = shaped(Emit(clause.expression));
                        if (condition->empty())
                            otherwise = value;
                        else
                            guarded.emplace_back(*condition, value);
                    }
                general     = Texts(Chain(guarded, otherwise, walked.rows, walked.cols));
                index_text_ = outer_text;
                return Code{};
            });
        } catch (...) {
            limit_       = outer_limit;
            temporaries_ = outer_temporaries;
            read_parameters_.insert(outer_reads.begin(), outer_reads.end());
            throw;
        }
        limit_           = outer_limit;
        temporaries_     = outer_temporaries;
        const auto reads = std::exchange(read_parameters_, std::move(outer_reads));
        read_parameters_.insert(reads.begin(), reads.end());
        if (constant && reads.empty()) return Fold(expression);
        const int highest = bases.empty() ? 0 : bases.rbegin()->first;
        for (int lag = 1; lag <= walked.depth; ++lag)
            if (!bases.count(highest - lag + 1))
                throw Reason("a limit whose terms read back past its base clauses");

        const bool        scalar    = walked.rows * walked.cols == 1;
        // Named once its text is known: a limit written again, as a function
        // applied cell by cell writes it, is the same function.
        const std::string function  = "\x1f";
        const std::string first     = std::to_string(highest + 1);
        const std::string shape     = scalar ? "" : Subscript(walked.rows, walked.cols);
        const std::string tolerance = Double(Convergence<Value>::tolerance);
        // A term's cells, one assignment each, or the term itself.
        const auto each = [&](const std::string& indent, const std::string& to, const auto& from) {
            std::string out;
            for (std::size_t c = 0; c < walked.rows * walked.cols; ++c)
                out += indent + to + (scalar ? "" : Subscript(c / walked.cols, c % walked.cols)) +
                       " = " + from(c) + ";\n";
            return out;
        };
        std::string text;
        text += "/* lim " + name + ", as the interpreter takes it: its terms from index " + first +
                " on, until\n * two agree within " + tolerance + (scalar ? "" : " in every cell") +
                " and so does what is left of the series, as the steps\n * shrink; NaN where "
                "that takes more than " +
                std::to_string(Convergence<Value>::max_terms) + " terms. */\n";
        text += "static inline " + std::string(scalar ? "double " : "void ") + function +
                "(const " + module_ + "* m_";
        for (const std::string& parameter : names) {
            const Code& argument = given.values.at(parameter);
            text += argument.Scalar()
                        ? ", double arg_" + parameter
                        : ", double arg_" + parameter + Subscript(argument.rows, argument.cols);
        }
        if (!scalar) text += ", double out_" + shape;
        text += ") {\n    (void)m_;\n";
        for (const std::string& parameter : names) text += "    (void)arg_" + parameter + ";\n";
        const int depth = std::max(walked.depth, 1);
        for (int lag = 1; lag <= depth; ++lag) {
            const auto  base  = bases.find(highest - lag + 1);
            std::string value = "0.0";
            if (base != bases.end()) {
                if (scalar) {
                    value = base->second.cells[0].text;
                } else {
                    value = "{";
                    for (std::size_t i = 0; i < walked.rows; ++i) {
                        value += i ? ", {" : "{";
                        for (std::size_t j = 0; j < walked.cols; ++j)
                            value += (j ? ", " : "") + base->second.At(i, j).text;
                        value += "}";
                    }
                    value += "}";
                }
            } else if (!scalar) {
                value = "{{0.0}}";
            }
            text += "    double t" + std::to_string(lag) + "_" + shape + " = " + value + ";\n";
        }
        if (!scalar) text += "    double t_" + shape + ";\n";
        text += "    double step_ = 0.0, before_ = 0.0;\n";
        text +=
            "    int    started_ = " + std::string(bases.empty() ? "0" : "1") + ", stepped_ = 0;\n";
        text += "    for (long long k_ = " + first + "; k_ <= " +
                std::to_string(highest + static_cast<int>(Convergence<Value>::max_terms)) +
                "; ++k_) {\n";
        if (scalar) {
            text += "        const double t_ = " + general[0] + ";\n";
        } else {
            text += each("        ", "t_", [&](std::size_t c) { return general[c]; });
        }
        text += "        if (started_) {\n";
        if (scalar) {
            text += "            step_ = fabs(t_ - t1_);\n";
        } else {
            text += "            step_ = 0.0;\n";
            text +=
                "            for (int i_ = 0; i_ < " + std::to_string(walked.rows) + "; ++i_)\n";
            text += "                for (int j_ = 0; j_ < " + std::to_string(walked.cols) +
                    "; ++j_) {\n";
            text += "                    const double d_ = fabs(t_[i_][j_] - t1_[i_][j_]);\n";
            text += "                    if (isnan(d_) || d_ > step_) step_ = d_;\n";
            text += "                }\n";
        }
        text += "            if (step_ <= " + tolerance +
                " && stepped_ &&\n                (!(before_ > 0) || (step_ / before_ < 1 &&\n"
                "                                    step_ * (step_ / before_) / (1 - step_ / "
                "before_) <= " +
                tolerance + "))) {\n";
        text += scalar ? "                return t_;\n"
                       : "                memcpy(out_, t_, sizeof t_);\n                return;\n";
        text +=
            "            }\n            before_  = step_;\n            stepped_ = 1;\n        }\n";
        text += "        started_ = 1;\n";
        for (int lag = depth; lag > 1; --lag) {
            const std::string to   = "t" + std::to_string(lag) + "_",
                              from = "t" + std::to_string(lag - 1) + "_";
            text += scalar ? "        " + to + " = " + from + ";\n"
                           : "        memcpy(" + to + ", " + from + ", sizeof " + to + ");\n";
        }
        text += scalar ? "        t1_ = t_;\n    }\n    return NAN;\n}\n\n"
                       : "        memcpy(t1_, t_, sizeof t_);\n    }\n" +
                             each("    ", "out_", [](std::size_t) { return std::string("NAN"); }) +
                             "}\n\n";
        auto [named, fresh] =
            limit_names_.emplace(text, module_ + "_lim" + std::to_string(limits_.size()));
        if (fresh)
            limits_.push_back(text.replace(text.find(function), function.size(), named->second));

        // Where it is read: a matrix argument, or a matrix answer, is an
        // array of the step's.
        if (!scalar && !temporaries_) throw Reason("a limit of matrices inside a limit's terms");
        std::string called = named->second + "(m_";
        for (const std::string& parameter : names) {
            const Code& argument = given.values.at(parameter);
            if (argument.Scalar()) {
                called += ", " + argument.cells[0].text;
                continue;
            }
            if (!temporaries_) throw Reason("a matrix argument of a limit inside a limit's terms");
            std::string rows;
            for (std::size_t i = 0; i < argument.rows; ++i) {
                std::string row;
                for (std::size_t j = 0; j < argument.cols; ++j)
                    row += (j ? ", " : "") + argument.At(i, j).text;
                rows += (i ? ", {" : "{") + row + "}";
            }
            called += ", " + Declare("arg\x1f" + rows, [&](const std::string& t) {
                          return std::vector<std::string>{"double " + t +
                                                          Subscript(argument.rows, argument.cols) +
                                                          " = {" + rows + "};"};
                      });
        }
        if (scalar) return Answer(Cell(called + ")", primary));
        const std::string out = Declare("lim\x1f" + called, [&](const std::string& t) {
            return std::vector<std::string>{"double " + t + shape + ";", called + ", " + t + ");"};
        });
        return Answer(Array(out, walked.rows, walked.cols));
    }

    // A term of the sequence a limit walks, read back from one of its own:
    // a local of its function, 't1_' the one before.
    Code Earlier(const ParametersCall<Value>& call) {
        const auto same = [](const PExpression<Value>& argument, const std::string& parameter) {
            const auto* ref = dynamic_cast<const RefExpression<Value>*>(argument.get());
            return ref && ref->Name() == parameter;
        };
        const auto& arguments = call.parameters_expression();
        bool own = arguments.size() + call.parameters_dict().size() == limit_->parameters.size();
        for (std::size_t i = 0; own && i < arguments.size(); ++i)
            own = same(arguments[i], limit_->parameters[i]);
        for (const auto& [parameter, argument] : call.parameters_dict())
            own = own && same(argument, parameter);
        if (!own) throw Reason(limit_->name + "'s term read with other arguments than its own");
        const int lag = Lag(call.subexpr(), limit_->name);
        if (lag == 0) throw Reason(limit_->name + " is defined by itself");
        if (!limit_->rows) throw Reason("a limit whose terms read back past its base clauses");
        limit_->depth = std::max(limit_->depth, lag);
        return Array("t" + std::to_string(lag) + "_", limit_->rows, limit_->cols);
    }

    PExpression<Value> visit(FuncExpression<Value>* expression) override {
        const std::string&           name  = expression->Name();
        const ParametersCall<Value>& call  = expression->Call();
        const bool calls = !call.parameters_expression().empty() || !call.parameters_dict().empty();
        if (call.limit()) return Limit(expression, name, call);
        if (limit_ && name == limit_->name && call.subexpr()) return Answer(Earlier(call));
        if (calls && !own_) {
            const auto [local, in] = Local(name);
            if (local) {
                if (call.subexpr() || call.limit()) throw Reason("a sequence with parameters");
                return Answer(Call(name, *local, call, in));
            }
        }
        const Found        found = Lookup(name);
        const std::string& key   = found.key;
        if (call.limit()) throw Reason("a limit");
        if (name == "floor" && found.where == &definitions_.Builtins()) return Floor(expression);
        if (calls) {
            if (!found.definition) throw Reason(key + " is not defined");
            if (call.subexpr()) throw Reason("a sequence with parameters");
            return Answer(Call(key, *found.definition, call, nullptr));
        }
        if (limit_) throw Reason(key + "_...: another sequence's term in a limit's terms");
        if (!reading_ && !basing_) throw Reason(key + "_...: a term read outside a general clause");
        const Reference<Value>* definition = found.definition;
        if (definition && !IsSequence(*definition)) throw Reason(key + " is not a sequence");
        Sequence& read = sequences_[key];
        if (!definition) {
            Unreserved(key);
            read.rows = read.cols = 1;
        }
        if (!reading_) return Answer(Seeded(key, read, call.subexpr()));
        if (const auto sample = Sampled(call.subexpr()))
            return Answer(Sample(key, read, sample->first, sample->second));
        if (const auto hold = Held(call.subexpr())) return Answer(Holding(key, read, *hold));
        const int lag = Lag(call.subexpr(), key) + shift_;
        Bases(key);
        if (!read.rows) Compile(key);
        // By its rate, before one with no base clause is computed again.
        if (&read != reading_ && read.period > 1)
            throw Reason(key + "_(...): read every step, and " + key + " is computed every " +
                         std::to_string(read.period));
        if (lag > 0 && ClosedForm(read) && !histories_.count(key)) return Answer(At(read, lag));
        (shift_ && early_ >= 0 ? earlies_[static_cast<std::size_t>(early_)].reads
         : deferring_          ? reading_->deferred
                               : reading_->reads)[key]
            .insert(lag);
        if (clause_reads_) (*clause_reads_)[key].insert(lag);
        Code code = Array("m_->" + key + "[" + std::to_string(lag) + "]", read.rows, read.cols);
        // Before its window holds the term, one with no base clause is
        // computed again at that index, as the interpreter answers it there
        // (DESIGN.md, C71); Checked keeps whichever the reader needs.
        if (lag > 0 && Recomputed(read) && read.compiled && !histories_.count(key)) {
            earlies_.push_back({key, lag, reading_, {}, early_});
            const int  id          = static_cast<int>(earlies_.size() - 1);
            const int  outer       = std::exchange(early_, id);
            const Code back        = At(read, lag);
            early_                 = outer;
            const std::string mark = "\x04" + std::to_string(id) + "\x05";
            for (std::size_t c = 0; c < code.cells.size(); ++c)
                code.cells[c].text = mark + back.At(c / code.cols, c % code.cols).text + "\x06" +
                                     code.cells[c].text + "\x07";
        }
        return Answer(code);
    }

    // A sequence with no base clause that reads no term is a closed form, and
    // the interpreter answers it at every index, before the model starts too:
    // 'c_n = a_(n-1)' with 'a_n = n/8' reads a_(-1) at 0. So a closed form read
    // back in time is compiled again at that index, rather than read from a
    // window that holds nothing from before the start.
    bool ClosedForm(const Sequence& sequence) const {
        return sequence.definition && sequence.compiled && sequence.bases.empty() &&
               sequence.reads.empty() && sequence.deferred.empty();
    }

    // One with no base clause, which the interpreter answers wherever its
    // clauses can be evaluated, below where the step starts it too.
    static bool Recomputed(const Sequence& sequence) {
        return sequence.definition && sequence.bases.empty() && !ByCells(*sequence.definition);
    }

    // A term computed again 'lag' before the reader's index, its clauses
    // tried as the interpreter tries them. What each guard and value reads is
    // read that much further back, and checked where it is read: before it
    // exists the interpreter reports it, and the term is NaN.
    Code At(const Sequence& sequence, int lag) {
        std::vector<std::pair<std::string, Code>> guarded;
        std::optional<Code>                       otherwise;
        const Home                                home(*this, sequence.definition);
        const std::string    index = std::exchange(index_, std::string());
        const std::string    text =
            std::exchange(index_text_, "(double)(m_->index_ - " + std::to_string(lag) + ")");
        const auto   places = std::exchange(places_, {});
        Reads* const reads  = std::exchange(clause_reads_, nullptr);
        const int    shift  = std::exchange(shift_, lag);
        const auto   mark   = [&](Reads& read) {
            if (read.empty()) return std::string();
            checks_.push_back({read, nullptr, false, nullptr, std::string(), {}, 0});
            return "\x02" + std::to_string(checks_.size() - 1) + "\x03";
        };
        for (const bool guard : {true, false}) {
            for (const Clause<Value>& clause : sequence.definition->Clauses()) {
                const ParametersDefinition<Value>& p = clause.parameters;
                if (otherwise || !p.general() || p.guarded() != guard) continue;
                index_ = p.index_name();
                Reads guard_reads, value_reads;
                clause_reads_ = &guard_reads;
                const std::optional<std::string> condition =
                    guard ? Condition(p.guard()) : std::optional<std::string>("");
                if (!condition) continue;
                clause_reads_           = &value_reads;
                const std::string test  = mark(guard_reads) + *condition;
                Code              value = Emit(clause.expression);
                if (const std::string check = mark(value_reads); !check.empty())
                    for (Cell& cell : value.cells) cell = Cell(check + cell.text, 0);
                if (condition->empty())
                    otherwise = value;
                else
                    guarded.emplace_back(test, value);
            }
        }
        index_        = index;
        index_text_   = text;
        places_       = places;
        clause_reads_ = reads;
        shift_        = shift;
        return Chain(guarded, otherwise, sequence.rows, sequence.cols);
    }

    static std::string Subscript(std::size_t i, std::size_t j) {
        return "[" + std::to_string(i) + "][" + std::to_string(j) + "]";
    }

    // The cells of a C array, 'name[i][j]', or 'name' alone for a single value.
    static Code Array(const std::string& name, std::size_t rows, std::size_t cols) {
        Code code;
        code.rows = rows;
        code.cols = cols;
        for (std::size_t i = 0; i < rows; ++i)
            for (std::size_t j = 0; j < cols; ++j)
                code.cells.push_back(Atom(rows * cols == 1 ? name : name + Subscript(i, j)));
        return code;
    }

    // A whole number that reads no parameter, or none.
    std::optional<int> Constant(const PExpression<Value>& e) {
        const auto [code, reads] = Reading(e);
        if (!code.constant || !reads.empty()) return std::nullopt;
        return Whole(code);
    }

    // 'x_(a*m + b)', read in a clause whose index is m: a sample of x every a
    // steps, a > 1, which makes the sequence computed a step at that rate.
    std::optional<std::pair<int, int>> Sampled(const PExpression<Value>& index) {
        if (index_.empty() || places_.count(index_)) return std::nullopt;
        const auto multiple = [&](const PExpression<Value>& e) -> std::optional<int> {
            const auto* product = dynamic_cast<MultExpression<Value>*>(e.get());
            if (!product) return std::nullopt;
            for (const auto& [side, other] : {std::pair(product->m_e1(), product->m_e2()),
                                              std::pair(product->m_e2(), product->m_e1())}) {
                const auto* ref = dynamic_cast<RefExpression<Value>*>(other.get());
                if (!ref || ref->Name() != index_) continue;
                if (const auto a = Constant(side); a && *a > 1) return a;
            }
            return std::nullopt;
        };
        if (const auto a = multiple(index)) return std::pair(*a, 0);
        const auto* sum = dynamic_cast<AddExpression<Value>*>(index.get());
        if (!sum) return std::nullopt;
        for (const auto& [side, other] :
             {std::pair(sum->m_e1(), sum->m_e2()), std::pair(sum->m_e2(), sum->m_e1())}) {
            if (const auto a = multiple(side))
                if (const auto b = Constant(other)) return std::pair(*a, *b);
        }
        return std::nullopt;
    }

    // 'y_(floor((n - b)/a) + d)': the latest term of y, held at the input's rate.
    std::optional<Hold> Held(const PExpression<Value>& index) {
        if (index_.empty() || places_.count(index_)) return std::nullopt;
        Hold                     hold{std::string(), 0, 0, 0};
        const Expression<Value>* at = index.get();
        if (const auto* sum = dynamic_cast<const AddExpression<Value>*>(at)) {
            if (const auto d = Constant(sum->m_e2())) {
                hold.d = *d;
                at     = sum->m_e1().get();
            } else if (const auto e = Constant(sum->m_e1())) {
                hold.d = *e;
                at     = sum->m_e2().get();
            } else {
                return std::nullopt;
            }
        }
        const auto* floor = dynamic_cast<const FuncExpression<Value>*>(at);
        if (!floor || floor->Name() != "floor" || floor->m_e2() ||
            floor->Call().parameters_expression().size() != 1 ||
            Lookup("floor").where != &definitions_.Builtins())
            return std::nullopt;
        const auto* ratio =
            dynamic_cast<DivExpression<Value>*>(floor->Call().parameters_expression()[0].get());
        if (!ratio) return std::nullopt;
        const auto a = Constant(ratio->m_e2());
        if (!a || *a < 2) return std::nullopt;
        hold.a = *a;
        if (const auto* ref = dynamic_cast<RefExpression<Value>*>(ratio->m_e1().get());
            ref && ref->Name() == index_)
            return hold;
        const auto* sum = dynamic_cast<AddExpression<Value>*>(ratio->m_e1().get());
        if (!sum) return std::nullopt;
        for (const auto& [side, other] :
             {std::pair(sum->m_e1(), sum->m_e2()), std::pair(sum->m_e2(), sum->m_e1())}) {
            const auto* ref = dynamic_cast<RefExpression<Value>*>(side.get());
            if (!ref || ref->Name() != index_) continue;
            if (const auto k = Constant(other)) {
                hold.b = -*k;
                return hold;
            }
        }
        return std::nullopt;
    }

    // A term a base clause reads, at a constant index: a lag back from the
    // step that computes the base, set when the step is written.
    Code Seeded(const std::string& key, Sequence& read, const PExpression<Value>& index) {
        const auto at = Constant(index);
        if (!at) throw Reason(key + "_(...): an index other than a constant, in a base clause");
        Sequence* const seeding = basing_;
        const int       base    = base_;
        Bases(key);
        if (!read.rows) Compile(key);
        seeding->seeds.push_back({key, base, *at});
        return Array("m_->" + key + "[\x19" + std::to_string(seeding->seeds.size() - 1) + "\x1a]",
                     read.rows, read.cols);
    }

    // Each base's reads as lags, once the steps computing the bases are known
    // and where each sequence read starts.
    void Seeds(const std::string& name, Sequence& sequence) {
        for (Seed& seed : sequence.seeds) {
            const Sequence& read = sequences_.at(seed.read);
            const int       step = sequence.period * seed.index + sequence.phase;
            seed.lag             = step - seed.at;
            if (seed.lag < 0)
                throw Refusal("cannot compile " + name + ": " + seed.read +
                              "_(...): a term after the one being computed");
            if (read.period != 1)
                throw Refusal("cannot compile " + name + ": " + seed.read +
                              "_(...): read every step, and " + seed.read + " is computed every " +
                              std::to_string(read.period));
            if (seed.at < read.start && !Historied(seed.read))
                throw Refusal("cannot compile " + name + ": " + name + "_" +
                              std::to_string(seed.index) + " reads " + seed.read + "_" +
                              std::to_string(seed.at) + ", before it starts at " +
                              std::to_string(read.start));
            sequence.seeded[seed.read].insert(seed.lag);
        }
    }

    // A sample read: its lag is the phase less b, known once every sample
    // of the clause is, so it is marked and set when the step is written.
    Code Sample(const std::string& key, Sequence& read, int a, int b) {
        const std::string written = key + "_(...)";
        if (shift_ || early_ >= 0 || limit_)
            throw Reason(written + ": a sample where a term is computed again");
        Bases(key);
        if (!read.rows) Compile(key);
        if (reading_->period != 1 && reading_->period != a)
            throw Reason(written + ": read every " + std::to_string(a) + " steps, and " + within_ +
                         " is computed every " + std::to_string(reading_->period));
        reading_->period = a;
        reading_->samples[key].insert(b);
        return Array("m_->" + key + "[\x0e" + std::to_string(b) + "\x0f]", read.rows, read.cols);
    }

    // A hold: where it reads in the held sequence's window depends on that
    // sequence's phase, so it is marked and set when the step is written.
    // In a term computed again, it is the same hold that much earlier.
    Code Holding(const std::string& key, Sequence& read, Hold hold) {
        const std::string written = key + "_(...)";
        if (deferring_ || limit_) throw Reason(written + ": a hold where a term is computed again");
        Bases(key);
        Compile(key);
        if (read.period == 1)
            throw Reason(written + ": an index other than a whole multiple of " + index_ +
                         " plus a constant");
        hold.read = key;
        hold.b += shift_;
        hold.early = early_;
        holds_.push_back(hold);
        reading_->holds.push_back(holds_.size() - 1);
        const std::string id = std::to_string(holds_.size() - 1);
        Code              code;
        code.rows = read.rows;
        code.cols = read.cols;
        for (std::size_t c = 0; c < read.rows * read.cols; ++c)
            code.cells.push_back(Atom("\x10" + id + "," + std::to_string(c) + "\x11"));
        return code;
    }

    // How far back a term reads: its own index, or that less a constant.
    int Lag(const PExpression<Value>& index, const std::string& name) {
        const std::string written = name + "_(...)";
        const int         offset  = Offset(index, written);
        if (offset > 0) throw Reason(written + ": a term after the one being computed");
        return -offset;
    }

    // How far an index is from the clause's own: 'n', or that plus constants
    // however they are spelled -- 'n-1', 'n-k-1' in a sum over k.
    int Offset(const PExpression<Value>& index, const std::string& written) {
        const std::string only =
            written + ": an index other than a whole multiple of " + index_ + " plus a constant";
        if (const auto* ref = dynamic_cast<RefExpression<Value>*>(index.get());
            ref && ref->Name() == index_ && !places_.count(index_))
            return 0;
        const auto* sum = dynamic_cast<AddExpression<Value>*>(index.get());
        if (!sum) throw Reason(only);
        const auto [left, left_reads]   = Reading(sum->m_e1());
        const auto [right, right_reads] = Reading(sum->m_e2());
        if (right.constant && !left.constant) return Offset(sum->m_e1(), written) + Whole(right);
        if (left.constant && !right.constant) return Whole(left) + Offset(sum->m_e2(), written);
        // A lag is the depth of a window, so a parameter that gives one is fixed.
        for (const auto* reads : {&right_reads, &left_reads})
            if (!reads->empty()) throw Fix{*reads};
        throw Reason(only);
    }

    static int Whole(const Code& code) {
        return Reasoned([&] { return AsIndex<Value>(*code.constant); });
    }

    PExpression<Value> visit(EqualExpression<Value>*) override {
        throw Reason("a local definition");
    }
    PExpression<Value> visit(TensorExpression<Value>*) override { throw Reason("a tensor"); }
    PExpression<Value> visit(CellExpression<Value>* expression) override {
        if (expression->Slice()) throw Reason("a tensor");
        const Code matrix = Emit(expression->Matrix());
        const Code row    = Known(expression->Row(), "a cell whose place is not a constant");
        if (!expression->Col()) {
            if (matrix.constant) return Fold(expression);
            const int i = Whole(row);
            if (i < 1 || static_cast<std::size_t>(i) > matrix.rows)
                throw Reason("row " + std::to_string(i) + " is outside a " +
                             std::to_string(matrix.rows) + "x" + std::to_string(matrix.cols) +
                             " matrix");
            Code line;
            line.cols = matrix.cols;
            for (std::size_t j = 0; j < matrix.cols; ++j)
                line.cells.push_back(matrix.At(static_cast<std::size_t>(i - 1), j));
            return Answer(std::move(line));
        }
        const Code col = Known(expression->Col(), "a cell whose place is not a constant");
        if (matrix.constant) return Fold(expression);
        const int i = Whole(row), j = Whole(col);
        if (i < 1 || static_cast<std::size_t>(i) > matrix.rows || j < 1 ||
            static_cast<std::size_t>(j) > matrix.cols)
            throw Reason("row " + std::to_string(i) + ", column " + std::to_string(j) +
                         " is outside a " + std::to_string(matrix.rows) + "x" +
                         std::to_string(matrix.cols) + " matrix");
        return Answer(matrix.At(static_cast<std::size_t>(i - 1), static_cast<std::size_t>(j - 1)));
    }
    PExpression<Value> visit(FactExpression<Value>*) override { throw Reason("a factorial"); }
    PExpression<Value> visit(GradExpression<Value>*) override {
        throw Reason("a derivative, for now");
    }
    // Unrolled, since each term is a line of C: a thousand is a filter no one
    // would write out as one.
    static constexpr int max_terms = 1000;

    // A sum or a product with constant bounds, unrolled with its index bound
    // as a constant, as a cell's names are: the terms that read it fold, and
    // the lags it gives are constants.
    PExpression<Value> visit(SeriesExpression<Value>* expression) override {
        if (!expression->Upper()) throw Reason("a sum or a product with no upper bound");
        const Code lower =
            Known(expression->Lower(), "a sum or a product whose bounds are not constants");
        const Code upper =
            Known(expression->Upper(), "a sum or a product whose bounds are not constants");
        const int first = Whole(lower), last = Whole(upper);
        if (last < first) return Answer(Literal(Value(Number(expression->Product() ? 1 : 0))));
        if (last - first >= max_terms)
            throw Reason("a sum or a product of more than " + std::to_string(max_terms) + " terms");
        const std::string&         name  = expression->Index();
        const auto                 found = places_.find(name);
        const std::optional<Value> outer =
            found == places_.end() ? std::nullopt : std::optional<Value>(found->second);
        Code total;
        bool constant = true;
        for (int k = first; k <= last; ++k) {
            places_[name]   = Value(Number(k));
            const Code term = Emit(expression->Body());
            constant        = constant && term.constant;
            total           = k == first              ? term
                              : expression->Product() ? Product(total, term)
                                                      : Broadcast(total, term, Added);
        }
        if (outer)
            places_[name] = *outer;
        else
            places_.erase(name);
        return constant ? Fold(expression) : Answer(total);
    }
    PExpression<Value> visit_other(Expression<Value>*) override { throw Reason("this expression"); }

    // 'fast.v_n': the name after the point, sought in that scope alone. An
    // unnamed instance has no name for the struct to give it.
    PExpression<Value> visit(MemberExpression<Value>* expression) override {
        if (const Reference<Value>* model = ModelOf(*expression->Object()))
            return Answer(Instance(*model, Call(*expression->Object()), expression->Member()));
        const Scope<Value>* object =
            Reasoned([&] { return &definitions_.Resolve(*expression->Object(), *scope_); });
        if (object->label.find('(') != std::string::npos)
            throw Reason("an unnamed instance, " + object->label);
        own_ = object;
        return expression->Member()->accept(*this);
    }

    Code Field(const std::string& name, const Value& value) {
        Unreserved(name);
        const Parameter parameter{value.Size().rows, value.Size().cols, Doubles(value)};
        parameters_.emplace(name, parameter);
        read_parameters_.insert(name);
        return Fields(name, Literal(value));
    }

    // The struct's field for a value of this shape, cell by cell.
    static Code Fields(const std::string& name, const Code& shape) {
        Unreserved(name);
        return Array("m_->" + name, shape.rows, shape.cols);
    }

    // A cell that is a name or a number is read as itself, which the C
    // compiler can see through, rather than from the field.
    static Code Read(Derived& derived) {
        Code code = Fields(derived.name, derived.code);
        for (std::size_t c = 0; c < code.cells.size(); ++c) {
            if (derived.code.cells[c].atom)
                code.cells[c] = derived.code.cells[c];
            else
                derived.read = true;
        }
        return code;
    }

    // A name here is letters and digits, so it can only collide with C's own;
    // one in an instance is several, 'fast.v'.
    static void Unreserved(const std::string& key) {
        static const std::set<std::string> keywords = {
            "auto",    "break",  "case",     "char",   "const",    "continue", "default",
            "do",      "double", "else",     "enum",   "extern",   "float",    "for",
            "goto",    "if",     "inline",   "int",    "long",     "register", "restrict",
            "return",  "short",  "signed",   "sizeof", "static",   "struct",   "switch",
            "typedef", "union",  "unsigned", "void",   "volatile", "while"};
        for (std::size_t start = 0; start <= key.size();) {
            const std::size_t end  = std::min(key.find('.', start), key.size());
            const std::string name = key.substr(start, end - start);
            if (keywords.count(name)) throw Reason(name + " is a word C keeps for itself");
            start = end + 1;
        }
    }

    // Where each sequence starts: at its lowest base clause, or, without one,
    // at the first index where every term it reads exists -- which is where
    // the interpreter, asked for the term, would first answer. An input starts
    // with the earliest.
    // A sequence at another rate, once every sample of its clauses is known:
    // its phase, the first step at which every sample exists; its base terms
    // keyed by the step that computes them; its samples as lags of the
    // input's; its own terms read back, in its own terms.
    void Rate(const std::string& name, Sequence& sequence) {
        const std::string head = "cannot compile " + name + ": ";
        for (const auto& [read, lags] : sequence.reads) {
            const int period = sequences_.at(read).period;
            if (read != name && period > 1)
                throw Refusal(head + read + "_(...): read every step, and " + read +
                              " is computed every " + std::to_string(period));
        }
        // A slow sequence read by another says what a hold at the input's
        // rate sampled says, but for a hold at another period, which no hold
        // says (C74).
        const auto another = [&](const std::string& read) {
            return Refusal(head + read +
                           "_(...): one sequence at another rate read by another; hold " + read +
                           " at the input's rate and sample the hold");
        };
        for (const std::size_t h : sequence.holds) {
            const Hold& hold   = holds_[h];
            const int   every  = sequence.period * hold.a;
            const int   period = sequences_.at(hold.read).period;
            if (every != period)
                throw Refusal(head + hold.read + "_(...): read every " + std::to_string(every) +
                              " steps, and " + hold.read + " is computed every " +
                              std::to_string(period));
            if (sequence.period > 1) throw another(hold.read);
        }
        if (sequence.period == 1) return;
        for (const auto& [read, offsets] : sequence.samples)
            if (sequences_.at(read).period > 1) throw another(read);
        for (const auto& [read, lags] : sequence.reads)
            if (read != name)
                throw Refusal(head + read + "_(...): read every step, and " + name +
                              " is computed every " + std::to_string(sequence.period));
        // Without a base clause, a term is computed at the step of its
        // latest sample, and the first is settled with the starts.
        int phase = std::numeric_limits<int>::min();
        if (!sequence.bases.empty()) {
            sequence.first = sequence.bases.begin()->first;
            phase          = -sequence.period * sequence.first;
        }
        for (const auto& [read, offsets] : sequence.samples)
            phase = std::max(phase, *offsets.rbegin());
        for (const Seed& seed : sequence.seeds)
            phase = std::max(phase, seed.at - sequence.period * seed.index);
        sequence.phase = phase;
        std::map<int, std::vector<std::string>> bases;
        for (auto& [m, cells] : sequence.bases)
            bases[sequence.period * m + phase] = std::move(cells);
        sequence.bases = std::move(bases);
        if (const auto own = sequence.reads.find(name); own != sequence.reads.end()) {
            sequence.own = own->second;
            sequence.reads.erase(own);
        }
        for (const auto& [read, offsets] : sequence.samples)
            for (const int b : offsets) sequence.reads[read].insert(phase - b);
    }

    // Where a hold reads in its sequence's window, from the first step at
    // which the term it names exists: one lag, or one of two a step apart.
    void Resolve(const std::string& name, Hold& hold) {
        const Sequence& held  = sequences_.at(hold.read);
        const int       a     = hold.a;
        const int       start = held.period * (held.first - held.filled) + held.phase;
        for (int n = hold.from; n < std::max(hold.from, start) + 2 * a; ++n) {
            const int lag =
                n < start ? -1 : Floor(n - held.phase, a) - Floor(n - hold.b, a) - hold.d;
            if (lag < 0)
                throw Refusal("cannot compile " + name + ": " + hold.read +
                              "_(...): read before it is computed; read the term before it");
            hold.least = n == hold.from ? lag : std::min(hold.least, lag);
            hold.most  = n == hold.from ? lag : std::max(hold.most, lag);
        }
    }

    static bool Ticks(const Sequence& sequence, int n) {
        return sequence.period == 1 || (n - sequence.phase) % sequence.period == 0;
    }

    // ' - k' or ' + k', for an index less a constant.
    static std::string Less(int k) {
        return k < 0 ? " + " + std::to_string(-k) : k > 0 ? " - " + std::to_string(k) : "";
    }

    // A sequence's text with what its rate decides set: the index, the lag
    // of each sample, and where each hold reads.
    std::string Rated(std::string text, const Sequence& sequence) const {
        const auto replace = [&](char open, char close, const auto& with) {
            for (std::size_t at = text.find(open); at != std::string::npos;
                 at             = text.find(open, at)) {
                const std::size_t end = text.find(close, at);
                const std::string set = with(text.substr(at + 1, end - at - 1));
                text.replace(at, end - at + 1, set);
                at += set.size();
            }
        };
        replace('\x12', '\x13', [&](const std::string& name) {
            const Sequence& own = sequences_.at(name);
            return own.period == 1 ? std::string("m_->index_")
                                   : "((m_->index_" + Less(own.phase) + ") / " +
                                         std::to_string(own.period) + ")";
        });
        replace('\x19', '\x1a', [&](const std::string& seed) {
            return std::to_string(sequence.seeds.at(std::stoul(seed)).lag);
        });
        replace('\x0e', '\x0f', [&](const std::string& b) {
            return std::to_string(sequence.phase - std::stoi(b));
        });
        replace('\x10', '\x11', [&](const std::string& mark) {
            const std::size_t comma = mark.find(',');
            const Hold&       hold  = holds_.at(std::stoul(mark.substr(0, comma)));
            const Sequence&   held  = sequences_.at(hold.read);
            const std::size_t c     = std::stoul(mark.substr(comma + 1));
            const std::string a     = std::to_string(hold.a);
            // C divides toward 0: each numerator is kept whole from the
            // hold's first step by whole periods, taken back from d.
            const auto whole = [&](int b) { return std::max(0, -Floor(hold.from - b, hold.a)); };
            const int  p = whole(held.phase), q = whole(hold.b);
            const std::string lag = hold.least == hold.most
                                        ? std::to_string(hold.least)
                                        : "(m_->index_" + Less(held.phase - p * hold.a) + ") / " +
                                              a + " - (m_->index_" + Less(hold.b - q * hold.a) +
                                              ") / " + a + Less(hold.d + p - q);
            std::string       cell =
                "m_->" + hold.read + "[" + lag + "]" +
                (held.rows * held.cols == 1 ? std::string()
                                            : Subscript(c / held.cols, c % held.cols));
            if (hold.from > sequence.start)
                cell = "(m_->index_ < " + std::to_string(hold.from) + " ? NAN : " + cell + ")";
            return cell;
        });
        return text;
    }

    int Starts() {
        std::optional<int> earliest;
        for (auto& [name, sequence] : sequences_) {
            if (sequence.bases.empty()) continue;
            sequence.start = sequence.bases.begin()->first;
            earliest       = std::min(earliest.value_or(sequence.start), sequence.start);
        }
        for (auto& [name, sequence] : sequences_) {
            if (!sequence.bases.empty()) continue;
            sequence.start = Tick(sequence, earliest.value_or(0));
            sequence.first = (sequence.start - sequence.phase) / sequence.period;
        }
        // The interpreter's guarded clauses answer below the lowest base
        // clause too, where its unguarded one does not: '_(-1)' of a sequence
        // based at 0 is a term wherever a guard holds. A step has no such
        // terms, so a read that could reach one is refused.
        for (const auto& [name, sequence] : sequences_) {
            int first = sequence.start;
            while (sequence.bases.count(first)) first += sequence.period;  // the general clauses
            for (const auto& [read, lags] : sequence.reads) {
                const Sequence& other = sequences_.at(read);
                if (other.bases.empty() || other.guarded.empty()) continue;
                const int reached = first - *lags.rbegin();
                if (reached < other.start)
                    throw Refusal("cannot compile " + name + ": " + name + "_" +
                                  std::to_string(first) + " reads " + read + "_" +
                                  std::to_string(reached) + ", below " + read +
                                  "'s base clauses, where only its guards could give a term");
            }
        }
        // One with no base clause starts where some path through its clauses
        // answers, and the interpreter answers it below the step's first index
        // too, wherever one does: a reader finds it there (C71).
        for (const auto& [name, sequence] : sequences_)
            if (Recomputed(sequence) || Historied(name))
                exists_[name] = std::numeric_limits<int>::min() / 2;
        // A slow one, at the first tick whose samples exist.
        for (std::size_t round = 0;; ++round) {
            bool moved = false;
            for (Hold& hold : holds_) {
                Sequence& held = sequences_.at(hold.read);
                // Before its first tick, the terms init folds from a history.
                int lowest = held.first;
                for (const Hold& other : holds_)
                    if (other.read == hold.read)
                        lowest = std::min(lowest,
                                          Floor(earliest.value_or(0) - other.b, other.a) + other.d);
                for (held.filled = 0; held.first - held.filled > lowest &&
                                      Folded(hold.read, held.first - held.filled - 1);)
                    ++held.filled;
                hold.from = hold.a * (held.first - held.filled - hold.d) + hold.b;
            }
            for (auto& [name, sequence] : sequences_) {
                if (!sequence.bases.empty() || !sequence.definition) continue;
                const int answers = Answers(sequence);
                // Before the stream, a history gives its terms, or Cover refuses.
                const int exists = Historied(name) && answers <= earliest.value_or(0)
                                       ? std::numeric_limits<int>::min() / 2
                                       : answers;
                if (const auto found = exists_.find(name);
                    found != exists_.end() && exists > found->second) {
                    found->second = exists;
                    moved         = true;
                }
                const int samples = sequence.period > 1 ? From(sequence.reads, true) : answers;
                const int needed  = Tick(sequence, std::max({sequence.start, answers, samples}));
                if (needed <= sequence.start) continue;
                sequence.start = needed;
                sequence.first = (needed - sequence.phase) / sequence.period;
                moved          = true;
            }
            if (!moved) break;
            if (round > sequences_.size())
                throw Refusal(
                    "cannot compile: a sequence with no base clause reads back into "
                    "itself, so it never starts");
        }
        Unreached(earliest.value_or(0));
        // Where a clause's reads begin is where it can be evaluated: before it,
        // the interpreter reports the term it could not read, and a step says
        // NaN. Only a sequence with guards has more than one path to choose.
        for (auto& [name, sequence] : sequences_) {
            for (Guarded& guarded : sequence.guarded) {
                guarded.guard_from = From(guarded.guard);
                guarded.value_from = From(guarded.value);
            }
        }
        // Without guards, the one clause is always evaluated, so a term it
        // reads before that term exists is an error in the model, said now.
        for (const auto& [name, sequence] : sequences_) {
            if (!sequence.guarded.empty()) continue;
            for (const auto& [read, lags] : sequence.reads) {
                const int begins = Exists(read);
                for (const int lag : lags) {
                    for (int n = sequence.start; n < begins + lag; ++n) {
                        if (sequence.bases.count(n) || !Ticks(sequence, n)) continue;
                        throw Refusal("cannot compile " + name + ": " + name + "_" +
                                      std::to_string(n) + " reads " + read + "_" +
                                      std::to_string(n - lag) + ", before it starts at " +
                                      std::to_string(begins));
                    }
                }
            }
        }
        return earliest.value_or(0);
    }

    // A term the step never computes. A hold of one is NaN where the
    // interpreter has none either, and refused where it could give one; a
    // sample of one with no base clause, read from a window that cannot
    // hold it, is refused where its clauses could give it.
    void Unreached(int earliest) const {
        for (const auto& [name, sequence] : sequences_) {
            const std::string head = "cannot compile " + name + ": " + name + "_";
            // Holds are read at the input's rate, where one with no base
            // clause could start with the step.
            int first = sequence.bases.empty() ? earliest : sequence.start;
            while (sequence.bases.count(first)) ++first;
            for (const std::size_t h : sequence.holds) {
                const Hold&     hold = holds_[h];
                const Sequence& held = sequences_.at(hold.read);
                if (hold.early >= 0 && !Needed(earlies_[static_cast<std::size_t>(hold.early)]))
                    continue;
                for (int n = first;; ++n) {
                    const int term = Floor(n - hold.b, hold.a) + hold.d;
                    if (term >= held.first - held.filled) break;
                    const std::string at = std::to_string(n) + " reads " + hold.read + "_" +
                                           std::to_string(term) + ", ";
                    if (!held.bases.empty() && !held.guarded.empty())
                        throw Refusal(head + at + "below " + hold.read +
                                      "'s base clauses, where only its guards could give a term");
                    if (held.bases.empty() &&
                        std::any_of(held.samples.begin(), held.samples.end(), [&](const auto& x) {
                            return held.period * term + *x.second.rbegin() >= Exists(x.first);
                        }))
                        throw Refusal(head + at + "before " + hold.read +
                                      "'s first tick, where its samples could give a term");
                }
            }
            int tick = sequence.start;
            while (sequence.bases.count(tick)) tick += sequence.period;
            for (const auto& [read, offsets] : sequence.samples) {
                const Sequence& sampled = sequences_.at(read);
                if (!Recomputed(sampled)) continue;
                for (const int b : offsets)
                    for (int k = tick - sequence.phase + b; k < sampled.start; k += sequence.period)
                        if (k >= Exists(read))
                            throw Refusal(head + std::to_string((k - b) / sequence.period) +
                                          " reads " + read + "_" + std::to_string(k) +
                                          ", before the step computes " + read +
                                          ", where its clauses could give a term");
            }
        }
    }

    // The whole part of n/a, a > 0, as floor gives it.
    static int Floor(int n, int a) { return n / a - (n % a < 0 ? 1 : 0); }

    // The first tick of a sequence from step n on.
    static int Tick(const Sequence& sequence, int n) {
        while (!Ticks(sequence, n)) ++n;
        return n;
    }

    // The first index where every term these reads name exists.
    // A sample's term exists from the stream's start, where a history is not
    // asked for one.
    int From(const Reads& reads, bool sampled = false) const {
        int from = std::numeric_limits<int>::min();
        for (const auto& [read, lags] : reads)
            from = std::max(
                from, (sampled && Historied(read) ? sequences_.at(read).start : Exists(read)) +
                          *lags.rbegin());
        return from;
    }

    // Where a sequence's terms begin: its start, or, for one computed again,
    // wherever its clauses answer.
    int Exists(const std::string& name) const {
        const auto found = exists_.find(name);
        return found == exists_.end() ? sequences_.at(name).start : found->second;
    }

    // The first index where some path through a sequence's clauses answers:
    // the guards tried before a clause must be evaluable, and the clause.
    int Answers(const Sequence& sequence) const {
        int held = std::numeric_limits<int>::min();
        for (const std::size_t h : sequence.holds) held = std::max(held, holds_[h].from);
        if (sequence.guarded.empty()) return std::max(held, From(sequence.reads));
        int guards = std::numeric_limits<int>::min();
        int first  = std::numeric_limits<int>::max();
        for (const Guarded& guarded : sequence.guarded) {
            guards = std::max(guards, From(guarded.guard));
            first  = std::min(first, std::max(guards, From(guarded.value)));
        }
        if (sequence.general.front() != "NAN")
            first = std::min(first, std::max(guards, From(sequence.general_reads)));
        return std::max(held, first);
    }

    // An input of the model compiled, or one with a history: what is read of
    // it before the stream is what init folds from that history, or refused.
    bool Historied(const std::string& name) const {
        return histories_.count(name) || (root_.model && !sequences_.at(name).definition);
    }

    // A term before the stream as the interpreter gives it, where only a
    // history does.
    bool Folded(const std::string& name, int n) {
        const auto [slot, fresh]        = folded_.try_emplace({name, n});
        const auto              history = histories_.find(name);
        const Reference<Value>* definition =
            history != histories_.end() ? history->second : sequences_.at(name).definition;
        if (fresh && definition && !Parametric(name)) {
            definitions_.Forget();
            definitions_.BeginEvaluation();
            try {
                const ParametersCall<Value> at(
                    PExpression<Value>(), std::make_shared<ValExpression<Value>>(Value(Number(n))));
                const Value term = definition->Eval(at, definitions_, true);
                if (definitions_.histories) {
                    const std::vector<double> cells = Doubles(term);
                    if (std::all_of(cells.begin(), cells.end(),
                                    [](double cell) { return std::isfinite(cell); }))
                        slot->second = term;
                    else
                        unfolded_[{name, n}] = "a term no double holds";
                }
            } catch (const std::runtime_error& error) {
                if (definitions_.histories)
                    unfolded_[{name, n}] = std::string("none: ") + error.what();
            }
        }
        return slot->second.has_value();
    }

    // Why a read before the stream is refused: what its history gave there,
    // or that it has none.
    std::string Unfolded(const std::string& read, int at) const {
        const auto why = unfolded_.find({read, at});
        return why != unfolded_.end() ? read + "'s history gives " + why->second
                                      : read + " has no history";
    }

    // Whether a term can read a parameter, which the host may assign after
    // init has folded it.
    bool Parametric(const std::string& name) const {
        std::set<std::string>    seen{name};
        std::vector<std::string> left{name};
        while (!left.empty()) {
            const Sequence& sequence = sequences_.at(left.back());
            left.pop_back();
            if (!sequence.parameters.empty()) return true;
            std::vector<std::string> reads;
            for (const Reads* each :
                 {&sequence.reads, &sequence.deferred, &sequence.samples, &sequence.seeded})
                for (const auto& [read, lags] : *each) reads.push_back(read);
            for (const std::size_t h : sequence.holds) reads.push_back(holds_[h].read);
            for (const std::string& read : reads)
                if (seen.insert(read).second) left.push_back(read);
        }
        return false;
    }

    // Whether the left of an 'and' or 'or' decides at step n, folded there.
    bool Decides(const Check& check, int n) {
        if (!check.left) return false;
        auto places         = check.places;
        places[check.index] = Value(Number(n - check.shift));
        const auto scope    = std::exchange(scope_, check.scope);
        const auto index    = std::exchange(index_, std::string());
        const auto outer    = std::exchange(places_, places);
        bool       decides  = false;
        try {
            const Code code = Quiet(check.left);
            decides         = code.constant && Value::truth(*code.constant) != check.conjunction;
        } catch (const Reason&) {
        }
        scope_  = scope;
        index_  = index;
        places_ = outer;
        return decides;
    }

    // Each read before the stream, from a reader's start until 'until', that
    // no history gives and nothing keeps.
    void Cover(const std::string& name, const Sequence& reader, const Reads& reads, int until,
               const Check* kept = nullptr) {
        for (const auto& [read, lags] : reads) {
            if (!Historied(read)) continue;
            for (const int lag : lags) {
                for (int n = reader.start; n < std::min(until, earliest_ + lag); ++n) {
                    if (!Ticks(reader, n) || reader.bases.count(n) || Folded(read, n - lag) ||
                        (kept && Decides(*kept, n)))
                        continue;
                    throw Refusal("cannot compile " + name + ": " + name + "_" +
                                  std::to_string(Floor(n - reader.phase, reader.period)) +
                                  " reads " + read + "_" + std::to_string(n - lag) +
                                  ", before the stream, where " + Unfolded(read, n - lag));
                }
            }
        }
    }

    // The reads before the stream; the step's deferred ones are checked as it
    // is written.
    void Histories(int earliest) {
        for (const auto& [name, last] : reach_)
            if (last >= earliest)
                throw Refusal("cannot compile " + name + ": its history reaches " + name + "_" +
                              std::to_string(earliest) + ", in the stream");
        for (const auto& [name, sequence] : sequences_) {
            if (!sequence.definition) continue;
            // A guarded clause's value is read only where its guard holds.
            Reads always = sequence.reads;
            for (const Guarded& guarded : sequence.guarded) {
                const auto& p = sequence.definition->Clauses()[guarded.clause].parameters;
                const Check kept{guarded.value,  p.guard(), true, sequence.definition->home,
                                 p.index_name(), {},        0};
                Cover(name, sequence, guarded.value, std::numeric_limits<int>::max(), &kept);
                for (const auto& [read, lags] : guarded.value)
                    for (const int lag : lags) always[read].erase(lag);
            }
            for (const Guarded& guarded : sequence.guarded)
                for (const Reads* reads : {&guarded.guard, &sequence.general_reads})
                    for (const auto& [read, lags] : *reads)
                        always[read].insert(lags.begin(), lags.end());
            Cover(name, sequence, always, std::numeric_limits<int>::max());
            // A base clause's reads, which Cover passes over.
            for (const Seed& seed : sequence.seeds)
                if (seed.at < earliest_ && Historied(seed.read) && !Folded(seed.read, seed.at))
                    throw Refusal("cannot compile " + name + ": " + name + "_" +
                                  std::to_string(seed.index) + " reads " + seed.read + "_" +
                                  std::to_string(seed.at) + ", before the stream, where " +
                                  Unfolded(seed.read, seed.at));
        }
        for (const Early& early : earlies_)
            for (const auto& [name, sequence] : sequences_)
                if (&sequence == early.reader && Needed(early))
                    Cover(name, sequence, early.reads, sequences_.at(early.name).start + early.lag);
    }

    // init's lines for what histories give, once every read has asked.
    std::string Folds(int earliest) const {
        std::string lines;
        for (const auto& [name, sequence] : sequences_) {
            const bool slow  = sequence.period > 1;
            const int  terms = slow              ? std::min(sequence.filled, sequence.depth)
                               : Historied(name) ? sequence.depth - 1
                                                 : 0;
            for (int k = 0; k < terms; ++k) {
                const auto term = folded_.find({name, (slow ? sequence.first : earliest) - 1 - k});
                if (term == folded_.end() || !term->second) continue;
                const std::vector<double> cells = Doubles(*term->second);
                if (cells.size() != sequence.rows * sequence.cols)
                    throw Refusal("cannot compile " + name + ": a history of another shape");
                for (std::size_t c = 0; c < cells.size(); ++c)
                    if (cells[c] != 0.0)
                        lines +=
                            "    m_->" + name + "[" + std::to_string(k) + "]" +
                            (cells.size() == 1 ? ""
                                               : Subscript(c / sequence.cols, c % sequence.cols)) +
                            " = " + Double(cells[c]) + ";\n";
            }
        }
        return lines;
    }

    std::vector<std::string> Order() const {
        std::map<std::string, std::set<std::string>> waiting;
        for (const auto& [name, sequence] : sequences_) {
            if (!sequence.definition) continue;
            waiting[name];
            // A hold's lag is counted from the window as the held term's tick
            // leaves it, so the held sequence comes first.
            for (const std::size_t h : sequence.holds) waiting[name].insert(holds_[h].read);
            for (const Reads* reads : {&sequence.reads, &sequence.deferred, &sequence.seeded}) {
                for (const auto& [read, lags] : *reads) {
                    if (!lags.count(0) || !sequences_.at(read).definition) continue;
                    if (read == name)
                        throw Refusal("cannot compile " + name + ": a term reads itself");
                    waiting[name].insert(read);
                }
            }
        }
        std::vector<std::string> order;
        while (!waiting.empty()) {
            const auto ready = std::find_if(waiting.begin(), waiting.end(),
                                            [](const auto& entry) { return entry.second.empty(); });
            if (ready == waiting.end())
                throw Refusal("cannot compile " + waiting.begin()->first +
                              ": it and the terms it reads need each other");
            const std::string name = ready->first;
            order.push_back(name);
            waiting.erase(ready);
            for (auto& entry : waiting) entry.second.erase(name);
        }
        return order;
    }

    static std::string Dimensions(std::size_t rows, std::size_t cols) {
        return rows * cols == 1 ? "" : Subscript(rows, cols);
    }

    // Those something reads: a cell read out of a product, or a clause that was
    // compiled only to be refused, leaves some that nothing does, and C would
    // say so.
    static std::string Temporaries(const std::vector<Temporary>& declared,
                                   const std::string& assignments, const std::string& indent) {
        std::string read = assignments, kept;
        for (auto temporary = declared.rbegin(); temporary != declared.rend(); ++temporary) {
            if (read.find(temporary->name) == std::string::npos) continue;
            std::string lines;
            for (const std::string& line : temporary->lines) lines += indent + line + "\n";
            read += lines;
            kept = lines + kept;
        }
        return kept;
    }

    // The interpreter's Gauss-Jordan (Matrix::Inverse), step for step: the
    // largest pivot, any rather than an exact zero, and NaN for every cell
    // where the matrix is singular, which the interpreter reports.
    std::string InverseHelper(std::size_t n) const {
        const std::string size = std::to_string(n);
        return "/* The inverse of a " + size + "x" + size +
               " matrix, in place, as the interpreter takes it: Gauss-Jordan\n"
               " * on the largest pivot, and NaN in every cell where there is none. */\n"
               "static inline void " +
               module_ + "_inverse" + size + "_(double a[" + size + "][" + size +
               "]) {\n"
               "    double r[" +
               size + "][" + size +
               "];\n"
               "    for (int i = 0; i < " +
               size + "; ++i)\n        for (int j = 0; j < " + size +
               "; ++j) r[i][j] = i == j;\n"
               "    for (int col = 0; col < " +
               size +
               "; ++col) {\n"
               "        int pivot = col;\n"
               "        for (int row = col + 1; row < " +
               size +
               "; ++row)\n"
               "            if (fabs(a[row][col]) > fabs(a[pivot][col]) ||\n"
               "                (a[pivot][col] == 0.0 && a[row][col] != 0.0))\n"
               "                pivot = row;\n"
               "        if (a[pivot][col] == 0.0) {\n"
               "            for (int i = 0; i < " +
               size + "; ++i)\n                for (int j = 0; j < " + size +
               "; ++j) a[i][j] = NAN;\n"
               "            return;\n"
               "        }\n"
               "        for (int j = 0; j < " +
               size +
               "; ++j) {\n"
               "            const double s = a[pivot][j], t = r[pivot][j];\n"
               "            a[pivot][j] = a[col][j];\n"
               "            r[pivot][j] = r[col][j];\n"
               "            a[col][j]   = s;\n"
               "            r[col][j]   = t;\n"
               "        }\n"
               "        const double scale = a[col][col];\n"
               "        for (int j = 0; j < " +
               size +
               "; ++j) {\n"
               "            a[col][j] = a[col][j] / scale;\n"
               "            r[col][j] = r[col][j] / scale;\n"
               "        }\n"
               "        for (int row = 0; row < " +
               size +
               "; ++row) {\n"
               "            const double factor = a[row][col];\n"
               "            if (row == col || factor == 0.0) continue;\n"
               "            for (int j = 0; j < " +
               size +
               "; ++j) {\n"
               "                a[row][j] = a[row][j] - factor * a[col][j];\n"
               "                r[row][j] = r[row][j] - factor * r[col][j];\n"
               "            }\n"
               "        }\n"
               "    }\n"
               "    memcpy(a, r, sizeof r);\n"
               "}\n\n";
    }

    // Each deferred right side's mark (see Right), as the check it needs
    // where the terms it reads may not exist yet, or nothing where they do.
    // Whether a term read back is computed again: where its reader can be
    // evaluated before the window holds it, and, inside another term computed
    // again, where that one is.
    bool Needed(const Early& early) const {
        const int from   = sequences_.at(early.name).start + early.lag;
        bool      before = false;
        for (int n = early.reader->start; n < from && !before; ++n)
            before = !early.reader->bases.count(n);
        return before &&
               (early.outer < 0 || Needed(earlies_[static_cast<std::size_t>(early.outer)]));
    }

    std::string Checked(std::string text, const std::string& name, const Sequence& sequence) {
        // A term read back, computed again where the reader can need it before
        // its window holds it; the innermost first, as the last marked.
        for (std::size_t at = text.rfind('\x04'); at != std::string::npos;
             at             = text.rfind('\x04')) {
            const std::size_t mid    = text.find('\x05', at);
            const std::size_t sep    = text.find('\x06', mid);
            const std::size_t end    = text.find('\x07', sep);
            const Early&      early  = earlies_.at(std::stoul(text.substr(at + 1, mid - at - 1)));
            const int         from   = sequences_.at(early.name).start + early.lag;
            const std::string window = text.substr(sep + 1, end - sep - 1);
            text.replace(at, end - at + 1,
                         Needed(early)
                             ? "(m_->index_ < " + std::to_string(from) + " ? " +
                                   text.substr(mid + 1, sep - mid - 1) + " : " + window + ")"
                             : window);
        }
        for (std::size_t at = text.find('\x02'); at != std::string::npos;
             at             = text.find('\x02', at)) {
            const std::size_t end = text.find('\x03', at);
            const Check&      kept = checks_.at(std::stoul(text.substr(at + 1, end - at - 1)));
            Cover(name, sequence, kept.reads, std::numeric_limits<int>::max(), &kept);
            const int   from = From(kept.reads);
            std::string check;
            for (int n = sequence.start; n < from && check.empty(); ++n)
                if (!sequence.bases.count(n))
                    check = "m_->index_ < " + std::to_string(from) + " ? NAN : ";
            text.replace(at, end - at + 1, check);
            at += check.size();
        }
        return text;
    }

    // The header's first comment: how to call it, in the terms of the file.
    std::string Interface(const std::string& module, const std::vector<std::string>& inputs,
                          const std::vector<std::string>& fields, int earliest) const {
        const auto list = [](const std::vector<std::string>& items) {
            std::string joined;
            for (std::size_t i = 0; i < items.size(); ++i)
                joined += (i == 0 ? "" : i + 1 == items.size() ? " and " : ", ") + items[i];
            return joined;
        };
        std::string arguments;
        for (const std::string& name : inputs) arguments += ", " + name;
        std::vector<std::string> terms, inputs_n, parameters, rated;
        for (const std::string& name : fields) {
            const Sequence&   sequence = sequences_.at(name);
            const int         depth    = sequence.depth;
            const std::string term =
                depth > 1 ? name + "\x01(k\x01<=\x01" + std::to_string(depth - 1) + ")" : name;
            if (sequence.period == 1) {
                terms.push_back(term);
                continue;
            }
            rated.push_back(term + ", computed at the steps " + std::to_string(sequence.period) +
                            "*m" + (sequence.phase ? Less(-sequence.phase) : std::string()));
        }
        for (const std::string& name : inputs) inputs_n.push_back(name + "_n");
        for (const auto& [name, parameter] : parameters_)
            parameters.push_back(parameter.initial.size() == 1
                                     ? name + "\x01=\x01" + Double(parameter.initial[0])
                                     : name + "\x01(" + std::to_string(parameter.rows) + "x" +
                                           std::to_string(parameter.cols) + ")");
        std::string text =
            "After a step, m.name[k] is name_(n-k) for each sequence: " + list(terms) + ".";
        if (!rated.empty())
            text += " At another rate, m.name[k] is name_(m-k), m its latest term's index: " +
                    list(rated) + ".";
        text = (inputs.empty() ? "A step takes no input. "
                : inputs.size() == 1
                    ? "A step takes " + list(inputs_n) + ", the input at its index. "
                    : "A step takes " + list(inputs_n) + ", the inputs at its index. ") +
               text;
        if (!parameters.empty())
            text += std::string(" The parameters are fields holding the ") +
                    (model_ ? "model's defaults" : "file's values") + " once " + module +
                    "_init has run: " + list(parameters) + ". After assigning one, call " + module +
                    "_update.";
        std::vector<std::string> fixed(fixed_.begin(), fixed_.end());
        if (!fixed.empty())
            text += " Compiled in, as a size, a bound or a lag cannot change: " + list(fixed) + ".";
        std::string out = "/* Using it:\n *\n";
        out += " *     " + module + " m;\n";
        out += " *     " + module + "_init(&m);\n";
        out += " *     " + module + "_step(&m" + arguments + ");  once for each index, the first " +
               std::to_string(earliest) + "\n";
        if (!fields.empty()) {
            const std::string& last  = fields.back();
            const Sequence&    shown = sequences_.at(last);
            out += " *     m." + last + (shown.rows * shown.cols == 1 ? "[0]" : "[0][i][j]") +
                   "  is then " + last + "_n" +
                   (shown.rows * shown.cols == 1 ? "" : ", row i+1 and column j+1") + "\n";
        }
        out += " *\n";
        std::string line;
        for (std::size_t at = 0; at < text.size();) {
            const std::size_t space = text.find(' ', at);
            const std::string word  = text.substr(at, space - at);
            at                      = space == std::string::npos ? text.size() : space + 1;
            if (!line.empty() && line.size() + 1 + word.size() > 76) {
                out += " * " + line + "\n";
                line.clear();
            }
            line += (line.empty() ? "" : " ") + word;
        }
        // '\x01' is a space the wrapping does not break at.
        std::replace(out.begin(), out.end(), '\x01', ' ');
        std::replace(line.begin(), line.end(), '\x01', ' ');
        return out + " * " + line + "\n */\n\n";
    }

    struct Member {
        std::string type, key, dimensions;
    };
    typedef std::vector<Member> Members;

    // The session's members in the order given, then one struct for each
    // instance, 'fast', holding its own in that order.
    static std::string Nested(const Members& members, const std::string& indent) {
        std::string                    out;
        std::map<std::string, Members> instances;
        for (const Member& member : members) {
            const std::size_t point = member.key.find('.');
            if (point == std::string::npos) {
                out += indent + member.type + " " + member.key + member.dimensions + ";\n";
            } else {
                instances[member.key.substr(0, point)].push_back(
                    {member.type, member.key.substr(point + 1), member.dimensions});
            }
        }
        for (const auto& [instance, own] : instances)
            out += indent + "struct {\n" + Nested(own, indent + "    ") + indent + "} " + instance +
                   ";\n";
        return out;
    }

    Compiled Print(const std::string& module, const std::string& source) {
        for (const auto& [name, sequence] : sequences_)
            if (!sequence.definition && parameters_.count(name))
                throw Refusal("cannot compile: " + name +
                              " is read both as a value and as a sequence");
        for (auto& [name, sequence] : sequences_) Rate(name, sequence);
        const int earliest = earliest_ = Starts();
        for (auto& [name, sequence] : sequences_)
            for (const std::size_t h : sequence.holds) Resolve(name, holds_[h]);
        for (auto& [name, sequence] : sequences_) Seeds(name, sequence);
        const std::vector<std::string> order    = Order();
        std::vector<std::string>       fields;
        for (const Early& early : earlies_)
            if (Needed(early))
                for (const auto& [read, lags] : early.reads)
                    early.reader->deferred[read].insert(lags.begin(), lags.end());
        for (auto& [name, sequence] : sequences_) {
            if (!sequence.definition) fields.push_back(name);
            for (const Reads* reads : {&sequence.reads, &sequence.deferred, &sequence.seeded})
                for (const auto& [read, lags] : *reads)
                    sequences_.at(read).depth =
                        std::max(sequences_.at(read).depth, *lags.rbegin() + 1);
            if (!sequence.own.empty())
                sequence.depth = std::max(sequence.depth, *sequence.own.rbegin() + 1);
        }
        for (const Hold& hold : holds_)
            if (hold.early < 0 || Needed(earlies_[static_cast<std::size_t>(hold.early)]))
                sequences_.at(hold.read).depth =
                    std::max(sequences_.at(hold.read).depth, hold.most + 1);
        Histories(earliest);
        // A model's in the order its signature gives them.
        if (model_) {
            const auto at = [&](const std::string& name) {
                const auto& p = model_->parameters;
                return std::find_if(p.begin(), p.end(),
                                    [&](const auto& q) { return q.name == name; });
            };
            std::stable_sort(
                fields.begin(), fields.end(),
                [&](const std::string& a, const std::string& b) { return at(a) < at(b); });
        }
        const std::vector<std::string> inputs = fields;
        fields.insert(fields.end(), order.begin(), order.end());

        std::string guard;
        for (const char c : module)
            guard += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        guard += "_H";

        std::string out;
        out += "/* Generated by 'inkamath --compile' from " + source + ": edit that, not this.\n";
        out += " * The arithmetic is in the order the definitions give it, so do not build it\n";
        out += " * with -ffast-math, which reorders. A compiler may also fuse a multiply and an\n";
        out += " * add, rounding once where the definitions round twice (-ffp-contract): often\n";
        out += " * closer to exact, but the step then depends on how it is built, so build\n";
        out += " * what 'inkamath --check' writes the way the step itself is built. */\n";
        out += "#ifndef " + guard + "\n#define " + guard + "\n\n";
        out += "#include <math.h>\n#include <string.h>\n\n";
        out += Interface(module, inputs, fields, earliest);
        out += "/* The parameters, which the host may assign, then what derives from them,\n";
        out += " * then the index of the latest step and each sequence's terms from that\n";
        out += " * index back. */\n";
        Members members;
        for (const auto& [name, parameter] : parameters_)
            members.push_back({"double", name, Dimensions(parameter.rows, parameter.cols)});
        for (const Derived& derived : derived_)
            if (derived.read)
                members.push_back(
                    {"double", derived.name, Dimensions(derived.code.rows, derived.code.cols)});
        members.push_back({"long long", "index_", ""});
        for (const std::string& name : fields) {
            const Sequence& sequence = sequences_.at(name);
            members.push_back({"double", name,
                               "[" + std::to_string(sequence.depth) + "]" +
                                   Dimensions(sequence.rows, sequence.cols)});
            if (clauses_ && !sequence.guarded.empty())
                members.push_back({"int", name + "_clause_", ""});
            if (!sequence.taken.empty())
                members.push_back(
                    {"int", name + "_clause_", "[" + std::to_string(sequence.taken.size()) + "]"});
        }
        out +=
            "typedef struct " + module + " {\n" + Nested(members, "    ") + "} " + module + ";\n\n";

        for (const std::size_t n : inverses_) out += InverseHelper(n);
        for (const std::string& limit : limits_) out += limit;

        out += "/* Computes what derives from the parameters: call it after assigning one. */\n";
        out += "static inline void " + module + "_update(" + module + "* m_) {\n";
        const auto read = [](const Derived& derived) { return derived.read; };
        if (std::none_of(derived_.begin(), derived_.end(), read)) out += "    (void)m_;\n";
        for (const Derived& derived : derived_) {
            if (!derived.read) continue;
            std::string assignments;
            for (std::size_t c = 0; c < derived.code.cells.size(); ++c)
                assignments += "    m_->" + derived.name +
                               (derived.code.Scalar()
                                    ? ""
                                    : Subscript(c / derived.code.cols, c % derived.code.cols)) +
                               " = " + derived.code.cells[c].text + ";\n";
            out += Temporaries(derived.temporaries, assignments, "    ") + assignments;
        }
        out += "}\n\n";

        out += "static inline void " + module + "_init(" + module + "* m_) {\n";
        out += "    memset(m_, 0, sizeof *m_);\n";
        for (const auto& [name, parameter] : parameters_) {
            const bool scalar = parameter.rows * parameter.cols == 1;
            for (std::size_t c = 0; c < parameter.initial.size(); ++c)
                out += "    m_->" + name +
                       (scalar ? "" : Subscript(c / parameter.cols, c % parameter.cols)) + " = " +
                       Double(parameter.initial[c]) + ";\n";
        }
        const std::size_t folds = out.size();
        out += "    m_->index_ = " + std::to_string(earliest - 1) + ";\n";
        out += "    " + module + "_update(m_);\n}\n\n";

        out += "/* Advances to the next index, the first at " + std::to_string(earliest) +
               ", and computes its terms. */\n";
        out += "static inline void " + module + "_step(" + module + "* m_";
        for (const std::string& name : inputs) out += ", double " + name;
        out += ") {\n    ++m_->index_;\n";
        // A sequence's window moves when it computes a term: every step, or
        // on its ticks.
        const auto shifted = [&](const std::string& name, const std::string& indent) {
            const Sequence& sequence = sequences_.at(name);
            std::string     lines;
            for (int k = sequence.depth - 1; k > 0; --k) {
                const std::string to   = "m_->" + name + "[" + std::to_string(k) + "]";
                const std::string from = "m_->" + name + "[" + std::to_string(k - 1) + "]";
                lines += sequence.rows * sequence.cols == 1
                             ? indent + to + " = " + from + ";\n"
                             : indent + "memcpy(" + to + ", " + from + ", sizeof " + to + ");\n";
            }
            return lines;
        };
        for (const std::string& name : fields)
            if (sequences_.at(name).period == 1) out += shifted(name, "    ");
        for (const std::string& name : inputs) out += "    m_->" + name + "[0] = " + name + ";\n";
        for (const std::string& name : order) {
            const Sequence&   sequence = sequences_.at(name);
            const bool        scalar   = sequence.general.size() == 1;
            const bool        rated    = sequence.period > 1;
            const bool        late     = sequence.start > earliest || rated;
            const std::string indent   = late ? "        " : "    ";
            if (late)
                out += "    if (m_->index_ >= " + std::to_string(sequence.start) +
                       (rated ? " && (m_->index_" + Less(sequence.phase) + ") % " +
                                    std::to_string(sequence.period) + " == 0"
                              : std::string()) +
                       ") {\n";
            if (rated) out += shifted(name, indent);
            std::string assignments;
            for (std::size_t c = 0; c < sequence.general.size(); ++c) {
                assignments += indent + "m_->" + name + "[0]" +
                               (scalar ? "" : Subscript(c / sequence.cols, c % sequence.cols)) +
                               " = ";
                for (const auto& [index, base] : sequence.bases)
                    assignments +=
                        "m_->index_ == " + std::to_string(index) + " ? " + base[c] + " : ";
                // Checked only where an index no base clause gives needs it.
                const auto before = [&](int from, const std::string& otherwise = "NAN") {
                    for (int n = sequence.start; n < from; ++n)
                        if (!sequence.bases.count(n))
                            return "m_->index_ < " + std::to_string(from) + " ? " + otherwise +
                                   " : ";
                    return std::string();
                };
                // Ahead of the cells, which the guards cannot read.
                if (clauses_ && c == 0 && !sequence.guarded.empty()) {
                    std::string kept = indent + "m_->" + name + "_clause_ = ";
                    for (const auto& [index, base] : sequence.bases)
                        kept += "m_->index_ == " + std::to_string(index) + " ? 0 : ";
                    for (const Guarded& guarded : sequence.guarded)
                        kept += before(guarded.guard_from, "0") + guarded.condition + " ? " +
                                std::to_string(guarded.clause + 1) + " : ";
                    assignments.insert(0,
                                       kept + std::to_string(sequence.general_clause + 1) + ";\n");
                }
                for (const Guarded& guarded : sequence.guarded) {
                    const std::string value = before(guarded.value_from);
                    assignments +=
                        before(guarded.guard_from) + guarded.condition + " ? " +
                        (value.empty() ? guarded.cells[c] : "(" + value + guarded.cells[c] + ")") +
                        " : ";
                }
                if (!sequence.guarded.empty()) assignments += before(From(sequence.general_reads));
                assignments += sequence.general[c] + ";\n";
            }
            // Ahead of the cells too, a cell's clause where no base term stands.
            std::string picks;
            for (std::size_t c = 0; c < sequence.taken.size(); ++c) {
                picks += indent + "m_->" + name + "_clause_[" + std::to_string(c) + "] = ";
                for (const auto& [index, base] : sequence.bases)
                    picks += "m_->index_ == " + std::to_string(index) + " ? 0 : ";
                picks += sequence.taken[c] + ";\n";
            }
            assignments.insert(0, picks);
            // Resolved before what is used is known: a term read back that is
            // not computed again leaves its temporaries unread.
            std::vector<Temporary> temporaries = sequence.temporaries;
            for (Temporary& temporary : temporaries)
                for (std::string& line : temporary.lines)
                    line = Checked(Rated(line, sequence), name, sequence);
            const std::string checked = Checked(Rated(assignments, sequence), name, sequence);
            out += Temporaries(temporaries, checked, indent) + checked;
            if (late) out += "    }\n";
        }
        out += "}\n\n#endif\n";
        out.insert(folds, Folds(earliest));
        Compiled compiled{out, earliest, inputs, {}, {}};
        for (const std::string& name : order) {
            const Sequence& sequence = sequences_.at(name);
            const bool      unnamed  = std::any_of(
                labels_.begin(), labels_.end(),
                [&](const std::string& label) { return name.rfind(label + ".", 0) == 0; });
            compiled.sequences.push_back(
                {name, sequence.rows, sequence.cols, sequence.period, sequence.phase,
                 sequence.period * sequence.first + sequence.phase, unnamed});
            if (clauses_ && (!sequence.guarded.empty() || !sequence.taken.empty()))
                compiled.guarded.push_back(name);
        }
        return compiled;
    }

    ReferenceStack<Value>&           definitions_;
    const Model<Value>*              model_;  // the model compiled, if one is
    const Scope<Value>&              root_;   // the scope compiled, the session's or the model's
    const Scope<Value>*              scope_;  // where the names being read are sought
    const Scope<Value>*              own_ = nullptr;  // the scope after a point, for one name
    Expansion*                       expansion_ = nullptr;  // the call being compiled, if one is
    int                              expanded_  = 0;        // how deep calls are
    std::vector<std::shared_ptr<const Scope<Value>>> held_;    // what expansions name
    std::string                                      within_;  // the sequence being compiled
    // Unnamed instances, by where they are written and what they read there.
    std::map<std::pair<const void*, std::string>, const Scope<Value>*> kept_;
    std::set<std::string>                                              labels_;
    std::map<std::string, Sequence>  sequences_;
    std::map<std::string, Parameter> parameters_;
    std::vector<Derived>             derived_;  // each after those it reads
    std::set<std::string>            fixed_;    // parameters compiled as constants; see Fix
    std::set<std::string>            aside_;    // definitions refused; see Refusals
    bool                             clauses_ = false;  // whether the step keeps them; see Build
    std::map<std::string, Value>     known_;    // globals that read only those
    std::set<std::string>            read_parameters_;      // by the value being compiled
    bool                             read_global_ = false;  // by the value being compiled
    std::set<std::string>            reading_plain_;
    Sequence*   reading_ = nullptr;  // the sequence whose general clause this is
    std::string index_;              // and the name of its index
    std::vector<Temporary>*          temporaries_ = nullptr;  // where this sequence's are declared
    Reads*      clause_reads_ = nullptr;               // what the clause being compiled reads
    bool                             deferring_ = false;  // compiling the right of an 'and' or 'or'
    int                        shift_ = 0;  // how far back the term being computed again is; see At
    std::vector<Early>         earlies_;
    int                        early_ = -1;    // the one being computed again
    std::map<std::string, int> exists_;        // where one with no base first answers
    std::vector<Check>         checks_;        // what a deferred right side reads, by its mark
    std::map<std::string, const Reference<Value>*> histories_;  // by the input they give
    std::map<std::string, int>                     reach_;      // the last index each gives
    std::map<std::pair<std::string, int>, std::optional<Value>> folded_;
    std::map<std::pair<std::string, int>, std::string> unfolded_;  // why a history gave none
    int                                                         earliest_ = 0;
    std::string index_text_   = "(double)m_->index_";  // the index, as the step has it
    std::string                      module_;
    std::set<std::size_t>            inverses_;  // the sizes a helper is needed for
    std::vector<std::string>         limits_;    // a function for each limit walked
    std::vector<Hold>                  holds_;
    Sequence*                          basing_ = nullptr;  // whose base clause is compiled, if one
    int                                base_   = 0;        // and its index
    std::map<std::string, std::string> limit_names_;  // each one's name, by its text
    // The sequence a limit's function is walking, while its clauses compile.
    struct Walked {
        std::string              name;
        std::vector<std::string> parameters;
        int                      depth = 0;           // how far back a term reads its own
        std::size_t              rows = 0, cols = 0;  // a term's, once a clause gives it
    };
    Walked*                          limit_           = nullptr;
    int                              temporary_count_ = 0;
    std::map<std::string, Value>     places_;             // a cell's row and column, by their names
    Code        code_;
};

#endif  // INKAMATH_COMPILE_HPP
