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
        definition_type& slot = CurrentScope()[ai_reference_name];
        // Updating and initialising are the same operation.
        std::shared_ptr<Reference<T>> updated =
                slot ? std::make_shared<Reference<T>>(*slot) : std::make_shared<Reference<T>>();
        updated->add_expression(ai_reference_name, ai_parameters, ai_expression, written);
        slot = std::move(updated);
    }

    std::string Describe(const std::string& ai_reference_name, const ParametersCall<T>& ai_parameters) {
        definition_type definition = Find(ai_reference_name);
        if(!definition) {
            throw std::runtime_error(ai_reference_name + " is not defined");
        }
        return definition->Describe(ai_parameters, *this);
    }

    T Eval(const std::string& ai_reference_name, const ParametersCall<T>& ai_parameters)  {
        Budget budget(*this);
        bool from_frame = false;
        definition_type definition = Find(ai_reference_name, &from_frame);
        if(!definition) {
            throw std::runtime_error(ai_reference_name + " is not defined");
        }
        // Only a global's answer may be memoised. A definition written inside
        // an expression binds a local, in a frame, under the same name as the
        // global it shadows for that line -- and the cache is keyed on the
        // name (MODERNIZATION.md, C49).
        return definition->Eval(ai_parameters, *this, !from_frame);
    }

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
    scope_type& CurrentScope() {
        return frames_.empty() ? globals_ : frames_.back();
    }

    definition_type Find(const std::string& name, bool* from_frame = nullptr) const {
        if(!frames_.empty()) {
            auto parameter = frames_.back().find(name);
            if(parameter != frames_.back().end()) {
                if(from_frame) *from_frame = true;
                return parameter->second;
            }
        }
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
    std::vector<scope_type> frames_;
};

#endif // EXPRESSION_STACK_HPP
