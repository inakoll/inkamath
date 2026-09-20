#ifndef EXPRESSION_STACK_HPP
#define EXPRESSION_STACK_HPP

#include <string>
#include <stdexcept>
#include "inkamath/mapstack.hpp"
#include "inkamath/reference.hpp"
#include "inkamath/pexpression.hpp"


template <typename T>
class ReferenceStack {
public:

    typedef Mapstack<std::string, Reference<T>> stack_type;

    // Measured against every golden transcript: the deepest legitimate
    // evaluation nests 33 references and the longest takes 6444 steps.
    static constexpr size_t max_depth = 256;
    static constexpr size_t max_steps = 1000000;

    // Call once per top-level evaluation; the stack outlives them all.
    void BeginEvaluation() {depth_ = 0; steps_ = 0;}

    ReferenceStack() {
        this->Set("pi", ParametersDefinition<T>(), PExpression<T>( new ValExpression<T>(T(3.1415926535898))));
        this->Set("e",  ParametersDefinition<T>(), PExpression<T>( new ValExpression<T>(T(2.7182818284590))));
        stack_.Push();
    }

    void Set(const std::string& ai_reference_name, const ParametersDefinition<T>& ai_parameters, PExpression<T>  ai_expression) {
        // Try to get a copy of the actual reference
        Reference<T> reference;
        stack_.Get(ai_reference_name, reference);

        // The fact that the reference was in the stack or not doesn't matter
        // Updating or initializing is the same operation
        reference.add_expression(ai_reference_name, ai_parameters, ai_expression);
        stack_.Set(ai_reference_name, reference);
    }

    T Eval(const std::string& ai_reference_name, const ParametersCall<T>& ai_parameters)  {
        Budget budget(*this);
        // Evaluation of an expression might mutate the internal stack_ object
        // The following line ensure that the stack will be restored at the end of the function
        // or in case of an exception thanks to RAII.
        // The context guard might have a huge impact on performance.
        // Consider to move it closer to the reference parameters assignation as a future optimisation.
        typename stack_type::Context guard(stack_);

        // Just evaluate the reference with the parameters if it's in the stack
        Reference<T> reference;
        if(stack_.Get(ai_reference_name, reference)) {
            return reference.Eval(ai_parameters,*this);
        }
        else {
            throw std::runtime_error(ai_reference_name + " is not defined");
        }
    }

    friend struct Guard;
    struct Guard {
    public:
        Guard(ReferenceStack<T>& stack) : guard(stack.stack_) {}
    private:
        typename stack_type::Context guard;
    };

    void Pop() {
        stack_.Pop();
    }

    void Clear() {
        stack_.Clear();
    }

private:
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
    mutable stack_type stack_;
};

#endif // EXPRESSION_STACK_HPP
