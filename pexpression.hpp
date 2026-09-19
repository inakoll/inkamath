#ifndef H_PEXPRESSION
#define H_PEXPRESSION

#include <memory>

template <typename T>
class Expression;

template <typename T>
using PExpression = std::shared_ptr<Expression<T>>;

#endif // H_PEXPRESSION
