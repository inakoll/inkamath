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
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

// What the compiler will not compile, and in which definition, where one
// is to blame.
struct Refusal : std::runtime_error {
    using std::runtime_error::runtime_error;
    Refusal(const std::string& definition, const std::string& why)
        : std::runtime_error("cannot compile " + definition + ": " + why), name(definition) {}
    std::string name;
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
            Extent      size;
            int         period = 1, phase = 0;  // term m at step period*m + phase
            int         start = 0;              // the step of its first term, at another rate
            bool        unnamed = false;          // of an instance a model writes unnamed
            bool        session = false;          // the session's, read by the model
        };
        std::string              header;
        int                      first;      // the index of the first step
        std::vector<std::string> inputs;     // the step's arguments, in order
        std::vector<std::size_t> cells;      // and how many cells each has
        std::vector<Sequence>    sequences;  // those it computes, in the order it does
        std::vector<std::string> guarded;    // those whose clause is kept, 'v_clause_'
        // Each instance the model writes unnamed, by its label, for the check
        // to make again where the instance checked is (C84).
        struct Unnamed {
            const Reference<Value>*                    model;
            const ParametersCall<Value>*               call;
            std::vector<std::pair<std::string, Value>> captured;
        };
        std::map<std::string, Unnamed> unnamed;
        // Each guard reading a limit or an optional input, each by name and
        // the kind of its status, 'runs out' or 'is absent', and whether it
        // was compiled outside grad and under it, for --check to list (DESIGN.md).
        using Named = std::pair<std::string, std::string>;
        struct Fallen {
            std::vector<Named> limits;
            bool               plain = false, grad = false;
        };
        std::map<const Expression<Value>*, Fallen> fallen;
    };

    // '--float' (DESIGN.md, a float target): a float wherever a double is.
    static inline bool floats = false;
    static std::string Real() { return floats ? "float" : "double"; }
    static bool Finite(double x) { return std::isfinite(floats ? static_cast<float>(x) : x); }
    // A constant folded from a number approximated past a thousand digits,
    // which the interpreter marks and a double cannot (C395).
    static inline bool approximated_ = false;
    static constexpr char rounded       = '\x16';
    friend class CheckC;  // which writes its inputs as constants are
    static inline const std::set<std::string> keywords = {
        "auto",    "break",  "case",     "char",   "const",    "continue", "default",
        "do",      "double", "else",     "enum",   "extern",   "float",    "for",
        "goto",    "if",     "inline",   "int",    "long",     "register", "restrict",
        "return",  "short",  "signed",   "sizeof", "static",   "struct",   "switch",
        "typedef", "union",  "unsigned", "void",   "volatile", "while"};
    // The macros of the headers a step includes that a name can spell (C347).
    static inline const std::set<std::string> macros = {"BUFSIZ", "EOF", "INFINITY", "NAN", "NULL"};
    // C++'s keywords that are not C's, the headers building as C++ too (C400).
    static inline const std::set<std::string> cxx = {
        "alignas", "alignof", "asm", "bitand", "bitor", "bool", "catch", "class", "compl",
        "concept", "consteval", "constexpr", "constinit", "decltype", "delete", "explicit",
        "export", "false", "friend", "mutable", "namespace", "new", "noexcept", "not", "nullptr",
        "operator", "private", "protected", "public", "requires", "template", "this", "throw",
        "true", "try", "typeid", "typename", "using", "virtual", "xor",
        // and those only a header's file name can spell
        "and", "and_eq", "char16_t", "char32_t", "char8_t", "co_await", "co_return", "co_yield",
        "const_cast", "dynamic_cast", "not_eq", "or", "or_eq", "reinterpret_cast", "static_assert",
        "static_cast", "thread_local", "wchar_t", "xor_eq"};
    [[nodiscard]] static std::string Keeper(const std::string& name) {
        if (keywords.count(name) || macros.count(name)) return "C";
        return cxx.count(name) ? "C++" : "";
    }

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
                if (refusal.name.empty() || !aside.insert(refusal.name).second) return refusals;
            }
        }
    }

