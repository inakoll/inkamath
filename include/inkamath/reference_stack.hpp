#ifndef EXPRESSION_STACK_HPP
#define EXPRESSION_STACK_HPP

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include "inkamath/pexpression.hpp"
#include "inkamath/reference.hpp"

// Two kinds of scope: the definitions of the session, a file or an instance,
// each over the built-ins, and the parameters of the call being evaluated. A
// call never sees its caller's parameters, so 'q = y+1' means the global y
// whichever call is on the stack (DESIGN.md, phase 4 item 5), and a
// definition reads the names of the scope it was written in (phase 15).
template <typename T>
class ReferenceStack {
public:

    // Definitions are shared and never mutated in place: a name evaluated
    // here may be redefined while its own body is running, and the evaluation
    // must go on seeing what it started with.
    typedef std::shared_ptr<const Reference<T>>                definition_type;
    typedef std::unordered_map<std::string, definition_type>   scope_type;

    // What a call's frame holds. A parameter, a default and a sequence index
    // are values; only a definition written inside an expression needs to be
    // one. One slot per name either way, because a local shadows a parameter
    // of the same name: 'h(x) = (x = 10) + x' answers 20.
    struct Binding {
        std::string     name;
        T               value;
        definition_type definition;   // null when the binding is a value
    };
    typedef std::vector<Binding> frame_type;

    // Measured against every golden transcript: the deepest legitimate
    // evaluation nests 33 references and the longest takes 6444 steps.
    static constexpr size_t max_depth = 256;
    static constexpr size_t max_steps = 1000000;

    // Memoised results, one per distinct call context. Sized so that a
    // session sweeping a parameter cannot grow the process without bound; the
    // older half goes when the newer fills, because a fill needs the latest
    // terms of every sequence it passes through (DESIGN.md, C69).
    static constexpr size_t max_memoised = 100000;

    // How far a fill goes from its base: each term has the budget of a line,
    // so this is what bounds its time, at a few seconds.
    static constexpr int max_filled = 10000000;

    // Call once per top-level evaluation; the stack outlives them all.
    void BeginEvaluation() {
        depth_       = 0;
        steps_       = 0;
        fill_failed_ = false;
    }

    ReferenceStack() {
        session_.parent = &builtins_;
        const Into builtins(*this, builtins_);
        // Every digit a double holds: the 2014 literals stopped at fourteen,
        // which is a 7e-15 error in pi, and that is what 'e^(i*pi)' reported
        // as the imaginary part of -1.
        this->Set("pi", ParametersDefinition<T>(), PExpression<T>( new ValExpression<T>(T(3.14159265358979323846))));
        this->Set("e",  ParametersDefinition<T>(), PExpression<T>( new ValExpression<T>(T(2.71828182845904523536))));
        // A name, so that a bound one can shadow it: a paper's sums run over i.
        typename T::value_type unit;
        char*                  end = nullptr;
        (void)numeric_interface<typename T::value_type>::parse(unit, "i", end);
        this->Set("i", ParametersDefinition<T>(), std::make_shared<ValExpression<T>>(T(unit)));
        // A definition like these, so a session may replace it; native,
        // because the language can compute it only by a search.
        EvaluationVisitor<T> evaluator(*this);
        const auto           x = std::make_shared<RefExpression<T>>("x");
        this->Set("floor", ParametersDefinition<T>(x, PExpression<T>(), evaluator),
                  std::make_shared<FloorExpression<T>>(x));
    }

    // Scopes point at one another.
    ReferenceStack(const ReferenceStack&)            = delete;
    ReferenceStack& operator=(const ReferenceStack&) = delete;

    // A memoised result may have read a global, so redefining one drops the
    // cache. A definition made while a frame is on the stack is a parameter
    // or an index, which is part of the key and cannot invalidate anything.
    void Set(const std::string& ai_reference_name, const ParametersDefinition<T>& ai_parameters, PExpression<T>  ai_expression, const std::string& written = std::string()) {
        if (open_ != 0) {
            definition_type& slot = FrameSlot(ai_reference_name).definition;
            slot =
                Extended(slot, nullptr, ai_reference_name, ai_parameters, ai_expression, written);
            return;
        }
        // An answer prints the unit as 'i', so no definition may make it
        // mean anything else; a bound name only shadows it.
        if (ai_reference_name == "i" && target_ != &builtins_)
            throw std::runtime_error("i is the imaginary unit, so it cannot be defined");
        Changed();
        const definition_type previous = Defined(ai_reference_name);
        definition_type       extended = previous;
        // A clause added to a built-in extends it, as it did when the
        // built-ins were the session's own.
        if (!extended && target_ == &session_) {
            const auto builtin = builtins_.names.find(ai_reference_name);
            if (builtin != builtins_.names.end()) extended = builtin->second;
        }
        // Made before it is stored, so that a clause refused leaves no name.
        extended =
            Extended(extended, target_, ai_reference_name, ai_parameters, ai_expression, written);
        target_->names[ai_reference_name] = extended;
        Causal(ai_reference_name, previous);
    }

