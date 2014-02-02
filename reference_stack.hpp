#ifndef EXPRESSION_STACK_HPP
#define EXPRESSION_STACK_HPP

#include <string>
#include "mapstack.hpp"

template <typename T>
class Expression;

template <typename T>
using PExpression = std::shared_ptr<Expression<T>>;

template <typename T>
class Reference;

template <typename T>
class ParametersDefinition;

template <typename T>
class ParametersCall;

template <typename T>
class ReferenceStack {
public:

    void Set(const std::string& ai_reference_name, const ParametersDefinition<T>& ai_parameters, PExpression  ai_expression) {
        // Try to get a copy of the actual reference
        Reference<T> reference;
        stack_.Get(ai_reference_name, reference);

        // The fact tha the reference was in the stack or not doesn't matter
        // Updating or initializing is the same operation
        reference.add_expression(ai_reference_name, ai_parameters, ai_expression);
        stack_.Set(ai_reference_name, reference);
    }


    T Eval(const std::string& ai_reference_name, const ParametersCall<T>& ai_parameters) const {
        // Evaluation of an expression might mutate the internal stack_ object
        // The following line ensure that the stack will be restored at the end of the function
        // or in case of an exception thanks to RAII.
        Context guard(stack_);

        // Just evaluate the reference with the parameters if it's in the stack
        Reference<T> reference;
        if(stack_.Get(ai_reference_name, reference)) {
            return reference.Eval(ai_parameters);
        }
        else {
            return T();
        }
    }

    void Clear() {
        stack_.Clear();
    }

private:
    mutable mapstack<std::string, Reference<T>> stack_;
};

#endif // EXPRESSION_STACK_HPP