private:
    static Compiled Run(ReferenceStack<Value>& definitions, const std::string& module,
                        const std::string& source, const Model<Value>* model,
                        const Scope<Value>* instance, const std::set<std::string>& aside,
                        bool clauses = false) {
        std::set<std::string> fixed, first;
        bool                  aware = false;
        for (;;) {
            CompileC compiler(definitions, model, instance ? *instance : definitions.Session());
            compiler.module_ = module;
            compiler.fixed_  = fixed;
            compiler.aside_  = aside;
            compiler.clauses_ = clauses;
            compiler.aware_   = aware;
            compiler.first_   = first;
            try {
                compiler.Define(compiler.root_);
                if (model) compiler.Signature();
                for (const std::string& name : first) compiler.Compile(name);
                // An unnamed instance compiled on the way defines more.
                for (bool more = true; more;) {
                    more = false;
                    for (const auto& [name, sequence] : Sorted(compiler.sequences_)) {
                        if (!sequence.definition || sequence.compiled) continue;
                        compiler.Compile(name);
                        more = true;
                    }
                }
                Compiled compiled = compiler.Print(module, source);
                // A mark left unresolved is no C, and the compiler's mistake (C112).
                if (std::any_of(compiled.header.begin(), compiled.header.end(),
                                [](unsigned char c) { return c < ' ' && c != '\n'; }))
                    throw std::runtime_error("a control character in the header, a compiler bug");
                // One that writes NaN anywhere past the line naming its source,
                // or a root C makes NaN (C350), is compiled again, aware.
                if (aware ||
                    (!compiler.roots_ && !WritesNan(compiled.header, compiled.header.find('\n'))))
                    return compiled;
                aware = true;
            } catch (const Fix& fix) {
                fixed.insert(fix.names.begin(), fix.names.end());
            } catch (const First& shaped) {
                first.insert(shaped.name);
            }
        }
    }

    // Whether C reads NAN in it from 'from' on: as a word, which no name can be.
    static bool WritesNan(const std::string& code, std::size_t from = 0) {
        return Writes(code, "NAN", from);
    }
    // Or may answer it, calling a function of the header that does (C211).
    // Each word of it looked up, not each name searched for: the sets grow
    // with every call under grad.
    bool MayNan(const std::string& code) const {
        const auto name = [&](std::size_t at) {
            return std::isalnum(static_cast<unsigned char>(code[at])) || code[at] == '_';
        };
        for (std::size_t at = 0, end; at < code.size(); at = end + 1) {
            for (end = at; end < code.size() && name(end);) ++end;
            if (end == at) continue;
            const std::string word = code.substr(at, end - at);
            if (word == "NAN" || nan_functions_.count(word) || nan_temporaries_.count(word))
                return true;
        }
        return false;
    }
    static bool Writes(const std::string& code, const std::string& word, std::size_t from = 0) {
        const auto name = [&](std::size_t at) {
            return std::isalnum(static_cast<unsigned char>(code[at])) || code[at] == '_';
        };
        for (std::size_t at = code.find(word, from); at != std::string::npos;
             at             = code.find(word, at + 1))
            if ((at == 0 || !name(at - 1)) && !name(at + word.size())) return true;
        return false;
    }

    // Parameters read where the compiled code needs a constant -- a size, a
    // bound, a lag, an exponent, a place -- which the struct cannot let the
    // host change. The file is compiled again with them fixed, and so a
    // constant everywhere they are read. A parameter is never read once fixed,
    // so this ends.
    struct Fix {
        std::set<std::string> names;
    };
    // A sequence compiling whose guards gave it a shape, where one whose shape
    // its clause would give reads it, is compiled first, so that the cycle
    // closes on its window (C403).
    struct First {
        std::string name;
    };

    // Why a definition cannot be compiled; Compile names the definition.
    struct Reason : std::runtime_error {
        using std::runtime_error::runtime_error;
    };
    // The interpreter's own error, folding a value that reads no name.
    struct Undefined : Reason {
        using Reason::Reason;
    };
    // A depth passed, which is the read's, not the term's it reached.
    struct Deep : Reason {
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
        // Marked, for Numbered to find each value written; Print erases the marks.
        Cell(std::string written, int precedence)
            : text('\x14' + std::move(written) + '\x15'), level(precedence) {}

        std::string text;
        int         level = primary;
        std::string magnitude;  // what it is the negation of, if it is one
        int         magnitude_level = primary;
        bool        atom            = false;  // a name or a number, as cheap to repeat as to store
        bool        number          = false;  // never NaN: a number, or the index
        std::string nan;                      // NaN where this is, if not only where itself is
    };

    static Cell Atom(std::string text, bool number = false) {
        Cell cell;
        cell.text   = std::move(text);
        cell.atom   = true;
        cell.number = number;
        return cell;
    }

    // Where a part is there, if not always: a condition, or where any of
    // several is. Made at each operation under grad and written at few, its
    // text is made where it is written (C231).
    struct Moving;
    using Where = std::shared_ptr<const Moving>;
    struct Code {
        Extent               size;
        std::vector<Cell>    cells;     // row by row, slice after slice
        std::optional<Value> constant;  // its exact value, where it reads no name
        bool                 whole = false;  // NaN in every cell where in one, as a term
        // Under grad (DESIGN.md, grad compiled): its part, none where the
        // interpreter's is absent, and where it is there, if not always.
        std::vector<Code> part;
        // In a nest of two grads, the part by the outer, a first derivative's
        // own, 'part' being the inner's (DESIGN.md, grad's order).
        std::vector<Code> outer;
        Where             moves;
        std::size_t       node = 0;  // under reverse mode, the node of the tape it is
        bool              zero = false;  // an adjoint 0 where its 'moves' does not hold

        bool Scalar() const { return cells.size() == 1 && !size.slices; }
        // Cell c of what it stretches to, as in arithmetic: a single value to
        // any shape, a matrix to every slice of a tensor.
        const Cell& At(std::size_t c) const { return cells[c % cells.size()]; }
        const Cell& At(std::size_t i, std::size_t j, std::size_t b = 0) const {
            return At((b * size.rows + i) * size.cols + j);
        }
    };

    struct Moving {
        std::string        text;
        std::vector<Where> any;
    };

    // Reverse mode's tape (DESIGN.md, the compiler's adjoint step), as
    // Derivative's, a node's value the code the sweep reads.
    enum class Kind { Seed, Sum, Negative, Left, Right, Quotient, Cells, Mark, Gather, Place };
    using Origin = std::pair<std::size_t, std::size_t>;  // a node's cell, from 0; node 0 for none
    struct Node {
        Kind                kind;
        Extent              extent;
        std::size_t         a, b, at;
        std::optional<Code> value;
        std::vector<Origin> from;
    };
    struct Tape {
        std::vector<Node>                  nodes;
        std::map<std::string, std::size_t> made;  // each by what it is, as the interpreter's (C323)
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

    // A term a base clause reads: 'x_at' in the base clause of 'index'.
    struct Seed {
        std::string read;
        int         index, at, lag = 0;
    };

    // A guarded clause, and what its guard and its value each read: its guard
    // is evaluated wherever the chain reaches it, its value only where it holds,
    // so each has its own index from which it can be.
    struct Guarded {
        std::string              condition;
        std::vector<std::string> cells;
        Reads                    guard, value;
        int                      guard_from = 0, value_from = 0;
        int                      clause = 0;  // its place among the definition's
        bool                     constant = false;
    };

    struct Sequence {
        const Reference<Value>*                 definition = nullptr;  // none for an input
        std::map<int, std::vector<std::string>> bases;                 // cells, by index
        std::vector<Guarded>                    guarded;               // in the order tried
        int general_clause = -1;  // the place of the clause that always applies, if one does
        Reads                                   general_reads;
        std::vector<std::string>                general;
        std::string present;  // of an inner optional input, where its argument is present
        Extent                                  size{0, 0};  // none until known
        bool                                    based = false, compiling = false, compiled = false;
        bool session = false;                           // the session's, read by the model
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
        // Of a grad by several names, its parts, which the first's step computes.
        std::vector<std::string> parts;
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
        Extent              size;
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
                // An inner optional input is absent before the stream but where a history gives it.
                if (!definition->Clauses().empty() || (definition->optional && &scope != &root_))
                    History(key, *definition);
                if (definition->argument->input) {
                    // An inner optional input its instance leaves out, absent at every step.
                    if (!definition->optional || &scope == &root_) continue;
                    Sequence& left  = sequences_[key];
                    left.definition = definition->argument.get();
                    const Home there(*this, definition.get());
                    if (!definition->declared.empty()) left.size = Stated(key, *definition);
                    continue;
                }
                Define(key, *definition->argument);
                if (!definition->declared.empty()) stated_[key] = definition.get();
                continue;
            }
            Define(key, *definition);
            const Scope<Value>* instance = nullptr;
            try {
                instance = definitions_.InstanceScope(definition);
            } catch (const std::runtime_error& error) {
                throw Refusal(key, error.what());
            }
            if (!instance) continue;
            outer_[instance] = &scope;
            Define(*instance);
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
        std::map<std::string, int>                     offsets;  // from the caller's index
        // Each argument as written, and where: a cell read past it is refused
        // by the name it reads (Unstated).
        std::map<std::string,
                 std::tuple<const Expression<Value>*, const Scope<Value>*, const Expansion*>>
            written = {};
    };

    // A clause's locals (DESIGN.md, locals compiled), why one is refused, and grads' names.
    struct Frame {
        std::map<std::string, Code>        values;
        std::map<std::string, std::string> refused;
        std::set<std::string>              graded;
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
        const Scope<Value>*& kept = kept_[{scope_, member.get(), values}];
        if (!kept) {
            // Two written in one sequence are told apart by a number.
            std::string label = within_ + "_" + model.Name() + values;
            for (int other = 2; !labels_.insert(label).second; ++other)
                label = within_ + "_" + model.Name() + std::to_string(other) + values;
            unnamed_[label] = {&model, &call, captured};
            held_.push_back(definitions_.Detached(model, call, *scope_, label, captured));
            kept = held_.back().get();
            outer_[kept] = scope_;
            Define(*kept);
        }
        own_ = kept;
        return Emit(member);
    }

    // Read again, it reads no term and calls nothing anew.
    static bool Plain(const Expression<Value>& e) {
        return !dynamic_cast<const FuncExpression<Value>*>(&e) &&
               std::all_of(e.Children().begin(), e.Children().end(),
                           [](const auto& c) { return !c || Plain(*c); });
    }

    // The names an expression reads, but not those after a point.
    static void Named(const PExpression<Value>& expression, std::set<std::string>& names) {
        if (!expression) return;
        if (!expression->Name().empty()) names.insert(expression->Name());
        const auto* member = dynamic_cast<const MemberExpression<Value>*>(expression.get());
        for (const PExpression<Value>& child : expression->Children())
            if (!member || child != member->Member()) Named(child, names);
    }

    static void Binding(const Expression<Value>& e, std::set<std::string>& names) {
        if (dynamic_cast<const EqualExpression<Value>*>(&e)) names.insert(e.Name());
        for (const PExpression<Value>& child : e.Children())
            if (child) Binding(*child, names);
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
    // are not seen inside it, nor its guard, as the interpreter's (C133).
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
            throw Deep("nested " + std::to_string(max_expanded) +
                       " deep, which a recursion its guards do not end would pass");
        const Setting<bool>  unguarded(guarding_, false);
        const Setting<Frame> own(frame_, {});
        return body();
    }
    static constexpr int max_expanded = 64;

    // A guarded value the interpreter refuses wherever it is taken, as log's
    // '| x <= 0 = 1/0', is what a step says there: NaN, as where no clause
    // applies (C86). Read where its guard held, it asks no status of a limit
    // the guard read to hold, which did not run out there and which the audit
    // would list (C368); the others only where the chain takes it (C370).
    Code Taken(const PExpression<Value>& expression, const std::string& condition,
               const std::string& before) {
        const std::set<std::string> own = condition.empty() ? std::set<std::string>() : sure_;
        std::set<std::string>       read;
        Code                        code;
        try {
            const Setting<std::set<std::string>*> reading(statuses_, &read);
            code = Emit(expression);
        } catch (const Undefined&) {
            if (condition.empty()) throw;
            code.cells.push_back(Atom("NAN"));
        }
        for (const std::string& status : read)
            if (statuses_ && !own.count(status))
                statuses_->insert(Joined(before, condition, status));
        return code;
    }
    // A status asked only where the chain reaches what reads it: past the
    // guards before, as Chain tests them, and where its own guard holds.
    std::string Joined(const std::string& before, const std::string& condition,
                       const std::string& status) {
        if (before.empty() && condition.empty()) return status;
        const std::string joined = "(" + before +
                                   (condition.empty() ? "1.0" : condition + " ? 1.0 : 0.0") +
                                   ") == 1.0 && (" + status + ")";
        what_[joined] = what_[status];
        return joined;
    }

    // Under reverse mode, a clause a moving guard chose is marked, as an inexact truth's is.
    Code Chosen(Code value) {
        if (tape_ && chosen_ && !value.part.empty())
            value.part[0] = Record(Kind::Mark, value.part[0].size, value.part[0].node);
        return value;
    }

    // A value that is not a sequence, its clauses tried as the interpreter
    // tries them: the guarded in the order written, then the one that always
    // applies.
    // At an index, as Reference::EvalImp tries them: a base clause where it
    // is written, and the general one down to the lowest base.
    Code Chained(const std::string& name, const Reference<Value>& definition,
                 std::optional<int> at = std::nullopt) {
        if (IsSequence(definition) != bool(at))
            throw Reason(name + (at ? " is not a sequence" : " is a sequence; index it"));
        const auto cells = [](const Clause<Value>& c) { return c.parameters.cells(); };
        if (std::any_of(definition.Clauses().begin(), definition.Clauses().end(), cells)) {
            if (at) throw Reason("a sequence with parameters by cells, for now");
            return Cells(name, definition);
        }
        std::vector<std::pair<std::string, Code>> guarded;
        std::optional<Code>                       otherwise;
        std::set<std::string>                     asked;
        std::string                               before;
        const Clause<Value>* const                lowest = definition.EndBase(true);
        const Setting<bool>                       chosen(chosen_, false);
        // Each clause sees its own index alone, as Reference::Selects binds it (C186).
        using Values                           = decltype(expansion_->values);
        const auto                     unbound = at ? expansion_->values : Values();
        std::optional<Setting<Values>> restore;
        if (at) restore.emplace(expansion_->values, unbound);
        for (const bool guard : {true, false}) {
            for (const Clause<Value>& clause : definition.Clauses()) {
                const ParametersDefinition<Value>& p    = clause.parameters;
                const bool                         base = at && !p.general();
                if (otherwise || (p.guarded() || base) != guard || (base && p.index() != *at) ||
                    (at && !guard && lowest && *at < lowest->parameters.index()))
                    continue;
                const Setting<Frame> own(frame_, {});
                if (at) expansion_->values = unbound;
                if (at && p.general())
                    expansion_->values.insert_or_assign(p.index_name(),
                                                        Literal(Value(Number(*at))));
                const std::optional<std::string> condition =
                    p.guarded() ? Condition(p.guard()) : std::optional<std::string>("");
                if (p.guarded())
                    for (const std::string& status : asked_)
                        asked.insert(Joined(before, "", status));
                if (!condition) continue;
                const Code value = Chosen(Taken(clause.expression, *condition, before));
                if (condition->empty())
                    otherwise = value;
                else
                    guarded.emplace_back(*condition, value);
                before += *condition + " ? 0.0 : ";
            }
        }
        Told(asked, otherwise.has_value());
        if (guarded.empty()) {
            if (!otherwise)
                throw Reason(at ? name + " has no clause for index " + std::to_string(*at)
                                : "no clause of " + name + " applies");
            return *otherwise;
        }
        // The interpreter answers each clause's own shape, which a step cannot (C138).
        const Extent size = guarded.front().second.size;
        for (const auto& [condition, value] : guarded)
            if (value.size != size || (otherwise && otherwise->size != size))
                throw Reason("the clauses of " + name + " have different shapes");
        return Chain(guarded, otherwise, size);
    }

    static Code Outward(Code c) {
        c.part  = std::move(c.outer);
        c.moves = c.part.empty() ? nullptr : c.part[0].moves;
        c.outer.clear();
        return c;
    }

    // Each cell as the step tests it: the guarded values in order, then the
    // one that always applies, else NaN, as where no clause applies (C86).
    Code Chain(const std::vector<std::pair<std::string, Code>>& guarded,
               const std::optional<Code>& otherwise, Extent size) {
        Code chain;
        chain.size = size;
        for (std::size_t c = 0; c < size.count(); ++c) {
            if (guarded.empty() && otherwise) {
                chain.cells.push_back(otherwise->At(c));
                continue;
            }
            std::string cell;
            for (const auto& [condition, value] : guarded)
                cell += condition + " ? " + value.At(c).text + " : ";
            cell += otherwise ? otherwise->At(c).text : "NAN";
            chain.cells.emplace_back(cell, 0);  // a conditional, below every operator
        }
        // In a nest, the outer part of the clause taken, chained as the part
        // below, of each clause as a value whose part it is.
        if ((otherwise && !otherwise->outer.empty()) ||
            std::ranges::any_of(guarded, [](const auto& g) { return !g.second.outer.empty(); })) {
            std::vector<std::pair<std::string, Code>> outers;
            for (const auto& [condition, value] : guarded)
                outers.emplace_back(condition, Outward(value));
            const Code by =
                Chain(outers, otherwise ? std::optional(Outward(*otherwise)) : std::nullopt, size);
            chain.outer = by.part;
            if (!by.part.empty()) chain.outer[0].moves = by.moves;
        }
        // Under grad, the part of the clause taken: 0 where it has none, NaN
        // where it is refused wherever taken, and beside it where it is there.
        bool any = otherwise && !otherwise->part.empty(), always = true;
        std::vector<std::pair<std::string, Code>> parts, where;
        const auto                                part = [](const Code& c) {
            if (!c.part.empty()) return c.part[0];
            return c.cells[0].text == "NAN" ? Of(Atom("NAN")) : Literal(Value(Number(0)));
        };
        // A flag, unlike a condition written out, which has a space, is an
        // int, which a NaN beside it makes a float (-Wconversion).
        const auto there = [this](const Code& c) {
            if (c.part.empty() && c.cells[0].text != "NAN") return Of(Atom("0"));
            if (!c.moves) return Of(Atom("1"));
            const std::string text = Text(c.moves);
            return Of(Atom("(" + text + (text.find(' ') == text.npos ? " != 0)" : ")")));
        };
        for (const auto& [condition, value] : guarded) {
            any    = any || !value.part.empty();
            always = always && there(value).cells[0].text == "1";
            parts.emplace_back(condition, part(value));
            where.emplace_back(condition, there(value));
        }
        if (!any) return chain;
        // Under reverse mode a gather of its clauses' nodes, its value where
        // each is taken, which the sweep passes what reaches it on to.
        Code                taken;
        std::vector<Origin> clauses;
        std::string         before;
        // Written as reals, as the temporary holding each is (MSVC's C4244).
        const auto clause = [&](const Code& value, const std::string& condition) {
            if (!value.part.empty()) {
                const std::string one = condition == "1" ? "1.0" : condition + " ? 1.0 : 0.0";
                taken.cells.emplace_back(before + one, 0);
                clauses.emplace_back(value.part[0].node, 0);
            }
            before += condition + " ? 0.0 : ";
        };
        for (const auto& [condition, value] : guarded) clause(value, condition);
        if (otherwise) clause(*otherwise, "1");
        chain.part = {
            tape_ ? Record(Kind::Gather, size, 0, taken, 0, 0, clauses)
                  : Chain(parts, otherwise ? std::optional(part(*otherwise)) : std::nullopt, size)};
        if (always && (!otherwise || there(*otherwise).cells[0].text == "1")) return chain;
        const std::optional<Code> rest =
            otherwise ? std::optional(there(*otherwise)) : std::nullopt;
        // A truth, as a test reads it: NaN where no clause applies is one.
        const std::string truth = "(" + Chain(where, rest, Extent{}).cells[0].text + ") != 0";
        chain.moves             = std::make_shared<const Moving>(Moving{truth, {}});
        return chain;
    }

    // The functions the interpreter lists as staircases, refused where their
    // argument moves, as the interpreter refuses them, rather than answer a
    // part that is wrong there, a bisection's staircase 0 and the like.
    void Staircase(const Reference<Value>& function, const Code& argument) const {
        if (!Still(argument) && definitions_.staircases.contains(&function))
            throw Reason("grad cannot differentiate " + function.Name() + " yet");
    }

    // A function called: its parameters bound to the arguments, read where the
    // call is, and its defaults to what they read inside it; read at an
    // index, 'f(x)_k', a term of a sequence with parameters (DESIGN.md).
    Code Call(const std::string& name, const Reference<Value>& function,
              const ParametersCall<Value>& call, Expansion* outer) {
        const ParametersDefinition<Value>& p = function.Clauses().front().parameters;
        Reasoned([&] { p.CheckArity(name, call); });
        // At a limit's walking index less a constant, walked beside it, its
        // arguments computed before the walk.
        std::optional<int> at, lag;
        if (call.subexpr() && limit_ && IsSequence(function) && !ByCells(function)) try {
                lag = Lag(call.subexpr(), name);
            } catch (const Reason&) {
            } catch (const Fix&) {
            }
        if (call.subexpr() && !lag)
            at = Whole(Known(call.subexpr(),
                             "a sequence with parameters read at an index that is not a constant"));
        std::optional<Setting<std::vector<Temporary>*>> based;
        if (lag) based.emplace(temporaries_, limit_->based);
        ++calls_;
        Expansion expansion{{}, {}, outer ? outer->scope : function.home, outer, {}, {}};
        std::vector<Code> nans;
        // An argument the caller's index, give or take a constant, indexes a term (C119).
        const auto bind = [&](const std::string& given, const PExpression<Value>& argument) {
            // A call's value, and its part, written at each reading, would
            // multiply at each call nested in it (C140): each is computed once.
            const int before = calls_;
            Code      value  = Emit(argument);
            bool      nan    = false;
            for (const Cell& c : value.cells) nan = nan || MayNan(c.text) || nans_.count(c.text);
            if ((at || lag || calls_ != before) && !value.constant) value = SharedAll(value);
            if (nan)
                for (const Cell& c : nans.emplace_back(value).cells) nans_.insert(c.text);
            expansion.values.emplace(given, value);
            expansion.written.emplace(given, std::tuple(argument.get(), scope_, expansion_));
            if (Plain(*argument)) try {
                    const Setting<Frame> bound(frame_, frame_);  // its locals bound above, once
                    expansion.offsets.emplace(given, Offset(argument, given));
                } catch (const Reason&) {
                } catch (const Fix&) {
                }
        };
        const std::vector<std::string>& names = p.parameters_names();
        for (std::size_t i = 0; i < call.parameters_expression().size(); ++i)
            bind(names[i], call.parameters_expression()[i]);
        for (const auto& [given, argument] : call.parameters_dict()) bind(given, argument);
        for (const auto& [given, value] : expansion.values) Staircase(function, value);
        Reasoned([&] {
            for (const auto& [given, value] : expansion.values)
                function.Divides(given, Value(value.size), definitions_);
        });
        Defaults(function, expansion);
        based.reset();
        Code result = lag  ? Beside(name, function, expansion, *lag)
                      : at ? Term(name, function, expansion, *at)
                           : Memoized(name, function, expansion, outer);
        // The interpreter computes an argument the function does not read, so
        // its refusal is the call's (C189).
        std::string written, nan;
        for (const Cell& cell : result.cells) written += cell.text + ' ';
        for (const Code& value : nans)
            if (std::none_of(value.cells.begin(), value.cells.end(),
                             [&](const Cell& c) { return Writes(written, c.text); }))
                nan += (aware_ = true, Nan(value));
        if (nan.empty()) return result;
        for (Cell& cell : result.cells) cell = Cell("(" + nan + cell.text + ")", primary);
        result.constant.reset();
        return result;
    }

    // Within a step, or a loop that binds places alone, a sum's or a
    // definition's by its cells, where it declares in another list, a call is
    // one value for the code it is given, unless it reads a term, which
    // depends on where: compiled at each place, a call by cells wrote all its
    // cells at each (C230). Kept by the temporaries it declared.
    // With the statuses of the limits it reads, which a guard calling it asks.
    using Memo = std::pair<const void*, std::map<std::tuple<const void*, std::string, std::string>,
                                                 std::pair<Code, std::set<std::string>>>>;
    Code Memoized(const std::string& name, const Reference<Value>& function, Expansion& expansion,
                  const Expansion* outer) {
        std::set<std::string> asked;
        const auto            compiled = [&] {
            const Setting<std::set<std::string>*> asking(statuses_, &asked);
            return Inside(expansion, [&] { return Chained(name, function); });
        };
        // A call that answered ran out of nothing, whichever limit it read.
        const auto told = [&](Code code) {
            for (const std::string& status : asked)
                if (read_) read_->insert(read_->end(), what_[status].begin(), what_[status].end());
            if (!statuses_ || asked.empty()) return code;
            if (!code.Scalar()) {
                statuses_->insert(asked.begin(), asked.end());
                return code;
            }
            std::string               ran;
            std::set<Compiled::Named> limits;
            for (const std::string& status : asked) {
                Or(ran, status);
                limits.insert(what_[status].begin(), what_[status].end());
            }
            const std::string status = "isnan(" + code.cells[0].text + ") && (" + ran + ")";
            statuses_->insert(status);
            what_[status] = limits;
            return code;
        };
        if (outer || !temporaries_ || memo_.first != temporaries_) return told(compiled());
        const auto key = std::tuple(static_cast<const void*>(&function), grad_, Given(expansion));
        if (const auto found = memo_.second.find(key); found != memo_.second.end()) {
            asked = found->second.second;
            return told(Reread(found->second.first));
        }
        const int situated = situated_;
        Code      code     = compiled();
        if (situated_ == situated) memo_.second.emplace(key, std::pair(code, asked));
        return told(code);
    }

    // A call's value read again is shared where its text is long: written
    // into each reader's, it would multiply as its readers nest, a
    // recurrence by cells exponentially (C357). The bound only caps that
    // growth while compiling; below it Numbered decides, as before.
    Code& Reread(Code& code) {
        for (Code& part : code.part) Reread(part);
        for (Cell& cell : code.cells)
            if (cell.text.size() > 1024) cell = Called(Of(cell)).cells[0];
        return code;
    }

    // The code of each value given, its shape, exact value, cells, moves and parts.
    std::string Given(const Expansion& expansion) {
        std::string args;
        const auto  text = [&](const auto& self, const Code& code) -> void {
            args += code.size.toString() + '\x1d';
            if (code.constant) Value::key(*code.constant, args);
            for (const Cell& cell : code.cells) args += cell.text + '\x1f';
            args += std::to_string(Id(code.moves)) + '\x1e';
            if (code.node) args += std::to_string(tapes_) + '#' + std::to_string(code.node);
            for (const Code& part : code.part) self(self, part);
            for (const Code& outer : code.outer) (args += '\x1c', self(self, outer));
        };
        for (const auto& [given, value] : expansion.values) text(text, value);
        return args;
    }
    // Where a part is there, as a key: one id for what is joined alike, and
    // so for one text, without the text, which writes a condition shared by
    // several paths once for each (C317).
    std::size_t Id(const Where& where) {
        if (!where) return 0;
        if (const auto found = ids_.find(where); found != ids_.end()) return found->second;
        std::vector<std::size_t> any;
        for (const Where& one : where->any) any.push_back(Id(one));
        const auto id = made_.try_emplace({where->text, std::move(any)}, made_.size() + 1);
        return ids_.emplace(where, id.first->second).first->second;
    }

    // Filled from the lowest base, by the interpreter's stride (Reference::Filled),
    // each term once for its arguments' code, shape and exact value (C187),
    // and a temporary once a term reads it; one the fill cannot compute is
    // refused where a read reaches it, as the interpreter's recursion is. A
    // term after the one being computed is computed where it is read, as a
    // call is, so a recursion from the tip unrolls where its guards fold.
    Code Term(const std::string& name, const Reference<Value>& function, Expansion& expansion,
              int k) {
        const std::string args = Given(expansion);
        int& computing = computing_.try_emplace({&function, args}, std::numeric_limits<int>::max())
                             .first->second;
        if (k == computing) throw Reason(name + " is defined by itself");
        const auto term = [&](int j, bool inner) {
            const std::tuple<const void*, const void*, std::string, int> key{temporaries_,
                                                                             &function, args, j};
            // A depth passed is the read's: it fails again as deep, not nearer the top.
            if (const auto failed = failed_.find(key);
                failed != failed_.end() && expanded_ >= failed->second.second)
                std::rethrow_exception(failed->second.first);
            auto found = terms_.find(key);
            if (found == terms_.end()) try {
                    const Setting<int> within(computing, j), deeper(terming_, terming_ + 1);
                    const Setting<std::set<std::string>*> unread(statuses_, nullptr);
                    const Code code = Inside(expansion, [&] { return Chained(name, function, j); });
                    found           = terms_.insert({key, {code, false}}).first;
                    failed_.erase(key);
                } catch (const Reason& reason) {
                    const int deep = dynamic_cast<const Deep*>(&reason) ? expanded_ : 0;
                    failed_.insert_or_assign(key, std::pair(std::current_exception(), deep));
                    throw;
                }
            if (inner && !found->second.second)
                found->second = {SharedAll(found->second.first), true};
            return found->second.first;
        };
        const Clause<Value>* lowest = function.EndBase(true);
        const long long      distance =
            lowest && k < computing ? static_cast<long long>(k) - lowest->parameters.index() : 0;
        if (distance > max_terms)
            throw Reason(name + "_" + std::to_string(k) + " is " + std::to_string(distance) +
                         " terms from its base, and a step writes out at most " +
                         std::to_string(max_terms));
        // Each term once for each depth it is tried at, not at each read: a
        // halving recursion reads each term with arguments of its own.
        const int stride = function.Stride();
        if (distance > 0) {
            const long long first = k - (distance - 1) / stride * stride;
            auto& [upto, deep]    = filled_
                                     .try_emplace({temporaries_, &function, args, k % stride},
                                                  first - stride, expanded_)
                                     .first->second;
            if (expanded_ < deep) std::tie(upto, deep) = std::pair(first - stride, expanded_);
            for (long long j = std::max(first, upto + stride); j < k; j += stride, deep = expanded_)
                try {
                    (void)term(static_cast<int>(j), true);
                } catch (const Reason&) {
                }
            upto = std::max(upto, static_cast<long long>(k) - stride);
        }
        return term(k, terming_ > 0);
    }

    // A default reads the others inside the function, and the sizes stated
    // before it, constants (DESIGN.md, a size bound by a signature).
    void Defaults(const Reference<Value>& function, Expansion& expansion) {
        const ParametersDefinition<Value>& p = function.Clauses().front().parameters;
        typename Reference<Value>::Measure measure(function);
        for (std::size_t i = 0; i < p.parameters_names().size(); ++i) {
            const std::string& parameter = p.parameters_names()[i];
            if (!expansion.values.count(parameter))
                expansion.values.emplace(parameter, Inside(expansion, [&] {
                                             return Emit(p.parameters_dict().at(parameter));
                                         }));
            const Extent given = expansion.values.at(parameter).size;
            for (const auto& [size, n] : Reasoned([&] { return measure(i, given); }))
                expansion.values.emplace(size, Literal(Value(Number(static_cast<int>(n)))));
        }
    }

    // A function of the prelude called on single values is a C function of
    // the header's, emitted once, its parameters named as a limit's are:
    // written where it is called, each of log's thirteen steps would write
    // its argument three times (DESIGN.md). A constant call folds, and one
    // of another shape is written where it is called, as any other.
    PExpression<Value> Prelude(const std::string& name, const Reference<Value>& function,
                               const ParametersCall<Value>& call) {
        const auto& names    = function.Clauses().front().parameters.parameters_names();
        bool        constant = true, scalar = call.parameters_expression().size() == names.size();
        std::string       given, moving;
        std::vector<Code> arguments;
        for (const PExpression<Value>& argument : call.parameters_expression()) {
            const Code& code = arguments.emplace_back(Emit(argument));
            constant         = constant && code.constant;
            scalar           = scalar && code.Scalar();
            given += (given.empty() ? "" : ", ") + code.cells[0].text;
            Staircase(function, code);
        }
        std::set<const Reference<Value>*> seen;
        // Under reverse mode, one of several arguments is written where called:
        // its part function gives one slope, which one argument's node takes.
        const auto parted = [](const Code& c) { return !c.part.empty(); };
        if (constant || !scalar || Iterates(function, seen) || Placed(function) ||
            (tape_ && names.size() > 1 && std::any_of(arguments.begin(), arguments.end(), parted)))
            return Answer(Call(name, function, call, nullptr));
        if (std::ranges::any_of(arguments, [](const Code& c) {
                return !c.outer.empty() || (!c.part.empty() && !c.part[0].outer.empty());
            }))
            return Answer(Nested(name, function, arguments));
        const std::string called = module_ + "_" + name;
        std::string       signature;
        for (const std::string& parameter : names)
            signature += (signature.empty() ? "double arg_" : ", double arg_") + parameter;
        // Its body, its parameters 'arg_x', and with parts 'part_x' for each that moves.
        const auto inside = [&](bool parts) {
            Expansion expansion{{}, {}, function.home, nullptr, {}, {}};
            for (std::size_t i = 0; i < names.size(); ++i) {
                Code argument = Array("arg_" + names[i], Extent{});
                if (parts && !arguments[i].part.empty())
                    argument.part = {Array("part_" + names[i], Extent{})};
                expansion.values.emplace(names[i], argument);
            }
            Defaults(function, expansion);  // the sizes its signature names (C213)
            const Setting<Walked*>                 outside(limit_, nullptr);
            const Setting<std::vector<Temporary>*> unshared(temporaries_, nullptr);
            const Setting<Tape*>                   forward(tape_, nullptr);
            return Inside(expansion, [&] { return Chained(name, function); });
        };
        if (!functions_.count(called)) {
            const Code body = inside(false);
            functions_.emplace(called, body.cells[0].text);
            if (MayNan(body.cells[0].text)) nan_functions_.insert(called);
            prelude_.push_back("static inline double " + called + "(" + signature +
                               ") {\n    return " + body.cells[0].text + ";\n}\n\n");
        }
        ++calls_;  // an argument this is in is computed once, as of a call of the source's (C155)
        Code value = Of(Cell(called + "(" + given + ")", primary));
        for (std::size_t i = 0; i < names.size(); ++i)
            if (!arguments[i].part.empty()) moving += names[i];
        if (moving.empty()) return Answer(value);
        std::vector<const Code*> read;
        for (const Code& argument : arguments) read.push_back(&argument);
        // Under grad, its part is a function too, 'exp_dx', of the values its
        // part reads and then the parts (DESIGN.md, grad compiled).
        const std::string derived = called + "_d" + moving, jumped = called + "_j" + moving;
        const std::string present = called + "_m" + moving;
        if (!parts_.count(derived)) {
            const Code body = inside(true);
            // Where its part is there, 'max_ma', its clause's, as the
            // interpreter takes it, not wherever an argument's is (C388).
            if (body.moves) {
                const Setting<std::vector<Temporary>*> unshared(temporaries_, nullptr);
                functions_.emplace(present, Text(body.moves));
                prelude_.push_back("static inline int " + present + "(" + signature +
                                   ") {\n    return " + functions_.at(present) + ";\n}\n\n");
            }
            // Its value where an argument moves, 'mod_ja', where that tests
            // for a jump, a floor's or an equality's, as the interpreter does.
            if (body.cells[0].text != functions_.at(called)) {
                functions_.emplace(jumped, body.cells[0].text);
                if (MayNan(body.cells[0].text)) nan_functions_.insert(jumped);
                prelude_.push_back("static inline double " + jumped + "(" + signature +
                                   ") {\n    return " + body.cells[0].text + ";\n}\n\n");
            }
            std::optional<std::vector<std::pair<bool, std::size_t>>> takes;
            if (!body.part.empty()) {
                const std::string& text = body.part[0].cells[0].text;
                std::string        parameters;
                takes.emplace();
                for (const bool part : {false, true})
                    for (std::size_t i = 0; i < names.size(); ++i) {
                        const std::string taken = (part ? "part_" : "arg_") + names[i];
                        if (!Writes(text, taken)) continue;
                        takes->emplace_back(part, i);
                        parameters += (parameters.empty() ? "double " : ", double ") + taken;
                    }
                prelude_.push_back("static inline double " + derived + "(" + parameters +
                                   ") {\n    return " + text + ";\n}\n\n");
            }
            parts_.emplace(derived, takes);
        }
        if (functions_.count(jumped)) {
            aware_ = true;  // a jump's NaN reaches a grad that drops this value (C113)
            const std::string moves = Text(Moves(read)), jumps = jumped + "(" + given + ")";
            const std::string plain = value.cells[0].text;
            value.cells[0].text =
                moves.empty() ? jumps : "(" + moves + " ? " + jumps + " : " + plain + ")";
        }
        const auto& takes = parts_.at(derived);
        value             = Called(value);  // first: compilers order arguments apart
        if (!takes) return Answer(value);
        // Under reverse mode, at a part of 1, its slope, which its node
        // multiplies by.
        std::string taken;
        for (const auto& [part, i] : *takes)
            taken += (taken.empty() ? "" : ", ") +
                     (part && tape_ ? Double(1.0)
                                    : (part ? arguments[i].part[0] : arguments[i]).cells[0].text);
        const Code part = Called(Of(Cell(derived + "(" + taken + ")", primary)));
        value = Parted(value, tape_ ? Times(part, PartOf(arguments[0])) : Part(part), read);
        if (functions_.count(present))
            value.moves =
                Both(value.moves,
                     std::make_shared<const Moving>(Moving{present + "(" + given + ")", {}}));
        return Answer(value);
    }

    // In a nest, its body compiled once with each argument's value and parts
    // as parameters, 'arg_x_p' and 'arg_x_o' by the inner and outer grads,
    // and its value and each part a function of the header of those it reads.
    Code Nested(const std::string& name, const Reference<Value>& function,
                const std::vector<Code>& arguments) {
        std::vector<std::pair<std::string, std::string>> leaves;  // a parameter, and its argument
        const auto mirror = [&](auto& self, const Code& given, const std::string& as) -> Code {
            Code code = Array(as, Extent{});
            leaves.emplace_back(as, given.cells[0].text);
            if (!given.part.empty()) code.part = {self(self, given.part[0], as + "_p")};
            if (!given.outer.empty()) code.outer = {self(self, given.outer[0], as + "_o")};
            return code;
        };
        const auto& names = function.Clauses().front().parameters.parameters_names();
        Expansion   expansion{{}, {}, function.home, nullptr, {}, {}};
        for (std::size_t i = 0; i < names.size(); ++i)
            expansion.values.emplace(names[i], mirror(mirror, arguments[i], "arg_" + names[i]));
        Defaults(function, expansion);
        Code body;
        {
            const Setting<Walked*>                 outside(limit_, nullptr);
            const Setting<std::vector<Temporary>*> unshared(temporaries_, nullptr);
            body = Inside(expansion, [&] { return Chained(name, function); });
        }
        const auto called = [&](const std::string& text, const std::string& type) {
            std::string parameters, given;
            for (const auto& [parameter, argument] : leaves)
                if (Writes(text, parameter)) {
                    parameters += (parameters.empty() ? "double " : ", double ") + parameter;
                    given += (given.empty() ? "" : ", ") + argument;
                }
            const std::string key = name + '\x1f' + parameters + '\x1f' + text;
            if (!nested_.count(key)) {
                nested_[key] = module_ + "_" + name + "_n" + std::to_string(nested_.size());
                if (type == "double" && MayNan(text)) nan_functions_.insert(nested_[key]);
                prelude_.push_back("static inline " + type + " " + nested_[key] + "(" + parameters +
                                   ") {\n    return " + text + ";\n}\n\n");
            }
            return nested_[key] + "(" + given + ")";
        };
        const auto out = [&](auto& self, const Code& node) -> Code {
            if (node.constant) return node;
            Code code = Called(Of(Cell(called(node.cells[0].text, "double"), primary)));
            if (!node.part.empty()) code.part = {self(self, node.part[0])};
            if (!node.outer.empty()) code.outer = {self(self, node.outer[0])};
            return code;
        };
        ++calls_;
        std::vector<const Code*> read;
        for (const Code& argument : arguments) read.push_back(&argument);
        // Where each part is there, its clause's, not wherever an argument's
        // is (C388).
        const auto there = [&](const Where& inside, const Where& given) {
            if (!inside) return given;
            const Setting<std::vector<Temporary>*> unshared(temporaries_, nullptr);
            return Both(given,
                        std::make_shared<const Moving>(Moving{called(Text(inside), "int"), {}}));
        };
        Code code = out(out, body);
        if (!code.part.empty()) code.moves = there(body.moves, Moves(read));
        if (!code.outer.empty())
            code.outer[0].moves = there(body.outer[0].moves, Moves(read, true));
        return code;
    }

    // Where its argument moves, a temporary, which every part over it reads
    // again (C140), a name that may be NaN where the call may (C189, C211).
    Code Called(Code code) {
        const bool nan = MayNan(code.cells[0].text);
        code           = Shared(code);
        if (nan && code.cells[0].atom) nan_temporaries_.insert(code.cells[0].text);
        return code;
    }

    // Whether an iterate's value does not move as it walks, so is computed
    // before the loop (DESIGN.md, a limit's invariants hoisted); a call to a
    // function with a loop, which GCC and Clang leave in it, is shared so.
    bool Invariant(const std::string& value) {
        return limit_ && temporaries_ == limit_->iterated && !Walks(value);
    }
    Code Looping(Code code) { return Invariant(code.cells[0].text) ? Called(code) : code; }

    // Whether a function of the prelude reads a cell at one of its
    // parameters, which a function of the header, its parameters doubles,
    // cannot place: it is written where it is called, its places constants
    // there (C356).
    static bool Placed(const Reference<Value>& function) {
        const auto& names = function.Clauses().front().parameters.parameters_names();
        const auto  reads = [&](const auto& self, const PExpression<Value>& e, bool place) -> bool {
            if (!e) return false;
            if (place && std::find(names.begin(), names.end(), e->Name()) != names.end())
                return true;
            const bool cell = dynamic_cast<const CellExpression<Value>*>(e.get());
            for (std::size_t i = 0; i < e->Children().size(); ++i)
                if (self(self, e->Children()[i], place || (cell && i > 0))) return true;
            return false;
        };
        return std::any_of(function.Clauses().begin(), function.Clauses().end(),
                           [&](const Clause<Value>& c) {
                               return reads(reads, c.expression, false) ||
                                      reads(reads, c.parameters.guard(), false);
                           });
    }

    // Whether a function of the prelude reads a sequence with parameters,
    // itself or through what it calls, which its function in the header
    // cannot hold: it is written where it is called, as of a matrix (C213).
    bool Iterates(const Reference<Value>& function, std::set<const Reference<Value>*>& seen) {
        const auto reads = [&](const auto& self, const PExpression<Value>& e) -> bool {
            const auto* call = dynamic_cast<const FuncExpression<Value>*>(e.get());
            if (call && call->m_e2()) return true;
            const auto found =
                call ? function.home->names.find(call->Name()) : function.home->names.end();
            if (found != function.home->names.end() && seen.insert(found->second.get()).second &&
                Iterates(*found->second, seen))
                return true;
            return e && std::any_of(e->Children().begin(), e->Children().end(),
                                    [&](const PExpression<Value>& c) { return self(self, c); });
        };
        return std::any_of(
            function.Clauses().begin(), function.Clauses().end(), [&](const Clause<Value>& c) {
                return reads(reads, c.expression) || reads(reads, c.parameters.guard());
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
        Expansion expansion{{}, {}, m.scope, nullptr, {}, {}};
        for (std::size_t i = 0; i < m.parameters.size(); ++i) {
            if (!bound[i]) continue;
            const Code& given =
                expansion.values.emplace(m.parameters[i].name, Emit(bound[i]->expression))
                    .first->second;
            if (!Still(given)) throw Reason("grad cannot differentiate through an instance yet");
        }
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
                if (scope == &root_ || !definition.passes.empty()) return {nullptr, scope, key};
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

    // An input marked optional, the model compiled's or an inner instance's (DESIGN.md).
    bool Optional(const std::string& key) const {
        const auto inner = sequences_.find(key);
        if (inner != sequences_.end() && inner->second.definition)
            return !inner->second.definition->passes.empty();
        return model_ && std::ranges::any_of(model_->parameters, [&](const auto& p) {
                   return p.optional && p.name == key;
               });
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
                const Reference<Value>& input = *root_.names.at(parameter.name);
                sequences_[parameter.name].size =
                    input.declared.empty() ? Extent{} : Stated(parameter.name, input);
            } else if (parameter.index.empty()) {
                try {
                    (void)Emit(std::make_shared<RefExpression<Value>>(parameter.name));
                } catch (const Reason& reason) {
                    throw Refusal(parameter.name, reason.what());
                }
            }
        }
    }

    // An input's size as its model states it (DESIGN.md, C83), read where it is.
    Extent Stated(const std::string& key, const Reference<Value>& input) {
        try {
            return input.Stated([&](const PExpression<Value>& bound) { return Size(bound); });
        } catch (const Reason& reason) {
            throw Refusal(key, reason.what());
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
            // A function is compiled where it is called, and a sequence with
            // parameters where a limit walks it.
            if (!clause.parameters.parameters_names().empty()) return;
            const ParametersDefinition<Value>& p = clause.parameters;
            if (p.guarded() && !p.cells() && !p.general())
                throw Refusal(name,
                              std::string("a guarded ") + (p.indexed() ? "base clause" : "value"));
        }
        // A base clause written after a guarded one is reached only if the guard
        // fails, which the chain a step computes would not say. A term by its
        // cells is the exception: its base comes first, however it was written.
        bool guarded = false;
        for (const Clause<Value>& clause : definition.Clauses()) {
            if (ByCells(definition)) break;
            guarded = guarded || (clause.parameters.guarded() && clause.parameters.general());
            if (guarded && clause.parameters.indexed() && !clause.parameters.general())
                throw Refusal(name, "a base clause after a guarded one");
        }
        if (!IsSequence(definition)) return;
        std::vector<std::string> parts;
        if (const auto* group = ReferenceStack<Value>::Group(definition))
            for (const std::string& v : group->Variables()) parts.push_back(name + "." + v);
        for (const std::string& key : parts.empty() ? std::vector{name} : parts) {
            sequences_[key].definition = &definition;
            sequences_[key].parts      = parts;
        }
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
                if (!history.declared.empty() && code.size != Stated(key, history))
                    throw Reason("a history of another shape");
                int& reach = reach_.try_emplace(key, top).first->second;
                reach      = std::max(reach, top);
            }
        } catch (const Reason& reason) {
            throw Refusal(key, reason.what());
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
        // Nor the reader's guard, which defers what it reads (C92).
        const bool outer_deferring  = std::exchange(deferring_, false);
        const Setting<Memo>  memo(memo_, {temporaries_, {}});
        const Setting<Frame> own(frame_, {});
        auto       outer_parameters = std::exchange(read_parameters_, {});
        const bool          outer_approximated = std::exchange(approximated_, false);
        try {
            body();
        } catch (const Reason& reason) {
            throw Refusal(Refused(name), reason.what());
        }
        if (std::exchange(approximated_, outer_approximated)) approximated_in_.insert(name);
        if (Rounded(sequences_.at(name))) rounded_in_.insert(name);
        sequences_.at(name).parameters.insert(read_parameters_.begin(), read_parameters_.end());
        read_parameters_.insert(outer_parameters.begin(), outer_parameters.end());
        reading_     = outer_reading;
        index_       = outer_index;
        within_      = outer_within;
        temporaries_ = outer_temporaries;
        places_      = outer_places;
        shift_       = outer_shift;
        deferring_   = outer_deferring;
    }

    static bool Rounded(const Sequence& s) {
        std::string text = s.present;
        for (const auto& [n, cells] : s.bases) text += Joined(cells);
        for (const Guarded& g : s.guarded) text += g.condition + Joined(g.cells);
        for (const Temporary& t : s.temporaries) text += t.value + Joined(t.lines);
        return (text + Joined(s.general)).find(rounded) != std::string::npos;
    }
    static std::string Joined(const std::vector<std::string>& lines) {
        std::string joined;
        for (const std::string& line : lines) joined += line;
        return joined;
    }

    // A cell the compiler would write out more than once -- an operand of a
    // matrix product, a single value stretched over a matrix -- is computed once
    // per step instead, into a temporary: the same value, in fewer lines; a
    // name that may be NaN where the value may, as Called's (C365).
    Cell Shared(const Cell& cell) {
        if (cell.atom || !temporaries_) return cell;
        const std::string name = Declare(cell.text, [&](const std::string& t) {
            return std::vector<std::string>{"const double " + t + " = " + cell.text + ";"};
        });
        if (MayNan(cell.text)) nan_temporaries_.insert(name);
        return Atom(name);
    }

    // The same value is the same temporary within one sequence's step.
    template <typename Lines>
    std::string Declare(const std::string& value, Lines lines) {
        const Setting<std::vector<Temporary>*> hoisted(
            temporaries_, Invariant(value) ? limit_->based : temporaries_);
        // Checked, as an ended list may have left its address to another.
        std::size_t& at = declared_[{temporaries_, value}];
        if (at < temporaries_->size() && (*temporaries_)[at].value == value)
            return (*temporaries_)[at].name;
        // A limit's own are numbered within its function, so that a limit
        // written again is the same text; a global's read there are not.
        const bool own =
            limit_ && (temporaries_ == limit_->based || temporaries_ == limit_->iterated);
        const std::string name = own ? "v" + std::to_string(limit_->count++) + "_"
                                     : "t" + std::to_string(temporary_count_++) + "_";
        at                     = temporaries_->size();
        temporaries_->push_back({name, value, lines(name)});
        return name;
    }
    Code Shared(Code code) {
        for (Cell& cell : code.cells) cell = Shared(cell);
        return code;
    }
    // Its parts too (C140).
    Code SharedAll(Code code) {
        for (Code& part : code.part) part = SharedAll(part);
        return Shared(code);
    }

    Code Broadcast(Code left, Code right, Cell (*combine)(const Cell&, const Cell&)) {
        if (!right.Scalar() && (left.Scalar() || left.size.slices < right.size.slices))
            left = Shared(left);
        if (!left.Scalar() && (right.Scalar() || right.size.slices < left.size.slices))
            right = Shared(right);
        Code code = Cellwise(left, right, combine);
        if (left.outer.empty() && right.outer.empty()) return code;
        // In a nest, its part by the outer grad: a^ + b^, a^*b + a*b^ or
        // (a^ - q*b^)/b, as Derivative's Sum, Product and Quotient.
        const Part a = Inner(left), b = Inner(right), l = OuterOf(left), r = OuterOf(right);
        const auto times = [&](const Part& x, const Part& y) -> Part {
            if (!x || !y || x->Scalar() || y->Scalar()) return Times(x, y);
            return Broadcast(*x, *y, Multiplied);
        };
        Part d = combine == Added        ? Plus(l, r)
                 : combine == Multiplied ? Plus(times(l, b), times(a, r))
                                         : Plus(l, r ? Part(Negative(*times(code, r))) : r);
        if (combine == Divided)
            d = Combined(
                d, b, [&](const Code& x, const Code& y) { return Broadcast(x, y, Divided); },
                std::divides<>());
        if (combine == Added && d->size != code.size) d = Plus(d, Literal(Value(code.size)));
        code.outer          = {*d};
        code.outer[0].moves = Moves({&left, &right}, true);
        return code;
    }

    void Shape(Sequence& sequence, const Code& code) {
        if (sequence.size.rows && code.size != sequence.size)
            throw Reason("its clauses have different shapes");
        sequence.size = code.size;
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
                const Setting<Frame> own(frame_, {});
                Sequence* const      outer = std::exchange(basing_, &sequence);
                const int       base  = std::exchange(base_, clause.parameters.index());
                const Code      code  = Emit(clause.expression);
                basing_               = outer;
                base_                 = base;
                Shape(sequence, code);
                sequence.bases[clause.parameters.index()] = Texts(code);
            }
        });
    }

    // What a refusal names: a part's grad, the definition, which is what is set
    // aside.
    std::string Refused(const std::string& name) const {
        if (const auto total = totals_.find(name); total != totals_.end())
            return total->second.reader;
        return sequences_.at(name).parts.empty() ? name : name.substr(0, name.rfind('.'));
    }

    // In turn, or earlier where a reader needs a shape no base clause gives.
    void Compile(const std::string& name) {
        Sequence& sequence = sequences_.at(name);
        if (sequence.compiled || !sequence.definition) return;
        const Home home(*this, sequence.definition);
        if (sequence.compiling) {
            for (const auto& [key, other] : sequences_)
                if (other.compiling && other.size.rows && !first_.count(key)) throw First{key};
            throw Refusal(Refused(name), "its shape depends on itself");
        }
        sequence.compiling = true;
        for (const std::string& part : sequence.parts) sequences_.at(part).compiling = true;
        Bases(name);
        if (!sequence.parts.empty()) return Grouped(sequence.parts);
        if (ByCells(*sequence.definition)) {
            CompileTerms(name, sequence);
            sequence.compiled = true;
            return;
        }
        Reads* const outer_reads = clause_reads_;
        // The guarded clauses in the order written, then the unguarded one, as
        // the interpreter tries them; a guard that always holds ends the chain.
        // One reading its own term where no base clause gives a shape takes the
        // unguarded one's, compiled first (C377).
        const auto& clauses = sequence.definition->Clauses();
        const bool  shaped =
            !sequence.size.rows &&
            (totals_.count(name) || std::ranges::any_of(clauses, [&](const auto& c) {
                 const std::string& own = sequence.definition->Name();
                 return c.parameters.guarded() &&
                        (Mentions(*c.expression, own) || Mentions(*c.parameters.guard(), own));
             }));
        bool       settled = false, general = false;
        for (const bool guarded : {!shaped, shaped}) {
            for (const Clause<Value>& clause : clauses) {
                const ParametersDefinition<Value>& p = clause.parameters;
                if (settled || (general && !guarded) || !p.general() || p.guarded() != guarded)
                    continue;
                Within(name, &sequence, p.index_name(), [&] {
                    // The index as the step has it, known once the rate is.
                    const std::string outer_text =
                        std::exchange(index_text_, "(double)\x12" + name + "\x13");
                    Reads guard_reads, value_reads;
                    clause_reads_ = &guard_reads;
                    const std::optional<std::string> condition =
                        guarded ? Condition(p.guard()) : std::optional<std::string>("");
                    if (!condition) return;
                    clause_reads_ = &value_reads;
                    std::set<std::string>                 asked;
                    const bool                            flagged = Optional(name);
                    const Setting<std::set<std::string>*> giving(given_, flagged ? &asked : given_),
                        asking(statuses_, flagged ? &asked : statuses_);
                    const Code code = Emit(clause.expression);
                    // Present where every input and chain it reads is, whatever its limits.
                    for (const std::string& status : asked)
                        if (status.starts_with("!m_->"))
                            sequence.present +=
                                (sequence.present.empty() ? "" : " && ") + status.substr(1);
                    clause_reads_ = nullptr;
                    Shape(sequence, code);
                    const int place =
                        static_cast<int>(&clause - sequence.definition->Clauses().data());
                    if (condition->empty()) {
                        sequence.general        = Texts(code);
                        sequence.general_reads  = value_reads;
                        sequence.general_clause = place;
                        settled                 = guarded;
                        general                 = true;
                    } else {
                        sequence.guarded.push_back({*condition, Texts(code), guard_reads,
                                                    value_reads, 0, 0, place,
                                                    code.constant.has_value()});
                    }
                    index_text_ = outer_text;
                });
            }
        }
        if (sequence.definition->input) {
            if (!sequence.size.rows) sequence.size = Extent{};
            sequence.present = "0";
        } else if (!general && sequence.guarded.empty())
            throw Refusal(name, "a sequence with no general clause");
        // Where no guard holds the interpreter says so; a step can only say NaN.
        if (!general) sequence.general.assign(sequence.size.count(), "NAN");
        clause_reads_     = outer_reads;
        sequence.compiled = true;
        // An input given a term of another size than its model states.
        if (const auto input = stated_.find(name); input != stated_.end()) {
            const Home   there(*this, input->second);
            const Extent stated = Stated(name, *input->second);
            if (sequence.size != stated)
                throw Refusal(name, input->second->Unlike(sequence.size, stated));
        }
    }

    // A grad by several names (DESIGN.md), its parts from one walk and one
    // sweep, in the first's step.
    void Grouped(const std::vector<std::string>& parts) {
        Sequence&            first  = sequences_.at(parts[0]);
        const Clause<Value>& clause = first.definition->Clauses().front();
        std::vector<Code>    codes;
        Within(parts[0], &first, clause.parameters.index_name(), [&] {
            const Setting<std::string> text(index_text_, "(double)\x12" + parts[0] + "\x13");
            const Setting<Reads*>      reads(clause_reads_, &first.general_reads);
            const Setting<std::pair<const GradExpression<Value>*, std::vector<Code>*>> all(
                grouped_, {ReferenceStack<Value>::Group(*first.definition), &codes});
            (void)Emit(clause.expression);
        });
        first.compiled = true;
        for (std::size_t k = parts.size(); k-- > 0;) {
            Sequence& part = sequences_.at(parts[k]);
            if (k) {
                part = first;
                part.temporaries.clear();
            }
            part.size    = codes[k].size;
            part.general = Texts(codes[k]);
        }
    }

    Code Emit(const PExpression<Value>& expression) {
        if (const auto lifted = lifted_.find(expression.get()); lifted != lifted_.end())
            return lifted->second;
        expression->accept(*this);
        return code_;
    }

    // A value, and the parameters it reads.
    std::pair<Code, std::set<std::string>> Reading(const PExpression<Value>& expression) {
        const Setting<Memo> unseen(memo_, {});  // a call kept outside would not say what it reads
        auto       outer = std::exchange(read_parameters_, {});
        const Code code  = Emit(expression);
        auto       reads = std::exchange(read_parameters_, std::move(outer));
        read_parameters_.insert(reads.begin(), reads.end());
        return {code, reads};
    }

    // A value the compiled code needs as a constant; see Fix.
    Code Known(const PExpression<Value>& expression, const std::string& refusal) {
        auto [code, reads] = Reading(expression);
        if (code.constant && code.part.empty()) return code;
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
        moved_ = moved_ || !code.part.empty();
        code_  = std::move(code);
        return PExpression<Value>();
    }
    PExpression<Value> Answer(Cell cell) {
        Code code;
        code.cells = {std::move(cell)};
        return Answer(std::move(code));
    }

    // The interpreter, where the expression is read: a cell's names and what
    // a call gives are locals there, and a function it calls is its own. With
    // 'every', those that are not constants too, for what asks only whether a
    // name is a local.
    template <typename F>
    auto Asked(F f, bool every = false) {
        definitions_.BeginEvaluation();
        const typename ReferenceStack<Value>::Within within(definitions_, scope_);
        const typename ReferenceStack<Value>::Frame  frame(definitions_);
        std::vector<const std::map<std::string, Code>*> calls{&frame_.values};  // the locals last
        for (const Expansion* call = expansion_; call; call = call->outer)
            calls.push_back(&call->values);
        for (auto call = calls.rbegin(); call != calls.rend(); ++call)
            for (const auto& [name, code] : **call)
                if ((code.constant || every) &&
                    (*call != &frame_.values || !frame_.graded.count(name)))
                    definitions_.BindValue(name, code.constant ? *code.constant : Value(Number(0)));
        for (const auto& [name, value] : places_) definitions_.BindValue(name, value);
        if (every && !index_.empty()) definitions_.BindValue(index_, Value(Number(0)));
        return f();
    }
    // Exactly, as the interpreter would, and rounded once: 0.1 + 0.2 is 3/10
    // here, which a C compiler folding the doubles would not find.
    PExpression<Value> Fold(Expression<Value>* expression) {
        return Asked([&] {
            EvaluationVisitor<Value> evaluator(definitions_);
            try {
                return Answer(Literal(expression->accept(evaluator)));
            } catch (const Reason&) {
                throw;
            } catch (const std::runtime_error& error) {
                throw Undefined(error.what());
            }
        });
    }
    // From the operands' constants, not the expression again, which may read
    // a name that moves where its value does not, in a call (C135).
    template <typename F, typename... Operands>
    static Code Exactly(F f, const Operands&... operands) {
        return Exact([&] { return f(*operands.constant...); });
    }

    static std::vector<double> Doubles(const Value& value) {
        approximated_ = approximated_ || Value::approximated(value);
        std::vector<double> doubles;
        for (std::size_t c = 0; c < value.Size().count(); ++c) {
            const auto number = value.data()[c].Inexact();
            if (number.imag() != 0) throw Reason("a complex number");
            doubles.push_back(number.real());
        }
        return doubles;
    }

    static Code Literal(const Value& value) {
        Code code;
        code.size     = value.Size();
        code.constant = value;
        const std::vector<double> doubles = Doubles(value);
        for (std::size_t c = 0; c < doubles.size(); ++c) {
            const double x    = doubles[c];
            Cell         cell = Atom(Double(std::abs(x)), !std::isnan(x));
            // Folded in double from inexact numbers, where a float step may compute
            // another (C396): marked, for Within to find written.
            if (floats && !Number::exact(value.data()[c]) && x != 0 && Finite(x))
                cell.text += rounded;
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
        if (!Finite(x)) return std::signbit(x) ? "-INFINITY" : "INFINITY";
        char        text[32];
        const auto end = floats ? std::to_chars(text, text + sizeof text, static_cast<float>(x)).ptr
                                : std::to_chars(text, text + sizeof text, x).ptr;
        std::string written(text, end);
        if (written.find_first_of(".e") == std::string::npos) written += ".0";
        return written + (floats ? "f" : "");
    }

    // Cell by cell, a single value stretching to the other's shape and a
    // matrix to a tensor's slices, as the interpreter's arithmetic does.
    template <typename Combine>
    static Code Cellwise(const Code& left, const Code& right, Combine combine) {
        const std::size_t slices = Slices(left, right);
        if (!left.Scalar() && !right.Scalar() &&
            (left.size.rows != right.size.rows || left.size.cols != right.size.cols))
            throw Mismatched(left, right, Extent::Unmet(left.size, right.size));
        Code code;
        code.size        = left.Scalar() ? right.size : left.size;
        code.size.slices = slices;
        for (std::size_t c = 0; c < code.size.count(); ++c)
            code.cells.push_back(combine(left.At(c), right.At(c)));
        return code;
    }

    // The interpreter takes each slice of a tensor of single values as one
    // value over a matrix, which the compiler does not yet.
    static Reason Mismatched(const Code& a, const Code& b, const std::string& words) {
        for (const Code* c : {&a, &b})
            if (c->size.slices && c->size.count() == c->size.slices)
                return Reason("a tensor whose slices are single values met by a matrix, for now");
        return Reason(words);
    }

    // How many slices a tensor met slice by slice gives, as Matrix::Sliced counts them.
    static std::size_t Slices(const Code& a, const Code& b) {
        if (a.size.slices && b.size.slices && a.size.slices != b.size.slices)
            throw Reason("a " + a.size.Described() + " and a " + b.size.Described() +
                         " have different numbers of slices");
        return std::max(a.size.slices, b.size.slices);
    }

    // A negative number written is added as a subtraction, the same in IEEE
    // arithmetic, and reads as what was written. A negation computed is 0 - b
    // and added as such, as the interpreter's: -0 - +0 is -0, -0 + (0 - +0)
    // +0 (C109).
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
    static Code Of(Cell cell) {
        Code code;
        code.cells = {std::move(cell)};
        return code;
    }

    // grad's arithmetic on parts (DESIGN.md, grad compiled), as Derivative's,
    // a part absent where its is, and exactly where both are constants.
    using Part = std::optional<Code>;
    static Part PartOf(const Code& code) { return code.part.empty() ? Part() : code.part[0]; }
    static Part OuterOf(const Code& code) { return code.outer.empty() ? Part() : code.outer[0]; }
    static bool Still(const Code& code) { return code.part.empty() && code.outer.empty(); }
    static Code Inner(Code code) {
        code.outer.clear();
        return code;
    }
    // What lays out cells lays out their parts by the outer grad of a nest
    // too, 0 where one has none.
    template <typename Lay>
    static Code Outer(Code code, const std::vector<Code>& from, Lay lay) {
        if (std::ranges::all_of(from, [](const Code& c) { return c.outer.empty(); })) return code;
        std::vector<Code>        outers;
        std::vector<const Code*> each;
        for (const Code& c : from) {
            outers.push_back(c.outer.empty() ? Literal(Value(c.size)) : c.outer[0]);
            each.push_back(&c);
        }
        code.outer          = {lay(outers)};
        code.outer[0].moves = Moves(each, true);
        return code;
    }
    template <typename F>
    static Code Exact(F f) {
        try {
            return Literal(f());
        } catch (const Reason&) {
            throw;
        } catch (const std::runtime_error& error) {
            throw Undefined(error.what());
        }
    }
    template <typename Written, typename Exactly>
    static Part Combined(const Part& a, const Part& b, Written written, Exactly exactly) {
        if (!a || !b) return std::nullopt;
        if (a->constant && b->constant)
            return Exact([&] { return exactly(*a->constant, *b->constant); });
        return written(*a, *b);
    }
    // A node of the tape, the same for the same operation on the same nodes
    // and values; a value, a temporary the sweep reads.
    Code Record(Kind kind, Extent extent, std::size_t a, std::optional<Code> value = {},
                std::size_t b = 0, std::size_t at = 0, std::vector<Origin> from = {}) {
        if (value && !value->constant) value = Shared(*value);
        std::string key = std::to_string(static_cast<int>(kind)) + ' ' + extent.toString() + ' ' +
                          std::to_string(a) + ' ' + std::to_string(b) + ' ' + std::to_string(at);
        for (const auto& [n, c] : from) key += ' ' + std::to_string(n) + ':' + std::to_string(c);
        if (value && value->constant) Value::key(*value->constant, key += '\x1d');
        for (const Cell& c : value ? value->cells : std::vector<Cell>()) key += '\x1f' + c.text;
        const auto [found, made] = tape_->made.try_emplace(key, tape_->nodes.size() + 1);
        if (made)
            tape_->nodes.push_back({kind, extent, a, b, at, std::move(value), std::move(from)});
        Code node;
        node.size = extent;
        node.node = found->second;
        return node;
    }
    // Parts laid out by what only moves cells, a node's cells gathered from
    // where each lands, written as an atom '@k' (Tape::Moved).
    template <typename Lay>
    Code Moved(std::vector<Code> parts, Lay lay) {
        std::vector<Origin> all;
        for (Code& part : parts) {
            if (!part.node) continue;
            const std::size_t node = std::exchange(part.node, 0);
            for (std::size_t k = 0; k < part.size.count(); ++k) {
                part.cells.push_back(Atom("@" + std::to_string(all.size())));
                all.emplace_back(node, k);
            }
        }
        const Code laid = lay(parts);
        if (all.empty()) return laid;
        std::vector<Origin> from(laid.cells.size());
        for (std::size_t k = 0; k < from.size(); ++k)
            if (laid.cells[k].text.starts_with("@"))
                from[k] = all[std::stoul(laid.cells[k].text.substr(1))];
        return Record(Kind::Gather, laid.size, 0, {}, 0, 0, std::move(from));
    }
    Part Plus(const Part& a, const Part& b) {
        if (!a || !b) return a ? a : b;
        if (a->node || b->node)
            return Record(Kind::Sum, Extent::Wider(a->size, b->size), a->node, {}, b->node);
        return Combined(
            a, b, [&](const Code& x, const Code& y) { return Broadcast(x, y, Added); },
            std::plus<>());
    }
    Part Times(const Part& a, const Part& b) {
        if (a && b && (a->node || b->node))
            return a->node ? Record(Kind::Right, Extent::Product(a->size, b->size), a->node, *b)
                           : Record(Kind::Left, Extent::Product(a->size, b->size), b->node, *a);
        return Combined(
            a, b, [&](const Code& x, const Code& y) { return Product(x, y); }, std::multiplies<>());
    }
    template <typename Exactly, typename Written>
    Part Mapped(const Part& part, Exactly exactly, Written written) {
        if (part && part->node)
            return Moved({*part}, [&](const std::vector<Code>& p) { return written(p[0]); });
        if (part && part->constant) return Exact([&] { return exactly(*part->constant); });
        return part ? Part(written(*part)) : part;
    }
    // Where any of these has its part: always, where one always has.
    // Of their parts by the outer grad of a nest with 'outer', each kept in
    // its own 'moves'.
    static Where Moves(const std::vector<const Code*>& codes, bool outer = false) {
        std::vector<Where> any;
        for (const Code* code : codes) {
            const std::vector<Code>& part = outer ? code->outer : code->part;
            if (part.empty()) continue;
            const Where& where = outer ? part[0].moves : code->moves;
            if (!where) return nullptr;
            any.push_back(where);
        }
        return any.empty() ? nullptr : std::make_shared<const Moving>(Moving{"", std::move(any)});
    }
    // Where any of several is, a flag of the list: a sweep's sum nests a
    // condition for each unit, which each cell it reaches wrote whole (DESIGN.md).
    std::string Text(const Where& where) {
        if (!where || where->any.empty()) return where ? where->text : "";
        std::string           text;
        std::set<std::string> seen;
        for (const Where& one : where->any)
            if (const std::string t = Text(one); seen.insert(t).second)
                text += (text.empty() ? "(" : " || (") + t + ")";
        if (!temporaries_ || seen.size() == 1) return text;
        return Declare("moves\x1f" + text, [&](const std::string& t) {
            return std::vector<std::string>{"const int " + t + " = " + text + ";"};
        });
    }
    // As the condition of a '?': a clause's own, itself one, bracketed (C235).
    std::string Whether(const Where& where) {
        return where->any.empty() ? "(" + where->text + ")" : Text(where);
    }
    // 0 where 'where' does not hold, as a part absent there.
    Code Unless(Code code, const Where& where) {
        if (!where) return code;
        for (Cell& cell : code.cells)
            cell = Cell("(" + Whether(where) + " ? " + cell.text + " : 0)", primary);
        code.constant.reset();
        return code;
    }
    static Code Parted(Code value, const Part& part, const std::vector<const Code*>& from) {
        value.part.clear();
        value.moves.reset();
        if (part) value.moves = Moves(from);
        if (part) value.part.push_back(*part);
        return value;
    }
    // A sum's part has its value's shape, which adding to more cells widens (C81).
    Code SumOf(Code value, const Code& a, const Code& b) {
        Part part = Plus(PartOf(a), PartOf(b));
        if (part && a.size != b.size && part->size != value.size)
            part = Plus(part, Literal(Value(value.size)));
        return Parted(std::move(value), part, {&a, &b});
    }
    Code ProductOf(Code value, const Code& a, const Code& b) {
        Part part = Plus(Times(PartOf(a), b), Times(a, PartOf(b)));
        // In a nest, its cross part in the interpreter's order, which the
        // part's own by the outer grad is not (Derivative::Product).
        if (part && !part->node && !part->outer.empty()) {
            const Part p = PartOf(a), q = PartOf(b);
            const Part in[] = {p ? Part(Inner(*p)) : p, q ? Part(Inner(*q)) : q};
            part->outer.clear();
            Part cross;
            for (const Part& term :
                 {Times(p ? OuterOf(*p) : p, Inner(b)), Times(in[0], OuterOf(b)),
                  Times(OuterOf(a), in[1]), Times(Inner(a), q ? OuterOf(*q) : q)})
                cross = Plus(cross, term);
            if (cross) part->outer = {*cross};
        }
        return Parted(std::move(value), part, {&a, &b});
    }
    // Where the interpreter refuses for the point's sake, NaN: where 'meet'
    // holds and what it reads moves there.
    std::string Jumps(const std::string& meet, const std::vector<const Code*>& codes) {
        if (std::all_of(codes.begin(), codes.end(), [](const Code* c) { return Still(*c); }))
            return "";
        std::vector<Where> any;
        bool               always = false;
        for (const bool outer : {false, true}) {
            if (const Where where = Moves(codes, outer))
                any.insert(any.end(), where->any.begin(), where->any.end());
            else
                always = always || std::ranges::any_of(codes, [&](const Code* c) {
                             return !(outer ? c->outer : c->part).empty();
                         });
        }
        const std::string moves =
            always ? "" : Text(std::make_shared<const Moving>(Moving{"", std::move(any)}));
        return meet + (moves.empty() ? "" : " && (" + moves + ")") + " ? NAN : ";
    }

    PExpression<Value> visit(ValExpression<Value>* expression) override {
        return Answer(Literal(expression->value));
    }

    PExpression<Value> visit(AddExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        return Answer(SumOf(left.constant && right.constant ? Exactly(std::plus<>(), left, right)
                                                            : Broadcast(left, right, Added),
                            left, right));
    }

    // A quotient's part is (a' - q*b')/b, q the quotient, from 0 where a has none.
    PExpression<Value> visit(DivExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        const Code value = left.constant && right.constant ? Exactly(std::divides<>(), left, right)
                                                           : Broadcast(left, right, Divided);
        const auto cellwise = [&](const Code& q, const Code& b) {
            if (b.node) return Record(Kind::Cells, Extent::Wider(q.size, b.size), b.node, q);
            return q.Scalar() || b.Scalar() ? *Times(q, b) : Broadcast(q, b, Multiplied);
        };
        const auto over = [&](const Part& a) -> Part {
            if (a && a->node)
                return Record(Kind::Quotient, Extent::Wider(a->size, right.size), a->node, right);
            return Combined(
                a, right, [&](const Code& x, const Code& y) { return Broadcast(x, y, Divided); },
                std::divides<>());
        };
        Part top = PartOf(left);
        if (const Part b = PartOf(right)) top = Plus(top, Negative(cellwise(value, *b)));
        Part part = over(top);
        // In a nest, its cross part in the interpreter's order (Derivative::Quotient).
        if (part && !part->node && !part->outer.empty()) {
            const Part a = PartOf(left), b = PartOf(right), d = Inner(*part);
            Part       cross = a ? OuterOf(*a) : a;
            for (const auto& [x, y] :
                 {std::pair(Part(Inner(value)), b ? OuterOf(*b) : b),
                  std::pair(OuterOf(value), b ? Part(Inner(*b)) : b), std::pair(d, OuterOf(right))})
                if (x && y) cross = Plus(cross, Negative(cellwise(*x, *y)));
            part->outer.clear();
            if (cross)
                part->outer = {*Combined(
                    cross, Inner(right),
                    [&](const Code& x, const Code& y) { return Broadcast(x, y, Divided); },
                    std::divides<>())};
        }
        // a'/b where b's clause has none, not an infinite quotient times 0 (C110).
        if (part && !part->node && right.moves) {
            const Part alone = over(PartOf(left));
            for (std::size_t k = 0; k < part->cells.size(); ++k)
                part->cells[k] = Cell("(" + Whether(right.moves) + " ? " + part->cells[k].text +
                                          " : " + (alone ? alone->At(k).text : "0") + ")",
                                      primary);
            part->constant.reset();
        }
        return Answer(Parted(value, part, {&left, &right}));
    }

    // A matrix product is a sum over the inner dimension, in the interpreter's
    // order. A^-1*b of constants is solved, as the interpreter solves it, from
    // A's constant (C193).
    PExpression<Value> visit(MultExpression<Value>* expression) override {
        const Code left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        if (!left.constant || !right.constant)
            return Answer(ProductOf(Product(left, right), left, right));
        const auto* power = dynamic_cast<PowExpression<Value>*>(expression->m_e1().get());
        const auto* exponent =
            power ? dynamic_cast<ValExpression<Value>*>(power->m_e2().get()) : nullptr;
        if (exponent && numeric_interface<Value>::exact(exponent->value) &&
            exponent->value == Value(Number(-1))) {
            const Code base = Emit(power->m_e1());
            const auto b    = [&] { return *right.constant; };
            if (base.constant)
                return Answer(ProductOf(
                    Exact([&] { return numeric_interface<Value>::solve(*base.constant, b); }), left,
                    right));
        }
        return Answer(ProductOf(Exactly(std::multiplies<>(), left, right), left, right));
    }

    // DESIGN.md, loops in generated code: an operand's C array, read in place
    // where its cells are one array's, or declared as one.
    struct Indexed {
        std::string base;
        bool        transposed = false;
        std::string At(bool sliced, const std::string& i, const std::string& j) const {
            return base + (sliced ? "[b_]" : "") +
                   (transposed ? "[" + j + "][" + i + "]" : "[" + i + "][" + j + "]");
        }
        std::string Key(bool sliced) const {
            return base + (transposed ? "'" : "") + (sliced ? "s" : "");
        }
    };
    static std::optional<Indexed> Viewed(const Code& code) {
        const Cell&       first = code.cells[0];
        const std::string at    = Subscript(code.size, 0);
        if (!first.atom || first.level != primary || !first.text.ends_with(at)) return std::nullopt;
        const std::string base  = first.text.substr(0, first.text.size() - at.size());
        const Extent      other = {code.size.cols, code.size.rows, code.size.slices};
        if (Texts(Array(base, code.size)) == Texts(code)) return Indexed{base};
        if (Texts(Transposed(Array(base, other))) == Texts(code)) return Indexed{base, true};
        return std::nullopt;
    }
    bool MayNan(const Code& code) const {
        return std::ranges::any_of(
            code.cells, [&](const Cell& c) { return MayNan(c.text) || nans_.count(c.text); });
    }
    Indexed Materialised(const Code& code, const std::string& type = "double") {
        if (const auto viewed = Viewed(code)) return *viewed;
        std::string init = code.size.slices ? "" : Rows(code);
        for (std::size_t b = 0; b < code.size.slices; ++b)
            init += (b ? ", {" : "{") + Rows(code, b) + "}";
        const std::string name = Declare("array\x1f" + type + init, [&](const std::string& t) {
            return std::vector<std::string>{type + " " + t + Dimensions(code.size) + " = {" + init +
                                            "};"};
        });
        if (MayNan(code)) nan_temporaries_.insert(name);
        return {name};
    }
    // The loops over a value's cells, the innermost last.
    static std::vector<std::string> Loops(Extent size) {
        std::vector<std::string> lines;
        const auto               loop = [&](const std::string& v, std::size_t n) {
            lines.push_back(std::string(4 * lines.size(), ' ') + "for (int " + v + " = 0; " + v +
                                          " < " + std::to_string(n) + "; ++" + v + ")");
        };
        if (size.slices) loop("b_", size.slices);
        loop("i_", size.rows);
        loop("j_", size.cols);
        return lines;
    }
    static std::string Places(Extent size) { return size.slices ? "[b_][i_][j_]" : "[i_][j_]"; }
    // Each cell's sum in the order Product writes it, 'a*b + c*d' as C parses it.
    Code Looped(const Code& left, const Code& right, Extent size) {
        const Indexed     a = Materialised(left), b = Materialised(right);
        const bool        ls = left.size.slices, rs = right.size.slices;
        const std::size_t inner = left.size.cols;
        const std::string key   = "product\x1f" + a.Key(ls) + "*" + b.Key(rs) + " " +
                                size.toString() + " " + std::to_string(inner);
        const std::string name = Declare(key, [&](const std::string& t) {
            std::vector<std::string> lines = Loops(size);
            const std::string        in(4 * lines.size(), ' ');
            lines.insert(lines.begin(),
                         "double " + t + (size.Single() ? "[1][1]" : Dimensions(size)) + ";");
            lines.back() += " {";
            lines.push_back(in + "double s_ = " + a.At(ls, "i_", "0") + " * " +
                            b.At(rs, "0", "j_") + ";");
            if (inner > 1)
                lines.push_back(in + "for (int p_ = 1; p_ < " + std::to_string(inner) +
                                "; ++p_) s_ = s_ + " + a.At(ls, "i_", "p_") + " * " +
                                b.At(rs, "p_", "j_") + ";");
            lines.push_back(in + t + Places(size) + " = s_;");
            lines.push_back(in.substr(4) + "}");
            return lines;
        });
        if (MayNan(left) || MayNan(right)) nan_temporaries_.insert(name);
        return size.Single() ? Of(Atom(name + "[0][0]")) : Array(name, size);
    }

    // Each left cell is read once for each right column, each right cell once
    // for each left row; from 27 multiplications, as a loop.
    Code Product(Code left, Code right) {
        if (left.Scalar() || right.Scalar()) return Broadcast(left, right, Multiplied);
        if (!left.outer.empty() || !right.outer.empty()) {
            Code code  = Product(Inner(left), Inner(right));
            code.outer = {
                *Plus(Times(OuterOf(left), Inner(right)), Times(Inner(left), OuterOf(right)))};
            code.outer[0].moves = Moves({&left, &right}, true);
            return code;
        }
        if (temporaries_ && left.size.cols == right.size.rows) {
            const Extent size{left.size.rows, right.size.cols, Slices(left, right)};
            if (size.count() * left.size.cols >= 27) return Looped(left, right, size);
        }
        if (right.size.cols > 1) left = Shared(left);
        if (left.size.rows > 1) right = Shared(right);
        const std::size_t slices = Slices(left, right);
        if (left.size.cols != right.size.rows)
            throw Mismatched(
                left, right,
                "a matrix product needs as many columns on the left as rows on the right");
        Code code;
        code.size = {left.size.rows, right.size.cols, slices};
        for (std::size_t c = 0; c < code.size.count(); ++c) {
            const std::size_t b = c / (code.size.rows * code.size.cols),
                              i = c / code.size.cols % code.size.rows, j = c % code.size.cols;
            Cell cell = Multiplied(left.At(i, 0, b), right.At(0, j, b));
            for (std::size_t k = 1; k < left.size.cols; ++k)
                cell = Added(cell, Multiplied(left.At(i, k, b), right.At(k, j, b)));
            code.cells.push_back(cell);
        }
        return code;
    }

    PExpression<Value> visit(NegExpression<Value>* expression) override {
        const Code operand = Emit(expression->m_e());
        const Part part    = PartOf(operand);
        return Answer(Parted(Negated(operand), part ? Part(Negative(*part)) : part, {&operand}));
    }

    // A subtraction from 0, as the interpreter's (C33): C's minus makes -0 of
    // +0 (C98).
    Code Negative(const Code& part) {
        return part.node ? Record(Kind::Negative, part.size, part.node) : Negated(part);
    }
    static Code Negated(Code code) {
        if (code.constant) return Exact([&] { return -*code.constant; });
        const std::string zero = floats ? "0.0f - " : "0.0 - ";
        for (Cell& cell : code.cells) {
            Cell negation(
                zero + (cell.magnitude.empty() ? Wrap(cell, product) : "(" + cell.text + ")"), sum);
            negation.atom            = cell.atom;
            cell                     = negation;
        }
        for (Code& outer : code.outer) outer = Negated(outer);
        return code;
    }

    PExpression<Value> visit(InexactExpression<Value>* expression) override {
        const Code operand = Emit(expression->m_e());
        const auto inexact = [](const Value& v) { return numeric_interface<Value>::inexact(v); };
        const Part part    = PartOf(operand);
        Code       value   = operand.constant ? Exactly(inexact, operand) : operand;
        // A number made inexact is rounded once, as a float step rounds it (C396).
        if (operand.constant && Value::exact(*operand.constant))
            for (Cell& cell : value.cells) {
                std::erase(cell.text, rounded);
                std::erase(cell.magnitude, rounded);
            }
        return Answer(Parted(value,
                             part && part->node
                                 ? Record(Kind::Mark, part->size, part->node)
                                 : Mapped(part, inexact, [](const Code& c) { return c; }),
                             {&operand}));
    }

    PExpression<Value> visit(TransposeExpression<Value>* expression) override {
        const Code operand = Emit(expression->m_e());
        const auto transpose = [](const Value& v) {
            return numeric_interface<Value>::transpose(v);
        };
        return Answer(Parted(operand.constant ? Exactly(transpose, operand) : Transposed(operand),
                             Mapped(PartOf(operand), transpose, Transposed), {&operand}));
    }
    static Code Transposed(const Code& operand) {
        Code code;
        code.size = {operand.size.cols, operand.size.rows, operand.size.slices};
        for (std::size_t b = 0; b < std::max<std::size_t>(code.size.slices, 1); ++b)
            for (std::size_t i = 0; i < code.size.rows; ++i)
                for (std::size_t j = 0; j < code.size.cols; ++j)
                    code.cells.push_back(operand.At(j, i, b));
        for (const Code& outer : operand.outer)
            (code.outer.push_back(Transposed(outer)), code.outer[0].moves = outer.moves);
        return code;
    }

    // Under grad (Derivative::Power), e^w's part is e^w*w', and a constant
    // power's c*u^(c-1)*u', none where c is 0, NaN where u is 0 and c < 1.
    PExpression<Value> visit(PowExpression<Value>* expression) override {
        Code base = Emit(expression->m_e1());
        if (limit_ && !index_.empty() && !places_.count(index_) && Still(base) &&
            Mentions(*expression->m_e2(), index_))
            if (const auto climbed = Climbed(base, expression->m_e2())) return Answer(*climbed);
        const Code exponent =
            base.Scalar()
                ? Emit(expression->m_e2())
                : Known(expression->m_e2(), "a matrix power whose exponent is not a constant");
        // x^0 takes no part, its derivative 0, as Derivative::Power's.
        if (!Still(base) && !base.Scalar() && Doubles(*exponent.constant)[0] != 0)
            throw Reason("a derivative of a matrix power, for now");
        Code value;
        if (base.constant && exponent.constant) {
            value = Exactly(numeric_interface<Value>::pow, base, exponent);
        } else {
            if (!exponent.Scalar()) throw Reason("a matrix cannot be an exponent");
            if (!base.Scalar()) return Answer(Power(base, exponent));
            // Read twice by its square, and once more by its part.
            if (exponent.constant && Doubles(*exponent.constant)[0] == 2) base = Shared(base);
            value = Powered(base, exponent);
        }
        if (!Still(exponent)) {
            if (!Euler(*expression->m_e1()))
                throw Reason("grad cannot differentiate a power whose exponent changes with " +
                             grad_ + ", unless its base is e");
            Part part = Times(value, PartOf(exponent));
            // In a nest, e^w*w12 + e^w*(w^*w'), as Derivative::Composed.
            if (part && !part->node && !part->outer.empty()) {
                const Code& w = exponent.part[0];
                part->outer.clear();
                if (const Part cross =
                        Plus(Times(Inner(value), OuterOf(w)),
                             Times(Inner(value), Times(OuterOf(exponent), Inner(w)))))
                    part->outer = {*cross};
            }
            return Answer(Parted(value, part, {&exponent}));
        }
        if (Still(base)) return Answer(value);
        if (!exponent.constant)
            throw Reason("a derivative of a power whose exponent is not a constant, for now");
        const Value c = *exponent.constant, coefficient = Value(Number(1)) * (c - Value(Number(0)));
        if (coefficient(1, 1) == Number(0)) return Answer(value);
        // u^(c-1) at 0 is C's pow's inf, where the interpreter divides by 0;
        // in a nest, u^(c-2) too where u moves by both grads.
        const Value lower = c - Value(Number(1));
        const Value second = coefficient * (c - Value(Number(1)));
        const Value below  = lower - Value(Number(1));
        const bool  deeper = !base.part.empty() && !base.outer.empty() && Doubles(second)[0] != 0;
        if ((Doubles(lower)[0] < 0 || (deeper && Doubles(below)[0] < 0)) &&
            (!base.constant || Doubles(*base.constant)[0] == 0.0)) {
            value.cells[0] = Cell("(" + Jumps(Wrap(base.cells[0], sum) + " == 0.0", {&base}) +
                                      value.cells[0].text + ")",
                                  primary);
            value.constant.reset();
        }
        if (base.part.empty()) return Answer(value);
        const Code power =
            !base.constant ? Powered(base, Literal(lower))
            : Doubles(lower)[0] < 0 && Doubles(*base.constant)[0] == 0.0
                ? Literal(Value(Number(HUGE_VAL)))
                : Exact([&] { return numeric_interface<Value>::pow(*base.constant, lower); });
        const Part slope = Times(Literal(coefficient), power);
        Part       part  = Times(slope, PartOf(base));
        // In a nest, its cross part f'*u12 + f''*(u^*u'), as Derivative::Composed.
        if (part && !part->node && !part->outer.empty()) {
            Part cross = Times(Inner(*slope), OuterOf(base.part[0]));
            if (Doubles(second)[0] != 0 && !base.outer.empty())
                cross =
                    Plus(cross, Unless(*Times(Doubles(below)[0] == 0
                                                  ? Literal(second)
                                                  : *Times(Literal(second),
                                                           Powered(Inner(base), Literal(below))),
                                              Times(OuterOf(base), Inner(base.part[0]))),
                                       Doubles(below)[0] < 0 ? base.outer[0].moves : nullptr));
            part->outer.clear();
            if (cross) part->outer = {Doubles(c)[0] < 2 ? Unless(*cross, base.moves) : *cross};
        }
        // None where its base's clause has none, not 0 times an infinity, where
        // its value is finite (C118).
        if (Doubles(lower)[0] < 0 && Doubles(c)[0] > 0 && !part->node)
            part = Unless(*part, base.moves);
        return Answer(Parted(value, part, {&base}));
    }

    // C's absorbs a NaN where the other operand is 1 or 0: pow(1, NaN) and
    // pow(NaN, 0) are 1. C's sqrt(-0) is -0, the interpreter's +0 (C163).
    // A square and an inverse are the interpreter's, not pow, which a
    // compiler folds into them at -O1 (C345).
    Code Powered(const Code& base, const Code& exponent) {
        const auto other = [](const Code& c, double v) {
            return c.constant && Doubles(*c.constant)[0] != v;
        };
        const std::string test =
            other(exponent, 0.0) || other(base, 1.0) ? "" : Nan(base, exponent);
        const double e = exponent.constant ? Doubles(*exponent.constant)[0] : 0.0;
        if ((!exponent.constant || e != std::floor(e)) &&
            !(base.constant && Doubles(*base.constant)[0] >= 0))
            roots_ = true;
        const Cell&  u = base.cells[0];
        Cell         power("pow(" + u.text + ", " + exponent.cells[0].text + ")", primary);
        if (e == 0.5) power = Cell("sqrt(0.0 + " + Wrap(u, product) + ")", primary);
        if (e == 2) power = Multiplied(Shared(u), Shared(u));
        if (e == -1) power = Divided(Atom("1.0", true), u);
        Cell cell = test.empty() ? power : Cell("(" + test + power.text + ")", primary);
        // Of a constant above 0 but 1, NaN where its exponent is: a test reads
        // that, not a second pow, which C, setting errno, does not share.
        if (const double b = base.constant ? Doubles(*base.constant)[0] : 0.0;
            b > 0 && (floats ? static_cast<float>(b) != 1.0f : b != 1.0))
            cell.nan = exponent.cells[0].text;
        // In a nest, its part by the outer grad, e^w*w^ or c*u^(c-1)*u^, as
        // Derivative::Power's first.
        Code out = Of(cell);
        if (const Part w = OuterOf(exponent)) {
            out.outer = {*Times(Of(cell), w)};
            out.outer[0].moves = w->moves;
        } else if (const Part moves = OuterOf(base);
                   moves && exponent.constant && Doubles(*exponent.constant)[0] != 0) {
            const Value c = *exponent.constant;
            out.outer     = {*Times(
                Times(Literal(c), Powered(Inner(base), Literal(c - Value(Number(1))))), moves)};
            // None where its clause has none, not 0 times u^(c-1) at 0 (C118).
            if (Doubles(c)[0] < 1) out.outer[0] = Unless(out.outer[0], moves->moves);
            out.outer[0].moves = moves->moves;
        }
        return out;
    }

    // The built-in e, as Derivative::Euler has it.
    bool Euler(const Expression<Value>& base) {
        return dynamic_cast<const RefExpression<Value>*>(&base) && base.Name() == "e" &&
               !places_.count("e") && !frame_.values.count("e") && !Expanded("e") &&
               Lookup("e").where == &definitions_.Builtins();
    }

    // In a header that writes NaN, a decision on a value is NaN where an
    // operand is, as the interpreter refuses it (DESIGN.md, a NaN reaches
    // every term that reads it): the test of each cell of the operands given
    // that is neither a number nor the index, written before the decision.
    // Several cells tested once per list: each read repeating the test was
    // most of a header (DESIGN.md, a matrix read by its cells).
    template <typename... Codes>
    std::string Nan(const Codes&... codes) {
        std::string           test;
        std::set<std::string> seen;
        for (const Code* code : {&codes...}) {
            std::string one;
            std::size_t n = 0;
            for (const Cell& cell : code->cells)
                if (aware_ && !code->constant && !cell.number &&
                    seen.insert(cell.nan.empty() ? cell.text : cell.nan).second)
                    one += (n++ ? " || isnan(" : "isnan(") +
                           (cell.nan.empty() ? cell.text : cell.nan) + ")";
            if (n > 1 && temporaries_)
                one = Declare("isnan\x1f" + one, [&](const std::string& t) {
                    return std::vector<std::string>{"const int " + t + " = " + one + ";"};
                });
            if (n) test += (test.empty() ? "" : " || ") + one;
        }
        return test.empty() ? test : test + " ? NAN : ";
    }

    // By squaring, as the interpreter does it, of the inverse for a negative
    // exponent. The interpreter starts from the identity and multiplies it in;
    // starting from the first factor instead changes only a cell that is
    // infinite, where the identity's zeros would make it NaN.
    Code Power(Code base, const Code& exponent) {
        if (base.size.slices)
            throw Reason("only a matrix has a power, not a " + base.size.Described());
        if (!exponent.constant) throw Reason("a matrix power whose exponent is not a constant");
        const auto power = (*exponent.constant)(1, 1).Inexact();
        if (power.imag() != 0 || power.real() != std::floor(power.real()) ||
            std::abs(power.real()) > 2147483647.0)
            throw Reason("a matrix power must be a whole number");
        if (base.size.rows != base.size.cols) throw Reason("only a square matrix has a power");
        const long long whole = static_cast<long long>(power.real());
        if (whole == 0) {
            // The identity whatever the base, but not a constant: a constant is
            // what reads no name, and an enclosing fold would read this one's.
            // NaN where a cell of the base is, as the interpreter refuses it.
            Code identity = Literal(Value::Identity(base.size));
            identity.constant.reset();
            aware_ = aware_ || MayNan(base);
            if (const std::string test = aware_ ? Nan(Shared(base)) : ""; !test.empty())
                for (Cell& cell : identity.cells)
                    cell = Cell("(" + test + cell.text + ")", primary);
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
        const std::size_t n    = matrix.size.rows;
        const std::string rows = Rows(matrix);
        inverses_.insert(n);
        const std::string name = Declare("inverse\x1f" + rows, [&](const std::string& t) {
            return std::vector<std::string>{
                "double " + t + Subscript(n, n) + " = {" + rows + "};",
                module_ + "_inverse" + std::to_string(n) + "_(" + t + ");"};
        });
        // The helper writes NaN into it, out of the cells' sight (C308).
        nan_temporaries_.insert(name);
        return Array(name, Extent{n, n});
    }

    static const char* Operator(Comparison op) {
        static const char* const ops[] = {" < ", " > ", " <= ", " >= ", " == ", " != "};
        return ops[static_cast<int>(op)];
    }

    PExpression<Value> visit(CompareExpression<Value>* expression) override {
        const Code       left = Emit(expression->m_e1()), right = Emit(expression->m_e2());
        const Comparison op = expression->Op();
        if (!(left.Scalar() && right.Scalar()) && op != Comparison::Equal &&
            op != Comparison::NotEqual)
            throw Reason("a comparison needs single values, not a " +
                         (left.Scalar() ? right : left).size.Described());
        if (left.size != right.size) throw Reason(Value::Unequal(left.size, right.size));
        // At a constant point too, a jump is tested, not folded.
        if (left.constant && right.constant && left.part.empty() && right.part.empty())
            return Answer(Exactly(
                [&](const Value& a, const Value& b) {
                    return numeric_interface<Value>::compare(a, b, expression->Op());
                },
                left, right));
        return Answer(Cell("(" + Compared(op, left, right) + " ? 1.0 : 0.0)", primary));
    }

    // A comparison as C tests it, NaN first where it reads one or jumps; two
    // matrices are equal where every cell is, as interpreted.
    std::string Compared(Comparison op, const Code& left, const Code& right) {
        std::string meet;
        for (std::size_t c = 0; c < left.cells.size(); ++c)
            meet +=
                (c ? " && " : "") + Wrap(left.cells[c], sum) + " == " + Wrap(right.cells[c], sum);
        const bool        equality = op == Comparison::Equal || op == Comparison::NotEqual;
        const std::string test =
            left.Scalar() ? Wrap(left.cells[0], sum) + Operator(op) + Wrap(right.cells[0], sum)
                          : (op == Comparison::Equal ? "(" : "!(") + meet + ")";
        // Under grad, an equality whose sides move takes its clause at the
        // point alone, as the interpreter's Holds; in a value, any comparison
        // jumps there (C320).
        return Nan(left, right) + (!guarding_ || equality ? Jumps(meet, {&left, &right}) : "") +
               test;
    }

    // A guard as C tests it. Empty where it always holds, and nothing where
    // it never does.
    std::optional<std::string> Condition(const PExpression<Value>& guard) {
        std::set<std::string>                 asked, unsure;
        std::vector<Compiled::Named>          read;
        const Setting<std::set<std::string>*> asking(statuses_, &asked), doubting(unsure_, &unsure);
        const Setting<std::vector<Compiled::Named>*> reading(read_, &read);
        const Setting<bool> guarding(guarding_, true), moved(moved_, false);
        const Frame                                  frame = frame_;
        const Code                                   code  = Quiet(guard);
        chosen_                  = chosen_ || moved_;
        unconverged_.clear();
        asked_.clear();
        sure_.clear();
        if (!code.Scalar()) throw Reason("a guard that is a matrix");
        if (code.constant) {
            if (!Holds(*code.constant)) return std::nullopt;
            return std::string();
        }
        if (!asked.empty()) {
            Compiled::Fallen&         fallen = fallen_[guard.get()];
            std::set<Compiled::Named> limits;
            for (const std::string& status : asked)
                limits.insert(what_[status].begin(), what_[status].end());
            for (const Compiled::Named& l : read)
                if (limits.count(l) && !std::count(fallen.limits.begin(), fallen.limits.end(), l))
                    fallen.limits.push_back(l);
            (grad_.empty() ? fallen.plain : fallen.grad) = true;
        }
        const auto taken = [this](const std::string& status) { return Falls(status); };
        const Setting<Frame> again(frame_, frame);
        const bool           defers = Defers(guard);
        frame_                      = frame;
        if (!defers && std::ranges::none_of(asked, taken)) return Test(guard);
        std::string       fell;
        const std::string truth = Shared(Cell(Truth(guard, &fell), primary)).text;
        if (std::ranges::none_of(asked, taken))
            return "isnan(" + truth + ") ? NAN : " + truth + " != 0.0";
        // A truth NaN where it fell for a status does not hold (DESIGN.md).
        asked_       = asked;
        std::set_difference(asked.begin(), asked.end(), unsure.begin(), unsure.end(),
                            std::inserter(sure_, sure_.end()));
        unconverged_ = "isnan(" + truth + ") && (" + fell + ")";
        // Before a walk's loop where neither moves, as its limits are.
        const std::string flag = "isnan(" + truth + ") && !(" + fell + ")";
        const std::string nan  = Looping(Of(Cell(flag, 0))).cells[0].text;
        // Hoisted or shared, the flag is a double.
        const std::string held = nan.find('(') == std::string::npos ? nan : "(" + nan + ")";
        return held + " != 0.0 ? NAN : " + truth + " == 1.0";
    }

    // A constant's truth, or the interpreter's refusal of it as a reason.
    template <typename... Needs>
    static bool Holds(const Value& value, const Needs&... needs) {
        return Reasoned([&] { return Value::truth(value, needs...); });
    }

    // A guard as the clause kept for --check tests it: 0 where it reads NaN,
    // as no clause is taken, rather than NaN in an int (C90). Its own test
    // is the first conditional outside every parenthesis.
    static std::string Picked(std::string condition) {
        int depth = 0;
        for (std::size_t at = 0; at < condition.size(); ++at) {
            depth += (condition[at] == '(') - (condition[at] == ')');
            if (depth == 0 && condition.compare(at, 9, " ? NAN : ") == 0)
                return condition.replace(at, 9, " ? 0 : ");
        }
        return condition;
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
        const Frame frame = frame_;
        const Code  code  = Emit(expression);
        if (code.constant) return Holds(*code.constant) ? "1" : "0";
        if (const auto* compare = dynamic_cast<CompareExpression<Value>*>(expression.get())) {
            const Setting<Frame> again(frame_, frame);
            const Code           left = Emit(compare->m_e1());
            return Compared(compare->Op(), left, Emit(compare->m_e2()));
        }
        return Nan(code) + Wrap(code.cells[0], sum) + " != 0.0";
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
        const bool        right   = std::exchange(right_, true);
        const std::string written = text(logic.m_e2());
        deferring_                = was;
        right_                    = right;
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

    // Whether an 'and' or 'or' in it defers its right side, as all do aware.
    // What is always read on the way is recorded, as it bounds the right's check.
    bool Defers(const PExpression<Value>& expression) {
        const auto* logic = dynamic_cast<LogicExpression<Value>*>(expression.get());
        if (!logic) {
            Emit(expression);
            return false;
        }
        if (aware_ || Defers(logic->m_e1())) return true;
        const std::size_t checks = checks_.size();
        bool              nested = false;
        const bool        defers = Right(*logic, [&](const PExpression<Value>& side) {
                                nested = Defers(side);
                                return std::string();
                            }).second.has_value();
        checks_.resize(checks);
        return defers || nested;
    }

    // Under grad no clause after is taken where a limit runs out: grad
    // refuses there. It is where an input is absent (DESIGN.md).
    bool Falls(const std::string& status) {
        return grad_.empty() || std::ranges::none_of(what_[status], [](const auto& l) {
                   return l.second == "runs out";
               });
    }

    // A truth as a double: 1, 0, or NaN where a term it needed did not exist.
    // Given fell, where it is NaN, whether the first operand that stopped it
    // asked a status that held, as the interpreter stops at the first refusal
    // it reads, an absence or a limit run out falling and NaN refused (C380).
    std::string Truth(const PExpression<Value>& expression, std::string* fell = nullptr) {
        const auto* logic = dynamic_cast<LogicExpression<Value>*>(expression.get());
        if (!logic) {
            std::set<std::string> own;
            std::string           test;
            {
                const Setting<std::set<std::string>*> asking(statuses_, fell ? &own : statuses_);
                test = Test(expression);
            }
            for (const std::string& status : own) {
                statuses_->insert(status);
                if (Falls(status)) Or(*fell, status);
            }
            return "(" + test + " ? 1.0 : 0.0)";
        }
        std::string       fl, fr;
        const std::string left =
            Shared(Cell(Truth(logic->m_e1(), fell ? &fl : nullptr), primary)).text;
        auto [right, check] = Right(*logic, [&](const PExpression<Value>& side) {
            return Truth(side, fell ? &fr : nullptr);
        });
        if (!fr.empty())
            Or(fl, left + (logic->Conjunction() ? " == 1.0" : " == 0.0") + " && (" + fr + ")");
        if (fell) *fell = fl;
        if (check) right = "(" + *check + right + ")";
        return "(" + left + " == 0.0 ? " + (logic->Conjunction() ? "0.0" : right) + " : " + left +
               " != " + left + " ? NAN : " + (logic->Conjunction() ? right : "1.0") + ")";
    }

    PExpression<Value> visit(LogicExpression<Value>* expression) override {
        const auto truth = [expression](const Code& code) {
            if (!code.Scalar()) throw Reason(std::string(expression->Word()) + " of a matrix");
            return code.constant ? std::optional<bool>(Holds(*code.constant, expression->Word()))
                                 : std::nullopt;
        };
        // A left side that decides is the answer, and the right is not read.
        const Frame               frame = frame_;
        const std::optional<bool> left  = truth(Emit(expression->m_e1()));
        if (left && *left != expression->Conjunction())
            return Answer(Literal(Value(Number(*left ? 1 : 0))));
        // What an 'or' reads on its right alone may have run out where it holds.
        const bool                  either = statuses_ && unsure_ && !expression->Conjunction();
        const std::set<std::string> before = either ? *statuses_ : std::set<std::string>();
        const std::optional<bool>   right  = truth(Quiet(expression->m_e2()));
        if (either)
            for (const std::string& status : *statuses_)
                if (!before.count(status)) unsure_->insert(status);
        std::set<std::string> bound;
        if (!left) Binding(*expression->m_e2(), bound);
        for (const std::string& name : bound)
            frame_.refused[name] = ", bound on the right of 'and' or 'or', read after it, for now";
        if (left && right) return Answer(Literal(Value(Number(*right ? 1 : 0))));
        const PExpression<Value> self = expression->self();
        const Setting<Frame>     again(frame_, frame);  // read again from where it began
        const bool               defers = Defers(self);
        frame_                          = frame;
        if (defers) return Answer(Cell(Truth(self), primary));
        return Answer(Cell("(" + Test(self) + " ? 1.0 : 0.0)", primary));
    }

    // floor as C's, exactly where what it is given is a constant; re and im
    // of a compiled value, which is real.
    PExpression<Value> Builtin(FuncExpression<Value>* expression) {
        const ParametersCall<Value>& call = expression->Call();
        if (call.parameters_expression().size() != 1 || !call.parameters_dict().empty())
            throw Reason(expression->Name() + " expects 1 argument");
        const Code operand = Emit(call.parameters_expression()[0]);
        if (expression->Name() == "re") return Answer(operand);
        if (expression->Name() == "im") {
            // 0, but NaN where its operand is, as the interpreter refuses it
            // there, as a call's unread argument (C204); in a guard 0, as abs's
            // first folds, its other clauses reading x.
            Code zero = Literal(Value(operand.size));
            if (guarding_ || operand.constant ||
                std::all_of(operand.cells.begin(), operand.cells.end(),
                            [](const Cell& c) { return c.number; }))
                return Answer(zero);
            const std::string test = (aware_ = true, Nan(Shared(operand)));
            for (Cell& cell : zero.cells) cell = Cell("(" + test + cell.text + ")", primary);
            zero.constant.reset();
            return Answer(zero);
        }
        if (operand.constant && operand.part.empty())
            return Answer(
                Exactly([](const Value& v) { return Value::Cells(v, Number::floor); }, operand));
        Code code;
        code.size = operand.size;
        for (const Cell& cell : operand.cells) {
            const std::string floor = "floor(" + cell.text + ")";
            const std::string jump  = Jumps(floor + " == " + Wrap(cell, sum), {&operand});
            code.cells.emplace_back(jump.empty() ? floor : "(" + jump + floor + ")", primary);
        }
        return Answer(code);
    }

    // '[A, B; C, D]' as the interpreter lays it out (docs/language.md, section 2),
    // and its parts, 0 of a block's shape where it has none (Derivative::Literal).
    PExpression<Value> visit(MatExpression<Value>* expression) override {
        std::vector<Code> blocks, parts;
        bool              any = false;
        for (const PExpression<Value>& child : expression->Children()) {
            const Code& block = blocks.emplace_back(Emit(child));
            if (block.size.slices)
                throw Reason("a tensor cannot be a block of a literal, only a matrix can");
            any = any || !block.part.empty();
            parts.push_back(block.part.empty() ? Literal(Value(block.size)) : block.part[0]);
        }
        std::vector<const Code*> from;
        for (const Code& block : blocks) from.push_back(&block);
        Code code = Laid(blocks, expression->Size());
        if (code.constant) code = Literal(*code.constant);
        const auto laid = [&](const std::vector<Code>& p) { return Laid(p, expression->Size()); };
        return Answer(Parted(code, any ? Part(Moved(parts, laid)) : Part(), from));
    }
    // Each band as tall or as wide as its largest block, a single value
    // stretched over its block and computed once per step, Shared; any
    // other block fills its place, as C41's corner is no meaning.
    Code Laid(const std::vector<Code>& blocks, Extent bands) {
        std::vector<std::size_t> rows(bands.rows, 1), cols(bands.cols, 1);
        for (std::size_t b = 0; b < blocks.size(); ++b) {
            rows[b / bands.cols] = std::max(rows[b / bands.cols], blocks[b].size.rows);
            cols[b % bands.cols] = std::max(cols[b % bands.cols], blocks[b].size.cols);
        }
        Code code;
        code.size = {std::reduce(rows.begin(), rows.end()), std::reduce(cols.begin(), cols.end())};
        code.constant = Value(code.size);
        for (std::size_t i = 0; i < rows.size(); ++i)
            for (std::size_t r = 0; r < rows[i]; ++r)
                for (std::size_t j = 0; j < cols.size(); ++j) {
                    const Code& block = blocks[i * cols.size() + j];
                    if (!block.Scalar() && block.size != Extent{rows[i], cols[j]})
                        throw Reason("a block that does not fill its band");
                    const bool stretched = block.Scalar() && rows[i] * cols[j] > 1;
                    const Cell one       = stretched ? Shared(block.cells[0]) : block.cells[0];
                    if (!block.constant) code.constant.reset();
                    for (std::size_t c = 0; c < cols[j]; ++c) {
                        const std::size_t at = block.Scalar() ? 0 : r * cols[j] + c;
                        code.cells.push_back(block.Scalar() ? one : block.cells[at]);
                        if (code.constant)
                            code.constant->data()[code.cells.size() - 1] =
                                block.constant->data()[at];
                    }
                }
        return Outer(code, blocks,
                     [&](const std::vector<Code>& outers) { return Laid(outers, bands); });
    }
    // A matrix or tensor of single parts, by place, exact where each is.
    static Code Assembled(const std::vector<Code>& parts, Extent size) {
        Code code = Literal(Value(size));
        for (std::size_t k = 0; k < parts.size(); ++k) {
            code.cells[k] = parts[k].cells[0];
            if (!parts[k].constant) code.constant.reset();
            if (code.constant) code.constant->data()[k] = (*parts[k].constant)(1, 1);
        }
        return Outer(code, parts,
                     [&](const std::vector<Code>& outers) { return Assembled(outers, size); });
    }

    PExpression<Value> visit(RefExpression<Value>* expression) override {
        const std::string& name = expression->Name();
        if (!own_) {
            if (const auto place = places_.find(name); place != places_.end())
                return Answer(Literal(place->second));
            if (!index_.empty() && name == index_) return Answer(Atom(index_text_, true));
            if (const auto refused = frame_.refused.find(name); refused != frame_.refused.end())
                throw Reason(name + refused->second);
            const auto local = frame_.values.find(name);
            if (local != frame_.values.end() && !frame_.graded.count(name))
                return Answer(local->second);
            if (const auto expanded = Expanded(name)) return Answer(*expanded);
        }
        const Found             found      = Lookup(name);
        const std::string&      key        = found.key;
        const Reference<Value>* definition = found.definition;
        if (!definition) {
            if (fixed_.count(key)) throw Reason(key + " is not defined");
            // An input is a sequence, not a 1x1 field (C319).
            if (model_ && std::ranges::any_of(model_->parameters, [&](const auto& p) {
                    return p.name == key && !p.index.empty() && !p.fallback;
                }))
                throw Reason(key + " is a sequence; index it");
            return Answer(Field(key, Value(Number(NAN))));
        }
        for (const std::string& by : graded_) {  // where a local did not hide it (C313)
            std::set<const Reference<Value>*> seen;
            Reasoned([&] { Derivative<Value>(definitions_).Definition(*definition, by, seen); });
        }
        ReadWhole(key, *definition, false, false);
        // 'f()' is 'f', a call with every default (C169).
        if (!definition->Clauses().empty() &&
            !definition->Clauses().front().parameters.parameters_names().empty()) {
            if (found.where == &definitions_.Builtins())
                return Prelude(key, *definition, ParametersCall<Value>());
            return Answer(Call(key, *definition, ParametersCall<Value>(), nullptr));
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
        // A global is evaluated in a scope of its own, where no index, place
        // or call's name is seen (C112).
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
            const Home                home(*this, definition);
            const Setting<Expansion*> uncalled(expansion_, nullptr);
            const Setting<Frame>      own(frame_, {});
            code = std::any_of(definition->Clauses().begin(), definition->Clauses().end(), cells)
                       ? Cells(key, *definition)
                       : Emit(definition->Clauses().front().expression);
        }
        reading_                  = reading;
        index_                    = index;
        places_                   = places;
        temporaries_              = outer_temporaries;
        const auto in = [&](const auto& term) { return std::get<0>(term.first) == &temporaries; };
        std::erase_if(terms_, in);
        std::erase_if(failed_, in);
        std::erase_if(filled_, in);
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
                                                        Fits fits, std::optional<Code> whole,
                                                        std::vector<std::string>* taken  = nullptr,
                                                        bool                      marked = false) {
        const std::string           nan    = Refused(whole);
        const std::optional<Extent> extent = Measured(name, definition, whole, fits);
        if (!extent) throw Reason(name + " has no size");
        Code shape;
        shape.size = *extent;
        for (const Clause<Value>& clause : definition.Clauses()) {
            const ParametersDefinition<Value>& p = clause.parameters;
            if (p.cells() && p.row_name().empty() && fits(clause))
                Named(p, shape.size,
                      name + "_" + (p.general() ? p.subscript() : std::to_string(p.index())));
        }
        std::vector<std::string> cells;
        for (std::size_t c = 0; c < shape.size.count(); ++c) {
            const int slice = static_cast<int>(c / (shape.size.rows * shape.size.cols)) + 1,
                      row   = static_cast<int>(c / shape.size.cols % shape.size.rows) + 1,
                      col   = static_cast<int>(c % shape.size.cols) + 1;
            std::string                chain, pick, ran;
            std::optional<std::string> last;
            int                        picked  = 0;
            const auto                 settles = [&](const Clause<Value>& clause) {
                const Setting<Frame> own(frame_, {});  // each cell its own (C406)
                const int place = static_cast<int>(&clause - definition.Clauses().data()) + 1;
                const ParametersDefinition<Value>& p = clause.parameters;
                const std::string outer = std::exchange(index_, p.general() ? p.index_name() : "");
                Reads                 guard_reads, value_reads;
                const Setting<Reads*> reads(clause_reads_, marked ? &guard_reads : clause_reads_);
                const std::optional<std::string> condition =
                    p.guarded() ? Condition(p.guard()) : std::optional<std::string>("");
                Ran(ran, condition);
                if (marked) clause_reads_ = &value_reads;
                const std::optional<Code> value =
                    condition ? std::optional<Code>(Emit(clause.expression)) : std::nullopt;
                index_ = outer;
                if (!value) return false;
                if (!value->Scalar()) throw Reason("a cell of " + name + " must be a single value");
                // Computed again (At), what each guard and value reads is checked where it is.
                const std::string text = Marked(value_reads) + value->cells[0].text;
                if (condition->empty()) {
                    last   = text;
                    picked = place;
                } else {
                    chain += Marked(guard_reads) + *condition + " ? " + text + " : ";
                    pick += Picked(*condition) + " ? " + std::to_string(place) + " : ";
                }
                return condition->empty();
            };
            Walk(definition, slice, row, col, fits, settles);
            if (!last) last = Unless(ran, whole ? whole->At(c).text : "0.0");
            cells.push_back(nan.empty() ? chain + *last : "(" + nan + chain + *last + ")");
            if (taken) taken->push_back(pick + std::to_string(picked));
        }
        return {shape, cells};
    }

    // The clauses that may give a cell, in the order the interpreter tries
    // them, those for every cell with its places bound, until one settles it.
    template <typename Fits, typename Settles>
    void Walk(const Reference<Value>& definition, int slice, int row, int col, Fits fits,
              Settles settles) {
        for (const Clause<Value>& clause : definition.Clauses()) {
            const ParametersDefinition<Value>& p = clause.parameters;
            if (p.cells() && p.row_name().empty() && fits(clause) && p.slice() == slice &&
                p.row() == row && p.col() == col && settles(clause))
                return;
        }
        for (const bool guarded : {true, false})
            for (const Clause<Value>& clause : definition.Clauses()) {
                const ParametersDefinition<Value>& p = clause.parameters;
                if (p.row_name().empty() || p.guarded() != guarded || !fits(clause)) continue;
                if (p.tensor()) places_[p.slice_name()] = Value(Number(slice));
                places_[p.row_name()] = Value(Number(row));
                places_[p.col_name()] = Value(Number(col));
                const bool settled    = settles(clause);
                places_.erase(p.slice_name());
                places_.erase(p.row_name());
                places_.erase(p.col_name());
                if (settled) return;
            }
    }

    // A clause for one cell, in the interpreter's words where it names more
    // or fewer indices than a read of the value takes, or a cell outside it.
    static void Named(const ParametersDefinition<Value>& p, Extent size, const std::string& name) {
        Value value(size);
        Reasoned([&] { Reference<Value>::Named(p, value, name); });
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
                               c.parameters.slice() == every.slice() &&
                               c.parameters.row() == every.row() &&
                               c.parameters.col() == every.col();
                    });
                const std::string term = name + "_" + std::to_string(index);
                const std::string cell =
                    "[" + (every.tensor() ? std::to_string(every.slice()) + "," : "") +
                    std::to_string(every.row()) + "," + std::to_string(every.col()) + "]";
                if (!own)
                    throw Reason(term + " and " + name + "_" + every.subscript() + cell +
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
                Named(p, shape.size, name + "_" + std::to_string(p.index()));
                std::string& cell =
                    cells[(static_cast<std::size_t>(p.slice() - 1) * shape.size.rows +
                           static_cast<std::size_t>(p.row() - 1)) *
                              shape.size.cols +
                          static_cast<std::size_t>(p.col() - 1)];
                cell              = "\x12" + name + "\x13 == " + std::to_string(p.index()) + " ? " +
                       value.cells[0].text + " : " + cell;
            }
            reading_         = reading;
            index_text_      = outer_text;
            sequence.general = cells;
        });
    }

    // The interpreter refuses the matrix written whole before a cell's own
    // clause is reached, so where a cell of it writes NaN every cell does (C136).
    std::string Refused(std::optional<Code>& whole) {
        if (!whole) return "";
        for (const Cell& c : whole->cells) aware_ |= !whole->constant && WritesNan(c.text);
        if (aware_ && !whole->constant) whole = Shared(*whole);
        return Nan(*whole);
    }

    // A matrix defined by its cells, one cell at a time with its names bound to
    // the cell's place. They are constants, so the size is decided here, and a
    // guard reading only them; one reading a value that moves is tested where
    // the cell is. Under grad it has a part where any cell has one.
    Code Cells(const std::string& name, const Reference<Value>& definition) {
        std::optional<Setting<Memo>> memo;
        if (memo_.first != temporaries_) memo.emplace(memo_, Memo{temporaries_, {}});
        std::optional<Code> whole;  // the matrix written whole, if it is
        for (const Clause<Value>& clause : definition.Clauses())
            if (!clause.parameters.cells()) whole = Emit(clause.expression);
        const std::string           nan = Refused(whole);
        const std::optional<Extent> extent =
            Measured(name, definition, whole, [](const Clause<Value>&) { return true; });
        if (!extent) throw Reason(name + " has no size");
        for (const Clause<Value>& clause : definition.Clauses()) {
            const ParametersDefinition<Value>& p = clause.parameters;
            if (p.cells() && p.row_name().empty()) Named(p, *extent, name);
        }
        Code                     code;
        Value                    exact(*extent);
        bool                     constant = true, any = false;
        std::vector<Code>        cells, parts;
        std::vector<const Code*> from;
        code.size = *extent;
        for (std::size_t c = 0; c < code.size.count(); ++c) {
            Code fallback = Literal(Value(Number(0)));
            const auto cell = [&](const Value& v) { return Value(v.data()[c % v.Size().count()]); };
            if (whole)
                fallback = Parted(
                    Single(*whole, c),
                    Mapped(PartOf(*whole), cell, [&](const Code& p) { return Single(p, c); }),
                    {&*whole});
            const Code& given = cells.emplace_back(
                CellOf(name, definition, static_cast<int>(c / (extent->rows * extent->cols)) + 1,
                       static_cast<int>(c / extent->cols % extent->rows) + 1,
                       static_cast<int>(c % extent->cols) + 1, fallback));
            code.cells.push_back(nan.empty()
                                     ? given.cells[0]
                                     : Cell("(" + nan + given.cells[0].text + ")", primary));
            if (given.constant) exact.data()[c] = (*given.constant)(1, 1);
            constant = constant && given.constant && nan.empty();
            any      = any || !given.part.empty();
            parts.push_back(given.part.empty() ? Literal(Value(Number(0))) : given.part[0]);
        }
        for (const Code& given : cells) from.push_back(&given);
        if (constant) code.constant = exact;
        code =
            Outer(code, cells, [&](const std::vector<Code>& o) { return Assembled(o, *extent); });
        // Under reverse mode, each cell's node placed into the matrix's.
        Code placed;
        for (std::size_t k = 0; tape_ && k < parts.size(); ++k)
            if (parts[k].node)
                placed = Record(Kind::Place, code.size, placed.node, {}, parts[k].node, k + 1);
        return Parted(code, !any ? Part() : tape_ ? placed : Assembled(parts, code.size), from);
    }

    // The size the interpreter measures (Reference::Measured), its bounds and
    // its reads compiled where the definition is: a function's where it is
    // called, whose shapes are known.
    template <typename Fits>
    std::optional<Extent> Measured(const std::string& name, const Reference<Value>& definition,
                                   const std::optional<Code>& whole, Fits fits) {
        return Reasoned([&] {
            return definition.Measured(
                whole ? std::optional<Extent>(whole->size) : std::nullopt, fits, name,
                [&](const Clause<Value>& clause, auto measure) {
                    const ParametersDefinition<Value>& p = clause.parameters;
                    const std::string                  outer =
                        std::exchange(index_, p.general() ? p.index_name() : "");
                    measure();
                    index_ = outer;
                },
                [&](const PExpression<Value>& e) { return Size(e); },
                [&](const PExpression<Value>& e) { return Emit(e).size; });
        });
    }

    // A cell is the chain of the clauses that may give it, as a function's
    // (Chained), the fallback its end.
    Code CellOf(const std::string& name, const Reference<Value>& definition, int slice, int row,
                int col, const Code& fallback) {
        std::vector<std::pair<std::string, Code>> guarded;
        std::optional<Code>                       otherwise;
        std::string                               ran, before;
        std::set<std::string>                     asked;
        const Setting<bool>                       chosen(chosen_, false);
        Walk(
            definition, slice, row, col, [](const Clause<Value>&) { return true; },
            [&](const Clause<Value>& clause) {
                const Setting<Frame>               own(frame_, {});
                const ParametersDefinition<Value>& p = clause.parameters;
                const std::optional<std::string>   condition =
                    p.guarded() ? Condition(p.guard()) : std::optional<std::string>("");
                Ran(ran, condition);
                if (p.guarded())
                    for (const std::string& status : asked_)
                        asked.insert(Joined(before, "", status));
                if (!condition) return false;
                const Code value = Chosen(Taken(clause.expression, *condition, before));
                if (!value.Scalar())
                    throw Reason("a cell of " + name + " must be a single value, not a " +
                                 value.size.Described());
                if (condition->empty())
                    otherwise = value;
                else
                    guarded.emplace_back(*condition, value);
                before += *condition + " ? 0.0 : ";
                return condition->empty();
            });
        Told(asked, otherwise.has_value());
        if (guarded.empty()) return otherwise ? *otherwise : fallback;
        Code last = otherwise ? *otherwise : Chosen(fallback);
        if (!otherwise && !ran.empty()) {
            last.cells[0] = Cell(Unless(ran, last.cells[0].text), primary);
            last.constant.reset();
        }
        return Chain(guarded, last, Extent{});
    }

    // A cell no clause gives is refused where a guard of its chain read a
    // limit that ran out, as the interpreter refuses it (DESIGN.md).
    void Ran(std::string& ran, const std::optional<std::string>& condition) const {
        if (condition && !condition->empty() && !unconverged_.empty()) Or(ran, unconverged_);
    }
    // Statuses joined, one that is a conjunction bracketed (C371).
    static void Or(std::string& ran, const std::string& status) {
        const bool both = status.find("&&") != std::string::npos;
        ran += (ran.empty() ? "" : " || ") + (both ? "(" + status + ")" : status);
    }
    // A chain with a clause after its guards has answered where their limits
    // ran out; one without is NaN there, which its caller's guard asks about.
    void Told(const std::set<std::string>& asked, bool answered) {
        if (statuses_ && !answered) statuses_->insert(asked.begin(), asked.end());
    }
    static std::string Unless(const std::string& ran, const std::string& cell) {
        return ran.empty() ? cell : "(" + ran + " ? NAN : " + cell + ")";
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
        Expansion                       given{{}, {}, scope_, expansion_, {}, {}};
        const std::vector<std::string>& names = p.parameters_names();
        for (std::size_t i = 0; i < call.parameters_expression().size(); ++i)
            given.values.emplace(names[i], Emit(call.parameters_expression()[i]));
        for (const auto& [parameter, argument] : call.parameters_dict())
            given.values.emplace(parameter, Emit(argument));
        Defaults(sequence, given);
        // 'arg_x': no name of the language has a '_', and no name of the
        // function's own begins so.
        Expansion inside{{}, {}, sequence.home ? sequence.home : scope_, nullptr, {}, {}};
        for (const std::string& parameter : names) {
            const Code& argument = given.values.at(parameter);
            inside.values.emplace(parameter, Array("arg_" + parameter, argument.size));
        }
        inside.values.insert(given.values.begin(), given.values.end());  // the sizes, constants
        return Converge(expression, name, "lim " + name, "terms", 0, names, given.values, inside,
                        [&](Walked& walked, std::map<int, Code>& bases, const auto& shaped) {
                            for (const Clause<Value>& clause : sequence.Clauses()) {
                                const ParametersDefinition<Value>& c = clause.parameters;
                                if (!c.indexed() || c.general()) continue;
                                if (c.guarded())
                                    throw Reason("a guarded base clause in a limit's terms");
                                const Setting<Frame> own(frame_, {});
                                bases[c.index()] = shaped(Emit(clause.expression));
                            }
                            walked.first = (bases.empty() ? 0 : bases.rbegin()->first) + 1;
                            temporaries_ = walked.iterated;
                            return Iterate(name, sequence, walked.first, shaped);
                        });
    }

    // A walk's terms, as locals of a function of their own given its
    // arguments: a limit's, or an unbounded sum's partial sums. 'walk' sets
    // its bases and answers its term at k_; 'counted' says how many of its
    // bases the interpreter counts among its terms, a sum's first partial
    // sum and none of a limit's.
    template <typename Terms>
    PExpression<Value> Converge(Expression<Value>* expression, const std::string& name,
                                const std::string& what, const std::string& terms, int counted,
                                const std::vector<std::string>&    names,
                                const std::map<std::string, Code>& given, Expansion& inside,
                                Terms walk) {
        // A limit's function fills arrays of rows and columns.
        for (const auto& [parameter, code] : given) {
            if (code.size.slices) throw Reason("a tensor in a limit, for now");
            if (!Still(code)) throw Reason("a derivative of a limit, for now");
        }
        bool constant = true;
        for (const auto& [parameter, code] : given) constant = constant && code.constant;
        Walked walked{name, what, names, 0, {0, 0}, &lists_.emplace_back(), &lists_.emplace_back()};
        Walked* const              outer_limit       = std::exchange(limit_, &walked);
        auto* const                outer_temporaries = std::exchange(temporaries_, walked.based);
        // Its names are its own: a step's t1_ is not its term before (C308).
        auto                       outer_nans        = std::exchange(nan_temporaries_, {});
        auto                       outer_reads       = std::exchange(read_parameters_, {});
        std::map<int, Code>        bases;
        Code                       general;
        const auto                 shaped = [&](const Code& code) {
            if (code.size.slices) throw Reason("a tensor in a limit, for now");
            if (walked.size.rows && code.size != walked.size)
                throw Reason("a limit of " + name + ", whose terms change size");
            walked.size = code.size;
            return code;
        };
        try {
            Inside(inside, [&] {
                const Setting<std::set<std::string>*> unread(statuses_, nullptr);
                general = walk(walked, bases, shaped);
                return Code{};
            });
        } catch (...) {
            limit_           = outer_limit;
            temporaries_     = outer_temporaries;
            nan_temporaries_ = std::move(outer_nans);
            read_parameters_.insert(outer_reads.begin(), outer_reads.end());
            throw;
        }
        limit_           = outer_limit;
        temporaries_     = outer_temporaries;
        nan_temporaries_ = std::move(outer_nans);
        const auto reads = std::exchange(read_parameters_, std::move(outer_reads));
        read_parameters_.insert(reads.begin(), reads.end());
        if (constant && reads.empty()) return Fold(expression);
        const int highest = bases.empty() ? 0 : bases.rbegin()->first;
        for (int lag = 1; lag <= walked.depth; ++lag)
            if (!bases.count(highest - lag + 1))
                throw Reason("a limit whose terms read back past its base clauses");

        const bool scalar = walked.size.count() == 1;
        // Named once its text is known: a limit written again, as a function
        // applied cell by cell writes it, is the same function.
        const std::string function  = "\x1f";
        const std::string first     = std::to_string(highest + 1);
        const std::string shape     = Dimensions(walked.size);
        const std::string tolerance = Double(Convergence<Value>::tolerance);
        // A term's cells, one assignment each, or the term itself.
        const auto each = [&](const std::string& indent, const std::string& to, const auto& from) {
            std::string out;
            for (std::size_t c = 0; c < walked.size.count(); ++c)
                out += indent + to + Subscript(walked.size, c) + " = " + from(c) + ";\n";
            return out;
        };
        std::string text;
        text += "/* " + what + ", as the interpreter takes it: its " + terms + " from index " +
                std::to_string(highest + 1 - counted) + " on, until\n * two agree within " +
                tolerance + (scalar ? "" : " in every cell") +
                " and so does what is left of the series, as the steps\n * shrink; NaN where "
                "that takes more than " +
                std::to_string(Convergence<Value>::max_terms) + " terms. */\n";
        // Asked with ran_, a scalar's answers 1 where its walk runs out and 0
        // where not; a matrix's answers so whatever is asked.
        text += "static inline double " + function + "(const " + module_ + "* m_" +
                (scalar ? ", int ran_" : "");
        for (const std::string& parameter : names) {
            const Code& argument = given.at(parameter);
            text += argument.Scalar() ? ", double arg_" + parameter
                                      : ", double arg_" + parameter + Dimensions(argument.size);
        }
        if (!scalar) text += ", double out_" + shape;
        text += ") {\n    (void)m_;\n";
        for (const std::string& parameter : names) text += "    (void)arg_" + parameter + ";\n";
        const int depth = std::max(walked.depth, 1);
        std::string based;
        for (int lag = 1; lag <= depth; ++lag) {
            const auto  base  = bases.find(highest - lag + 1);
            std::string value = scalar ? "0.0" : "{{0.0}}";
            if (base != bases.end())
                value = scalar ? base->second.cells[0].text : "{" + Rows(base->second) + "}";
            based += "    double t" + std::to_string(lag) + "_" + shape + " = " + value + ";\n";
        }
        // Each sequence walked beside it: its window, moved on as t1_ is.
        std::string moved;
        for (std::size_t b = 0; b < walked.beside.size(); ++b) {
            const auto&       window = walked.beside[b].window;
            const Code&       term   = *walked.beside[b].term;
            const std::size_t lags   = window.size();
            for (std::size_t lag = 1; lag <= lags; ++lag)
                based += "    double " + Window(b, lag) + Dimensions(term.size) + " = " +
                         (term.Scalar() ? window[lag - 1].cells[0].text
                                        : "{" + Rows(window[lag - 1]) + "}") +
                         ";\n";
            for (std::size_t lag = lags; lag > 1; --lag)
                moved += term.Scalar()
                             ? "        " + Window(b, lag) + " = " + Window(b, lag - 1) + ";\n"
                             : "        memcpy(" + Window(b, lag) + ", " + Window(b, lag - 1) +
                                   ", sizeof " + Window(b, lag) + ");\n";
            for (std::size_t c = 0; lags && c < term.size.count(); ++c)
                moved += "        " + Window(b, 1) + Subscript(term.size, c) + " = " +
                         term.At(c).text + ";\n";
        }
        const std::string term =
            scalar ? "        const double t_ = " + general.At(0).text + ";\n"
                   : each("        ", "t_", [&](std::size_t c) { return general.At(c).text; });
        const std::string iterate = Temporaries(*walked.iterated, term + moved, "        ") + term;
        // Where the walk ends, NaN, and what it answers asked whether it ran out.
        const auto end = [&](const std::string& in, const std::string& ran) {
            return scalar ? in + "return ran_ ? " + ran + " : NAN;\n"
                          : each(in, "out_", [](std::size_t) { return std::string("NAN"); }) + in +
                                "return " + ran + ";\n";
        };
        // The term kept, answered where the walk ends without closing.
        const auto kept = [&](const std::string& in) {
            return scalar
                       ? in + "if (held_) return ran_ ? 0.0 : kept_;\n"
                       : in + "if (held_) {\n" + in + "    memcpy(out_, kept_, sizeof kept_);\n" +
                             in + "    return 0.0;\n" + in + "}\n";
        };
        // A term with a NaN cell refuses the limit, as the interpreter's (DESIGN.md),
        // or past a term kept ends the walk on that one.
        const auto nan = [&](const std::string& t, const std::string& in, bool held) {
            const std::string cols = std::to_string(walked.size.cols);
            return scalar ? in + "if (isnan(" + t + ")) " +
                                end("", held ? "0.0 : held_ ? kept_" : "0.0")
                          : in + "for (int c_ = 0; c_ < " + std::to_string(walked.size.count()) +
                                "; ++c_)\n" + in + "    if (isnan(" + t + "[c_ / " + cols +
                                "][c_ % " + cols + "])) {\n" + (held ? kept(in + "        ") : "") +
                                end(in + "        ", "0.0") + in + "    }\n";
        };
        text += Temporaries(*walked.based, based + iterate + moved, "    ") + based;
        if (!bases.empty() &&
            (!bases.rbegin()->second.constant || Value::Nan(*bases.rbegin()->second.constant)))
            text += nan("t1_", "    ", false);
        if (!scalar) text += "    double t_" + shape + ";\n";
        text += "    double step_ = 0.0, before_ = 0.0, close_ = 0.0, closed_ = 0.0, kept_" +
                (scalar ? " = 0.0" : shape) + ";\n";
        text += "    int    started_ = " + std::string(bases.empty() ? "0" : "1") +
                ", stepped_ = 0, held_ = 0;\n";
        text +=
            "    for (long long k_ = " + first + "; k_ <= " +
            std::to_string(highest - counted + static_cast<int>(Convergence<Value>::max_terms)) +
            "; ++k_) {\n";
        text += iterate;
        text += nan("t_", "        ", true);
        text += "        if (started_) {\n";
        // A step within an ulp or two of the term may be an exact 0 rounded,
        // and is taken for one (C353). Each against max(1, |its term|), as
        // the interpreter measures it.
        const auto scale = [](const std::string& t) { return " / fmax(1.0, fabs(" + t + "))"; };
        const std::string cell = "t_[i_][j_]";
        const std::string noise =
            floats ? "" : " && d_ > 2 * DBL_EPSILON * fabs(" + cell + ")" + scale(cell);
        if (scalar) {
            text += "            step_ = fabs(t_ - t1_);\n";
            if (!floats)
                text += "            if (step_ <= 2 * DBL_EPSILON * fabs(t_)) step_ = 0.0;\n";
            text += "            close_ = step_ == 0 ? 0.0 : step_ / fabs(t_);\n";
            text += "            step_ = step_" + scale("t_") + ";\n";
        } else {
            text += "            step_ = 0.0;\n            close_ = 0.0;\n";
            if (floats) text += "            int near_ = 1;\n";
            text += "            for (int i_ = 0; i_ < " + std::to_string(walked.size.rows) +
                    "; ++i_)\n";
            text += "                for (int j_ = 0; j_ < " + std::to_string(walked.size.cols) +
                    "; ++j_) {\n";
            text += "                    const double d_ = fabs(t_[i_][j_] - t1_[i_][j_])" +
                    scale(cell) + ";\n";
            text += "                    if (isnan(d_) || (d_ > step_" + noise + ")) step_ = d_;\n";
            text +=
                "                    const double r_ = d_ == 0 ? 0.0 : fabs(t_[i_][j_] - "
                "t1_[i_][j_]) / fabs(" +
                cell + ");\n";
            text += std::string("                    if (isnan(r_) || (r_ > close_") +
                    (floats ? "" : " && r_ > 2 * DBL_EPSILON") + ")) close_ = r_;\n";
            if (floats)
                text += "                    near_ &= d_ <= 2 * FLT_EPSILON * fabsf(t_[i_][j_])" +
                        scale(cell) + " && isfinite(t_[i_][j_]);\n";
            text += "                }\n";
        }
        // A float closes on finite floats a unit or two apart (DESIGN.md, a float target);
        // a step of 0 is left to the rule below, which a pause does not pass (C279).
        if (floats)
            text += scalar ? "            if (step_ > 0 && step_ <= 2 * FLT_EPSILON * fabsf(t_)" +
                                 scale("t_") +
                                 " && isfinite(t_) && stepped_) return ran_ ? 0.0 : t_;\n"
                           : "            if (near_ && step_ > 0 && stepped_) {\n"
                             "                memcpy(out_, t_, sizeof t_);\n"
                             "                return 0.0;\n            }\n";
        // Within the tolerance of each cell's own size, the limit; absolute
        // below 1, kept as the answer of a walk that never gets there.
        const auto settles = [&](const std::string& s, const std::string& b) {
            return s + " <= " + tolerance + " && stepped_ &&\n                (!(" + b +
                   " > 0) || (" + s + " / " + b + " < 1 &&\n                 " + s + " * (" + s +
                   " / " + b + ") / (1 - " + s + " / " + b + ") <= " + tolerance + " &&\n" +
                   "                 (" + s + " > 0 || " + b + " <= " + tolerance + ")))";
        };
        text += "            if (" + settles("close_", "closed_") + ") {\n";
        text += scalar
                    ? "                return ran_ ? 0.0 : t_;\n"
                    : "                memcpy(out_, t_, sizeof t_);\n                return 0.0;\n";
        text += "            }\n            if (!held_ && " + settles("step_", "before_") + ") {\n";
        text += scalar ? "                kept_ = t_;\n"
                       : "                memcpy(kept_, t_, sizeof t_);\n";
        text += "                held_ = 1;\n            }\n";
        text +=
            "            before_  = step_;\n            closed_  = close_;\n"
            "            stepped_ = 1;\n        }\n";
        text += "        started_ = 1;\n";
        for (int lag = depth; lag > 1; --lag) {
            const std::string to   = "t" + std::to_string(lag) + "_",
                              from = "t" + std::to_string(lag - 1) + "_";
            text += scalar ? "        " + to + " = " + from + ";\n"
                           : "        memcpy(" + to + ", " + from + ", sizeof " + to + ");\n";
        }
        text += moved;
        text +=
            scalar ? "        t1_ = t_;\n    }\n" : "        memcpy(t1_, t_, sizeof t_);\n    }\n";
        text += kept("    ") + end("    ", "1.0") + "}\n\n";
        auto [named, fresh] =
            limit_names_.emplace(text, module_ + "_lim" + std::to_string(limits_.size()));
        if (fresh)
            limits_.push_back(text.replace(text.find(function), function.size(), named->second));
        // NaN where its walk does not converge (C336).
        nan_functions_.insert(named->second);

        // Where it is read: a matrix argument, or a matrix answer, is an
        // array of the step's, or of the enclosing limit's.
        std::string arguments;
        for (const std::string& parameter : names) {
            const Code& argument = given.at(parameter);
            if (argument.Scalar()) {
                arguments += ", " + argument.cells[0].text;
                continue;
            }
            const std::string rows = Rows(argument);
            arguments += ", " + Declare("arg\x1f" + rows, [&](const std::string& t) {
                             return std::vector<std::string>{
                                 "double " + t + Dimensions(argument.size) + " = {" + rows + "};"};
                         });
        }
        const std::string called = named->second + "(m_" + (scalar ? ", 0" : "") + arguments;
        if (statuses_) {
            // A matrix's answer unread, in an array: C++ has no compound literal (C393).
            const std::string unread =
                scalar ? "" : Declare("unread\x1f" + shape, [&](const std::string& t) {
                    return std::vector<std::string>{"double " + t + shape + ";"};
                });
            const std::string status = named->second + "(m_" +
                                       (scalar ? ", 1" + arguments : arguments + ", " + unread) +
                                       ") != 0.0";
            statuses_->insert(status);
            what_[status] = {{what, "runs out"}};
            if (read_) read_->push_back({what, "runs out"});
        }
        if (scalar) return Answer(Looping(Of(Cell(called + ")", primary))));
        const std::string out = Declare("lim\x1f" + called, [&](const std::string& t) {
            return std::vector<std::string>{"double " + t + shape + ";", called + ", " + t + ");"};
        });
        nan_temporaries_.insert(out);
        return Answer(Array(out, walked.size));
    }

    // A walked sequence's term at k_, its clauses tried as Chained tries them
    // at an index: a base clause from where the walk starts, and the clause
    // that always applies NaN below the lowest, as where no clause does (C86).
    template <typename Shaped>
    Code Iterate(const std::string& name, const Reference<Value>& sequence, int first,
                 const Shaped& shaped) {
        for (const Clause<Value>& clause : sequence.Clauses())
            if (clause.parameters.general()) index_ = clause.parameters.index_name();
        if (index_.empty())
            throw Reason(name + " has no general clause" +
                         (name == limit_->name ? ", so it has no limit" : ""));
        const Setting<std::string>                text(index_text_, "(double)k_");
        std::vector<std::pair<std::string, Code>> guarded;
        std::optional<Code>                       otherwise;
        for (const bool guard : {true, false})
            for (const Clause<Value>& clause : sequence.Clauses()) {
                const ParametersDefinition<Value>& c    = clause.parameters;
                const bool                         base = !c.general();
                if (otherwise || (c.guarded() || base) != guard || (base && c.index() < first))
                    continue;
                if (base && c.guarded()) throw Reason("a guarded base clause in a limit's terms");
                const Setting<std::string>       named(index_, base ? index_ : c.index_name());
                const Setting<Frame>             own(frame_, {});
                const std::optional<std::string> condition =
                    base    ? "k_ == " + std::to_string(c.index())
                    : guard ? Condition(c.guard())
                            : std::optional<std::string>("");
                if (!condition) continue;
                const Code value = shaped(Emit(clause.expression));
                if (condition->empty())
                    otherwise = value;
                else
                    guarded.emplace_back(*condition, value);
            }
        const Clause<Value>* const lowest = sequence.EndBase(true);
        if (otherwise && lowest && lowest->parameters.index() > first) {
            for (Cell& cell : otherwise->cells)
                cell = Cell(
                    "k_ < " + std::to_string(lowest->parameters.index()) + " ? NAN : " + cell.text,
                    0);
            otherwise->constant.reset();
        }
        const Extent size = otherwise         ? otherwise->size
                            : guarded.empty() ? Extent{}
                                              : guarded.front().second.size;
        for (const auto& [condition, value] : guarded)
            if (value.size != size)
                throw Reason("the clauses of " + name + " have different shapes");
        return Chain(guarded, otherwise, size);
    }

    // Whether code read in a limit's terms changes as it walks.
    bool Walks(std::string read) {
        const Walked& walked = *limit_;
        for (const Temporary* temporary : Kept(*walked.based, read))
            for (const std::string& line : temporary->lines) read += line + ' ';
        std::set<std::string> words, moving{"k_"};
        Words(read, words);
        for (int l = 1; l <= walked.depth; ++l) moving.insert("t" + std::to_string(l) + "_");
        for (const Temporary& temporary : *walked.iterated) moving.insert(temporary.name);
        for (std::size_t b = 0; b < walked.beside.size(); ++b)
            for (std::size_t l = 1; l <= walked.beside[b].window.size(); ++l)
                moving.insert(Window(b, l));
        return std::ranges::any_of(words, [&](const std::string& w) { return moving.contains(w); });
    }

    // 'A^(k+c)' in a limit's terms, k its walking index and A the same at
    // each: walked beside it, each power the one before times A from A^(f+c)
    // set before the walk, f the index before its first: one product a term
    // where squaring takes log k. Not where f+c < 0, an inverse the
    // interpreter may never take.
    std::optional<Code> Climbed(const Code& base, const PExpression<Value>& exponent) {
        const std::optional<int> c = Above(exponent, "a power");
        std::string              read;
        for (const Cell& cell : base.cells) read += cell.text + ' ';
        if (!c || Walks(read)) return std::nullopt;
        return Climb("^" + std::to_string(*c) + " " + Rows(base), base,
                     [&](int f) { return Power(base, Literal(Value(Number(f + *c)))); });
    }

    // c where an index is a limit's walking index plus c, none where f+c < 0,
    // f the index before its first.
    std::optional<int> Above(const PExpression<Value>& index, const std::string& written) {
        int c = 0;
        try {
            c = Offset(index, written);
        } catch (const Reason&) {
            return std::nullopt;
        } catch (const Fix&) {
            return std::nullopt;
        }
        if (limit_->first - 1 + c < 0) return std::nullopt;
        return c;
    }

    // A term walked beside a limit's, by its key: 'at(f)' before the walk,
    // f the index before its first, then each the one before times factor.
    Code Climb(const std::string& key, const Code& factor, const std::function<Code(int)>& at) {
        Walked&     walked = *limit_;
        std::size_t b      = 0;
        while (b < walked.beside.size() &&
               (walked.beside[b].function || walked.beside[b].args != key))
            ++b;
        if (b < walked.beside.size()) return *walked.beside[b].term;
        Walked::Beside& beside = walked.beside.emplace_back(Walked::Beside{nullptr, key, {}});
        {
            const Setting<std::vector<Temporary>*> before(temporaries_, walked.based);
            beside.window.push_back(SharedAll(at(walked.first - 1)));
        }
        const Setting<std::vector<Temporary>*> in(temporaries_, walked.iterated);
        Code term = Product(Array(Window(b, 1), factor.size), factor);
        for (Cell& cell : term.cells)
            cell = Atom(Declare(cell.text, [&](const std::string& t) {
                return std::vector<std::string>{"const double " + t + " = " + cell.text + ";"};
            }));
        return *(beside.term = term);
    }

    // 'sum_(q=a) f(q)', 'prod_(q=a) f(q)': the limit of its partial sums as
    // the interpreter takes it, walked as a limit's terms are, its base f(a)
    // and its term at k_ t1_ plus, or times, f(k_). What f reads that does not
    // move as q does is computed once where the sum is, the walk's arguments.
    PExpression<Value> Sum(SeriesExpression<Value>* expression) {
        const int first =
            Whole(Known(expression->Lower(), "a sum or a product whose bounds are not constants"));
        const std::string& q   = expression->Index();
        const std::string what = std::string(expression->Product() ? "prod_(" : "sum_(") + q + "=" +
                                 std::to_string(first) + ")";
        std::vector<PExpression<Value>> unmoved;
        std::set<std::string>           bound;  // a local is its term's
        Binding(*expression->Body(), bound);
        const auto carried = [&] {
            for (const std::string& name : bound)
                frame_.refused[name] =
                    ", carried past a term of a sum without an upper bound, for now";
        };
        carried();
        bound.insert(q);
        Unmoved(expression->Body(), bound, what, unmoved);
        std::map<std::string, Code>              given;
        std::map<const Expression<Value>*, Code> lifted;
        std::vector<std::string>                 names;
        for (const PExpression<Value>& part : unmoved) {
            const Code code = Emit(part);
            if (code.constant && code.part.empty()) {
                lifted.emplace(part.get(), code);
                continue;
            }
            names.push_back(std::to_string(names.size()));
            lifted.emplace(part.get(), Array("arg_" + names.back(), code.size));
            given.emplace(names.back(), code);
        }
        Expansion inside{{}, {}, scope_, nullptr, {}, {}};
        const Setting<std::map<const Expression<Value>*, Code>> in(lifted_, lifted);
        return Converge(
            expression, what, what, expression->Product() ? "partial products" : "partial sums", 1,
            names, given, inside,
            [&](Walked& walked, std::map<int, Code>& bases, const auto& shaped) {
                carried();
                places_[q]   = Value(Number(first));
                bases[first] = shaped(Emit(expression->Body()));
                places_.erase(q);
                walked.first = first + 1;
                temporaries_ = walked.iterated;
                index_       = q;
                const Setting<std::string> text(index_text_, "(double)k_");
                const Code                 term  = shaped(Emit(expression->Body()));
                const Code                 total = Array("t1_", walked.size);
                return expression->Product() ? Product(total, term) : Broadcast(total, term, Added);
            });
    }

    // The largest parts of a sum's term that read neither its index nor a
    // name bound inside it. The rest is compiled in the walk's function, which
    // does not see an instance's own definitions: their values are its caller's.
    void Unmoved(const PExpression<Value>& e, const std::set<std::string>& bound,
                 const std::string& what, std::vector<PExpression<Value>>& out) {
        if (!e) return;
        if (std::none_of(bound.begin(), bound.end(),
                         [&](const std::string& name) { return Mentions(*e, name); })) {
            out.push_back(e);
            return;
        }
        if (dynamic_cast<MemberExpression<Value>*>(e.get())) return;
        if (const auto* call = dynamic_cast<FuncExpression<Value>*>(e.get())) {
            if (Local(call->Name()).first)
                throw Reason(call->Name() + ", a function of an instance, called at " + what +
                             "'s index, for now");
            for (const auto& argument : call->Call().parameters_expression())
                Unmoved(argument, bound, what, out);
            for (const auto& [given, argument] : call->Call().parameters_dict())
                Unmoved(argument, bound, what, out);
            Unmoved(call->Call().subexpr(), bound, what, out);
            return;
        }
        const auto* series = dynamic_cast<SeriesExpression<Value>*>(e.get());
        const auto* grad   = dynamic_cast<GradExpression<Value>*>(e.get());
        for (const PExpression<Value>& child : e->Children()) {
            std::set<std::string> inner = bound;
            if (series && child == series->Body()) inner.insert(series->Index());
            if (grad && child == grad->Body())
                inner.insert(grad->Variables().begin(), grad->Variables().end());
            Unmoved(child, inner, what, out);
        }
    }

    static std::string Window(std::size_t b, std::size_t lag) {
        return "w" + std::to_string(b) + "_" + std::to_string(lag) + "_";
    }

    // 't(x)_k' in a limit's terms, k its walking index less a constant: a
    // sequence walked beside the limit's own, in its loop, its term at k_ a
    // temporary and those before it a window, 'w0_1_', set before the loop.
    // Its arguments are computed there too, so one that moves as the walk
    // does is refused.
    Code Beside(const std::string& name, const Reference<Value>& function, Expansion& expansion,
                int lag) {
        Walked&           walked  = *limit_;
        const std::string read_at = name + "_(...): read at " + walked.what + "'s index ";
        std::string       read;
        for (const auto& [given, value] : expansion.values) {
            if (!Still(value))
                throw Reason(name + "_(...): a derivative of a term " + walked.what +
                             " walks, for now");
            for (const Cell& cell : value.cells) read += cell.text + ' ';
        }
        if (Walks(read)) throw Reason(read_at + "with arguments that change as it walks");
        const std::string args = Given(expansion);
        std::size_t       b    = 0;
        while (b < walked.beside.size() &&
               (walked.beside[b].function != &function || walked.beside[b].args != args))
            ++b;
        if (b == walked.beside.size()) {
            // Its own terms read with other arguments would walk without end.
            for (const Walked::Beside& other : walked.beside)
                if (other.function == &function && !other.term)
                    throw Reason(read_at + "with other arguments in its own terms");
            Walked::Beside& beside =
                walked.beside.emplace_back(Walked::Beside{&function, args, expansion});
            const Setting<std::vector<Temporary>*> in(temporaries_, walked.iterated);
            Code                                   term = Inside(beside.expansion, [&] {
                return Iterate(name, function, walked.first, std::identity{});
            });
            // Each its own temporary: a window moves on from it after another moved.
            for (Cell& cell : term.cells)
                cell = Atom(Declare(cell.text, [&](const std::string& t) {
                    return std::vector<std::string>{"const double " + t + " = " + cell.text + ";"};
                }));
            beside.term = term;
        }
        Walked::Beside& beside = walked.beside[b];
        if (lag == 0 && !beside.term) throw Reason(name + " is defined by itself");
        const Clause<Value>* const lowest = function.EndBase(true);
        while (beside.window.size() < static_cast<std::size_t>(lag)) {
            const Setting<std::vector<Temporary>*> before(temporaries_, walked.based);
            const Setting<std::string>             windowing(walked.windowing, name);
            const int  at    = walked.first - 1 - static_cast<int>(beside.window.size());
            const bool below = lowest && at < lowest->parameters.index();
            Code       term;
            try {
                term = Term(name, function, beside.expansion, at);
            } catch (const Reason& reason) {
                // No clause below its lowest base: NaN, as a step says (C86),
                // in its terms' shape; a guard's term the step cannot compute
                // is refused, which the interpreter answers.
                if (!below ||
                    reason.what() != name + " has no clause for index " + std::to_string(at))
                    throw;
                term = Term(name, function, beside.expansion, lowest->parameters.index());
                for (Cell& cell : term.cells) cell = Atom("NAN");
                term.constant.reset();
            }
            beside.window.push_back(SharedAll(term));
        }
        if (beside.term && !beside.window.empty() && beside.term->size != beside.window[0].size)
            throw Reason("a limit reading " + name + ", whose terms change size");
        return lag ? Array(Window(b, static_cast<std::size_t>(lag)), beside.window[0].size)
                   : *beside.term;
    }

    // A term of the sequence a limit walks, read back from one of its own:
    // a local of its function, 't1_' the one before. Its own are its
    // arguments' values, not their names, which a call rebinds (C352).
    Code Earlier(const ParametersCall<Value>& call) {
        // A window is set before the loop, where the limit has no term to read.
        if (!limit_->windowing.empty())
            throw Reason(limit_->windowing + "_(...): its terms before the walk read lim " +
                         limit_->name + "'s own");
        const auto same = [&](const PExpression<Value>& argument, const std::string& parameter) {
            const Code code = Emit(argument), own = Array("arg_" + parameter, code.size);
            return std::equal(code.cells.begin(), code.cells.end(), own.cells.begin(),
                              own.cells.end(),
                              [](const Cell& a, const Cell& b) { return a.text == b.text; });
        };
        const auto& arguments = call.parameters_expression();
        bool own = arguments.size() + call.parameters_dict().size() == limit_->parameters.size();
        for (std::size_t i = 0; own && i < arguments.size(); ++i)
            own = same(arguments[i], limit_->parameters[i]);
        for (const auto& [parameter, argument] : call.parameters_dict())
            own = own && same(argument, parameter);
        if (!own) throw Reason(limit_->name + "'s term read with other arguments than its own");
        const int lag = Lag(call.subexpr(), limit_->name);
        // A walked term is computed before the limit's own.
        for (auto beside = limit_->beside.rbegin(); lag == 0 && beside != limit_->beside.rend();
             ++beside)
            if (!beside->term)
                throw Reason(beside->function->Name() + "_(...): reads lim " + limit_->name +
                             "'s term at its index");
        if (lag == 0) throw Reason(limit_->name + " is defined by itself");
        if (!limit_->size.rows) throw Reason("a limit whose terms read back past its base clauses");
        limit_->depth = std::max(limit_->depth, lag);
        return Array("t" + std::to_string(lag) + "_", limit_->size);
    }

    PExpression<Value> visit(FuncExpression<Value>* expression) override {
        const std::string&           name  = expression->Name();
        const ParametersCall<Value>& call  = expression->Call();
        const bool calls = !call.parameters_expression().empty() || !call.parameters_dict().empty();
        situated_ += call.limit() || !calls;
        if (call.limit()) return Limit(expression, name, call);
        if (const auto local = frame_.values.find(name); !own_ && local != frame_.values.end())
            return !calls && !call.subexpr() ? Answer(local->second)
                                             : throw Reason(name + (calls ? " takes no arguments"
                                                                          : " is not a sequence"));
        if (std::set<std::string> bound; call.subexpr()) {  // asked more than once (C409)
            Binding(*call.subexpr(), bound);
            if (!bound.empty()) throw Reason(*bound.begin() + ", bound in a term's index, for now");
        }
        if (limit_ && name == limit_->name && call.subexpr()) return Answer(Earlier(call));
        if (calls && !own_) {
            const auto [local, in] = Local(name);
            if (local) return Answer(Call(name, *local, call, in));
        }
        const Found        found = Lookup(name);
        const std::string& key   = found.key;
        if (call.limit()) throw Reason("a limit");
        if ((name == "floor" || name == "re" || name == "im") &&
            found.where == &definitions_.Builtins())
            return Builtin(expression);
        if (found.definition) ReadWhole(key, *found.definition, calls, call.subexpr() != nullptr);
        if (calls) {
            if (!found.definition) throw Reason(key + " is not defined");
            if (found.where == &definitions_.Builtins() && !call.subexpr())
                return Prelude(key, *found.definition, call);
            return Answer(Call(key, *found.definition, call, nullptr));
        }
        return Answer(Term(key, found.definition, call));
    }

    // A grad by several names read whole is refused in the interpreter's words.
    static void ReadWhole(const std::string& key, const Reference<Value>& definition, bool called,
                          bool indexed) {
        const auto* group = ReferenceStack<Value>::Group(definition);
        if (!group) return;
        if (indexed && !IsSequence(definition)) throw Reason(key + " is not a sequence");
        throw Reason(key + " is a gradient by several names; read one of its parts (" + key +
                     (called ? "(...)." : ".") + group->Variable() + (indexed ? "_(...)" : "") +
                     ")");
    }

    // A term of a sequence, read at the index the call gives.
    Code Term(const std::string& key, const Reference<Value>* definition,
              const ParametersCall<Value>& call) {
        std::set<std::string>* const          asking = statuses_;
        const Setting<std::set<std::string>*> unread(statuses_, nullptr);  // a term's own
        if (limit_)
            throw Reason(key + "_...: another sequence's term in " +
                         (limit_->what == "lim " + limit_->name ? "a limit" : limit_->what) +
                         "'s terms");
        if (!reading_ && !basing_)
            throw Reason(key +
                         "_...: a term read outside a general clause; write its value, or "
                         "pass it as a parameter");
        if (definition && !IsSequence(*definition)) throw Reason(key + " is not a sequence");
        Sequence& read = sequences_[key];
        // One the model does not define, the session's, is computed in its step too.
        if (definition && !read.definition) {
            Define(key, *definition);
            read.session = true;
        }
        if (!definition) {
            Unreserved(key);
            if (!read.size.rows) read.size = Extent{};
        }
        if (!reading_) return Seeded(key, read, call.subexpr());
        if (const auto sample = Sampled(call.subexpr())) {
            Flag(asking, key, "\x0e" + std::to_string(sample->second) + "\x0f");
            return Sample(key, read, sample->first, sample->second);
        }
        if (const auto hold = Held(call.subexpr())) {
            Code code = Holding(key, read, *hold);
            Flag(asking, key, "");  // its status at the term held is not kept
            return code;
        }
        const int lag = Lag(call.subexpr(), key) + shift_;
        // A cycle is told by its reads (Reaches), and this one, which an
        // early term's are not, is recorded once what it reads is compiled.
        pending_.emplace_back(shift_ && early_ >= 0 ? nullptr : reading_, key);
        const struct Pop {
            decltype(pending_)& pending;
            ~Pop() { pending.pop_back(); }
        } pop{pending_};
        Bases(key);
        if (!read.size.rows) Compile(key);
        // Refused by its rate once the reader's is known (Rate), before one
        // with no base clause is computed again.
        if (&read != reading_ && read.period > 1) {
            reading_->reads[key].insert(lag);
            return Array("m_->" + key + "[0]", read.size);
        }
        const auto part =
            static_cast<std::size_t>(std::ranges::find(read.parts, key) - read.parts.begin());
        Flag(asking, key, std::to_string(lag));
        if (lag > 0 && ClosedForm(read) && !histories_.count(key)) return At(read, lag, part);
        (shift_ && early_ >= 0 ? earlies_[static_cast<std::size_t>(early_)].reads
         : deferring_          ? reading_->deferred
                               : reading_->reads)[key]
            .insert(lag);
        if (clause_reads_) (*clause_reads_)[key].insert(lag);
        Code code  = Array("m_->" + key + "[" + std::to_string(lag) + "]", read.size);
        code.whole = true;
        // Before its window holds the term, one with no base clause is
        // computed again at that index, as the interpreter answers it there
        // (DESIGN.md, C71); Checked keeps whichever the reader needs. A
        // recurrence's are folded by init instead (C343).
        if (lag > 0 && Recomputed(read) && read.compiled && !Historied(key) &&
            !Reaches(key, reading_)) {
            earlies_.push_back({key, lag, reading_, {}, early_});
            const int  id          = static_cast<int>(earlies_.size() - 1);
            const int  outer       = std::exchange(early_, id);
            const Code back        = At(read, lag, part);
            early_                 = outer;
            const std::string mark = "\x04" + std::to_string(id) + "\x05";
            code.whole             = false;
            for (std::size_t c = 0; c < code.cells.size(); ++c)
                code.cells[c].text = mark + back.At(c).text + "\x06" + code.cells[c].text + "\x07";
        }
        return code;
    }

    // Where a guard's truth is NaN, an optional input's flag says why, by the
    // name the guard writes; an inner one's argument asks too whether a chain
    // it reads gave the term (DESIGN.md, an inner instance's optional input).
    void Flag(std::set<std::string>* asking, const std::string& key, const std::string& at) {
        const std::string& label = scope_->label;
        const std::string  written =
            key.starts_with(label + ".") ? key.substr(label.size() + 1) : key;
        const Reference<Value>* read = sequences_.at(key).definition;  // a chain that may give none
        const bool chain = asking && asking == given_ && read && !read->input && !ByCells(*read) &&
                           std::ranges::none_of(read->Clauses(), [](const Clause<Value>& c) {
                               return c.parameters.general() && !c.parameters.guarded();
                           });
        // Refused where a guard reads the input that is given it.
        if (chain && at != "0") {
            back_[within_] = within_ + " is given " + key +  // both named as Qualified does
                             (at.empty() ? "_(...), whose chain may give none, held, for now"
                                         : "_(" + index_ + "-" + at +
                                               "), whose chain may give none, read back in time, "
                                               "for now");
            return;
        }
        if (asking && back_.count(key)) throw Reason(back_.at(key));
        if (!asking || at.empty() || !(chain || Optional(key))) return;
        const std::string status = "!m_->" + key + (chain ? "_given_" : "_present[" + at + "]");
        if (chain) chains_.insert(key);
        asking->insert(status);
        what_[status] = {{written, "is absent"}};
        if (read_) read_->push_back({written, "is absent"});
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
        return sequence.definition && sequence.bases.empty() &&
               std::ranges::all_of(sequence.definition->Clauses(),  // no term's own cells
                                   [](const auto& c) { return c.parameters.general(); }) &&
               sequence.definition->passes.empty();  // an inner optional input is absent there
    }

    std::string Marked(Reads& read) {
        if (read.empty()) return std::string();
        checks_.push_back({read, nullptr, false, nullptr, std::string(), {}, 0});
        return "\x02" + std::to_string(checks_.size() - 1) + "\x03";
    }

    // A term computed again 'lag' before the reader's index, its clauses
    // tried as the interpreter tries them. What each guard and value reads is
    // read that much further back, and checked where it is read: before it
    // exists the interpreter reports it, and the term is NaN.
    Code At(const Sequence& sequence, int lag, std::size_t part = 0) {
        std::vector<std::pair<std::string, Code>> guarded;
        std::optional<Code>                       otherwise;
        const Home                                home(*this, sequence.definition);
        const std::string    index = std::exchange(index_, std::string());
        const std::string    text =
            std::exchange(index_text_, "(double)(m_->index_ - " + std::to_string(lag) + ")");
        const auto   places = std::exchange(places_, {});
        Reads* const reads  = std::exchange(clause_reads_, nullptr);
        const int    shift  = std::exchange(shift_, lag);
        const auto   mark   = [&](Reads& read) { return Marked(read); };
        const bool   by     = ByCells(*sequence.definition);
        for (const bool guard : {true, false}) {
            for (const Clause<Value>& clause : sequence.definition->Clauses()) {
                const ParametersDefinition<Value>& p = clause.parameters;
                if (by || otherwise || !p.general() || p.guarded() != guard) continue;
                const Setting<Frame> own(frame_, {});
                index_ = p.index_name();
                Reads guard_reads, value_reads;
                clause_reads_ = &guard_reads;
                const std::optional<std::string> condition =
                    guard ? Condition(p.guard()) : std::optional<std::string>("");
                if (!condition) continue;
                clause_reads_           = &value_reads;
                const std::string test  = mark(guard_reads) + *condition;
                std::vector<Code> parts;
                const Setting<std::pair<const GradExpression<Value>*, std::vector<Code>*>> all(
                    grouped_, {ReferenceStack<Value>::Group(*sequence.definition), &parts});
                Code value = Emit(clause.expression);
                if (!parts.empty()) value = parts[part];
                if (const std::string check = mark(value_reads); !check.empty())
                    for (Cell& cell : value.cells) cell = Cell(check + cell.text, 0);
                if (condition->empty())
                    otherwise = value;
                else
                    guarded.emplace_back(test, value);
            }
        }
        // A term by its cells, each cell's chain as the step's (C412).
        if (by) {
            Reads whole;
            clause_reads_ = &whole;
            for (const Clause<Value>& clause : sequence.definition->Clauses())
                if (clause.parameters.general() && !clause.parameters.cells()) {
                    index_    = clause.parameters.index_name();
                    otherwise = Emit(clause.expression);
                }
            clause_reads_ = nullptr;
            if (otherwise)
                for (Cell& cell : otherwise->cells) cell = Cell(mark(whole) + cell.text, 0);
            const auto every = [](const Clause<Value>& c) { return c.parameters.general(); };
            const auto [shape, cells] = TermCells(sequence.definition->Name(), *sequence.definition,
                                                  every, otherwise, nullptr, true);
            otherwise                 = shape;
            for (const std::string& cell : cells) otherwise->cells.push_back(Cell(cell, 0));
        }
        index_        = index;
        index_text_   = text;
        places_       = places;
        clause_reads_ = reads;
        shift_        = shift;
        return Chain(guarded, otherwise, sequence.size);
    }

    static std::string Subscript(std::size_t i, std::size_t j) {
        return "[" + std::to_string(i) + "][" + std::to_string(j) + "]";
    }
    // Cell c's of an array of this size, row by row and slice after slice, or
    // none for a single value; a tensor keeps its slice, as it keeps ';;'.
    static std::string Subscript(Extent size, std::size_t c) {
        if (size.count() == 1 && !size.slices) return "";
        const std::string at = Subscript(c / size.cols % size.rows, c % size.cols);
        return size.slices ? "[" + std::to_string(c / (size.rows * size.cols)) + "]" + at : at;
    }

    // A matrix as C initialises an array, '{a, b}, {c, d}', or a tensor's slice.
    static std::string Rows(const Code& code, std::size_t b = 0) {
        std::string rows;
        for (std::size_t i = 0; i < code.size.rows; ++i) {
            rows += i ? ", {" : "{";
            for (std::size_t j = 0; j < code.size.cols; ++j)
                rows += (j ? ", " : "") + code.At(i, j, b).text;
            rows += "}";
        }
        return rows;
    }

    // The cells of a C array, 'name[i][j]', or 'name' alone for a single value.
    static Code Array(const std::string& name, Extent size) {
        Code code;
        code.size = size;
        for (std::size_t c = 0; c < size.count(); ++c)
            code.cells.push_back(Atom(name + Subscript(size, c)));
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
            const auto* times = dynamic_cast<MultExpression<Value>*>(e.get());
            if (!times) return std::nullopt;
            for (const auto& [side, other] : {std::pair(times->m_e1(), times->m_e2()),
                                              std::pair(times->m_e2(), times->m_e1())}) {
                const auto* ref = dynamic_cast<RefExpression<Value>*>(other.get());
                if (!ref || ref->Name() != index_) continue;
                if (const auto a = Constant(side); a && *a > 1) return a;
            }
            return std::nullopt;
        };
        if (const auto a = multiple(index)) return std::pair(*a, 0);
        const auto* plus = dynamic_cast<AddExpression<Value>*>(index.get());
        if (!plus) return std::nullopt;
        for (const auto& [side, other] :
             {std::pair(plus->m_e1(), plus->m_e2()), std::pair(plus->m_e2(), plus->m_e1())}) {
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
        if (const auto* plus = dynamic_cast<const AddExpression<Value>*>(at)) {
            if (const auto d = Constant(plus->m_e2())) {
                hold.d = *d;
                at     = plus->m_e1().get();
            } else if (const auto e = Constant(plus->m_e1())) {
                hold.d = *e;
                at     = plus->m_e2().get();
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
        const auto* plus = dynamic_cast<AddExpression<Value>*>(ratio->m_e1().get());
        if (!plus) return std::nullopt;
        for (const auto& [side, other] :
             {std::pair(plus->m_e1(), plus->m_e2()), std::pair(plus->m_e2(), plus->m_e1())}) {
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
        if (!read.size.rows) Compile(key);
        seeding->seeds.push_back({key, base, *at});
        return Array("m_->" + key + "[\x19" + std::to_string(seeding->seeds.size() - 1) + "\x1a]",
                     read.size);
    }

    // Each base's reads as lags, once the steps computing the bases are known
    // and where each sequence read starts.
    void Seeds(const std::string& name, Sequence& sequence) {
        for (Seed& seed : sequence.seeds) {
            const Sequence& read = sequences_.at(seed.read);
            const int       step = sequence.period * seed.index + sequence.phase;
            seed.lag             = step - seed.at;
            if (seed.lag < 0)
                throw Refusal(name, seed.read + "_(...): a term after the one being computed");
            if (read.period != 1)
                throw Refusal(name, seed.read + "_(...): read every step, and " + seed.read +
                                        " is computed every " + std::to_string(read.period));
            if (seed.at < read.start && !Historied(seed.read))
                throw Refusal(name, name + "_" + std::to_string(seed.index) + " reads " +
                                        seed.read + "_" + std::to_string(seed.at) +
                                        ", before it starts at " + std::to_string(read.start));
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
        if (!read.size.rows) Compile(key);
        if (reading_->period != 1 && reading_->period != a)
            throw Reason(written + ": read every " + std::to_string(a) + " steps, and " + within_ +
                         " is computed every " + std::to_string(reading_->period));
        reading_->period = a;
        reading_->samples[key].insert(b);
        return Array("m_->" + key + "[\x0e" + std::to_string(b) + "\x0f]", read.size);
    }

    // A hold: where it reads in the held sequence's window depends on that
    // sequence's phase, so it is marked and set when the step is written.
    // In a term computed again, it is the same hold that much earlier.
    Code Holding(const std::string& key, Sequence& read, Hold hold) {
        const std::string written = key + "_(...)";
        if (right_) throw Reason(written + ": a hold on the right of 'and' or 'or'");
        // A guard's is read wherever the guard is tested (C378).
        if ((deferring_ && !guarding_) || limit_)
            throw Reason(written + ": a hold where a term is computed again");
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
        code.size = read.size;
        for (std::size_t c = 0; c < read.size.count(); ++c)
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
        // In a call, or under a grad that takes its name, the index is not seen (C119).
        const std::string only =
            written + (index_.empty() ? ": a term read where " + within_ + "'s index is not seen"
                                      : ": an index other than a whole multiple of " + index_ +
                                            " plus a constant");
        const auto* ref = dynamic_cast<RefExpression<Value>*>(index.get());
        if (ref && ref->Name() == index_ && !places_.count(index_)) return 0;
        if (ref && expansion_ && expansion_->offsets.count(ref->Name()) &&
            !places_.count(ref->Name()))
            return expansion_->offsets.at(ref->Name());
        const auto* plus = dynamic_cast<AddExpression<Value>*>(index.get());
        if (!plus) throw Reason(only);
        const auto [left, left_reads]   = Reading(plus->m_e1());
        const auto [right, right_reads] = Reading(plus->m_e2());
        if (right.constant && !left.constant) return Offset(plus->m_e1(), written) + Whole(right);
        if (left.constant && !right.constant) return Whole(left) + Offset(plus->m_e2(), written);
        // A lag is the depth of a window, so a parameter that gives one is fixed.
        for (const auto* reads : {&right_reads, &left_reads})
            if (!reads->empty()) throw Fix{*reads};
        throw Reason(only);
    }

    static int Whole(const Code& code) {
        return Reasoned([&] { return AsIndex<Value>(*code.constant); });
    }

    PExpression<Value> visit(EqualExpression<Value>* local) override {
        Reasoned([&] { EvaluationVisitor<Value>::Unparametrized(*local); });
        const std::string& name = local->Name();
        if (places_.count(name) || name == index_)
            throw Reason(name + " is the sum's index, so a local cannot define it");
        if (frame_.graded.count(name))
            throw Reason(name + " is grad's variable, so a local cannot define it");
        if (!local->m_e1()->Children().empty()) throw Reason("a local by cells, for now");
        Code code = Emit(local->m_e2());
        for (Cell& cell : code.cells) cell.atom &= cell.level == primary;  // a name or a number
        frame_.refused.erase(name);
        return Answer(frame_.values.insert_or_assign(name, SharedAll(code)).first->second);
    }
    // '[a;; b]', slice by slice as Matrix::Stack stacks them, and their parts too.
    PExpression<Value> visit(TensorExpression<Value>* expression) override {
        std::vector<Code> slices, parts;
        bool              constant = true, any = false;
        for (const PExpression<Value>& child : expression->Children()) {
            const Code& slice = slices.emplace_back(Emit(child));
            constant          = constant && slice.constant;
            any               = any || !slice.part.empty();
            parts.push_back(slice.part.empty() ? Literal(Value(slice.size)) : slice.part[0]);
        }
        std::vector<const Code*> from;
        for (const Code& slice : slices) from.push_back(&slice);
        const auto stacked = [&] {
            std::vector<Value> values;
            for (const Code& slice : slices) values.push_back(*slice.constant);
            return Value::Stack(values);
        };
        return Answer(Parted(constant ? Exact(stacked) : Stack(slices),
                             any ? Part(Moved(parts, Stack)) : Part(), from));
    }
    static Code Stack(const std::vector<Code>& slices) {
        Code code;
        code.size = {slices[0].size.rows, slices[0].size.cols, slices.size()};
        for (const Code& slice : slices) {
            if (slice.size != slices[0].size)
                throw Reason("the slices of a tensor have one size, not " +
                             slices[0].size.toString() + " and " + slice.size.toString());
            code.cells.insert(code.cells.end(), slice.cells.begin(), slice.cells.end());
        }
        return Outer(code, slices, Stack);
    }

    // A row, a cell, or a tensor's slice or cell, its places constants, and so
    // its part. Which cells it takes, or why it takes none, is the
    // interpreter's answer for a value of its size whose cells are numbered.
    PExpression<Value> visit(CellExpression<Value>* expression) override {
        Code             matrix = Emit(expression->Matrix());
        std::vector<int> at;
        for (const PExpression<Value>* place :
             {&expression->Slice(), &expression->Row(), &expression->Col()})
            if (*place) at.push_back(Whole(Known(*place, "a cell whose place is not a constant")));
        // The interpreter refuses a matrix whole where it refuses a cell. One
        // that writes NaN in a cell not taken makes the header write it.
        // A parameter's cell is its argument's, NaN where that may be (C335).
        for (const Cell& c : matrix.cells)
            aware_ = aware_ || (!matrix.constant && (MayNan(c.text) || nans_.count(c.text)));
        if (aware_ && !matrix.whole && !matrix.constant) matrix = Shared(matrix);
        const std::string nan = matrix.whole ? "" : Nan(matrix);
        if (!nan.empty())
            for (Cell& c : matrix.cells) c = Cell("(" + nan + c.text + ")", primary);
        const auto read = [&](const Value& v) {
            return at.size() == 3   ? numeric_interface<Value>::cell(v, at[0], at[1], at[2])
                   : at.size() == 2 ? numeric_interface<Value>::cell(v, at[0], at[1])
                                    : numeric_interface<Value>::row(v, at[0]);
        };
        if (matrix.constant && matrix.part.empty()) return Answer(Exactly(read, matrix));
        Value numbered(matrix.size), taken;
        for (std::size_t c = 0; c < matrix.size.count(); ++c)
            numbered.data()[c] = Number(static_cast<int>(c));
        try {
            taken = read(numbered);
        } catch (const std::runtime_error& error) {
            const auto bound = [&](std::size_t k) { return std::to_string(at[k]); };
            if (at.size() == 1 && at[0] > 1) Unstated(*expression->Matrix(), "j<=" + bound(0));
            if (at.size() == 2 && at[0] > 0 && at[1] > 0)
                Unstated(*expression->Matrix(), "j<=" + bound(0) + ", k<=" + bound(1));
            if (at.size() == 3 && at[0] > 0 && at[1] > 0 && at[2] > 0)
                Unstated(*expression->Matrix(),
                         "b<=" + bound(0) + ", j<=" + bound(1) + ", k<=" + bound(2));
            throw Reason(error.what());
        }
        const auto window = [&](const Code& m) {
            const auto cells = [&](const Code& from) {
                Code c;
                c.size = taken.Size();
                for (std::size_t k = 0; k < c.size.count(); ++k)
                    c.cells.push_back(
                        from.At(static_cast<std::size_t>(taken.data()[k].Inexact().real())));
                return c;
            };
            return Outer(cells(m), {m}, [&](const std::vector<Code>& o) { return cells(o[0]); });
        };
        return Answer(Parted(window(matrix), Mapped(PartOf(matrix), read, window), {&matrix}));
    }
    // A cell past an input whose model states no size, or a parameter it
    // gives no default, which compile as single values (C83), or past a
    // single value computed from one, refused with the size that would hold it,
    // read where it stands or through the parameters it is given to.
    void Unstated(const Expression<Value>& given, const std::string& bounds) const {
        const Expression<Value>* at    = &given;
        const Scope<Value>*      scope = scope_;
        for (const Expansion* in = expansion_;;) {
            if (!dynamic_cast<const FuncExpression<Value>*>(at) &&
                !dynamic_cast<const RefExpression<Value>*>(at))
                return;
            while (in && !in->values.count(at->Name()) && !in->own.count(at->Name()))
                in = in->outer;
            if (!in) break;
            const auto w = in->written.find(at->Name());
            if (w == in->written.end()) return;
            std::tie(at, scope, in) = w->second;
        }
        if (!model_ || scope != &root_) return;
        const Expression<Value>& read = *at;
        std::set<std::string>    seen;
        std::vector<std::string> next{read.Name()};
        while (!next.empty()) {
            const std::string name = next.back();
            next.pop_back();
            if (!seen.insert(name).second) continue;
            for (const typename Model<Value>::Parameter& p : model_->parameters)
                if (p.name == name && !p.fallback && p.bounds.empty())
                    throw Reason(
                        (name == read.Name() ? "" : read.Name() + " reads ") + p.name +
                        (name == read.Name() ? " is" : ",") + " a single value, as " +
                        model_->header.substr(0, model_->header.find('(')) +
                        (p.index.empty()
                             ? " gives it no default: give it one of its size, at least " + bounds
                             : " states no size for it: write '" + p.name + "_" + p.index + "[" +
                                   bounds + "]'"));
            const auto sequence = sequences_.find(name);
            if (sequence == sequences_.end() || sequence->second.size.count() != 1) continue;
            for (const auto& [reads, lags] : sequence->second.reads) next.push_back(reads);
            next.insert(next.end(), sequence->second.parameters.begin(),
                        sequence->second.parameters.end());
        }
    }
    // A factorial of a constant is one; at a limit's walking index plus a
    // constant, the one before times that; of what else moves, the header's
    // helper's.
    PExpression<Value> visit(FactExpression<Value>* expression) override {
        const auto fact = [](const Value& v) { return Value(numeric_interface<Value>::fact(v)); };
        const Code operand = Emit(expression->m_e());
        if (!Still(operand) || (!operand.constant && !operand.Scalar()))
            throw Reason("a factorial");
        if (!operand.constant && limit_ && !index_.empty() && !places_.count(index_) &&
            Mentions(*expression->m_e(), index_))
            if (const std::optional<int> c = Above(expression->m_e(), "a factorial"))
                return Answer(Climb("!" + std::to_string(*c), operand, [&](int f) {
                    return Exactly(fact, Literal(Value(Number(f + *c))));
                }));
        if (!operand.constant) {
            // A function of the prelude, kept where something calls it, which
            // a sum folded does not. It stops at an infinite product (C47).
            const std::string helper = module_ + "_fact_";
            if (nan_functions_.insert(helper).second)
                prelude_.push_back(
                    "/* !x, as the interpreter takes it: NaN where x is not a whole number at "
                    "least 0. */\nstatic inline double " +
                    helper +
                    "(double x) {\n"
                    "    double f = 1.0, i = 1.0;\n"
                    "    if (!(x >= 0 && x == floor(x))) return NAN;\n"
                    "    while (i < x && isfinite(f)) f *= ++i;\n    return f;\n}\n\n");
            return Answer(Looping(Of(Cell(helper + "(" + operand.cells[0].text + ")", primary))));
        }
        return Answer(Exactly(fact, operand));
    }
    // grad compiled (DESIGN.md): its body once for each cell of the point, each
    // value carrying its part, the name's a constant seed, as the interpreter's;
    // by a point of several cells, or by several names, once, each value
    // carrying a node of a tape swept back (DESIGN.md, the compiler's adjoint
    // step), but for a matrix by single values.
    PExpression<Value> visit(GradExpression<Value>* expression) override {
        if (expression->Several() && expression != grouped_.first)
            Reasoned(EvaluationVisitor<Value>::Unread);
        std::vector<Code> points, parts;
        for (std::size_t v = 0; v < expression->Variables().size(); ++v)
            points.push_back(Emit(expression->Point(v)));
        // Its body is a value, in a guard too, where a comparison jumps (C414).
        const Setting<bool> valued(guarding_, false);
        Asked([&] { Reasoned([&] { Derivative<Value>(definitions_).Names(*expression); }); }, true);
        const bool hessian =
            Reasoned([&] { return Derivative<Value>::Hessian(*expression, points[0].size); });
        // Its inner grad's point reads v, where its body may not.
        if (hessian) {
            auto& inner = static_cast<GradExpression<Value>&>(*expression->Body());
            Asked([&] { Reasoned([&] { Derivative<Value>(definitions_).Names(inner); }); }, true);
        }
        if (depth_ + (hessian ? 1 : 0) > 1)
            throw Reason("a derivative of a derivative of a derivative, for now");
        // grad_ joins several names by ' or '.
        if (depth_ && (expression->Several() || grad_.find(" or ") != std::string::npos))
            throw Reason("a derivative of a derivative by several names, for now");
        if (depth_ && tape_) throw Forward();
        if (depth_) return Answer(Nest(*expression, points[0]));
        const Frame frame = frame_;
        if (!hessian && (points.size() > 1 || points[0].size.count() > 1)) try {
                Tape tape;
                parts = Graded(*expression, points, &tape);
            } catch (const Forward&) {
            }
        if (parts.empty()) parts = (frame_ = frame, Graded(*expression, points, nullptr, hessian));
        // A nest at constant points reading nothing that moves is folded, as
        // a first derivative's values are, and NaN where the interpreter refuses.
        if (!depth_ && dynamic_cast<GradExpression<Value>*>(expression->Body().get()) &&
            Fixed(*expression, points))
            try {
                return Fold(expression);
            } catch (const Undefined&) {
                for (Cell& cell : parts[0].cells) cell = Atom("NAN");
                parts[0].constant.reset();
            }
        if (expression->Several()) *grouped_.second = parts;
        return Answer(parts[0]);
    }
    // Whether a nest's innermost body, each grad's name its point's value, is
    // a constant that reads no parameter.
    bool Fixed(GradExpression<Value>& grad, std::vector<Code> points) {
        if (!std::ranges::all_of(points, [](const Code& p) { return p.constant.has_value(); }))
            return false;
        const auto                                  reads = std::exchange(read_parameters_, {});
        Expansion                                   bound{{}, {}, scope_, expansion_, {}, {}};
        const Setting<Expansion*>                   binding(expansion_, &bound);
        const Setting<std::map<std::string, Value>> places(places_, places_);
        const Setting<std::string>                  indexed(index_, index_);
        const Setting<Frame>                        probed(frame_, frame_);
        Code                                        body;
        try {
            for (GradExpression<Value>* g = &grad; g;) {
                for (std::size_t v = 0; v < points.size(); ++v) {
                    bound.values.insert_or_assign(g->Variables()[v], points[v]);
                    places_.erase(g->Variables()[v]);
                    if (index_ == g->Variables()[v]) index_.clear();
                }
                auto* inner = dynamic_cast<GradExpression<Value>*>(g->Body().get());
                if (!inner) body = Emit(g->Body());
                points.clear();
                for (std::size_t v = 0; inner && v < inner->Variables().size(); ++v)
                    points.push_back(Emit(inner->Point(v)));
                g = inner;
            }
        } catch (const Undefined&) {
        }
        const bool fixed = body.constant && read_parameters_.empty();
        read_parameters_.insert(reads.begin(), reads.end());
        return fixed;
    }
    // Forward over forward (DESIGN.md, grad's order): the inner grad's pass
    // as at first order, the values it reads moving by the outer grad by
    // their outer parts, and its part's outer part the outer grad's part.
    Code Nest(GradExpression<Value>& expression, Code point) {
        const auto inward = [](Code& c) {
            if (c.part.empty()) return;
            c.outer = std::move(c.part);
            c.part.clear();
            c.outer[0].moves = c.moves;
            c.constant.reset();
            c.moves.reset();
        };
        std::vector<std::pair<Expansion*, std::map<std::string, Code>>> saved;
        for (Expansion* e = expansion_; e; e = e->outer) {
            saved.emplace_back(e, e->values);
            for (auto& [name, code] : e->values) inward(code);
        }
        inward(point);
        const std::map<std::string, Code> before = frame_.values;  // put back where not bound
        for (auto& [name, code] : frame_.values) inward(code);
        struct Restore {
            std::vector<std::pair<Expansion*, std::map<std::string, Code>>>& each;
            ~Restore() {
                for (auto& [e, values] : each) e->values = std::move(values);
            }
        } restore{saved};
        Code out = Graded(expression, {point}, nullptr)[0];
        std::set<std::string> rebound;
        Binding(*expression.Body(), rebound);
        for (auto& [name, code] : frame_.values) {  // its part by the outer, its exact value kept
            const auto was = before.find(name);
            code = was != before.end() && !rebound.count(name) ? was->second : Outward(code);
        }
        out.part = std::move(out.outer);
        out.outer.clear();
        out.moves.reset();
        if (!out.part.empty()) out.constant.reset();
        return out;
    }
    // A grad holding a nest, in reverse mode, gives way to forward mode.
    struct Forward {};

    // Each name's part, in the order written; none where reverse mode gives
    // way. Of a Hessian, a pass for each cell (i, j), i <= j, by v[i] around
    // v[j], each mirrored.
    std::vector<Code> Graded(GradExpression<Value>& expression, const std::vector<Code>& points,
                             Tape* tape, bool hessian = false) {
        const std::vector<std::string>& names = expression.Variables();
        Code                            body, values;
        std::vector<Code>               parts;
        std::vector<bool>               any(names.size());
        const Setting<Tape*>            taping(tape_, tape);
        const Frame                     frame = frame_;  // each pass binds its own
        if (tape) ++tapes_;
        std::string with;
        for (const std::string& name : names) with += (with.empty() ? "" : " or ") + name;
        std::vector<std::string> around = graded_;
        around.insert(around.end(), names.begin(), names.end());
        const Setting<std::vector<std::string>> graded(graded_, std::move(around));
        const std::size_t n  = points[0].cells.size();
        const auto        at = [&](std::size_t i, std::size_t j) {
            if (i > j) std::swap(i, j);
            return i * n - i * (i - 1) / 2 + j - i;
        };
        for (std::size_t v = 0; v < (tape ? 1 : names.size()); ++v)
            for (std::size_t k = 0; k < (tape      ? 1
                                         : hessian ? n * (n + 1) / 2
                                                   : points[v].cells.size());
                 ++k) {
                Expansion   bound{{}, {}, scope_, expansion_, {}, {}};
                auto        places = places_;
                std::string index  = index_;
                for (std::size_t w = 0; w < names.size(); ++w) {
                    Code x = points[w];
                    // Each its own node, though two be alike (C323).
                    if (tape) x.part = {Record(Kind::Seed, x.size, 0, {}, 0, w)};
                    const auto seed = [&](std::size_t c) {
                        Value one(Number(1));
                        if (x.Scalar()) return Literal(one);
                        Value cell     = Value(x.size);
                        cell.data()[c] = Number(1);
                        return Literal(cell);
                    };
                    std::size_t i = 0, j = k;
                    for (; hessian && j >= n - i; ++i) j -= n - i;
                    if (!tape && w == v) x.part = {seed(hessian ? i + j : k)};
                    if (!tape && w == v && hessian) {
                        x.outer = {seed(i)};
                        x.constant.reset();
                    }
                    bound.values.emplace(names[w], x);
                    places.erase(names[w]);
                    if (index == names[w]) index.clear();
                }
                frame_ = frame;
                frame_.graded.insert(names.begin(), names.end());
                const Setting<std::map<std::string, Value>> hidden(places_, places);
                const Setting<std::string>                  indexed(index_, index);
                const Setting<Expansion*>                   binding(expansion_, &bound);
                const Setting<std::string>                  differentiating(grad_, with);
                const Setting<std::size_t> deeper(depth_, depth_ + (hessian ? 2 : 1));
                body = Emit(hessian ? static_cast<GradExpression<Value>&>(*expression.Body()).Body()
                                    : expression.Body());
                if (tape && names.size() > 1 && !body.Scalar() &&
                    std::ranges::all_of(points, &Code::Scalar))
                    return {};
                const auto kind = [](const Code& c) { return c.size.slices ? "tensor" : "matrix"; };
                for (const Code& point : points)
                    if (!point.Scalar() && !body.Scalar())
                        throw Reason(std::string("grad of a ") + kind(body) +
                                     " with respect to a " + kind(point) +
                                     " is a Jacobian, which it does not give");
                if (!body.constant)
                    values.cells.insert(values.cells.end(), body.cells.begin(), body.cells.end());
                if (tape && !body.part.empty()) parts = Swept(body.part[0].node, points);
                const Part part =
                    hessian && !body.part.empty() ? OuterOf(body.part[0]) : PartOf(body);
                if (!tape) parts.push_back(part ? *part : Literal(Value(body.size)));
                any[v] = any[v] || part;
            }
        // Past the grad, a local keeps its value and the parts of the grads around it (C322).
        for (auto& [name, local] : frame_.values)
            local = {local.size,  local.cells, local.constant,
                     local.whole, {},          hessian ? std::vector<Code>() : local.outer,
                     {},          0,           false};
        frame_.graded = frame.graded;
        // NaN where the value differentiated is, in a header that writes NaN,
        // as one does where that value writes it and no part reads it (C113),
        // in any walk: each moves by its own name, and the interpreter refuses
        // all where one jumps (C341).
        for (const Cell& c : values.cells) aware_ |= MayNan(c.text);  // a temporary's too
        const std::string nan = aware_ ? Nan(Shared(values)) : "";
        std::vector<Code> outs;
        for (std::size_t v = 0, first = 0; v < names.size(); ++v) {
            const Code&       point = points[v];
            const std::size_t cells = tape ? 1 : point.cells.size();
            const auto        from  = parts.begin() + static_cast<std::ptrdiff_t>(first);
            std::vector<Code> square;
            for (std::size_t i = 0; hessian && i < n; ++i)
                for (std::size_t j = 0; j < n; ++j) square.push_back(parts[at(i, j)]);
            const Extent size = hessian ? Extent{n, n} : point.Scalar() ? body.size : point.size;
            Code         out =
                !any[tape ? 0 : v] ? Literal(Value(size))
                        : hessian          ? Assembled(square, size)
                        : tape || point.Scalar()
                            ? parts[first]
                            : Assembled({from, from + static_cast<std::ptrdiff_t>(cells)}, point.size);
            first += cells;
            out.part.clear();
            for (Cell& cell : out.cells)
                if (!nan.empty()) cell = Cell("(" + nan + cell.text + ")", primary);
            if (!nan.empty()) out.constant.reset();
            outs.push_back(std::move(out));
        }
        return outs;
    }
    // The sweep back, Tape::Back written out: each node's adjoint by its
    // cells, from the body's 1 back to the seed's, none where no path reaches
    // or it folds to an exact 0, the interpreter's Nothing, and a term with
    // one absent left out.
    using Adjoint = std::optional<Code>;
    struct Adjoints {
        Extent               size;
        std::vector<Adjoint> cells;
    };
    static Code Single(const Code& code, std::size_t c) {
        Code one;
        one.cells = {code.At(c)};
        if (code.constant) one.constant = Value(code.constant->data()[c % code.cells.size()]);
        for (const Code& outer : code.outer)
            (one.outer.push_back(Single(outer, c)), one.outer[0].moves = outer.moves);
        return one;
    }
    static Adjoint Some(Code code) {
        const bool zero = code.constant && numeric_interface<Value>::exact(*code.constant) &&
                          *code.constant == Value(Number(0));
        return zero ? Adjoint() : Adjoint(std::move(code));
    }
    // An adjoint there only where its 'moves' holds, a clause's condition.
    Where Both(const Where& x, const Where& y) {
        if (!x || !y) return x ? x : y;
        return std::make_shared<const Moving>(Moving{"(" + Text(x) + ") && (" + Text(y) + ")", {}});
    }
    template <typename F>
    Adjoint Folded(F f, const Code& x, const Code& y, Cell (*written)(const Cell&, const Cell&)) {
        Adjoint out =
            Some(x.constant && y.constant ? Exactly(f, x, y) : Of(written(x.cells[0], y.cells[0])));
        if (out) out->moves = Both(x.moves, y.moves);
        return out;
    }
    Adjoint Mul(const Code& x, const Code& y) {
        return Folded(std::multiplies<>(), x, y, Multiplied);
    }
    // One there only where its condition holds is added only there, 'c ?
    // acc + t : acc', and the sum is there where either is.
    void Into(Adjoint& acc, const Adjoint& term) {
        if (!term) return;
        if (!acc && (!term->moves || term->zero)) return void(acc = term);
        if (!term->moves) {
            if ((acc = Folded(std::plus<>(), *acc, *term, Added))) acc->moves.reset();
            return;
        }
        const Code    was   = acc ? Shared(*acc) : Literal(Value(Number(0)));
        const Adjoint both  = acc ? Folded(std::plus<>(), was, *term, Added) : term;
        Where         where = term->moves;
        if (acc)
            where = acc->moves ? std::make_shared<const Moving>(Moving{"", {acc->moves, where}})
                               : nullptr;
        acc = Of(Cell(Whether(term->moves) + " ? " + (both ? *both : was).cells[0].text + " : " +
                          was.cells[0].text,
                      0));
        acc->moves = where;
        acc->zero  = true;
    }
    Adjoint Total(const Adjoints& g) {
        Adjoint total;
        for (const Adjoint& c : g.cells) Into(total, c);
        return total;
    }
    static Adjoints Slice(const Adjoints& g, std::size_t b) {
        if (!g.size.slices) return g;
        const auto n = static_cast<std::ptrdiff_t>(g.size.rows * g.size.cols);
        return {{g.size.rows, g.size.cols, 0},
                {g.cells.begin() + n * static_cast<std::ptrdiff_t>(b),
                 g.cells.begin() + n * static_cast<std::ptrdiff_t>(b + 1)}};
    }
    // From 27 multiplications, a loop where every adjoint is there, each
    // operand's all under a condition or none, and no term folds: each term
    // added where its conditions hold, as Into adds it, and where any did the
    // array's second slice (DESIGN.md, loops in generated code).
    std::optional<Adjoints> Looped(const Adjoints& x, bool tx, const Adjoints& y, bool ty) {
        const auto there = [](const Adjoints& a) {
            return std::ranges::all_of(a.cells, [&](const Adjoint& c) {
                return c && c->cells.size() == 1 && !c->moves == !a.cells[0]->moves;
            });
        };
        const auto moving = [](const Adjoints& a) {
            return std::ranges::none_of(a.cells,
                                        [](const Adjoint& c) { return bool(c->constant); });
        };
        const auto code = [](const Adjoints& a, bool t, const auto& cell) {
            Code c;
            c.size = a.size;
            for (const Adjoint& one : a.cells) c.cells.push_back(cell(*one));
            return t ? Transposed(c) : c;
        };
        if (!temporaries_ || !there(x) || !there(y) || (!moving(x) && !moving(y)))
            return std::nullopt;
        const auto   own = [](const Code& c) { return c.cells[0]; };
        const Code   l = code(x, tx, own), r = code(y, ty, own);
        const Extent size{l.size.rows, r.size.cols, std::max(l.size.slices, r.size.slices)};
        if (l.Scalar() || r.Scalar() || l.size.cols != r.size.rows ||
            size.count() * l.size.cols < 27)
            return std::nullopt;
        Adjoints   out{size, {}};
        const bool cx = bool(x.cells[0]->moves), cy = bool(y.cells[0]->moves);
        if (!cx && !cy) {
            const Code looped = Looped(l, r, size);
            for (std::size_t k = 0; k < size.count(); ++k) out.cells.push_back(Single(looped, k));
            return out;
        }
        if (size.Single()) return std::nullopt;
        const auto flags = [&](const Adjoints& a, bool t) {
            return Materialised(code(a, t, [&](const Code& c) { return Atom(Whether(c.moves)); }),
                                "int");
        };
        const Indexed          a = Materialised(l), b = Materialised(r);
        const bool             ls = l.size.slices, rs = r.size.slices;
        std::optional<Indexed> fa, fb;
        if (cx) fa = flags(x, tx);
        if (cy) fb = flags(y, ty);
        const auto when = [&](const std::string& p) {
            std::string w = fa ? fa->At(ls, "i_", p) : "";
            return fb ? w + (fa ? " && " : "") + fb->At(rs, p, "j_") : w;
        };
        const auto term = [&](const std::string& p) {
            return a.At(ls, "i_", p) + " * " + b.At(rs, p, "j_");
        };
        const std::string key = "conditioned\x1f" + a.Key(ls) + "*" + b.Key(rs) + "|" +
                                (fa ? fa->base : "") + "|" + (fb ? fb->base : "") + " " +
                                size.toString() + " " + std::to_string(l.size.cols);
        const std::string name = Declare(key, [&](const std::string& t) {
            std::vector<std::string> lines = Loops(size);
            const std::string        in(4 * lines.size(), ' ');
            lines.insert(lines.begin(), "double " + t + "[2]" + Dimensions(size) + ";");
            lines.back() += " {";
            for (const std::string& line : std::vector<std::string>{
                     "int f_ = " + when("0") + ";", "double s_ = f_ ? " + term("0") + " : 0.0;",
                     "for (int p_ = 1; p_ < " + std::to_string(l.size.cols) + "; ++p_) {",
                     "    const int c_ = " + when("p_") + ";",
                     "    s_ = c_ ? s_ + " + term("p_") + " : s_;", "    f_ = f_ || c_;", "}",
                     t + "[0]" + Places(size) + " = s_;",
                     t + "[1]" + Places(size) + " = f_ ? 1.0 : 0.0;"})
                lines.push_back(in + line);
            lines.push_back(in.substr(4) + "}");
            return lines;
        });
        if (MayNan(l) || MayNan(r)) nan_temporaries_.insert(name);
        const Code values = Array(name + "[0]", size), held = Array(name + "[1]", size);
        for (std::size_t k = 0; k < size.count(); ++k) {
            Code one  = Single(values, k);
            one.moves = std::make_shared<const Moving>(Moving{held.cells[k].text + " != 0", {}});
            one.zero  = true;
            out.cells.push_back(std::move(one));
        }
        return out;
    }
    // x*y, either transposed, slice by slice where either is a tensor.
    Adjoints Sparse(const Adjoints& x, bool tx, const Adjoints& y, bool ty) {
        if (auto looped = Looped(x, tx, y, ty)) return *looped;
        const std::size_t slices = std::max(x.size.slices, y.size.slices);
        Adjoints c{{tx ? x.size.cols : x.size.rows, ty ? y.size.rows : y.size.cols, slices}, {}};
        const std::size_t inner = tx ? x.size.rows : x.size.cols;
        const auto        at    = [](const Adjoints& a, bool t, std::size_t i,
                           std::size_t j) -> const Adjoint& {
            return t ? a.cells[j * a.size.cols + i] : a.cells[i * a.size.cols + j];
        };
        for (std::size_t b = 0; b < std::max<std::size_t>(slices, 1); ++b) {
            const Adjoints l = Slice(x, b), r = Slice(y, b);
            for (std::size_t i = 0; i < c.size.rows; ++i)
                for (std::size_t j = 0; j < c.size.cols; ++j) {
                    Adjoint total;
                    for (std::size_t k = 0; k < inner; ++k)
                        if (const Adjoint &u = at(l, tx, i, k), &v = at(r, ty, k, j); u && v)
                            Into(total, Mul(*u, *v));
                    c.cells.push_back(total);
                }
        }
        return c;
    }
    // g's cells present, each with v's by f.
    template <typename F>
    static Adjoints Each(const Adjoints& g, const Code& v, F f) {
        Adjoints out{Extent::Wider(g.size, v.size), {}};
        for (std::size_t k = 0; k < out.size.count(); ++k) {
            const Adjoint& c = g.cells[k % g.cells.size()];
            out.cells.push_back(c ? f(*c, Single(v, k)) : Adjoint());
        }
        return out;
    }
    // Summed down to the shape of what was widened (C81).
    Adjoints Reduce(Adjoints g, Extent to) {
        if (g.size == to) return g;
        if (to.Single()) return {to, {Total(g)}};
        Adjoints total = Slice(g, 0);
        for (std::size_t b = 1; b < g.size.slices; ++b)
            for (std::size_t k = 0; k < total.cells.size(); ++k)
                Into(total.cells[k], Slice(g, b).cells[k]);
        return total;
    }
    std::vector<Code> Swept(std::size_t body, const std::vector<Code>& points) {
        const std::vector<Node>&             nodes = tape_->nodes;
        std::vector<std::optional<Adjoints>> adjoint(nodes.size() + 1);
        const auto at = [&](std::size_t n) -> Adjoints& {
            const Extent e = nodes[n - 1].extent;
            return adjoint[n] ? *adjoint[n]
                              : adjoint[n].emplace(Adjoints{e, std::vector<Adjoint>(e.count())});
        };
        // A chain's passed on as it is made to each clause's node, there
        // where its condition holds.
        const auto add = [&](const auto& self, std::size_t n, std::size_t k, Adjoint c) -> void {
            const Node& x = nodes[n - 1];
            if (x.kind != Kind::Gather || !x.value) return Into(at(n).cells[k], c);
            for (std::size_t i = 0; c && i < x.from.size(); ++i) {
                const std::size_t clause = x.from[i].first;
                const Moving      where{Wrap(x.value->cells[i], sum) + " != 0", {}};
                Adjoint           taken = c;
                taken->zero             = false;
                taken->moves            = Both(c->moves, std::make_shared<const Moving>(where));
                self(self, clause, k % nodes[clause - 1].extent.count(), taken);
            }
        };
        const auto into = [&](std::size_t n, Adjoints g) {
            if (!n) return;
            g = Reduce(std::move(g), nodes[n - 1].extent);
            for (std::size_t k = 0; k < g.cells.size(); ++k) add(add, n, k, g.cells[k]);
        };
        into(body, Adjoints{Extent{}, {Literal(Value(Number(1)))}});
        const auto times = [this](const Code& c, const Code& v) { return Mul(c, v); };
        for (std::size_t n = nodes.size(); n > points.size(); --n) {
            if (!adjoint[n]) continue;
            Adjoints g = std::move(*adjoint[n]);
            for (Adjoint& c : g.cells)
                if (c && !c->constant) c = Shared(*c);  // read by several products
            const Node&       x = nodes[n - 1];
            const Code* const v = x.value ? &*x.value : nullptr;
            const Extent      a = x.a ? nodes[x.a - 1].extent : Extent{};
            switch (x.kind) {
                case Kind::Seed:
                    break;
                case Kind::Sum:
                    into(x.b, g);
                    into(x.a, std::move(g));
                    break;
                case Kind::Negative:
                    for (Adjoint& c : g.cells)
                        if (c) c = Negated(*c);
                    into(x.a, std::move(g));
                    break;
                case Kind::Left:
                case Kind::Right:
                    if (v->Scalar() || a.Single()) {
                        const Adjoints p = Each(g, *v, times);
                        into(x.a, v->Scalar() ? p : Adjoints{a, {Total(p)}});
                    } else {
                        Adjoints value{v->size, {}};
                        for (std::size_t k = 0; k < v->cells.size(); ++k)
                            value.cells.push_back(Single(*v, k));
                        into(x.a, x.kind == Kind::Left ? Sparse(value, true, g, false)
                                                       : Sparse(g, false, value, true));
                    }
                    break;
                case Kind::Quotient:
                    into(x.a, Each(g, *v, [this](const Code& c, const Code& d) {
                             return Folded(std::divides<>(), c, d, Divided);
                         }));
                    break;
                case Kind::Cells:
                    into(x.a, Each(g, *v, times));
                    break;
                case Kind::Mark:
                    into(x.a, std::move(g));
                    break;
                case Kind::Place:
                    into(x.b, {Extent{}, {g.cells[x.at - 1]}});
                    g.cells[x.at - 1].reset();
                    into(x.a, std::move(g));
                    break;
                case Kind::Gather:
                    for (std::size_t k = 0; k < x.from.size(); ++k)
                        if (const auto [from, c] = x.from[k]; from) add(add, from, c, g.cells[k]);
                    break;
            }
        }
        // Each seed's adjoints, the seeds being the first nodes.
        std::vector<Code> parts;
        for (std::size_t s = 1; s <= points.size(); ++s) {
            Code out = Literal(Value(points[s - 1].size));
            for (std::size_t k = 0; adjoint[s] && k < out.cells.size(); ++k)
                if (const Adjoint& c = adjoint[s]->cells[k]) {
                    out.cells[k] = c->cells[0];
                    if (!c->constant) out.constant.reset();
                    if (out.constant) out.constant->data()[k] = (*c->constant)(1, 1);
                }
            parts.push_back(std::move(out));
        }
        return parts;
    }

    // Unrolled, since each term is a line of C: a thousand is a filter no one
    // would write out as one.
    static constexpr int max_terms = 1000;

    // A sum or a product with constant bounds, unrolled with its index bound
    // as a constant, as a cell's names are: the terms that read it fold, and
    // the lags it gives are constants.
    PExpression<Value> visit(SeriesExpression<Value>* expression) override {
        if (!expression->Upper()) return Sum(expression);
        if (const auto total = Running(*expression)) return Answer(*total);
        const Code lower =
            Known(expression->Lower(), "a sum or a product whose bounds are not constants");
        const Code upper =
            Known(expression->Upper(), "a sum or a product whose bounds are not constants");
        const int first = Whole(lower), last = Whole(upper);
        if (expression->Reduction() && last < first) throw Reason(expression->Empty(first, last));
        if (last < first) return Answer(Literal(Value(Number(expression->Product() ? 1 : 0))));
        const std::string many = expression->Reduction()
                                     ? std::string("the ") + expression->Noun() + " has"
                                     : "a sum or a product of";
        if (last - first >= max_terms)
            throw Reason(many + " more than " + std::to_string(max_terms) + " terms");
        const std::string&         name  = expression->Index();
        const auto                 found = places_.find(name);
        const std::optional<Value> outer =
            found == places_.end() ? std::nullopt : std::optional<Value>(found->second);
        Code               total;
        bool               constant = true;
        std::vector<Value> terms;
        std::optional<Setting<Memo>> memo;
        if (memo_.first != temporaries_) memo.emplace(memo_, Memo{temporaries_, {}});
        std::vector<Code> ranked;
        for (int k = first; k <= last; ++k) {
            places_[name]   = Value(Number(k));
            const Code term = Emit(expression->Body());
            if (expression->Reduction()) {
                ranked.push_back(Ranked(*expression, term));
                continue;
            }
            constant        = constant && term.constant;
            if (constant) terms.push_back(*term.constant);
            total           = k == first              ? term
                              : expression->Product() ? ProductOf(Product(total, term), total, term)
                                                      : SumOf(Broadcast(total, term, Added), total, term);
        }
        if (outer)
            places_[name] = *outer;
        else
            places_.erase(name);
        if (expression->Reduction()) return Answer(Reduced(*expression, ranked, first));
        const auto exact = [&] {
            Value value = terms[0];
            for (std::size_t k = 1; k < terms.size(); ++k)
                value = expression->Product() ? value * terms[k] : value + terms[k];
            return value;
        };
        return Answer(constant ? Parted(Exact(exact), PartOf(total), {&total}) : total);
    }

    // A sum or a product up to the clause's index less a constant c, kept as
    // a sequence of the step's own, as a recurrence by hand is (DESIGN.md):
    // 's0_n | n < a + c = 0', and 's0_n' the sum, which its own clause
    // compiles as its term at n - 1 plus the sum's at n - c. One for each place.
    struct Accumulator {
        std::string                  reader;
        SeriesExpression<Value>*     series;
        std::map<std::string, Value> places;
        int                          lag;
        std::string                  upto;
        ParametersCall<Value>        now        = {};  // its term at n
        Reference<Value>             definition = {};
    };
    std::optional<Code> Running(SeriesExpression<Value>& series) {
        const auto own = totals_.find(within_);
        if (own != totals_.end() && own->second.series == &series) return Accumulated(own->second);
        if (limit_ || index_.empty() || places_.count(index_) || !Mentions(*series.Upper(), index_))
            return std::nullopt;
        const std::string what = std::string(series.Indexes() ? "an " : "a ") + series.Noun();
        const std::string upto = what + " up to " + index_;
        if (own != totals_.end()) throw Reason(what + " up to an enclosing sum's index, for now");
        if (series.Reduction() || !grad_.empty())
            throw Reason(upto + (grad_.empty() ? "" : " under grad") + ", for now");
        if (Mentions(*series.Body(), index_))
            throw Reason(upto + " whose terms read " + index_ + "; write it as a recurrence");
        int c = -1;
        try {
            c = -Offset(series.Upper(), index_);
        } catch (const Reason&) {
        }
        if (c < 0)
            throw Reason(what + " whose last index is neither a constant nor " + index_ +
                         " less a constant");
        const int a = Whole(Known(series.Lower(), what + " whose first index is not a constant"));
        auto      found = std::ranges::find_if(totals_, [&](const auto& total) {
            const Accumulator& t = total.second;
            return t.series == &series && t.definition.home == scope_ && t.places == places_;
        });
        if (found == totals_.end()) {
            const std::string key = scope_->Qualified("s" + std::to_string(totals_.size()) + "_");
            found = totals_.try_emplace(key, within_, &series, places_, c, upto).first;
            Accumulator&             total = found->second;
            const auto               n     = std::make_shared<RefExpression<Value>>(index_);
            EvaluationVisitor<Value> evaluator(definitions_);
            const auto               below = std::make_shared<CompareExpression<Value>>(
                Comparison::Less, n, std::make_shared<ValExpression<Value>>(Value(Number(a + c))));
            total.now = {nullptr, n};
            total.definition.add_expression(
                key, {nullptr, n, evaluator, below},
                std::make_shared<ValExpression<Value>>(Value(Number(series.Product()))));
            total.definition.add_expression(key, {nullptr, n, evaluator},
                                            PExpression<Value>(PExpression<Value>(), &series));
            total.definition.home      = scope_;
            sequences_[key].definition = &total.definition;
        }
        const Code  code  = Term(found->first, &found->second.definition, found->second.now);
        const auto& bases = reading_->bases;
        for (int n = bases.empty() ? 0 : bases.begin()->first; !code.Scalar() && n < a + c; ++n)
            if (!bases.count(n))
                throw Reason(upto + " empty at " + std::to_string(n) + ", a single " +
                             (series.Product() ? "1" : "0") + " there and a " +
                             code.size.Described() + " after");
        return code;
    }
    Code Accumulated(Accumulator& total) {
        Code term;
        {
            const Setting<std::map<std::string, Value>> places(places_, total.places);
            const Setting<int>                          lagged(shift_, total.lag);
            const Setting<std::string>                  bound(index_, total.series->Index());
            const Setting<std::string>                  text(
                index_text_, total.lag ? "(double)(m_->index_ - " + std::to_string(total.lag) + ")"
                                                        : index_text_);
            term = Emit(total.series->Body());
        }
        if (!term.Scalar())  // its history of its terms' size
            total.definition.add_expression(
                within_, total.definition.Clauses()[0].parameters,
                std::make_shared<ValExpression<Value>>(
                    total.series->Product() ? Value::Identity(term.size) : Value(term.size)));
        sequences_.at(within_).size = term.size;
        const Setting<int> back(shift_, 1);
        const Code         before = Term(within_, &total.definition, total.now);
        return total.series->Product() ? Product(before, term) : Broadcast(before, term, Added);
    }
    // A reduction's term, as the interpreter ranks it; under grad, for now, refused.
    static const Code& Ranked(const SeriesExpression<Value>& series, const Code& term) {
        if (!Still(term))
            throw Reason(std::string("a derivative of ") + (series.Indexes() ? "an " : "a ") +
                         series.Noun() + ", for now");
        if (!term.constant && !term.Scalar())
            throw Reason(std::string("the ") + series.Noun() + " needs single values, not a " +
                         term.size.Described() + "; write it by its cells");
        if (term.constant) (void)Exact([&] { return series.Ranked(*term.constant); });
        return term;
    }

    // Unrolled as a sum is: a later term kept where it ranks strictly higher,
    // the index beside it, NaN where a term is in a header that writes NaN.
    Code Reduced(const SeriesExpression<Value>& series, const std::vector<Code>& terms, int first) {
        if (std::all_of(terms.begin(), terms.end(), [](const Code& c) { return c.constant; }))
            return Exact([&] {
                std::vector<Value> values;
                for (const Code& term : terms) values.push_back(*term.constant);
                return series.Reduced(values, first);
            });
        Code kept = Shared(terms[0]), at = Literal(Value(Number(first)));
        for (std::size_t k = 1; k < terms.size(); ++k) {
            const Code        next = Shared(terms[k]);
            const std::string a = Wrap(kept.cells[0], sum), b = Wrap(next.cells[0], sum);
            const std::string i = Literal(Value(Number(first + static_cast<int>(k)))).cells[0].text;
            const std::string test =
                "(" + Nan(kept, next) + (series.High() ? a + " < " + b : b + " < " + a) + " ? ";
            at   = Shared(Of(Cell(test + i + " : " + at.cells[0].text + ")", primary)));
            kept = Shared(Of(Cell(test + b + " : " + a + ")", primary)));
        }
        return series.Indexes() ? at : kept;
    }
    PExpression<Value> visit_other(Expression<Value>*) override { throw Reason("this expression"); }

    // 'fast.v_n': the name after the point, sought in that scope alone. An
    // unnamed instance has no name for the struct to give it.
    PExpression<Value> visit(MemberExpression<Value>* expression) override {
        if (const Reference<Value>* model = ModelOf(*expression->Object()))
            return Answer(Instance(*model, Call(*expression->Object()), expression->Member()));
        // 'g.x_n', a sequence of the step, of a grad by several names (DESIGN.md).
        if (const auto group = Reasoned([&] {
                const typename ReferenceStack<Value>::Within within(definitions_, scope_);
                const typename ReferenceStack<Value>::Frame  frame(definitions_);
                return definitions_.GroupOf(*expression->Object());
            })) {
            const std::string  name  = group->home->Qualified(group->Name());
            const std::string& part  = expression->Member()->Name();
            const auto&        names = ReferenceStack<Value>::Group(*group)->Variables();
            const auto*        in =
                dynamic_cast<const MemberExpression<Value>*>(expression->Object().get());
            if (!IsSequence(*group))
                throw Reason(name +
                             ", a gradient by several names that is not a sequence, for now");
            if (!group->Clauses().front().parameters.parameters_names().empty())
                throw Reason(name + ", a gradient by several names that takes arguments, for now");
            if (!in && Call(*expression->Object()).arguments())
                throw Reason(name + " takes no arguments");
            if (in) own_ = Reasoned([&] { return &definitions_.Resolve(*in->Object(), *scope_); });
            if (own_ && own_->label.find('(') != std::string::npos)
                throw Reason("an unnamed instance, " + own_->label);
            if (std::ranges::find(names, part) == names.end())
                throw Reason(name + "." + part + " is not defined");
            const ParametersCall<Value>& call = Call(*expression->Member());
            if (call.arguments()) throw Reason(name + "." + part + " takes no arguments");
            if (!call.subexpr())
                throw Reason(name + "." + part + " is a sequence; index it (" + name + "." + part +
                             "_0)");
            return Answer(Term(Lookup(group->Name()).key + "." + part, group.get(), call));
        }
        const Scope<Value>* object =
            Reasoned([&] { return &definitions_.Resolve(*expression->Object(), *scope_); });
        if (object->label.find('(') != std::string::npos)
            throw Reason("an unnamed instance, " + object->label);
        own_ = object;
        return expression->Member()->accept(*this);
    }

    Code Field(const std::string& name, const Value& value) {
        Unreserved(name);
        const Parameter parameter{value.Size(), Doubles(value)};
        parameters_.emplace(name, parameter);
        read_parameters_.insert(name);
        return Fields(name, Literal(value));
    }

    // The struct's field for a value of this shape, cell by cell.
    static Code Fields(const std::string& name, const Code& shape) {
        Unreserved(name);
        return Array("m_->" + name, shape.size);
    }

    // A cell that is a name or a number is read as itself, which the C
    // compiler can see through, rather than from the field; not a temporary
    // of the update's, which the step does not declare (C220).
    static Code Read(Derived& derived) {
        Code code = Fields(derived.name, derived.code);
        for (std::size_t c = 0; c < code.cells.size(); ++c) {
            const Cell& cell = derived.code.cells[c];
            const auto  own  = [&](const Temporary& t) { return cell.text.starts_with(t.name); };
            if (cell.atom && std::ranges::none_of(derived.temporaries, own))
                code.cells[c] = cell;
            else
                derived.read = true;
        }
        return code;
    }

    // A name here is letters and digits, so it can only collide with C's or
    // C++'s own; one in an instance is several, 'fast.v'.
    static void Unreserved(const std::string& key) {
        for (std::size_t start = 0; start <= key.size();) {
            const std::size_t end  = std::min(key.find('.', start), key.size());
            const std::string name = key.substr(start, end - start);
            if (const std::string keeper = Keeper(name); !keeper.empty())
                throw Reason(name + " is a word " + keeper + " keeps for itself");
            start = end + 1;
        }
    }

    // A sequence at another rate, once every sample of its clauses is known:
    // its phase, the first step at which every sample exists; its base terms
    // keyed by the step that computes them; its samples as lags of the
    // input's; its own terms read back, in its own terms.
    void Rate(const std::string& name, Sequence& sequence) {
        // A slow sequence read by another says what a hold at the input's
        // rate sampled says, but for a hold at another period, which no hold
        // says (C74).
        const auto another = [&](const std::string& read) {
            return Refusal(name, read +
                                     "_(...): one sequence at another rate read by another; hold " +
                                     read + " at the input's rate and sample the hold");
        };
        // What the right of an 'and' or 'or' reads too (C234).
        for (const Reads* some : {&sequence.reads, &sequence.deferred})
            for (const auto& [read, lags] : *some) {
                const int period = sequences_.at(read).period;
                if (read != name && totals_.count(read) && std::max(period, sequence.period) > 1)
                    throw Refusal(name, totals_.at(read).upto + " in a sequence computed every " +
                                            std::to_string(std::max(period, sequence.period)) +
                                            ", for now");
                if (read != name && period > 1 && sequence.period > 1) throw another(read);
                if (read != name && period > 1)
                    throw Refusal(name, read + "_(...): read every step, and " + read +
                                            " is computed every " + std::to_string(period));
            }
        for (const std::size_t h : sequence.holds) {
            const Hold& hold   = holds_[h];
            const int   every  = sequence.period * hold.a;
            const int   period = sequences_.at(hold.read).period;
            if (every != period)
                throw Refusal(name, hold.read + "_(...): read every " + std::to_string(every) +
                                        " steps, and " + hold.read + " is computed every " +
                                        std::to_string(period));
            if (sequence.period > 1) throw another(hold.read);
        }
        if (sequence.period == 1) return;
        for (const auto& [read, offsets] : sequence.samples)
            if (sequences_.at(read).period > 1) throw another(read);
        for (const Reads* some : {&sequence.reads, &sequence.deferred})
            for (const auto& [read, lags] : *some)
                if (read != name)
                    throw Refusal(name, read + "_(...): read every step, and " + name +
                                            " is computed every " +
                                            std::to_string(sequence.period));
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
        if (const auto own = sequence.deferred.find(name); own != sequence.deferred.end())
            sequence.own.insert(own->second.begin(), own->second.end());
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
                throw Refusal(
                    name,
                    hold.read + "_(...): read before it is computed; read the term before it");
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
            std::string       cell = "m_->" + hold.read + "[" + lag + "]" + Subscript(held.size, c);
            if (hold.from > sequence.start)
                cell = "(m_->index_ < " + std::to_string(hold.from) + " ? NAN : " + cell + ")";
            return cell;
        });
        return text;
    }

    // Where each sequence starts: at its lowest base clause, or, without one,
    // at the first index where every term it reads exists -- which is where
    // the interpreter, asked for the term, would first answer. An input starts
    // with the earliest.
    int Starts() {
        std::optional<int> earliest;
        for (auto& [name, sequence] : sequences_) {
            if (sequence.bases.empty()) continue;
            sequence.start = sequence.bases.begin()->first;
            sequence.first = (sequence.start - sequence.phase) / sequence.period;
            earliest       = std::min(earliest.value_or(sequence.start), sequence.start);
        }
        earliest_ = earliest.value_or(0);
        for (auto& [name, sequence] : sequences_) {
            if (!sequence.bases.empty()) continue;
            sequence.start = Tick(sequence, earliest.value_or(0));
            sequence.first = (sequence.start - sequence.phase) / sequence.period;
        }
        // The interpreter's guarded clauses answer below the lowest base
        // clause too, where its unguarded one does not: '_(-1)' of a sequence
        // based at 0 is a term wherever a guard holds. A step has no such
        // terms, so a read that could reach one is refused, but before the
        // stream, where init folds them (C351).
        for (const auto& [name, sequence] : sequences_) {
            int first = sequence.start;
            while (sequence.bases.count(first)) first += sequence.period;  // the general clauses
            for (const auto& [read, lags] : sequence.reads) {
                const Sequence& other = sequences_.at(read);
                if (other.bases.empty() || other.guarded.empty() || Descends(read) ||
                    Historied(read))
                    continue;
                const int reached = first - *lags.rbegin();
                if (reached < other.start)
                    throw Refusal(name,
                                  name + "_" + std::to_string(first) + " reads " + read + "_" +
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
        // One based where the stream starts has no term below it where no
        // guard holds: a reader starts above the highest such, if no term
        // it reads further down has one either (C376).
        for (const auto& [name, sequence] : sequences_) {
            if (!Under(name)) continue;
            int deepest = 0;
            for (const auto& [reader, other] : sequences_)
                if (const auto read = other.reads.find(name); read != other.reads.end())
                    deepest = std::max(deepest, *read->second.rbegin());
            std::optional<int> none;
            bool               below = true;
            for (int n = earliest_ - 1; n >= earliest_ - deepest; --n) {
                const bool held = std::ranges::any_of(sequence.guarded, [&](const Guarded& g) {
                    const auto& p = sequence.definition->Clauses()[g.clause].parameters;
                    return !Decides(
                        {{}, p.guard(), true, sequence.definition->home, p.index_name(), {}, 0}, n);
                });
                if (!held && !none) none = n;
                below = below && !(held && none);
            }
            if (none && below) exists_[name] = *none + 1;
        }
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
                // An inner optional input starts with the stream, as its argument may not (C391).
                const int answers = Optional(name) ? earliest.value_or(0) : Answers(sequence);
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
                guarded.guard_from = From(name, guarded.guard);
                guarded.value_from = From(name, guarded.value);
            }
        }
        // Without guards, the one clause is always evaluated, so a term it
        // reads before that term exists is an error in the model, said now.
        for (const auto& [name, sequence] : sequences_) {
            if (!sequence.guarded.empty() || Optional(name)) continue;
            for (const auto& [read, lags] : sequence.reads) {
                const int begins = Exists(read);
                for (const int lag : lags) {
                    for (int n = sequence.start; n < begins + lag; ++n) {
                        if (sequence.bases.count(n) || !Ticks(sequence, n)) continue;
                        throw Refusal(name, name + "_" + std::to_string(n) + " reads " + read +
                                                "_" + std::to_string(n - lag) +
                                                ", before it starts at " + std::to_string(begins));
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
            const std::string head = name + "_";
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
                        throw Refusal(
                            name, head + at + "below " + hold.read +
                                      "'s base clauses, where only its guards could give a term");
                    if (held.bases.empty() &&
                        std::any_of(held.samples.begin(), held.samples.end(), [&](const auto& x) {
                            return held.period * term + *x.second.rbegin() >= Exists(x.first);
                        }))
                        throw Refusal(name,
                                      head + at + "before " + hold.read +
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
                            throw Refusal(name, head + std::to_string((k - b) / sequence.period) +
                                                    " reads " + read + "_" + std::to_string(k) +
                                                    ", before the step computes " + read +
                                                    ", where its clauses could give a term");
            }
        }
    }

    // The whole part of n/a, a > 0, as floor gives it.
    static int Floor(int n, int a) { return n / a - (n % a < 0 ? 1 : 0); }

    // Whether a sequence's first guard always reads the sequence itself at or
    // before the index asked, through sequences of one clause: below its base
    // clauses that guard asks lower still, so no guard answers there, as in
    // the interpreter. A right side not always read is not among the reads.
    bool Descends(const std::string& name) const {
        std::vector<std::pair<std::string, int>> next;
        for (const auto& [read, lags] : sequences_.at(name).guarded.front().guard)
            next.emplace_back(read, *lags.rbegin());
        std::set<std::string> seen;
        while (!next.empty()) {
            const auto [read, lag] = next.back();
            next.pop_back();
            if (read == name && lag >= 0) return true;
            const Sequence& s = sequences_.at(read);
            if (!seen.insert(read).second || !s.bases.empty() || !s.guarded.empty() ||
                !s.definition)
                continue;
            for (const auto& [further, lags] : s.reads)
                next.emplace_back(further, lag + *lags.rbegin());
        }
        return false;
    }

    // The first tick of a sequence from step n on.
    static int Tick(const Sequence& sequence, int n) {
        while (!Ticks(sequence, n)) ++n;
        return n;
    }

    // Those of a clause of name's, but a slow one's own, which init folds
    // or Cover refuses (C344).
    int From(const std::string& name, Reads reads) const {
        if (Own(name)) reads.erase(name);
        return From(reads);
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

    // An input of the model compiled, one with a history, or a recurrence:
    // what is read of it before the stream is what init folds, or refused.
    bool Historied(const std::string& name) const {
        return histories_.count(name) || (root_.model && !sequences_.at(name).definition) ||
               (Recurrent(name) && sequences_.at(name).own.empty()) || Under(name);
    }

    // One based where the stream starts whose guards answer below: its terms
    // before the stream are what init folds, as a history's are (C351).
    bool Under(const std::string& name) const {
        const Sequence& sequence = sequences_.at(name);
        return sequence.definition && sequence.period == 1 && !sequence.guarded.empty() &&
               !sequence.bases.empty() && sequence.bases.begin()->first == earliest_ &&
               !ByCells(*sequence.definition) && !Descends(name);
    }

    // One with no base clause that reads its own terms back, through others
    // with none too, below whose base clauses no term is (C349): those
    // before the stream are what init folds, as an input's history's are
    // (C343).
    bool Recurrent(const std::string& name) const {
        const Sequence& sequence = sequences_.at(name);
        return Recomputed(sequence) && (!sequence.own.empty() || Reaches(name, &sequence));
    }
    // A slow one whose terms read its own back: those before its first are
    // what init folds, or refused (C344).
    bool Own(const std::string& name) const {
        const Sequence& sequence = sequences_.at(name);
        return !sequence.own.empty() && sequence.definition && !ByCells(*sequence.definition);
    }
    bool Reaches(const std::string& from, const Sequence* to) const {
        std::set<std::string>    seen{from};
        std::vector<std::string> left{from};
        while (!left.empty()) {
            const Sequence& sequence = sequences_.at(left.back());
            left.pop_back();
            std::vector<std::string> reads;
            for (const Reads* each : {&sequence.reads, &sequence.deferred})
                for (const auto& [read, lags] : *each) reads.push_back(read);
            for (const auto& [reader, read] : pending_)
                if (reader == &sequence) reads.push_back(read);
            for (const std::string& read : reads) {
                if (&sequences_.at(read) == to) return true;
                if (seen.insert(read).second && sequences_.at(read).bases.empty())
                    left.push_back(read);
            }
        }
        return false;
    }

    // A term before the stream that a guard holding there gives as a
    // constant, whatever a parameter is: 'g_n | n < 0 = 1'.
    bool Given(const std::string& name, int n) {
        const Sequence& sequence = sequences_.at(name);
        for (const Guarded& guarded : sequence.guarded) {
            const auto& p = sequence.definition->Clauses()[guarded.clause].parameters;
            Check guard{{}, p.guard(), false, sequence.definition->home, p.index_name(), {}, 0};
            if (Decides(guard, n)) return guarded.constant;
            guard.conjunction = true;
            if (!Decides(guard, n)) return false;
        }
        return false;
    }

    // A term before the stream as the interpreter gives it, where only a
    // history does.
    bool Folded(const std::string& name, int n) {
        const auto [slot, fresh]        = folded_.try_emplace({name, n});
        const auto              history = histories_.find(name);
        const Reference<Value>* definition =
            history != histories_.end() ? history->second : sequences_.at(name).definition;
        // A recurrence's own term, where no parameter can change it: an input
        // its term reads there has none to give, and the term is refused (C403).
        const bool given = (Recurrent(name) || Own(name) || Under(name)) &&
                           (Parametric(name).empty() || Given(name, n));
        // A history's clause that holds there reads no parameter, whatever
        // the argument it is tried before reads (C379).
        const bool historic =
            history != histories_.end() &&
            std::ranges::any_of(history->second->Clauses(), [&](const Clause<Value>& c) {
                const auto& p = c.parameters;
                if (!p.general()) return p.index() == n;
                const Check holds{{}, p.guard(), false, history->second->home, p.index_name(),
                                  {}, 0};
                return p.guarded() && Decides(holds, n);
            });
        const std::string parameter = given || historic ? "" : Parametric(name);
        const std::string assigned  = "reads " + parameter + ", which the host may assign after init";
        // One with no default leaves init nothing to evaluate.
        const bool unset = model_ && std::ranges::any_of(model_->parameters, [&](const auto& p) {
            return p.name == parameter && p.index.empty() && !p.fallback;
        });
        if (fresh && definition) {
            definitions_.Forget();
            definitions_.BeginEvaluation();
            try {
                const ParametersCall<Value> at(
                    PExpression<Value>(), std::make_shared<ValExpression<Value>>(Value(Number(n))));
                const Value term = definition->Eval(at, definitions_, true);
                if (!parameter.empty()) {
                    unfolded_[{name, n}] = assigned;
                } else if (definitions_.histories || given) {
                    const std::vector<double> cells = Doubles(term);
                    if (std::all_of(cells.begin(), cells.end(), Finite)) {
                        slot->second = term;
                        if (Value::approximated(term)) approximated_in_.insert(name);
                    } else {
                        unfolded_[{name, n}] = "gives a term no " + Real() + " holds";
                    }
                }
            } catch (const std::runtime_error& error) {
                if (unset)
                    unfolded_[{name, n}] = assigned;
                else if (definitions_.histories || given || Under(name))
                    unfolded_[{name, n}] = std::string("gives none: ") + error.what();
            }
        }
        return slot->second.has_value();
    }

    // Why a read before the stream is refused: what its history gave there,
    // or that it has none.
    std::string Unfolded(const std::string& read, int at) const {
        const auto why = unfolded_.find({read, at});
        return why != unfolded_.end()
                   ? (totals_.count(read) ? "it " : read + "'s history ") + why->second
                   : read + " has no history";
    }

    // A parameter a term can read, which the host may assign after init has
    // folded it, or with 'inputs' an input; none, empty.
    std::string Parametric(const std::string& name, bool inputs = false) const {
        std::set<std::string>    seen{name};
        std::vector<std::string> left{name};
        while (!left.empty()) {
            const std::string at       = left.back();
            const Sequence&   sequence = sequences_.at(at);
            left.pop_back();
            if (!sequence.parameters.empty()) return *sequence.parameters.begin();
            if (inputs && !sequence.definition) return at;
            std::vector<std::string> reads;
            for (const Reads* each :
                 {&sequence.reads, &sequence.deferred, &sequence.samples, &sequence.seeded})
                for (const auto& [read, lags] : *each) reads.push_back(read);
            for (const std::size_t h : sequence.holds) reads.push_back(holds_[h].read);
            for (const std::string& read : reads)
                if (seen.insert(read).second) left.push_back(read);
        }
        return "";
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
        std::vector<Temporary> scratch;  // what a term read there declares, kept nowhere (C188)
        const Setting<std::vector<Temporary>*> within(temporaries_, &scratch);
        try {
            const Code code = Quiet(check.left);
            decides         = code.constant && Holds(*code.constant) != check.conjunction;
        } catch (const Reason&) {
        }
        scope_  = scope;
        index_  = index;
        places_ = outer;
        const auto in = [&](const auto& term) { return std::get<0>(term.first) == &scratch; };
        std::erase_if(terms_, in);
        std::erase_if(failed_, in);
        std::erase_if(filled_, in);
        return decides;
    }

    // Each read before the stream, from a reader's start until 'until', that
    // no history gives and nothing keeps.
    void Cover(const std::string& name, const Sequence& reader, const Reads& reads, int until,
               const std::vector<Check>& kept = {}) {
        const auto kept_at = [&](int n) {
            return std::ranges::any_of(kept, [&](const Check& c) { return Decides(c, n); });
        };
        for (const auto& [read, lags] : reads) {
            // A slow one's own, in its terms, before those init folds (C343).
            if (&sequences_.at(read) == &reader && reader.period > 1 && Own(read)) {
                // Init folds back from the first, so a read past the term
                // that stopped it is refused for that term, named first.
                const int stop = reader.first - reader.filled - 1;
                for (const bool at_stop : {true, false})
                    for (const int lag : lags)
                        for (int m = reader.first; m - lag < reader.first - reader.filled; ++m)
                            if ((m - lag == stop) == at_stop && !kept_at(m) &&
                                !reader.bases.count(reader.period * m + reader.phase))
                                throw Refusal(
                                    name,
                                    name + "_" + std::to_string(m) + " reads " + read + "_" +
                                        std::to_string(m - lag) +
                                        (reader.bases.empty()
                                             ? ", before the stream, " +
                                                   (at_stop ? std::string()
                                                            : "below " + read + "_" +
                                                                  std::to_string(stop) + ", ") +
                                                   "where " + Unfolded(read, stop)
                                         : reader.guarded.empty()
                                             ? ", before it starts at " +
                                                   std::to_string(reader.first)
                                             : ", below " + read +
                                                   "'s base clauses, where only its "
                                                   "guards could give a term"));
                continue;
            }
            if (!Historied(read)) continue;
            for (const int lag : lags) {
                for (int n = reader.start; n < std::min(until, earliest_ + lag); ++n) {
                    if (!Ticks(reader, n) || reader.bases.count(n) || Folded(read, n - lag) ||
                        kept_at(n) || Optional(read))
                        continue;
                    const std::string& who   = totals_.count(name) ? totals_.at(name).reader : name;
                    const auto         total = totals_.find(read);
                    const std::string  term  = total == totals_.end()
                                                   ? read + "_" + std::to_string(n - lag)
                                                   : total->second.reader + "'s " +
                                                       total->second.series->Noun() + " up to " +
                                                       std::to_string(n - lag - total->second.lag);
                    throw Refusal(who, who + "_" +
                                           std::to_string(Floor(n - reader.phase, reader.period)) +
                                           " reads " + term + ", before the stream, where " +
                                           Unfolded(read, n - lag));
                }
            }
        }
    }

    // The reads before the stream; the step's deferred ones are checked as it
    // is written.
    void Histories(int earliest) {
        // A slow one's own terms before its first, as a hold's are (C343).
        for (auto& [name, sequence] : sequences_)
            while (Own(name) && sequence.filled < *sequence.own.rbegin() &&
                   Folded(name, sequence.first - sequence.filled - 1))
                ++sequence.filled;
        for (const auto& [name, last] : reach_)
            if (last >= earliest)
                throw Refusal(name, "its history reaches " + name + "_" + std::to_string(earliest) +
                                        ", in the stream");
        for (const auto& [name, sequence] : sequences_) {
            if (!sequence.definition) continue;
            // A guarded clause's value is read only where its guard holds.
            Reads always = sequence.reads;
            for (const Guarded& guarded : sequence.guarded) {
                const auto& p = sequence.definition->Clauses()[guarded.clause].parameters;
                const Check kept{guarded.value,  p.guard(), true, sequence.definition->home,
                                 p.index_name(), {},        0};
                Cover(name, sequence, guarded.value, std::numeric_limits<int>::max(), {kept});
                for (const auto& [read, lags] : guarded.value)
                    for (const int lag : lags) always[read].erase(lag);
            }
            // The general clause's, where no guard holds (C343).
            std::vector<Check> holds;
            for (const Guarded& guarded : sequence.guarded) {
                const auto& p = sequence.definition->Clauses()[guarded.clause].parameters;
                holds.push_back(
                    {{}, p.guard(), false, sequence.definition->home, p.index_name(), {}, 0});
            }
            Cover(name, sequence, sequence.general_reads, std::numeric_limits<int>::max(), holds);
            for (const auto& [read, lags] : sequence.general_reads)
                for (const int lag : lags) always[read].erase(lag);
            for (const Guarded& guarded : sequence.guarded)
                for (const auto& [read, lags] : guarded.guard)
                    always[read].insert(lags.begin(), lags.end());
            Cover(name, sequence, always, std::numeric_limits<int>::max());
            // A base clause's reads, which Cover passes over.
            for (const Seed& seed : sequence.seeds)
                if (seed.at < earliest_ && Historied(seed.read) && !Folded(seed.read, seed.at))
                    throw Refusal(name, name + "_" + std::to_string(seed.index) + " reads " +
                                            seed.read + "_" + std::to_string(seed.at) +
                                            ", before the stream, where " +
                                            Unfolded(seed.read, seed.at));
        }
        for (std::size_t id = 0; id < earlies_.size(); ++id)
            for (const auto& [name, sequence] : sequences_)
                if (const Early& early = earlies_[id]; &sequence == early.reader && Needed(early))
                    Cover(name, sequence, early.reads, sequences_.at(early.name).start + early.lag,
                          Kept(sequence, "\x04" + std::to_string(id) + "\x05"));
    }

    // What a mark stands for, read by the general clause alone, is read
    // only where no guard holds (C413).
    std::vector<Check> Kept(const Sequence& sequence, const std::string& mark) const {
        const auto in = [&](const std::string& text) {
            return text.find(mark) != std::string::npos;
        };
        std::vector<Check> holds;
        if (std::ranges::none_of(sequence.general, in) ||
            std::ranges::any_of(sequence.guarded, [&](const Guarded& g) {
                return in(g.condition) || std::ranges::any_of(g.cells, in);
            }))
            return holds;
        for (const Guarded& guarded : sequence.guarded) {
            const auto& p = sequence.definition->Clauses()[guarded.clause].parameters;
            holds.push_back(
                {{}, p.guard(), false, sequence.definition->home, p.index_name(), {}, 0});
        }
        return holds;
    }

    // init's lines for what histories give, once every read has asked.
    std::string Folds(int earliest) const {
        std::string lines;
        for (const auto& [name, sequence] : sequences_) {
            const bool optional = Optional(name);  // absent before the stream but where given
            for (int k = 0; optional && k < sequence.depth; ++k)
                lines += Unset(name, std::to_string(k));
            const bool slow  = sequence.period > 1;
            const int  terms = slow              ? std::min(sequence.filled, sequence.depth)
                               : Historied(name) ? sequence.depth - 1
                                                 : 0;
            for (int k = 0; k < terms; ++k) {
                const auto term = folded_.find({name, (slow ? sequence.first : earliest) - 1 - k});
                if (term == folded_.end() || !term->second) continue;
                const std::vector<double> cells = Doubles(*term->second);
                if (term->second->Size() != sequence.size)
                    throw Refusal(name, "a history of another shape");
                for (std::size_t c = 0; c < cells.size(); ++c)
                    if (cells[c] != 0.0 || optional)
                        lines += "    m_->" + name + "[" + std::to_string(k) + "]" +
                                 Subscript(sequence.size, c) + " = " + Double(cells[c]) + ";\n";
                if (optional)
                    lines += "    m_->" + name + "_present[" + std::to_string(k) + "] = 1;\n";
            }
        }
        return lines;
    }

    // An optional input's cells at k, NaN.
    std::string Unset(const std::string& name, const std::string& k) const {
        const std::string at = "m_->" + name + "[" + k + "]";
        return Cells(name) == 1 ? "    " + at + " = NAN;\n"
                                : "    for (size_t c_ = 0; c_ < sizeof " + at +
                                      " / sizeof(double); ++c_) ((double*)" + at + ")[c_] = NAN;\n";
    }

    // Where no order computes every base a base clause reads at its own
    // index before it, as with a_0 = v_0 and v_n = a_n, such a base is also
    // computed ahead of its reader, by 'ahead' (C402).
    using Ahead = std::multimap<std::string, std::pair<std::string, int>>;
    std::vector<std::string> Order(Ahead* ahead = nullptr) const {
        std::map<std::string, std::set<std::string>> waiting;
        const auto early = [&](const Sequence& reader, const Seed& seed) {
            const Sequence& read = sequences_.at(seed.read);
            return ahead && reader.parts.empty() && read.parts.empty() && read.bases.count(seed.at);
        };
        for (const auto& [name, sequence] : sequences_) {
            if (!sequence.definition) continue;
            waiting[name];
            if (!sequence.parts.empty() && sequence.parts[0] != name)
                waiting[name].insert(sequence.parts[0]);
            // A hold's lag is counted from the window as the held term's tick
            // leaves it, so the held sequence comes first.
            for (const std::size_t h : sequence.holds) waiting[name].insert(holds_[h].read);
            for (const Reads* reads : {&sequence.reads, &sequence.deferred}) {
                for (const auto& [read, lags] : *reads) {
                    if (!lags.count(0) || !sequences_.at(read).definition) continue;
                    if (read == name) throw Refusal(name, "a term reads itself");
                    waiting[name].insert(read);
                }
            }
            for (const Seed& seed : sequence.seeds)
                if (seed.lag == 0 && sequences_.at(seed.read).definition && !early(sequence, seed))
                    waiting[name].insert(seed.read);
        }
        std::vector<std::string> order;
        while (!waiting.empty()) {
            const auto ready = std::find_if(waiting.begin(), waiting.end(),
                                            [](const auto& entry) { return entry.second.empty(); });
            if (ready == waiting.end())
                throw Refusal(waiting.begin()->first, "it and the terms it reads need each other");
            const std::string name = ready->first;
            order.push_back(name);
            waiting.erase(ready);
            for (auto& entry : waiting) entry.second.erase(name);
        }
        const auto place = [&](const std::string& name) {
            return std::find(order.begin(), order.end(), name) - order.begin();
        };
        // And ahead of it, what that base reads at its index and is not computed yet.
        const auto hoist = [&](const auto& self, const std::string& name, const Sequence& reader,
                               const Seed& seed, std::size_t depth) -> void {
            if (seed.lag || !sequences_.at(seed.read).definition || place(seed.read) < place(name))
                return;
            if (!early(reader, seed) || depth > order.size())
                throw Refusal(name, "it and the terms it reads need each other");
            const Sequence& read = sequences_.at(seed.read);
            for (const Seed& own : read.seeds)
                if (own.index == seed.at) self(self, name, read, own, depth + 1);
            ahead->emplace(name, std::pair(seed.read, seed.at));
        };
        for (const std::string& name : order)
            for (const Seed& seed : sequences_.at(name).seeds)
                hoist(hoist, name, sequences_.at(name), seed, 0);
        return order;
    }

    static std::string Dimensions(Extent size) {
        if (size.count() == 1 && !size.slices) return "";
        return (size.slices ? "[" + std::to_string(size.slices) + "]" : "") +
               Subscript(size.rows, size.cols);
    }

    static void Words(const std::string& text, std::set<std::string>& words) {
        const char* const word = "_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        for (std::size_t at = 0, end = 0; end != std::string::npos; at = end + 1) {
            end = text.find_first_not_of(word, at);
            words.insert(text.substr(at, end - at));
        }
    }

    // Those something reads: a cell read out of a product, or a clause that was
    // compiled only to be refused, leaves some that nothing does, and C would
    // say so.
    static std::vector<const Temporary*> Kept(const std::vector<Temporary>& declared,
                                              const std::string&            assignments) {
        std::set<std::string> read;  // names: a search of the text kept grows with its square
        Words(assignments, read);
        std::vector<const Temporary*> kept;
        for (auto temporary = declared.rbegin(); temporary != declared.rend(); ++temporary) {
            if (!read.count(temporary->name)) continue;
            for (const std::string& line : temporary->lines) Words(line, read);
            kept.push_back(&*temporary);
        }
        std::reverse(kept.begin(), kept.end());
        return kept;
    }
    static std::string Temporaries(const std::vector<Temporary>& declared,
                                   const std::string& assignments, const std::string& indent) {
        std::string out;
        for (const Temporary* temporary : Kept(declared, assignments))
            for (const std::string& line : temporary->lines) out += indent + line + "\n";
        return out;
    }

    // DESIGN.md, shared at every value written twice: a value of more than
    // one operation that a list writes more than once, where a name is
    // shorter, is declared once before its first reader, the outermost first,
    // and one a temporary holds is read by its name. It reads the texts once
    // every decision on NaN has: the same operations on the same operands.
    std::vector<Temporary> Numbered(const std::vector<Temporary>& declared,
                                    std::string&                  assignments) {
        struct Written {
            std::size_t                   uses = 0, length = 0;  // as printed
            std::vector<std::string_view> within;                // the values its text writes
            std::string                   name;
            bool                          shared = false, declared = false;
        };
        std::unordered_map<std::string_view, Written> values;
        const auto                                    scan = [&](std::string_view text) {
            std::vector<std::tuple<std::size_t, std::size_t, std::vector<std::string_view>>> open;
            for (std::size_t at = 0, marks = 0; at < text.size(); ++at)
                if (text[at] == '\x14')
                    open.emplace_back(at, marks++, std::vector<std::string_view>());
                else if (text[at] == '\x15') {
                    auto [from, before, within] = std::move(open.back());
                    open.pop_back();
                    const std::string_view value = text.substr(from, at + 1 - from);
                    const auto [entry, fresh] = values.try_emplace(value);
                    ++marks;
                    if (fresh) entry->second.within = std::move(within);
                    entry->second.length = value.size() - (marks - before);
                    if (open.empty())
                        ++entry->second.uses;
                    else
                        std::get<2>(open.back()).push_back(value);
                }
        };
        const std::string written = std::move(assignments);
        const auto        kept    = Kept(declared, written);
        // A value held as its line writes it, which Rated and Checked rewrote.
        std::vector<std::string_view> held;
        for (const Temporary* t : kept) {
            const std::string_view line = t->lines[0];
            const std::string      head = "const double " + t->name + " = \x14";
            held.push_back(line.starts_with(head)
                               ? line.substr(head.size() - 1, line.size() - head.size())
                               : "");
            for (const std::string& text : t->lines) scan(text);
        }
        scan(written);
        for (std::size_t t = 0; t < kept.size(); ++t)
            if (const auto at = values.find(held[t]); at != values.end() && at->second.name.empty())
                at->second.name = kept[t]->name;
        std::vector<std::pair<const std::string_view, Written>*> order;
        for (auto& value : values) order.push_back(&value);
        std::ranges::sort(order,
                          [](auto* a, auto* b) { return a->first.size() > b->first.size(); });
        for (auto* entry : order) {
            Written& w = entry->second;
            w.shared   = !w.name.empty() ||
                       (w.uses > 1 && !w.within.empty() && (w.uses - 1) * w.length > 40);
            for (const std::string_view inner : w.within)
                values.at(inner).uses += w.shared ? 1 : w.uses;
        }
        std::vector<Temporary> numbered;
        const auto             write = [&](auto& self, std::string_view text) -> std::string {
            std::vector<std::size_t> ends(text.size()), open;
            for (std::size_t at = 0; at < text.size(); ++at)
                if (text[at] == '\x14')
                    open.push_back(at);
                else if (text[at] == '\x15') {
                    ends[open.back()] = at;
                    open.pop_back();
                }
            std::string out;
            for (std::size_t at = 0; at < text.size(); ++at) {
                Written* w = nullptr;
                if (text[at] == '\x14') w = &values.at(text.substr(at, ends[at] + 1 - at));
                if (!w || !w->shared) {
                    if (text[at] != '\x14' && text[at] != '\x15') out += text[at];
                    continue;
                }
                if (w->name.empty()) w->name = "t" + std::to_string(temporary_count_++) + "_";
                if (!std::exchange(w->declared, true))
                    numbered.push_back(
                        {w->name,
                         "",
                         {"const double " + w->name + " = " +
                          self(self, text.substr(at + 1, ends[at] - at - 1)) + ";"}});
                // A name needs no brackets, but a call's.
                const unsigned char before = out.size() > 1 ? out[out.size() - 2] : ' ';
                const bool bare = out.ends_with('(') && !std::isalnum(before) && before != '_' &&
                                  text.substr(ends[at] + 1).starts_with(')');
                out.resize(out.size() - bare);
                out += w->name;
                at = ends[at] + bare;
            }
            return out;
        };
        for (std::size_t t = 0; t < kept.size(); ++t) {
            if (const auto at = values.find(held[t]);
                at != values.end() && at->second.shared && at->second.name == kept[t]->name) {
                write(write, at->first);
                continue;
            }
            Temporary copy{kept[t]->name, kept[t]->value, {}};
            for (const std::string& line : kept[t]->lines) copy.lines.push_back(write(write, line));
            numbered.push_back(std::move(copy));
        }
        assignments = write(write, written);
        return numbered;
    }

    // Where the model holds what an instance's lists carry, 'v_.u_.a_held': each
    // list in one branch of a union with its siblings', stepped in turn, as no
    // value outlives its list. It reserves the path and every enclosing one.
    std::string Place(const Scope<Value>* scope) {
        const auto outer = outer_.find(scope);
        if (outer == outer_.end()) return scratch_.try_emplace("v_").first->first;
        const std::string& in     = outer->second->label;
        std::string        member = scope->label.substr(in.size() + !in.empty());
        std::replace(member.begin(), member.end(), '.', '_');
        return scratch_.try_emplace(Place(outer->second) + ".u_." + member).first->first;
    }

    // Their type, 'twice_a_held_values_': no scope names an instance and a sequence alike.
    std::string Typed(std::string path) const {
        for (std::size_t at; (at = path.find(".u_.")) != path.npos;) path.replace(at, 4, "_");
        return module_ + path.substr(2) + "_values_";
    }

    // Its own values, then a union of its instances'.
    std::string Declared(const std::string& path) const {
        std::string types, members = scratch_.at(path), inner;
        for (const auto& [at, values] : scratch_)
            if (at.starts_with(path) && at.rfind(".u_.") == path.size()) {
                types += Declared(at);
                inner += "        " + Typed(at) + " " + at.substr(path.size() + 4) + ";\n";
            }
        if (!inner.empty()) members += "    union {\n" + inner + "    } u_;\n";
        return types + "typedef struct " + Typed(path) + " {\n" + members + "} " + Typed(path) +
               ";\n\n";
    }

    // A step's list over 4 * split_ bytes, which compilers optimise in time
    // that grows faster than its length, written as functions of about split_
    // bytes, in order: a temporary's lines or a statement each, so at least one.
    // A value a later one reads crosses in the step's struct of values into a
    // local of its own name, so that every line is written as before. Every
    // temporary is read: Numbered keeps no other.
    std::string Split(const std::vector<Temporary>& kept, const std::string& assignments,
                      const std::string& indent, const Scope<Value>* home) {
        std::string whole;
        for (const Temporary& temporary : kept)
            for (const std::string& line : temporary.lines) whole += indent + line + "\n";
        whole += assignments;
        if (whole.size() <= 4 * split_) return whole;
        std::vector<std::string> texts;
        for (const Temporary& temporary : kept) {
            texts.emplace_back();
            for (const std::string& line : temporary.lines) texts.back() += "    " + line + "\n";
        }
        for (std::size_t at = 0, end = 0; at < assignments.size(); at = end) {
            end = assignments.find('\n', at) + 1;
            // A statement may run on: an 'if' and its loops.
            if (at == 0 || assignments[at - 2] == ';') texts.emplace_back();
            texts.back() +=
                "    " + assignments.substr(at + indent.size(), end - at - indent.size());
        }
        std::vector<std::size_t> first{0};  // each function's first text
        for (std::size_t t = 0, size = 0; t < texts.size(); size += texts[t++].size())
            if (size && size + texts[t].size() > split_) {
                first.push_back(t);
                size = 0;
            }
        first.push_back(texts.size());
        const std::size_t                  functions = first.size() - 1;
        std::vector<std::set<std::string>> words(functions);
        std::vector<std::string>           loads(functions), stores(functions);
        for (std::size_t f = 0; f < functions; ++f)
            for (std::size_t t = first[f]; t < first[f + 1]; ++t) Words(texts[t], words[f]);
        for (std::size_t f = 0; f < functions; ++f)
            for (std::size_t t = first[f]; t < first[f + 1] && t < kept.size(); ++t) {
                // 'const double t5_ = ...;', or an array, 'double t5_[3][3] = {...};'.
                const std::string& name  = kept[t].name;
                const std::string& line  = kept[t].lines[0];
                const bool         array = !line.starts_with("const ");
                const std::string  member =
                    array ? line.substr(0, line.find_first_of(" ;", line.find(name)))
                           : line.substr(6, line.find(" = ") - 6);
                const auto copy = [&](const std::string& to, const std::string& from) {
                    return array ? "    memcpy(" + to + ", " + from + ", sizeof " + name + ");\n"
                                 : "    " + to + " = " + from + ";\n";
                };
                bool read = false;
                for (std::size_t later = f + 1; later < functions; ++later)
                    if (words[later].count(name)) {
                        read = true;
                        loads[later] += array ? "    " + member + ";\n" + copy(name, "v_->" + name)
                                              : "    const " + member + " = v_->" + name + ";\n";
                    }
                if (!read) continue;
                scratch_[Place(home)] += "    " + member + ";\n";
                stores[f] += copy("v_->" + name, name);
            }
        std::string out;
        for (std::size_t f = 0; f < functions; ++f) {
            // Passed the values only where it carries some: there may be none.
            const bool  carries = !loads[f].empty() || !stores[f].empty();
            std::string body    = (words[f].count("m_") ? "" : "    (void)m_;\n") + loads[f];
            for (std::size_t t = first[f]; t < first[f + 1]; ++t) body += texts[t];
            const std::string called = module_ + "_step" + std::to_string(++splits_) + "_";
            pieces_ += "static " + upper_ + "_NOINLINE_ void " + called + "(" + module_ + "* m_" +
                       (carries ? ", " + Typed(Place(home)) + "* v_" : "") + ") {\n" + body +
                       stores[f] + "}\n\n";
            out += indent + called + (carries ? "(m_, &m_->" + Place(home) + ");\n" : "(m_);\n");
        }
        return out;
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
               module_ + "_inverse" + size + "_(double a_[" + size + "][" + size +
               "]) {\n"
               "    double r_[" +
               size + "][" + size +
               "];\n"
               "    for (int i_ = 0; i_ < " +
               size + "; ++i_)\n        for (int j_ = 0; j_ < " + size +
               "; ++j_) r_[i_][j_] = i_ == j_ ? 1.0 : 0.0;\n"
               "    for (int col_ = 0; col_ < " +
               size +
               "; ++col_) {\n"
               "        int pivot_ = col_;\n"
               "        for (int row_ = col_ + 1; row_ < " +
               size +
               "; ++row_)\n"
               "            if (fabs(a_[row_][col_]) > fabs(a_[pivot_][col_]) ||\n"
               "                (a_[pivot_][col_] == 0.0 && a_[row_][col_] != 0.0))\n"
               "                pivot_ = row_;\n"
               "        if (a_[pivot_][col_] == 0.0) {\n"
               "            for (int i_ = 0; i_ < " +
               size + "; ++i_)\n                for (int j_ = 0; j_ < " + size +
               "; ++j_) a_[i_][j_] = NAN;\n"
               "            return;\n"
               "        }\n"
               "        for (int j_ = 0; j_ < " +
               size +
               "; ++j_) {\n"
               "            const double s_ = a_[pivot_][j_], t_ = r_[pivot_][j_];\n"
               "            a_[pivot_][j_] = a_[col_][j_];\n"
               "            r_[pivot_][j_] = r_[col_][j_];\n"
               "            a_[col_][j_]   = s_;\n"
               "            r_[col_][j_]   = t_;\n"
               "        }\n"
               "        const double scale_ = a_[col_][col_];\n"
               "        for (int j_ = 0; j_ < " +
               size +
               "; ++j_) {\n"
               "            a_[col_][j_] = a_[col_][j_] / scale_;\n"
               "            r_[col_][j_] = r_[col_][j_] / scale_;\n"
               "        }\n"
               "        for (int row_ = 0; row_ < " +
               size +
               "; ++row_) {\n"
               "            const double factor_ = a_[row_][col_];\n"
               "            if (row_ == col_ || factor_ == 0.0) continue;\n"
               "            for (int j_ = 0; j_ < " +
               size +
               "; ++j_) {\n"
               "                a_[row_][j_] = a_[row_][j_] - factor_ * a_[col_][j_];\n"
               "                r_[row_][j_] = r_[row_][j_] - factor_ * r_[col_][j_];\n"
               "            }\n"
               "        }\n"
               "    }\n"
               "    memcpy(a_, r_, sizeof r_);\n"
               "}\n\n";
    }

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

    // Each deferred right side's mark (see Right), as the check it needs
    // where the terms it reads may not exist yet, or nothing where they do.
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
            std::vector<Check> keep = Kept(sequence, text.substr(at, end - at + 1));
            keep.push_back(kept);
            Cover(name, sequence, kept.reads, std::numeric_limits<int>::max(), keep);
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

    std::size_t Cells(const std::string& name) const { return sequences_.at(name).size.count(); }

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
        for (const std::string& name : inputs)
            arguments += ", " + name + (Optional(name) ? ", " + name + "_present" : "");
        std::vector<std::string> terms, inputs_n, parameters, rated;
        for (const std::string& name : fields) {
            const Sequence&   sequence = sequences_.at(name);
            const int         depth    = sequence.depth;
            const auto        total    = totals_.find(name);
            const std::string term =
                (depth > 1 ? name + "\x01(k\x01<=\x01" + std::to_string(depth - 1) + ")" : name) +
                (total == totals_.end()
                     ? ""
                     : " for " + total->second.reader + "'s " + total->second.series->Noun());
            if (sequence.period == 1) {
                terms.push_back(term);
                continue;
            }
            rated.push_back(term + ", computed at the steps " + std::to_string(sequence.period) +
                            "*m" + (sequence.phase ? Less(-sequence.phase) : std::string()));
        }
        for (const std::string& name : inputs) {
            inputs_n.push_back(
                name + "_n" +
                (Cells(name) == 1 ? "" : "\x01(" + sequences_.at(name).size.toString() + ")"));
        }
        for (const auto& [name, parameter] : parameters_)
            parameters.push_back(parameter.initial.size() == 1
                                     ? name + "\x01=\x01" + Double(parameter.initial[0])
                                     : name + "\x01(" + parameter.size.toString() + ")");
        std::string text =
            terms.empty()
                ? ""
                : "After a step, m.name[k] is name_(n-k) for each sequence: " + list(terms) + ".";
        if (!rated.empty())
            text += std::string(terms.empty() ? "" : " ") +
                    "At another rate, m.name[k] is name_(m-k), m its latest term's index: " +
                    list(rated) + ".";
        const auto sliced = [&](auto& name) { return sequences_.at(name).size.slices > 0; };
        if (std::any_of(inputs.begin(), inputs.end(), [&](auto& name) { return Cells(name) > 1; }))
            text = "An input of more than one cell is a pointer to its cells, row by row" +
                   std::string(std::any_of(inputs.begin(), inputs.end(), sliced)
                                   ? ", slice after slice. "
                                   : ". ") +
                   text;
        text = (inputs.empty() ? "A step takes no input. "
                : inputs.size() == 1
                    ? "A step takes " + list(inputs_n) + ", the input at its index. "
                    : "A step takes " + list(inputs_n) + ", the inputs at its index. ") +
               text;
        // With no sequence, no clause follows the input's.
        if (text.back() == ' ') text.pop_back();
        std::vector<std::string> flagged;
        for (const std::string& name : inputs)
            if (Optional(name)) flagged.push_back(name);
        if (!flagged.empty())
            text += " After an optional input, " + list(flagged) +
                    ", comes its flag, 0 where it is absent and its value is not read; "
                    "m.name_present[k] is 1 where name_(n-k) was given.";
        if (!parameters.empty())
            text += std::string(" The parameters are fields holding the ") +
                    (model_ ? "model's defaults" : "file's values") + " once " + module +
                    "_init has run: " + list(parameters) + ". After assigning one, call " + module +
                    "_update.";
        std::vector<std::string> fixed(fixed_.begin(), fixed_.end());
        if (!fixed.empty())
            text += " Compiled in as constants, these cannot change: " + list(fixed) + ".";
        std::vector<std::string> approximated(approximated_in_.begin(), approximated_in_.end());
        if (!approximated.empty())
            text += " A value folded for " + list(approximated) +
                    " from a number approximated past a thousand digits, as the interpreter "
                    "marks it, may be far from exact.";
        if (floats) text += " Every value is a float, and every operation rounds to one.";
        std::vector<std::string> doubled(rounded_in_.begin(), rounded_in_.end());
        if (!doubled.empty())
            text += " A value folded for " + list(doubled) +
                    " from inexact numbers was computed at the interpreter's precision, not a "
                    "float's, and a float step may compute another.";
        if (aware_)
            text +=
                " A term the interpreter would refuse is NaN, and so is every term that reads "
                "one, through a guard or a comparison as through arithmetic. Built with "
                "-ffinite-math-only, which -ffast-math implies, GCC removes the tests that make "
                "it so, and Clang warns of each NaN.";
        std::string out = "/* Using it:\n *\n";
        out += " *     " + module + " m;\n";
        out += " *     " + module + "_init(&m);\n";
        out += " *     " + module + "_step(&m" + arguments + ");  once for each index, the first " +
               std::to_string(earliest) + "\n";
        if (!fields.empty()) {
            const std::string& last  = fields.back();
            const Sequence&    shown = sequences_.at(last);
            std::string        place = "[i][j]", said = ", row i+1 and column j+1";
            if (shown.size.slices) {
                place = "[b]" + place;
                said  = ", slice b+1" + said;
            }
            if (Dimensions(shown.size).empty()) place = said = "";
            out += " *     m." + last + "[0]" + place + "  is then " + last + "_n" + said + "\n";
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

    // A sequence's cells assigned in its step, in the order the interpreter
    // tries its clauses; with 'only', that base term's alone.
    std::string Assigned(const std::string& name, const std::string& indent,
                         const std::vector<std::string>* only = nullptr) {
        const Sequence& sequence = sequences_.at(name);
        const bool      scalar   = sequence.general.size() == 1;
        std::string     assignments;
        for (std::size_t c = 0; c < sequence.general.size(); ++c) {
            assignments += indent + "m_->" + name + "[0]" + Subscript(sequence.size, c) + " = ";
            if (only) {
                assignments += (*only)[c] + ";\n";
                continue;
            }
            for (const auto& [index, base] : sequence.bases)
                assignments += "m_->index_ == " + std::to_string(index) + " ? " + base[c] + " : ";
            // Checked only where an index no base clause gives needs it.
            const auto before = [&](int from, const std::string& otherwise = "NAN") {
                for (int n = sequence.start; n < from; ++n)
                    if (!sequence.bases.count(n))
                        return "m_->index_ < " + std::to_string(from) + " ? " + otherwise + " : ";
                return std::string();
            };
            // Ahead of the cells, which the guards cannot read.
            if (clauses_ && c == 0 && !sequence.guarded.empty()) {
                std::string kept = indent + "m_->" + name + "_clause_ = ";
                for (const auto& [index, base] : sequence.bases)
                    kept += "m_->index_ == " + std::to_string(index) + " ? 0 : ";
                for (const Guarded& guarded : sequence.guarded)
                    kept += before(guarded.guard_from, "0") + Picked(guarded.condition) + " ? " +
                            std::to_string(guarded.clause + 1) + " : ";
                assignments.insert(0, kept + std::to_string(sequence.general_clause + 1) + ";\n");
            }
            for (const Guarded& guarded : sequence.guarded) {
                const std::string value = before(guarded.value_from);
                assignments +=
                    before(guarded.guard_from) + guarded.condition + " ? " +
                    (value.empty() ? guarded.cells[c] : "(" + value + guarded.cells[c] + ")") +
                    " : ";
            }
            if (!sequence.guarded.empty() || Optional(name))
                assignments += before(From(name, sequence.general_reads));
            assignments += sequence.general[c] + ";\n";
        }
        // From 9 cells, one loop where each cell's line is the first's at its
        // own place, without the marks Numbered shares by; written out where a
        // hold or a term read back is marked, or a function is called, which a
        // loop would call at each cell (DESIGN.md, loops in generated code).
        const std::size_t n     = sequence.general.size();
        const std::string first = Subscript(sequence.size, 0), places = Places(sequence.size);
        const std::vector<std::string> nest  = Loops(sequence.size);
        const auto                     loops = [&](const std::string& in) {
            std::string out;
            for (const std::string& line : nest) out += (out.empty() ? "" : "\n") + in + line;
            return out;
        };
        if (n >= 9) {
            std::vector<std::size_t> starts;
            for (std::size_t k = 0; k < assignments.size(); k = assignments.find('\n', k) + 1)
                starts.push_back(k);
            const auto line = [&](std::size_t l) {
                std::string text =
                    assignments.substr(starts[l], assignments.find('\n', starts[l]) - starts[l]);
                std::erase_if(
                    text, [](char ch) { return ch == '\x14' || ch == '\x15' || ch == rounded; });
                return text;
            };
            const auto at = [&](std::string text, const std::string& place) {
                for (std::size_t k = 0; (k = text.find(first, k)) != std::string::npos; ++k)
                    if (text[k + first.size()] != '[') text.replace(k, first.size(), place);
                return text;
            };
            const std::size_t head  = starts.size() - n;
            const std::string one   = line(head);
            const auto        calls = [&] {
                for (std::size_t k = 0; (k = one.find('(', k + 1)) != std::string::npos;)
                    if (k > 5 &&
                        (std::isalnum(static_cast<unsigned char>(one[k - 1])) || one[k - 1] == '_'))
                        if (one.compare(k - 5, 5, "isnan")) return true;
                return false;
            };
            bool same = one.find_first_of("\x04\x10") == std::string::npos && !calls();
            for (std::size_t c = 1; same && c < n; ++c)
                same = at(one, Subscript(sequence.size, c)) == line(head + c);
            if (same)
                assignments.replace(
                    starts[head], std::string::npos,
                    loops(indent) + " " + at(one, places).substr(indent.size()) + "\n");
        }
        // Ahead of the cells too, a cell's clause where no base term stands.
        std::string picks;
        for (std::size_t c = 0; !only && c < sequence.taken.size(); ++c) {
            picks += indent + "m_->" + name + "_clause_[" + std::to_string(c) + "] = ";
            for (const auto& [index, base] : sequence.bases)
                picks += "m_->index_ == " + std::to_string(index) + " ? 0 : ";
            picks += sequence.taken[c] + ";\n";
        }
        assignments.insert(0, picks);
        // The interpreter refuses a matrix term whole where it refuses a cell;
        // from 9 cells, a NaN is passed to the first, which decides.
        if (aware_ && !scalar) {
            const std::string cell = "m_->" + name + "[0]";
            std::string       any  = "isnan(" + cell + first + ")";
            if (n >= 9)
                assignments += loops(indent) + "\n" + indent + std::string(4 * nest.size(), ' ') +
                               "if (isnan(" + cell + places + ")) " + cell + first + " = NAN;\n";
            else
                for (std::size_t c = 1; c < n; ++c)
                    any += " || isnan(" + cell + Subscript(sequence.size, c) + ")";
            assignments += indent + "if (" + any + ")\n" + loops(indent + "    ") + " " + cell +
                           places + " = NAN;\n";
        }
        return assignments;
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
        Ahead                    ahead;
        std::vector<std::string> order;
        try {
            order = Order();
        } catch (const Refusal&) {
            order = Order(&ahead);
        }
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
        upper_ = guard;
        guard += "_H";

        std::string out;
        out += "/* Generated by 'inkamath --compile" + std::string(floats ? " --float" : "") +
               "' from " + source + ": edit that, not this.\n";
        out += " * The arithmetic is in the order the definitions give it, so do not build it\n";
        out += " * with -ffast-math, which reorders. A compiler may also fuse a multiply and an\n";
        out += " * add, rounding once where the definitions round twice (-ffp-contract): often\n";
        out += " * closer to exact, but the step then depends on how it is built, so build\n";
        out += " * what 'inkamath --check' writes the way the step itself is built. */\n";
        out += "#ifndef " + guard + "\n#define " + guard + "\n\n";
        const std::size_t includes = out.size();
        out += "#include <math.h>\n#include <string.h>\n\n";
        out += Interface(module, inputs, fields, earliest);
        const std::size_t declared = out.size();
        out += "/* The parameters, which the host may assign, then what derives from them,\n";
        out += " * then the index of the latest step and each sequence's terms from that\n";
        out += " * index back. */\n";
        Members members;
        for (const auto& [name, parameter] : parameters_)
            members.push_back({"double", name, Dimensions(parameter.size)});
        for (const Derived& derived : derived_)
            if (derived.read)
                members.push_back({"double", derived.name, Dimensions(derived.code.size)});
        members.push_back({"long long", "index_", ""});
        for (const std::string& name : fields) {
            const Sequence& sequence = sequences_.at(name);
            members.push_back(
                {"double", name,
                 "[" + std::to_string(sequence.depth) + "]" + Dimensions(sequence.size)});
            if (Optional(name))
                members.push_back(
                    {"int", name + "_present", "[" + std::to_string(sequence.depth) + "]"});
            if (chains_.count(name)) members.push_back({"int", name + "_given_", ""});
            if (clauses_ && !sequence.guarded.empty())
                members.push_back({"int", name + "_clause_", ""});
            if (!sequence.taken.empty())
                members.push_back(
                    {"int", name + "_clause_", "[" + std::to_string(sequence.taken.size()) + "]"});
        }
        out +=
            "typedef struct " + module + " {\n" + Nested(members, "    ") + "} " + module + ";\n\n";

        const std::size_t functions = out.size();

        out += "/* Computes what derives from the parameters: call it after assigning one. */\n";
        out += "static inline void " + module + "_update(" + module + "* m_) {\n";
        const auto read = [](const Derived& derived) { return derived.read; };
        if (std::none_of(derived_.begin(), derived_.end(), read)) out += "    (void)m_;\n";
        for (const Derived& derived : derived_) {
            if (!derived.read) continue;
            std::string assignments;
            for (std::size_t c = 0; c < derived.code.cells.size(); ++c)
                assignments += "    m_->" + derived.name + Subscript(derived.code.size, c) + " = " +
                               derived.code.cells[c].text + ";\n";
            const auto numbered = Numbered(derived.temporaries, assignments);
            out += Temporaries(numbered, assignments, "    ") + assignments;
        }
        out += "}\n\n";

        out += "static inline void " + module + "_init(" + module + "* m_) {\n";
        out += "    memset(m_, 0, sizeof *m_);\n";
        for (const auto& [name, parameter] : parameters_) {
            for (std::size_t c = 0; c < parameter.initial.size(); ++c)
                out += "    m_->" + name + Subscript(parameter.size, c) + " = " +
                       Double(parameter.initial[c]) + ";\n";
        }
        const std::size_t folds = out.size();
        out += "    m_->index_ = " + std::to_string(earliest - 1) + ";\n";
        out += "    " + module + "_update(m_);\n}\n\n";

        const std::size_t stepping = out.size();
        out += "/* Advances to the next index, the first at " + std::to_string(earliest) +
               ", and computes its terms. */\n";
        out += "static inline void " + module + "_step(" + module + "* m_";
        // An input x is 'x_in_', which neither 'm_', a temporary 't0_' nor
        // a function 'model_f' can be named (C342).
        for (const std::string& name : inputs)
            out += (Cells(name) == 1
                        ? ", double " + name + "_in_"
                        : ", const double " + name + "_in_[" + std::to_string(Cells(name)) + "]") +
                   (Optional(name) ? ", int " + name + "_present_in_" : "");
        out += ") {\n";
        out += "    ++m_->index_;\n";
        // A sequence's window moves when it computes a term: every step, or
        // on its ticks.
        const auto shifted = [&](const std::string& name, const std::string& indent) {
            const Sequence& sequence = sequences_.at(name);
            std::string     lines;
            for (int k = sequence.depth - 1; k > 0; --k) {
                const std::string to   = "m_->" + name + "[" + std::to_string(k) + "]";
                const std::string from = "m_->" + name + "[" + std::to_string(k - 1) + "]";
                lines += Dimensions(sequence.size).empty()
                             ? indent + to + " = " + from + ";\n"
                             : indent + "memcpy(" + to + ", " + from + ", sizeof " + to + ");\n";
                if (Optional(name))
                    lines += indent + "m_->" + name + "_present[" + std::to_string(k) + "] = m_->" +
                             name + "_present[" + std::to_string(k - 1) + "];\n";
            }
            return lines;
        };
        for (const std::string& name : fields)
            if (sequences_.at(name).period == 1) out += shifted(name, "    ");
        for (const std::string& name : inputs) {
            // An optional input absent is NaN, and read by no value (DESIGN.md).
            const std::string given =
                Optional(name) ? "    if (" + name + "_present_in_)\n    " : "";
            out += Cells(name) == 1
                       ? "    m_->" + name + "[0]" + Subscript(sequences_.at(name).size, 0) +
                             " = " + (given.empty() ? "" : name + "_present_in_ ? ") + name +
                             "_in_" + (given.empty() ? "" : " : NAN") + ";\n"
                       : given + "    memcpy(m_->" + name + "[0], " + name + "_in_, sizeof m_->" +
                             name + "[0]);\n" +
                             (given.empty() ? "" : "    else\n    " + Unset(name, "0"));
            if (!given.empty())
                out += "    m_->" + name + "_present[0] = " + name + "_present_in_ != 0;\n";
        }
        // Resolved before what is used is known: a term read back that is
        // not computed again leaves its temporaries unread.
        const auto written = [&](const std::string& name, const std::string& assignments,
                                 const std::string& indent) {
            const Sequence&        sequence    = sequences_.at(name);
            std::vector<Temporary> temporaries = sequence.temporaries;
            for (Temporary& temporary : temporaries)
                for (std::string& line : temporary.lines)
                    line = Checked(Rated(line, sequence), name, sequence);
            std::string checked  = Checked(Rated(assignments, sequence), name, sequence);
            const auto  numbered = Numbered(temporaries, checked);
            return Split(numbered, checked, indent, sequence.definition->home);
        };
        for (const std::string& name : order) {
            const Sequence&   sequence = sequences_.at(name);
            // A grad by several names' parts are written with the first.
            if (!sequence.parts.empty() && sequence.parts[0] != name) continue;
            for (auto [at, end] = ahead.equal_range(name); at != end; ++at) {
                const auto& [base, n] = at->second;
                out += "    if (m_->index_ == " + std::to_string(n) + ") {\n" +
                       written(base, Assigned(base, "        ", &sequences_.at(base).bases.at(n)),
                               "        ") +
                       "    }\n";
            }
            const bool        rated    = sequence.period > 1;
            const bool        late     = sequence.start > earliest || rated;
            const std::string indent   = late ? "        " : "    ";
            if (late)
                out += "    if (m_->index_ >= " + std::to_string(sequence.start) +
                       (rated ? " && (m_->index_" + Less(sequence.phase) + ") % " +
                                    std::to_string(sequence.period) + " == 0"
                              : std::string()) +
                       ") {\n";
            std::string assignments;
            for (const std::string& part :
                 sequence.parts.empty() ? std::vector{name} : sequence.parts) {
                if (rated) out += shifted(part, indent);
                assignments += Assigned(part, indent);
                const Sequence& given = sequences_.at(part);
                // Before its argument's reads begin, absent below the base
                // clauses of one it reads, and refused before the start of
                // one that has none (C391).
                if (Optional(part)) {
                    int below = std::numeric_limits<int>::min();
                    for (const auto& [term, lags] : given.general_reads)
                        if (!sequences_.at(term).bases.empty())
                            below = std::max(below, Exists(term) + *lags.rbegin());
                    const int begins = From(part, given.general_reads);
                    assignments +=
                        indent + "m_->" + part + "_present[0] = " +
                        (begins <= given.start ? ""
                                               : "m_->index_ < " + std::to_string(begins) +
                                                     (below < begins ? " ? 1 : " : " ? 0 : ")) +
                        (given.present.empty() ? "1" : given.present) + ";\n";
                }
                // A chain gave the term where a guard held or was NaN, a double (C331).
                std::string gave = indent + "m_->" + part + "_given_ = ";
                for (const auto& [index, base] : given.bases)
                    gave += "m_->index_ == " + std::to_string(index) + " ? 1 : ";
                for (const Guarded& guarded : given.guarded) {
                    // Before its guard's reads begin the term is refused, NaN, as Assigned writes.
                    const std::string from = std::to_string(guarded.guard_from);
                    for (int n = given.start; n < guarded.guard_from; ++n)
                        if (!given.bases.count(n)) {
                            gave += "m_->index_ < " + from + " ? 1 : ";
                            break;
                        }
                    gave += "(" + guarded.condition + ") != 0.0 ? 1 : ";
                }
                if (chains_.count(part))
                    assignments += gave + (given.general_clause < 0 ? "0" : "1") + ";\n";
            }
            out += written(name, assignments, indent);
            if (late) out += "    }\n";
        }
        out += "}\n\n#endif\n";
        if (splits_) {
            const std::string define = "#define " + upper_ + "_NOINLINE_";
            out.insert(stepping,
                       "/* The step's longest lists, each written as functions, which compilers\n"
                       " * optimise faster than one function of them all. */\n" +
                           ("#if defined(__GNUC__)\n" + define) + " __attribute__((noinline))\n" +
                           ("#elif defined(_MSC_VER)\n" + define) + " __declspec(noinline)\n" +
                           ("#else\n" + define) + "\n#endif\n\n" + pieces_);
        }
        out.insert(folds, Folds(earliest));
        // A function of the header where something calls it, which a fold, a
        // floor or a comparison may not (C355); each calls only those before.
        std::vector<std::pair<std::string, std::string>> helpers;
        for (const std::size_t n : inverses_)
            helpers.emplace_back(module_ + "_inverse" + std::to_string(n) + "_", InverseHelper(n));
        for (const std::string& function : prelude_) {
            const std::size_t open = function.find('('), name = function.rfind(' ', open) + 1;
            helpers.emplace_back(function.substr(name, open - name), function);
        }
        for (std::size_t i = 0; i < limits_.size(); ++i)
            helpers.emplace_back(module_ + "_lim" + std::to_string(i), limits_[i]);
        std::string kept, calls = out.substr(functions);
        for (auto helper = helpers.rbegin(); helper != helpers.rend(); ++helper) {
            if (!Writes(calls, helper->first)) continue;
            calls += helper->second;
            kept = helper->second + kept;
        }
        out.insert(functions, kept);
        if (!scratch_.empty()) {
            out.insert(out.find("\n} " + module + ";\n", declared) + 1,
                       "    " + Typed("v_") + " v_;\n");
            out.insert(
                declared,
                "/* What a part of the step computes and a later part reads, held by the\n"
                " * model rather than the stack; instances stepped in turn share a place. */\n" +
                    Declared("v_"));
        }
        if (Writes(kept, "DBL_EPSILON") || Writes(kept, "FLT_EPSILON"))
            out.insert(includes, "#include <float.h>\n");
        std::erase_if(out, [](char c) { return c == '\x14' || c == '\x15' || c == rounded; });
        // Past the line naming the source, the words of the compiler's own C
        // for a double as a float's: every constant is written as one already.
        const auto inside = [&](std::size_t at) {
            return std::isalnum(static_cast<unsigned char>(out[at])) || out[at] == '_' ||
                   out[at] == '.';
        };
        for (std::size_t at = out.find('\n'); floats && at < out.size(); ++at)
            for (const std::string from :
                 {"double", "fabs(", "fmax(", "floor(", "pow(", "sqrt(", "0.0", "1.0"})
                if (!inside(at - 1) && out.compare(at, from.size(), from) == 0 &&
                    (from.back() == '(' || !inside(at + from.size()))) {
                    if (from == "double")
                        out.replace(at, from.size(), "float");
                    else
                        out.insert(at + from.size() - (from.back() == '(' ? 1 : 0), "f");
                }
        Compiled compiled{out, earliest, inputs, {}, {}, {}, unnamed_, fallen_};
        for (const std::string& name : inputs) compiled.cells.push_back(Cells(name));
        for (const std::string& name : order) {
            if (totals_.count(name)) continue;  // which the interpreter does not name
            const Sequence& sequence = sequences_.at(name);
            const bool      unnamed  = std::any_of(
                labels_.begin(), labels_.end(),
                [&](const std::string& label) { return name.rfind(label + ".", 0) == 0; });
            compiled.sequences.push_back({name, sequence.size, sequence.period, sequence.phase,
                                          sequence.period * sequence.first + sequence.phase,
                                          unnamed, sequence.session});
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
    Frame                            frame_;  // the locals the code being compiled sees
    std::vector<std::string> graded_;         // the names of the grads it is under, outermost first
    int                              expanded_  = 0;        // how deep calls are
    int                              calls_     = 0;        // how many were compiled
    std::vector<std::shared_ptr<const Scope<Value>>> held_;    // what expansions name
    std::string                                      within_;  // the sequence being compiled
    // Unnamed instances, by the scope that writes them, where it does and
    // what they read there.
    std::map<std::tuple<const Scope<Value>*, const void*, std::string>, const Scope<Value>*> kept_;
    std::set<std::string>                                              labels_;
    std::map<std::string, Compiled::Unnamed>                           unnamed_;
    std::map<std::string, Sequence>  sequences_;
    std::map<std::string, Parameter> parameters_;
    std::vector<Derived>             derived_;  // each after those it reads
    std::set<std::string>            fixed_;    // parameters compiled as constants; see Fix
    std::set<std::string>            first_;    // see First
    std::set<std::string>            approximated_in_;  // what reads one approximated_ says
    std::set<std::string>            rounded_in_;       // what writes a constant marked rounded
    std::set<std::string>            aside_;    // definitions refused; see Refusals
    bool                             clauses_ = false;  // whether the step keeps them; see Build
    bool                             aware_ = false;  // whether it writes NaN, and so tests for it
    std::set<std::string>            nans_;             // arguments' cells that may be NaN
    std::set<std::string>            nan_functions_;    // the header's that may answer NaN
    std::set<std::string>            nan_temporaries_;  // and the step's or limit's, by name
    std::map<Where, std::size_t>     ids_;              // see Id
    std::map<std::pair<std::string, std::vector<std::size_t>>, std::size_t> made_;
    bool                             guarding_ = false;  // in a guard, which asks only for values
    // Where a guard's truth is NaN, what asks whether a limit it reads ran out.
    std::set<std::string>*           statuses_ = nullptr;
    std::set<std::string>*             given_    = nullptr;  // an inner optional input's argument's
    std::set<std::string>              chains_;              // whose status such an argument asks
    std::map<std::string, std::string> back_;  // such an argument reading a chain back, refused
    std::map<std::string, std::set<Compiled::Named>>     what_;  // each status's limits, by name
    std::map<const Expression<Value>*, Compiled::Fallen> fallen_;
    std::string unconverged_;  // where the last guard compiled read a limit that ran out
    std::set<std::string> asked_;  // and the statuses it asked
    std::set<std::string>  sure_;   // of those, the ones read where it holds (C368)
    std::set<std::string>* unsure_             = nullptr;  // read on an 'or''s right alone
    std::vector<Compiled::Named>*    read_ = nullptr;  // the limits a guard reads, first read first
    bool                             roots_    = false;  // whether C may take a root of x < 0
    bool moved_  = false;  // whether a value with a part was compiled, as a guard asks
    bool chosen_ = false;  // whether a guard of the chain compiled read one
    std::map<std::string, Value>     known_;    // globals that read only those
    std::set<std::string>            read_parameters_;      // by the value being compiled
    bool                             read_global_ = false;  // by the value being compiled
    std::set<std::string>            reading_plain_;
    Sequence*   reading_ = nullptr;  // the sequence whose general clause this is
    std::string index_;              // and the name of its index
    std::string                      grad_;  // the name grad differentiates by, in its body
    // Each prelude part function, 'exp_dx': which values and parts it takes,
    // or none where there is no part.
    std::map<std::string, std::optional<std::vector<std::pair<bool, std::size_t>>>> parts_;
    std::vector<Temporary>*          temporaries_ = nullptr;  // where this sequence's are declared
    std::map<std::pair<const void*, std::string>, std::size_t> declared_;  // each value's place
    // Terms of sequences with parameters, by the list of temporaries they are
    // in, the definition, its arguments' code and the index, and why the fill
    // could not compute those it could not, and how deep where a depth passed;
    // and the index of the term of each being computed.
    std::map<std::tuple<const void*, const void*, std::string, int>, std::pair<Code, bool>> terms_;
    std::map<std::tuple<const void*, const void*, std::string, int>,
             std::pair<std::exception_ptr, int>>
                                                       failed_;
    std::map<std::pair<const void*, std::string>, int> computing_;
    std::vector<std::pair<const Sequence*, std::string>> pending_;  // reads by reader, compiling
    // The fill's last term tried, by residue, and the deepest it was tried at.
    std::map<std::tuple<const void*, const void*, std::string, int>, std::pair<long long, int>>
                                                       filled_;
    Memo                                               memo_;  // the loop's, if one binds places
    int                                                situated_ = 0;        // terms read, counted
    int                                                terming_ = 0;        // terms being computed
    Reads*      clause_reads_ = nullptr;               // what the clause being compiled reads
    bool                       deferring_    = false;  // compiling a guard, or as right_
    bool                       right_        = false;  // compiling the right of an 'and' or 'or'
    int                        shift_ = 0;  // how far back the term being computed again is; see At
    std::vector<Early>         earlies_;
    int                        early_ = -1;    // the one being computed again
    std::map<std::string, int> exists_;        // where one with no base first answers
    std::vector<Check>         checks_;        // what a deferred right side reads, by its mark
    std::map<std::string, const Reference<Value>*> histories_;  // by the input they give
    std::map<std::string, const Reference<Value>*> stated_;     // inputs given a term, sized
    std::map<std::string, int>                     reach_;      // the last index each gives
    std::map<std::pair<std::string, int>, std::optional<Value>> folded_;
    std::map<std::pair<std::string, int>, std::string> unfolded_;  // why a history gave none
    int                                                         earliest_ = 0;
    std::string index_text_   = "(double)m_->index_";  // the index, as the step has it
    std::string                      module_;
    std::set<std::size_t>            inverses_;  // the sizes a helper is needed for
    std::vector<std::string>         limits_;    // a function for each limit walked
    std::vector<std::string>           prelude_;   // and for each function of the prelude called
    std::map<std::string, std::string> functions_;  // their names and values
    std::vector<Hold>                  holds_;
    Sequence*                          basing_ = nullptr;  // whose base clause is compiled, if one
    int                                base_   = 0;        // and its index
    std::map<std::string, std::string> limit_names_;  // each one's name, by its text
    // The sequence a limit's function is walking, while its clauses compile.
    struct Walked {
        std::string              name;
        std::string              what;  // 'lim s', or 'sum_(q=0)', as a refusal names it
        std::vector<std::string> parameters;
        int                      depth = 0;           // how far back a term reads its own
        Extent                   size{0, 0};          // a term's, once a clause gives it
        std::vector<Temporary>*  based;               // its temporaries, the bases'
        std::vector<Temporary>*  iterated;            // and an iterate's
        int                      count = 0;
        int                      first = 0;  // the index its walk starts at
        std::string              windowing = {};  // the sequence whose window is being set
        struct Beside {
            const Reference<Value>* function;
            std::string             args;
            Expansion               expansion;
            std::optional<Code>     term   = {};  // at k_, once compiled
            std::vector<Code>       window = {};  // its terms before the walk, by lag
        };
        std::deque<Beside> beside = {};  // sequences with parameters, powers, factorials beside it
    };
    std::deque<std::vector<Temporary>> lists_;  // those, kept as a step's are
    static constexpr std::size_t       split_  = 16384;  // a part's bytes; see Split
    int                                splits_ = 0;      // the parts written
    std::string                        pieces_, upper_;  // their functions, the guard's
    std::map<std::string, std::string> scratch_;         // members, by path; see Place
    std::map<const Scope<Value>*, const Scope<Value>*> outer_;  // where each instance is written
    Walked*                          limit_           = nullptr;
    std::map<const Expression<Value>*, Code> lifted_;  // an unbounded sum's arguments, by part
    int                              temporary_count_ = 0;
    std::map<std::string, Value>     places_;             // a cell's row and column, by their names
    std::map<std::string, Accumulator>       totals_;             // by key
    Code        code_;
    Tape* tape_  = nullptr;  // the grad's being compiled in reverse mode
    std::size_t                        depth_ = 0;        // the grads around, a nest's 2
    std::map<std::string, std::string> nested_;  // a nest's prelude functions, by what they compute
    int   tapes_ = 0;        // how many were, so that a memo's key tells their nodes apart
    // The grad by several names a step computes the parts of, and where they go.
    std::pair<const GradExpression<Value>*, std::vector<Code>*> grouped_{};
};

#endif  // INKAMATH_COMPILE_HPP