    // A model, a file used, or a name brought in from one.
    void Put(const std::string& name, definition_type definition) {
        Changed();
        const definition_type previous = Defined(name);
        target_->names[name]           = std::move(definition);
        Causal(name, previous);
    }

    // Told of each guard of a general clause a term's selection asks, and
    // whether it held, while its index is bound: what --check listens with.
    std::function<void(const Reference<T>&, const Clause<T>&, int, bool, EvaluationVisitor<T>&)>
        guards;

    [[nodiscard]] Scope<T>&       Target() const { return *target_; }
    [[nodiscard]] Scope<T>&       Builtins() { return builtins_; }
    [[nodiscard]] const Scope<T>& Builtins() const { return builtins_; }
    [[nodiscard]] const Scope<T>& Session() const { return session_; }

    // A model's instance with every default and no input given, which is what
    // compiling it by name compiles.
    // Unlabelled, so that its names are the compiled header's own.
    std::shared_ptr<const Scope<T>> Defaults(const Reference<T>& model) {
        return Instantiate(model, Bound(model.model->parameters.size()), &session_, std::string(),
                           std::string(), {});
    }

    // A named instance with its parameters as given and no input, which is
    // what checking it compiles; null for any other definition.
    std::shared_ptr<const Scope<T>> Unfed(const definition_type& definition) {
        const auto instance = InstanceOf(*definition);
        if (!instance) return nullptr;
        const Reference<T>& model = *instance->model;
        Bound               bound = model.model->Bind(model.Name(), *instance->call);
        for (size_t i = 0; i < bound.size(); ++i)
            if (!model.model->parameters[i].index.empty()) bound[i].reset();
        return Instantiate(model, bound, definition->home, std::string(), std::string(), {});
    }

    // An instance the compiler names, of one written where it is read, with
    // the values it reads there: kept by the compiler, not here.
    std::shared_ptr<const Scope<T>> Detached(
        const Reference<T>& model, const ParametersCall<T>& call, const Scope<T>& written,
        std::string label, const std::vector<std::pair<std::string, T>>& captured) {
        return Instantiate(model, model.model->Bind(model.Name(), call), &written, std::move(label),
                           std::string(), captured);
    }

    // The scope of a named instance, made if it has not been; null for any
    // other definition.
    const Scope<T>* InstanceScope(const definition_type& definition) {
        if (!InstanceOf(*definition)) return nullptr;
        return &Holder(definition, *definition->Clauses().front().expression, *definition->home);
    }

    // The scope an instance or a file names, read from a scope.
    const Scope<T>& Resolve(Expression<T>& object, const Scope<T>& from) {
        const Within within(*this, &from);
        const Frame  frame(*this);
        return Object(object);
    }

    // Names are sought from a scope while one of these lives.
    struct Within {
        Within(ReferenceStack& stack, const Scope<T>* scope)
            : stack_(stack), previous_(stack.scope_) {
            if (scope) stack_.scope_ = scope;
        }
        ~Within() { stack_.scope_ = previous_; }
        Within(const Within&)            = delete;
        Within& operator=(const Within&) = delete;

    private:
        ReferenceStack& stack_;
        const Scope<T>* previous_;
    };

    // Definitions go into a scope, and names are sought there, while one of
    // these lives: a file's as it is read, the built-ins' as they are made.
    struct Into {
        Into(ReferenceStack& stack, Scope<T>& scope)
            : stack_(stack), target_(stack.target_), scope_(stack.scope_) {
            stack_.target_ = &scope;
            stack_.scope_  = &scope;
        }
        ~Into() {
            stack_.target_ = target_;
            stack_.scope_  = scope_;
        }
        Into(const Into&)            = delete;
        Into& operator=(const Into&) = delete;

