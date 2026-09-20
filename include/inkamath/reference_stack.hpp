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

    // Call once per top-level evaluation; the stack outlives them all.
    void BeginEvaluation() {depth_ = 0; steps_ = 0;}

    ReferenceStack() {
        this->Set("pi", ParametersDefinition<T>(), PExpression<T>( new ValExpression<T>(T(3.1415926535898))));
        this->Set("e",  ParametersDefinition<T>(), PExpression<T>( new ValExpression<T>(T(2.7182818284590))));
    }

    void Set(const std::string& ai_reference_name, const ParametersDefinition<T>& ai_parameters, PExpression<T>  ai_expression, const std::string& written = std::string()) {
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
        definition_type definition = Find(ai_reference_name);
        if(!definition) {
            throw std::runtime_error(ai_reference_name + " is not defined");
        }
        return definition->Eval(ai_parameters, *this);
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

    definition_type Find(const std::string& name) const {
        if(!frames_.empty()) {
            auto parameter = frames_.back().find(name);
            if(parameter != frames_.back().end()) return parameter->second;
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
    scope_type globals_;
    std::vector<scope_type> frames_;
};

#endif // EXPRESSION_STACK_HPP
