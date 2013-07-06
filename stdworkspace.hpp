#ifndef H_STDWORKPSACE
#define H_STDWORKPSACE

#include "workspace.hpp"

#define PExpression std::shared_ptr<Expression<T> >

template <typename T>
class stdworkspace : public WorkSp<T>
{
public:
    stdworkspace()
    {
        this->SetFunc("pi", PExpression( new ValExpression<T>(T(3.1415926535898))));
        this->SetFunc("e",  PExpression( new ValExpression<T>(T(2.7182818284590))));
    }
private:

    // Forbid stdworkspace modifications 
	// this is bullshit since WorkSp<T>::SetExpr is used everywhere else
	// Todo : think of a real solution...
    const PExpression SetExpr(const std::string& name, PExpression exp)
    {
        return WorkSp<T>::SetExpr(name,exp);
    }
};

#undef PExpression

#endif // H_STDWORKPSACE