    private:
        ReferenceStack& stack_;
        Scope<T>*       target_;
        const Scope<T>* scope_;
    };

    // A parameter, a default or a sequence index, bound in the frame the call
    // has already opened. It was a whole Reference wrapping a heap
    // ValExpression: a make_shared, a Clause and a ParametersDefinition per
    // binding, per call, and again per term of a sequence for the index.
    void BindValue(const std::string& name, const T& value) {
        Binding& slot = FrameSlot(name);
        slot.value = value;
        slot.definition.reset();
    }

    std::string Describe(const std::string& ai_reference_name, const ParametersCall<T>& ai_parameters) {
        if(const Binding* binding = FindBinding(ai_reference_name)) {
            return Definition(*binding)->Describe(ai_parameters, *this);
        }
        definition_type definition = FindGlobal(ai_reference_name);
        if(!definition) {
            throw std::runtime_error(scope_->Qualified(ai_reference_name) + " is not defined");
        }
        if (definition->model) return definition->model->Describe();
        if (definition->file) return "use " + definition->Name();
        return definition->Describe(ai_parameters, *this);
    }

    T Eval(const std::string& ai_reference_name, const ParametersCall<T>& ai_parameters)  {
        Budget budget(*this);
        // The frame is searched first and answers even when it holds a value,
        // or a call's parameter would stop shadowing a global of its name.
        // Nothing it answers may be memoised: a local shares its name with the
        // global it shadows, and the cache is keyed on the name (C49).
        if(const Binding* binding = FindBinding(ai_reference_name)) {
            if(!binding->definition && Plain(ai_parameters)) return binding->value;
            return Definition(*binding)->Eval(ai_parameters, *this, false);
        }
        definition_type definition = FindGlobal(ai_reference_name);
        if(!definition) {
            throw std::runtime_error(scope_->Qualified(ai_reference_name) + " is not defined");
        }
        return Evaluate(*definition, ai_parameters);
    }

    // 'g.y_3': the index and the arguments are the caller's, and the name is
    // sought in the object's scope alone.
    T Member(const MemberExpression<T>& member) {
        Budget          budget(*this);
        const Scope<T>& scope = Object(*member.Object());
        return Evaluate(*Own(scope, member.Member()->Name()), Call(*member.Member()));
    }

    // 'g.k = 3', which the session may not do: an instance is changed where
    // it is defined, and a file where it is written.
    [[noreturn]] void Outside(const MemberExpression<T>& member) {
        const Scope<T>&   scope = Object(*member.Object());
        const std::string name  = scope.Qualified(member.Member()->Name());
        if (!scope.file.empty())
            throw std::runtime_error(name + " is defined in " + scope.file + ", and only there");
        if (!scope.defined.empty()) {
            throw std::runtime_error(scope.label + " is defined by " + scope.defined + "; define " +
                                     scope.label + " again to change it");
        }
        throw std::runtime_error(name + " is defined by its model");
    }

    // Filling a recurrence needs room to nest a few references per term, and
    // a fill never starts another: it would redo the same terms. One that
    // failed would fail again at every level the error passes on its way out.
    [[nodiscard]] bool CanFill() const {
        return !filling_ && !fill_failed_ && depth_ <= max_depth / 2;
    }
    void FillFailed() { fill_failed_ = true; }

    struct Filling {
        explicit Filling(ReferenceStack<T>& stack) : stack_(stack), steps_(stack.steps_) {
            stack_.filling_ = true;
        }
        ~Filling() { stack_.filling_ = false; }
        Filling(const Filling&)            = delete;
        Filling& operator=(const Filling&) = delete;

        // A fill stands for asking each term on a line of its own, so each
        // has the budget that line would have had.
        void Next() { stack_.steps_ = steps_; }

    private:
        ReferenceStack<T>& stack_;
        size_t             steps_;
    };

    [[nodiscard]] const scope_type& Globals() const { return session_.names; }

    // A name as the session reads it, its own or a built-in.
    [[nodiscard]] definition_type Find(const std::string& name) const {
        for (const Scope<T>* scope = &session_; scope; scope = scope->parent) {
            const auto found = scope->names.find(name);
            if (found != scope->names.end()) return found->second;
        }
        return definition_type();
    }

