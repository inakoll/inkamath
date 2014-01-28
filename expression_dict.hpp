#ifndef H_EXPR_DICT
#define H_EXPR_DICT

#include <string>

#define PExpression std::shared_ptr<Expression<T> >

template <typename T>
class Expression;

template <typename T>
using ExprDict = std::unordered_map<std::string,PExpression >;

#undef PExpression

#endif // H_EXPR_DICT
