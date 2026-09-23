#ifndef EXPRESSION_STACK_HPP
#define EXPRESSION_STACK_HPP

#include <string>
#include <stdexcept>
#include <memory>
#include <unordered_map>
#include <vector>
#include "inkamath/reference.hpp"
#include "inkamath/pexpression.hpp"

// Two scopes and no more: the definitions the user has made, and the
// parameters of the call being evaluated. A call never sees its caller's
// parameters, so 'q = y+1' means the global y whichever call is on the
// stack (MODERNIZATION.md, phase 4 item 5).
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
    // session sweeping a parameter cannot grow the process without bound;
    // when it fills, the whole map goes, which costs time and never an
    // answer.
    static constexpr size_t max_memoised = 100000;

    // Call once per top-level evaluation; the stack outlives them all.
    void BeginEvaluation() {depth_ = 0; steps_ = 0;}

    ReferenceStack() {
        // Every digit a double holds: the 2014 literals stopped at fourteen,
        // which is a 7e-15 error in pi, and that is what 'e^(i*pi)' reported
        // as the imaginary part of -1.
        this->Set("pi", ParametersDefinition<T>(), PExpression<T>( new ValExpression<T>(T(3.14159265358979323846))));
        this->Set("e",  ParametersDefinition<T>(), PExpression<T>( new ValExpression<T>(T(2.71828182845904523536))));
    }

    // A memoised result may have read a global, so redefining one drops the
    // cache. A definition made while a frame is on the stack is a parameter
    // or an index, which is part of the key and cannot invalidate anything.
    void Set(const std::string& ai_reference_name, const ParametersDefinition<T>& ai_parameters, PExpression<T>  ai_expression, const std::string& written = std::string()) {
        if(frames_.empty()) {
            memoised_.clear();
        }
        definition_type& slot = DefinitionSlot(ai_reference_name);
        // Updating and initialising are the same operation.
        std::shared_ptr<Reference<T>> updated =
                slot ? std::make_shared<Reference<T>>(*slot) : std::make_shared<Reference<T>>();
        updated->add_expression(ai_reference_name, ai_parameters, ai_expression, written);
        slot = std::move(updated);
    }

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
            throw std::runtime_error(ai_reference_name + " is not defined");
        }
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
            throw std::runtime_error(ai_reference_name + " is not defined");
        }
        return definition->Eval(ai_parameters, *this, true);
    }

    // Whether a call's frame is open to bind in.
    [[nodiscard]] bool Framed() const { return !frames_.empty(); }

    // One step of an evaluation that reads no name, and so never passes
    // through Eval: a sum of a constant still has to end.
    void Step() { Budget step(*this); }

    // MODERNIZATION.md, phase 9. A call's answer depends on the definition,
    // the index, the argument values and the globals; the first three are the
    // key and the fourth is handled by clearing. What it is worth: the
    // arithmetic-geometric mean is 2^(n+1)-1 calls for 2n+1 answers.
    const T* Memoised(const std::string& key) const {
        auto found = memoised_.find(key);
        return found == memoised_.end() ? nullptr : &found->second;
    }

    void Memoise(const std::string& key, const T& evaluation) {
        if(memoised_.size() >= max_memoised) {
            memoised_.clear();
        }
        memoised_.emplace(key, evaluation);
    }

    // A binding made to try something out. A guard needs its index bound to be
    // asked at all, and a guard that does not hold must leave nothing behind
    // (MODERNIZATION.md, C53).
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
        explicit Frame(ReferenceStack<T>& stack) : stack_(stack) {stack_.frames_.emplace_back();}
        ~Frame() {stack_.frames_.pop_back();}
        Frame(const Frame&) = delete;
        Frame& operator=(const Frame&) = delete;
    private:
        ReferenceStack<T>& stack_;
    };

private:
    friend struct Trial;

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
        if(frames_.empty()) return nullptr;
        for(const Binding& binding : frames_.back()) {
            if(binding.name == name) return &binding;
        }
        return nullptr;
    }

    // A frame is open whenever a binding is made: the call opens it before it
    // binds anything. _GLIBCXX_ASSERTIONS in the sanitizer build is what says
    // so if that ever stops being true (MODERNIZATION.md, C15).
    Binding& FrameSlot(const std::string& name) {
        for(Binding& binding : frames_.back()) {
            if(binding.name == name) return binding;
        }
        frames_.back().push_back(Binding{name, T(), definition_type()});
        return frames_.back().back();
    }

    void DropBinding(const std::string& name) {
        frame_type& frame = frames_.back();
        for(size_t i = 0; i < frame.size(); ++i) {
            if(frame[i].name == name) {frame.erase(frame.begin() + i); return;}
        }
    }

    definition_type& DefinitionSlot(const std::string& name) {
        return frames_.empty() ? globals_[name] : FrameSlot(name).definition;
    }

    definition_type FindGlobal(const std::string& name) const {
        auto global = globals_.find(name);
        return global == globals_.end() ? definition_type() : global->second;
    }

    // Depth alone does not bound time: the arithmetic-geometric mean nests
    // shallowly but branches twice per level. Steps do.
    struct Budget {
        explicit Budget(ReferenceStack& stack) : stack_(stack) {
            if(stack_.depth_ >= max_depth) {
                throw std::runtime_error("evaluation nests more than "
                                         + std::to_string(max_depth) + " references deep");
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
    std::unordered_map<std::string, T> memoised_;
    scope_type globals_;
    std::vector<frame_type> frames_;
};

#endif // EXPRESSION_STACK_HPP