    // Whether a name is still the one the interpreter starts with.
    [[nodiscard]] bool Builtin(const std::string& name) const {
        return session_.names.count(name) == 0 && builtins_.names.count(name) != 0;
    }

    // A name as it is read where evaluation stands, past the frame; and
    // whether the frame binds it.
    [[nodiscard]] definition_type Global(const std::string& name) const { return FindGlobal(name); }
    [[nodiscard]] bool            Binds(const std::string& name) const { return FindBinding(name); }

    // Whether a call's frame is open to bind in.
    [[nodiscard]] bool Framed() const { return open_ != 0; }

    // One step of an evaluation that reads no name, and so never passes
    // through Eval: a sum of a constant still has to end.
    void Step() { Budget step(*this); }

    // DESIGN.md, phase 9. A call's answer depends on the definition,
    // the index, the argument values and the globals; the first three are the
    // key and the fourth is handled by clearing. What it is worth: the
    // arithmetic-geometric mean is 2^(n+1)-1 calls for 2n+1 answers.
    const T* Memoised(const MemoKey& key) const {
        for (const auto* generation : {&memoised_, &older_}) {
            const auto found = generation->find(key);
            if (found != generation->end()) return &found->second;
        }
        return nullptr;
    }

    void Memoise(const MemoKey& key, const T& evaluation) {
        if (memoised_.size() >= max_memoised / 2) {
            older_ = std::move(memoised_);
            memoised_.clear();
        }
        memoised_.emplace(key, evaluation);
    }

    // A binding made to try something out. A guard needs its index bound to be
    // asked at all, and a guard that does not hold must leave nothing behind
    // (DESIGN.md, C53).
    struct Trial {
        Trial(ReferenceStack<T>& stack, const std::string& name)
            : stack_(stack), name_(name)
        {
            if(const Binding* existing = stack_.FindBinding(name_)) {
                previous_ = *existing;
                had_ = true;
            }
        }
        ~Trial() {
            if(kept_) return;
            if(had_) stack_.FrameSlot(name_) = previous_;
            else     stack_.DropBinding(name_);
        }
        void keep() {kept_ = true;}
        Trial(const Trial&) = delete;
        Trial& operator=(const Trial&) = delete;
    private:
        ReferenceStack<T>& stack_;
        std::string     name_;
        Binding         previous_;
        bool            had_ = false;
        bool            kept_ = false;
    };

    // The scope of one call's parameters.
    struct Frame {
    public:
        // A closed frame keeps its storage for the next call: allocating it
        // per call was most of what a call allocated.
        explicit Frame(ReferenceStack<T>& stack) : stack_(stack) {
            if (stack_.open_ == stack_.frames_.size()) stack_.frames_.emplace_back();
            ++stack_.open_;
        }
        ~Frame() { stack_.frames_[--stack_.open_].clear(); }
        Frame(const Frame&) = delete;
        Frame& operator=(const Frame&) = delete;
    private:
        ReferenceStack<T>& stack_;
    };

private:
    friend struct Trial;

    definition_type Defined(const std::string& name) const {
        const auto found = target_->names.find(name);
        return found == target_->names.end() ? definition_type() : found->second;
    }

    // A loop without a delay has no term to start from: a definition that
    // closes one is taken back, and says which (DESIGN.md, phase 15).
    void Causal(const std::string& name, const definition_type& previous) {
        try {
            Loops{*this, {}, {}}.Check();
        } catch (const std::runtime_error&) {
            if (previous)
                target_->names[name] = previous;
            else
                target_->names.erase(name);
            Changed();
            throw;
        }
    }

    // The terms each general clause reads at its own index, followed from
    // every definition of the scope being defined in, the session's and those
    // of the instances they name. A guarded clause may break the loop, so
    // only one that always applies is followed: nothing that answers is
    // refused, and a loop through a guard is still found where it is read.
    struct Loops {
        ReferenceStack& stack;

        struct Visit {
            const Reference<T>* definition;
            std::string         label;
        };
        std::map<const Reference<T>*, bool> done;  // false while on the path
        std::vector<Visit>                  path;

        void Check() {
            Roots(*stack.target_);
            if (stack.target_ != &stack.session_) Roots(stack.session_);
        }

        void Roots(const Scope<T>& scope) {
            for (const auto& [name, definition] :
                 std::map(scope.names.begin(), scope.names.end())) {
                Follow(*definition, scope.Qualified(name));
                const Scope<T>* instance = nullptr;
                try {
                    instance = stack.InstanceScope(definition);
                } catch (const std::runtime_error&) {
                    continue;  // said where it is read
                }
                if (instance) Roots(*instance);
            }
        }

        void Follow(const Reference<T>& definition, const std::string& label) {
            if (const auto seen = done.find(&definition); seen != done.end()) {
                if (seen->second) return;
                const auto from = std::find_if(path.begin(), path.end(), [&](const Visit& v) {
                    return v.definition == &definition;
                });
                const std::string index = "_" + Index(*from->definition);
                std::string       loop  = from->label + index;
                if (from + 1 == path.end()) loop += " reads itself";
                for (auto at = from + 1; at != path.end(); ++at)
                    loop += (at == from + 1 ? " reads " : ", which reads ") + at->label + index;
                if (from + 1 != path.end()) loop += ", which reads " + from->label + index;
                throw std::runtime_error("a loop without a delay: " + loop);
            }
            done[&definition] = false;
            path.push_back({&definition, label});
            if (definition.Value())
                for (const Clause<T>& clause : definition.Clauses()) Reads(definition, clause);
            path.pop_back();
            done[&definition] = true;
        }

        static std::string Index(const Reference<T>& definition) {
            for (const Clause<T>& clause : definition.Clauses())
                if (clause.parameters.general()) return clause.parameters.index_name();
            return "n";
        }

        void Reads(const Reference<T>& definition, const Clause<T>& clause) {
            const ParametersDefinition<T>& p = clause.parameters;
            if (!p.general() || p.guarded() || !p.parameters_names().empty() || !definition.home)
                return;
            Walk(definition, clause.expression, p.index_name());
        }

        void Walk(const Reference<T>& definition, const PExpression<T>& expression,
                  const std::string& index) {
            if (!expression) return;
            if (const auto* series = dynamic_cast<const SeriesExpression<T>*>(expression.get());
                series && series->Index() == index)
                return;  // its own index hides the clause's
            const Scope<T>*      where = definition.home;
            const Expression<T>* read  = expression.get();
            if (const auto* member = dynamic_cast<const MemberExpression<T>*>(read)) {
                try {
                    where = &stack.Resolve(*member->Object(), *definition.home);
                } catch (const std::runtime_error&) {
                    return;  // said where it is read
                }
                read = member->Member().get();
            }
            const auto* term = dynamic_cast<const FuncExpression<T>*>(read);
            const auto* at =
                term ? dynamic_cast<const RefExpression<T>*>(term->m_e2().get()) : nullptr;
            if (at && at->Name() == index && !term->m_e1() && !term->limit()) {
                const bool own = where != definition.home;
                for (const Scope<T>* scope = where; scope; scope = own ? nullptr : scope->parent) {
                    const auto found = scope->names.find(term->Name());
                    if (found == scope->names.end()) continue;
                    Follow(*found->second, scope->Qualified(term->Name()));
                    break;
                }
            }
            if (read != expression.get()) return;
            for (const PExpression<T>& child : expression->Children())
                Walk(definition, child, index);
        }
    };

    // A top-level definition may change what any memoised answer or instance
    // read, so both go. Nothing is evaluating when one is made.
    void Changed() {
        memoised_.clear();
        older_.clear();
        named_.clear();
        unnamed_.clear();
    }

    // Updating and initialising are the same operation.
    static definition_type Extended(const definition_type& existing, const Scope<T>* home,
                                    const std::string&             name,
                                    const ParametersDefinition<T>& parameters,
                                    PExpression<T> expression, const std::string& written) {
        std::shared_ptr<Reference<T>> updated = existing && existing->Value()
                                                    ? std::make_shared<Reference<T>>(*existing)
                                                    : std::make_shared<Reference<T>>();
        updated->add_expression(name, parameters, std::move(expression), written);
        updated->home = home;
        return updated;
    }

    static std::string Label(const Reference<T>& definition) {
        return definition.home ? definition.home->Qualified(definition.Name()) : definition.Name();
    }

    // What an instance was defined as, 'gain(x_n = n)', from how it was written.
    static std::string Defined(const Reference<T>& definition) {
        const std::string& written = definition.Clauses().front().written;
        const size_t       equal   = written.find('=');
        const size_t       start =
            equal == std::string::npos ? equal : written.find_first_not_of(' ', equal + 1);
        return start == std::string::npos ? std::string() : written.substr(start);
    }

    static const ParametersCall<T>& Call(const Expression<T>& expression) {
        static const ParametersCall<T> plain;
        const auto*                    call = dynamic_cast<const FuncExpression<T>*>(&expression);
        return call ? call->Call() : plain;
    }

    const definition_type& Own(const Scope<T>& scope, const std::string& name) const {
        const auto found = scope.names.find(name);
        if (found == scope.names.end())
            throw std::runtime_error(scope.Qualified(name) + " is not defined");
        return found->second;
    }

    // A name's value, where it has one; most are plain values.
    T Evaluate(const Reference<T>& definition, const ParametersCall<T>& call) {
        if (definition.Value() && !definition.Applied()) return definition.Eval(call, *this, true);
        return Applied(definition, call);
    }

    // A value written as a name applied to arguments, which has none if it
    // is an instance; or a model, a file or an input, none of which has one.
    T Applied(const Reference<T>& definition, const ParametersCall<T>& call) {
        if (definition.Value()) {
            const auto instance = InstanceOf(definition);
            if (!instance) return definition.Eval(call, *this, true);
            const std::string label = Label(definition);
            throw std::runtime_error(label + " is an instance of " + instance->model->Name() +
                                     "; read one of its names (" + label + "." +
                                     instance->model->model->Example() + ")");
        }
        const std::string label = Label(definition);
        if (definition.model) {
            (void)definition.model->Bind(label, call);
            if (call.arguments()) {
                throw std::runtime_error("an instance of " + label +
                                         " has no value of its own; read one of its names, as " +
                                         label + "(...)." + definition.model->Example());
            }
            throw std::runtime_error(label + " is a model; define an instance of it (" +
                                     definition.Name().substr(0, 1) + " = " + label + "(...))");
        }
        if (definition.file)
            throw std::runtime_error(label + " is a file, and has no value of its own");
        int        index   = 0;
        const bool indexed = call.TryEvalIndex(*this, index);
        throw std::runtime_error(label + (indexed ? "_" + std::to_string(index) : "") +
                                 " is an input, and nothing defines it");
    }

    struct Instance {
        definition_type          model;
        const ParametersCall<T>* call;
    };

    // Whether a definition is an instance: a model applied to arguments, and
    // nothing else, 'g = gain(x_n = n)'. Asked where it was written, and in a
    // frame of its own as its evaluation would be, so that the model is the
    // one it names there.
    std::optional<Instance> InstanceOf(const Reference<T>& definition) {
        if (!definition.home || !definition.Value() || !definition.Applied()) return std::nullopt;
        Expression<T>* callee = definition.Clauses().front().expression.get();
        Expression<T>* object = nullptr;
        if (const auto* member = dynamic_cast<const MemberExpression<T>*>(callee)) {
            object = member->Object().get();
            callee = member->Member().get();
        }
        const Within    within(*this, definition.home);
        const Frame     frame(*this);
        definition_type model;
        if (object) {
            const Scope<T>& where = Object(*object);
            const auto      found = where.names.find(callee->Name());
            if (found != where.names.end()) model = found->second;
        } else {
            model = FindGlobal(callee->Name());
        }
        if (!model || !model->model) return std::nullopt;
        return Instance{model, &Call(*callee)};
    }

    // The scope of an instance or a file, from what names it: 'g', 'filters',
    // 'gain(k = 3)' or 'filters.lowpass(a = 1/2)'.
    const Scope<T>& Object(Expression<T>& object) {
        if (auto* member = dynamic_cast<MemberExpression<T>*>(&object)) {
            const Scope<T>& outer = Object(*member->Object());
            return Holder(Own(outer, member->Member()->Name()), *member->Member(), outer);
        }
        const definition_type definition = FindGlobal(object.Name());
        if (!definition)
            throw std::runtime_error(scope_->Qualified(object.Name()) + " is not defined");
        return Holder(definition, object, *scope_);
    }

    const Scope<T>& Holder(const definition_type& definition, Expression<T>& node,
                           const Scope<T>& through) {
        if (definition->file) return *definition->file;
        if (definition->model) {
            // Unnamed, and so one per place it is written, per scope it is
            // evaluated in, and per value it reads of the frame.
            const std::string name  = through.Qualified(definition->Name());
            const auto        bound = definition->model->Bind(name, Call(node));
            const auto        read  = Captured(bound);
            std::string       key;
            for (const auto& [captured, value] : read) {
                key += '\0';
                key += captured;
                key += '=';
                numeric_interface<T>::key(value, key);
            }
            auto& slot = unnamed_[std::make_tuple(static_cast<const void*>(&node),
                                                  static_cast<const void*>(scope_), key)];
            if (!slot.second) {
                slot = {node.self(), Instantiate(*definition, bound, scope_, name + "(...)",
                                                 std::string(), read)};
            }
            return *slot.second;
        }
        if (const auto instance = InstanceOf(*definition)) {
            std::shared_ptr<Scope<T>>& slot = named_[definition.get()];
            if (!slot) {
                const Reference<T>& model = *instance->model;
                slot = Instantiate(model, model.model->Bind(model.Name(), *instance->call),
                                   definition->home, Label(*definition), Defined(*definition), {});
            }
            return *slot;
        }
        throw std::runtime_error(Label(*definition) + " is neither an instance nor a file");
    }

    typedef std::vector<std::optional<typename Model<T>::Argument>> Bound;
    typedef std::vector<std::pair<std::string, T>>                  Captures;

    // The values an unnamed instance's arguments read of the frame they were
    // written in, other than the index an input's own clause binds.
    Captures Captured(const Bound& bound) const {
        Captures captured;
        if (open_ == 0) return captured;
        std::set<std::string> read;
        for (const auto& argument : bound) {
            if (!argument) continue;
            std::set<std::string> names;
            Reads(argument->expression, names);
            names.erase(argument->index);
            read.insert(names.begin(), names.end());
        }
        for (const Binding& binding : frames_[open_ - 1]) {
            if (read.count(binding.name) == 0) continue;
            if (binding.definition) {
                throw std::runtime_error("an instance cannot read " + binding.name +
                                         ", which is defined only on its line");
            }
            captured.emplace_back(binding.name, binding.value);
        }
        return captured;
    }

    static void Reads(const PExpression<T>& expression, std::set<std::string>& names) {
        if (!expression) return;
        if (const auto* member = dynamic_cast<const MemberExpression<T>*>(expression.get())) {
            Reads(member->Object(), names);
            for (const PExpression<T>& child : member->Member()->Children()) Reads(child, names);
            return;
        }
        if (!expression->Name().empty()) names.insert(expression->Name());
        for (const PExpression<T>& child : expression->Children()) Reads(child, names);
    }

    // A model's parameters and body, installed in a scope of their own. An
    // argument reads where it was written; a default and the body read the
    // instance, then where the model was written: its file, or the session.
    std::shared_ptr<Scope<T>> Instantiate(const Reference<T>& model, const Bound& bound,
                                          const Scope<T>* written, std::string label,
                                          std::string defined, const Captures& captured) {
        const Model<T>& m     = *model.model;
        auto            scope = std::make_shared<Scope<T>>();
        scope->parent         = m.scope;
        scope->label          = std::move(label);
        scope->defined        = std::move(defined);
        scope->model          = &m;
        // A constant index in the body is evaluated as it is installed, and
        // reads nothing of the frame that happens to be open.
        const Into           into(*this, *scope);
        const Frame          frame(*this);
        EvaluationVisitor<T> evaluator(*this);
        for (size_t i = 0; i < m.parameters.size(); ++i) {
            const typename Model<T>::Parameter& parameter = m.parameters[i];
            const auto&                         given     = bound[i];
            const PExpression<T>& expression = given ? given->expression : parameter.fallback;
            auto                  definition = std::make_shared<Reference<T>>(parameter.name);
            if (!expression) {
                definition->input = true;
                definition->home  = scope.get();
            } else {
                const std::string& index = given ? given->index : parameter.index;
                definition->add_expression(
                    parameter.name,
                    index.empty() ? ParametersDefinition<T>()
                                  : ParametersDefinition<T>(
                                        PExpression<T>(), std::make_shared<RefExpression<T>>(index),
                                        evaluator),
                    expression);
                definition->home = given ? written : scope.get();
                if (given) definition->captured = captured;
            }
            scope->names[parameter.name] = std::move(definition);
        }
        for (const typename Model<T>::Statement& statement : m.body) {
            definition_type& slot = scope->names[statement.name];
            if (statement.model) {
                auto nested       = std::make_shared<Model<T>>(*statement.model);
                nested->scope     = scope.get();
                auto definition   = std::make_shared<Reference<T>>(statement.name);
                definition->model = std::move(nested);
                definition->home  = scope.get();
                slot              = std::move(definition);
                continue;
            }
            auto* equal = static_cast<EqualExpression<T>*>(statement.definition.get());
            slot        = Extended(slot, scope.get(), statement.name, evaluator.Parameters(equal),
                                   equal->m_e2(), statement.written);
        }
        return scope;
    }

    static bool Plain(const ParametersCall<T>& call) {
        return !call.indexed() && !call.limit()
            && call.parameters_expression().empty() && call.parameters_dict().empty();
    }

    // A value binding asked for more than its value -- '?x', 'x(1)', 'x_0' --
    // is answered by the definition it used to be, so that the diagnostics are
    // the definition's.
    definition_type Definition(const Binding& binding) const {
        if(binding.definition) return binding.definition;
        std::shared_ptr<Reference<T>> reference = std::make_shared<Reference<T>>();
        reference->add_expression(binding.name, ParametersDefinition<T>(),
                                  PExpression<T>(new ValExpression<T>(binding.value)));
        return reference;
    }

    const Binding* FindBinding(const std::string& name) const {
        if (open_ == 0) return nullptr;
        for (const Binding& binding : frames_[open_ - 1]) {
            if(binding.name == name) return &binding;
        }
        return nullptr;
    }

    // A frame is open whenever a binding is made: the call opens it before it
    // binds anything. _GLIBCXX_ASSERTIONS in the sanitizer build is what says
    // so if that ever stops being true (DESIGN.md, C15).
    Binding& FrameSlot(const std::string& name) {
        frame_type& frame = frames_[open_ - 1];
        for (Binding& binding : frame) {
            if(binding.name == name) return binding;
        }
        frame.push_back(Binding{name, T(), definition_type()});
        return frame.back();
    }

    void DropBinding(const std::string& name) {
        frame_type& frame = frames_[open_ - 1];
        for(size_t i = 0; i < frame.size(); ++i) {
            if(frame[i].name == name) {frame.erase(frame.begin() + i); return;}
        }
    }

    definition_type FindGlobal(const std::string& name) const {
        for (const Scope<T>* scope = scope_; scope; scope = scope->parent) {
            const auto found = scope->names.find(name);
            if (found != scope->names.end()) return found->second;
        }
        return definition_type();
    }

    // Depth alone does not bound time: the arithmetic-geometric mean nests
    // shallowly but branches twice per level. Steps do.
    struct Budget {
        explicit Budget(ReferenceStack& stack) : stack_(stack) {
            if(stack_.depth_ >= max_depth) {
                throw DepthExceeded("evaluation nests more than " + std::to_string(max_depth) +
                                    " references deep");
            }
            if(stack_.steps_ >= max_steps) {
                throw std::runtime_error("evaluation gave up after "
                                         + std::to_string(max_steps) + " steps");
            }
            ++stack_.depth_;
            ++stack_.steps_;
        }
        ~Budget() {--stack_.depth_;}
        Budget(const Budget&) = delete;
        Budget& operator=(const Budget&) = delete;

    private:
        ReferenceStack& stack_;
    };

    size_t depth_ = 0;
    size_t steps_ = 0;
    bool                                     filling_     = false;
    bool                                     fill_failed_ = false;
    std::unordered_map<MemoKey, T, MemoHash> memoised_, older_;
    Scope<T>                                 builtins_, session_;
    const Scope<T>*                          scope_  = &session_;  // where names are sought
    Scope<T>*                                target_ = &session_;  // where definitions go
    // Instances, made when first read and kept until a definition changes
    // what they read: the named by their definition, the unnamed by where
    // they are written, holding that expression so that its address is not
    // reused while it names one.
    std::unordered_map<const Reference<T>*, std::shared_ptr<Scope<T>>> named_;
    std::map<std::tuple<const void*, const void*, std::string>,
             std::pair<PExpression<T>, std::shared_ptr<Scope<T>>>>
                                             unnamed_;
    std::vector<frame_type>                  frames_;  // the open ones first, then spares
    size_t                                   open_ = 0;
};

#include "inkamath/derivative.hpp"

#endif // EXPRESSION_STACK_HPP
