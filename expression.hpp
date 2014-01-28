#ifndef H_EXPR
#define H_EXPR

#include <iostream>
#include <string>
#include <algorithm> // max
#include <cmath>
#include <list>
#include <utility> // pair
#include <iterator> // back_inserter
#include <unordered_map>

#include <memory>

#include "pmath.hpp"
#include "expression_visitor.hpp"
#include "matrix.hpp"
#include "workspace_manager.hpp"
#include "numeric_interface.hpp"

template <typename T>
using PExpression = std::shared_ptr<Expression<T>>;

#define _EXPRESION_EPSILON 1E-10

template <typename T>
class EmptyExpression;

template <typename T>
class Expression;

template <typename T>
using ExprContext = std::unordered_map<std::string,PExpression<T> >;

template <typename T>
class Expression : public std::enable_shared_from_this<Expression<T>>
{
public:
    Expression() {}
    virtual ~Expression() {}
    virtual T Eval(void) const = 0;
    virtual PExpression<T> Clone() const = 0;
	
    PExpression<T> self() {
        return this->shared_from_this();
    }
	

    virtual PExpression<T> accept(class ExprVisitor<T> &v) = 0;
    virtual bool isEmpty(void) const
    {
        return false;
    }
    virtual T EvalParameters(void) const
    {
        return T();
    }
    virtual std::string Name()
    {
        return std::string();
    }
    virtual std::pair<size_t,size_t> Size() const
    {
        return std::make_pair(1,1);
    }
    virtual size_t VirtualSize() const
    {
        return (this->Size().first)*(this->Size().second);
    }
    virtual PExpression<T> GetExpr(size_t)
    {
        return PExpression<T>(this);
    }
    virtual bool isRef()
    {
        return false;
    }
    virtual std::list<std::string> Parameters(void) const
    {
        return m_params;
    }
    virtual PExpression<T> SubExpr() const
    {
        return PExpression<T>(new EmptyExpression<T>());
    }
    virtual void SetParameters(std::list<std::string> params)
    {
        m_params=params;
    }
	
    void setDefinitions(const ExprContext<T>& definitions) {
		definitions_ = definitions;
	}

    std::list<std::string> m_params;
protected:
private:
    // interdiction de la copie
    Expression(const Expression<T>& e);
    Expression& operator=(const Expression<T>& e);
	
    ExprContext<T> definitions_;
};

template <typename T>
class EmptyExpression : public Expression<T>
{
public:
    EmptyExpression() : Expression<T>() {}
    virtual std::pair<size_t,size_t> Size() const
    {
        return std::make_pair(0,0);
    }
    virtual PExpression<T> Clone() const
    {
        return PExpression<T>(new EmptyExpression());
    }
    virtual T Eval(void) const
    {
        return T();
    }

    virtual PExpression<T> accept(class ExprVisitor<T> &v) {
        return v.visit(this);
    }

    virtual bool isEmpty(void) const
    {
        return true;
    }
};

template <typename T>
class UnaryExpression : public Expression<T>
{
public:
    UnaryExpression(PExpression<T> e) : m_e(e)
    {
        if (e) Expression<T>::m_params = e->Expression<T>::m_params;
    }
    virtual ~UnaryExpression()
    {
        m_e.reset();
    }
    PExpression<T> m_e;

};

template <typename T>
class BinaryExpression : public Expression<T>
{
public:
    BinaryExpression(PExpression<T> e1, PExpression<T> e2) : m_e1(e1), m_e2(e2)
    {
        if (e1 && e2)
        {
            Expression<T>::m_params = e1->Expression<T>::m_params;
            Expression<T>::m_params.insert(Expression<T>::m_params.end(),
                                e2->Expression<T>::m_params.begin(),
                                e2->Expression<T>::m_params.end());
        }
    }
    virtual ~BinaryExpression()
    {
        m_e1.reset();
        m_e2.reset();
    }

	
    PExpression<T> m_e1;
    PExpression<T> m_e2;
};

template <typename T>
class EqualExpression : public BinaryExpression<T>
{
public:
    EqualExpression(PExpression<T> e1, PExpression<T> e2)
    : BinaryExpression<T>(e1,e2)
    {}

    virtual std::pair<size_t,size_t> Size() const
    {
        return std::make_pair(
        BinaryExpression<T>::m_e2->Size().first,
        BinaryExpression<T>::m_e2->Size().second);
    }

    virtual PExpression<T> Clone() const
    {
        return PExpression<T>(new EqualExpression(
                                 BinaryExpression<T>::m_e1->Clone(),
                                 BinaryExpression<T>::m_e2->Clone()));
    }

    virtual T Eval(void) const
    {
        T ret;
        std::string name = BinaryExpression<T>::m_e1->Name();
        PExpression<T> SubExpr = BinaryExpression<T>::m_e1->SubExpr();
        if (!SubExpr->isEmpty() && SubExpr->Name() == "")
        {
            T subval = SubExpr->Eval();
            WorkSpManager<T>::Get()->
                    SetSub(name,numeric_interface<T>::toInt(subval),BinaryExpression<T>::m_e2);

            ret = WorkSpManager<T>::Get()->
                    GetExpr(name,numeric_interface<T>::toInt(subval))->Eval();
        }
        else
        {
            WorkSpManager<T>::Get()->
                    SetFunc(name,BinaryExpression<T>::m_e1, BinaryExpression<T>::m_e2);

            ret = BinaryExpression<T>::m_e1->Eval();
        }
        return ret;
    }

    virtual PExpression<T> accept(class ExprVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};


template <typename T>
class AddExpression : public BinaryExpression<T>
{
public:
    AddExpression(PExpression<T> e1, PExpression<T> e2)
    : BinaryExpression<T>(e1,e2)
    {}

    virtual std::pair<size_t,size_t> Size() const
    {
        return std::make_pair(
            BinaryExpression<T>::m_e1->Size().first,
            BinaryExpression<T>::m_e1->Size().second);
    }

    virtual PExpression<T> Clone() const
    {
        return PExpression<T>(new AddExpression(
                BinaryExpression<T>::m_e1->Clone(),
                BinaryExpression<T>::m_e2->Clone()));
    }

    virtual T Eval(void) const
    {
        return (BinaryExpression<T>::m_e1)->Eval() +
               (BinaryExpression<T>::m_e2)->Eval();
    }

    virtual PExpression<T> accept(class ExprVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};

template <typename T>
class NegExpression : public UnaryExpression<T>
{
public:
    NegExpression(PExpression<T> e) : UnaryExpression<T>(e) {}
    virtual std::pair<size_t,size_t> Size() const
    {
        return std::make_pair(
            UnaryExpression<T>::m_e->Size().first,
            UnaryExpression<T>::m_e->Size().second);
    }
    virtual PExpression<T> Clone() const
    {
        return PExpression<T>(new NegExpression(UnaryExpression<T>::m_e->Clone()));
    }
    virtual T Eval(void) const
    {
        return -((UnaryExpression<T>::m_e)->Eval());
    }

    virtual PExpression<T> accept(class ExprVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};

template <typename T>
class MultExpression : public BinaryExpression<T>
{
public:
    MultExpression(PExpression<T> e1, PExpression<T> e2)
    : BinaryExpression<T>(e1,e2)
    {}
    virtual std::pair<size_t,size_t> Size() const
    {
        return std::make_pair(
                BinaryExpression<T>::m_e1->Size().first,
                BinaryExpression<T>::m_e2->Size().second);
    }
    virtual PExpression<T> Clone() const
    {
        return PExpression<T>(new MultExpression(
            BinaryExpression<T>::m_e1->Clone(),
            BinaryExpression<T>::m_e2->Clone()));
    }
    virtual T Eval(void) const
    {
        return (BinaryExpression<T>::m_e1)->Eval() *
               (BinaryExpression<T>::m_e2)->Eval();
    }

    virtual PExpression<T> accept(class ExprVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};

template <typename T>
class DivExpression : public BinaryExpression<T>
{
public:
    DivExpression(PExpression<T> e1, PExpression<T> e2) : BinaryExpression<T>(e1,e2) {}
    virtual std::pair<size_t,size_t> Size() const
    {
        return std::make_pair(
            BinaryExpression<T>::m_e1->Size().first,
            BinaryExpression<T>::m_e1->Size().second);
    }
    virtual PExpression<T> Clone() const
    {
        return PExpression<T>(new DivExpression(
            BinaryExpression<T>::m_e1->Clone(),
            BinaryExpression<T>::m_e2->Clone()));
    }
    virtual T Eval(void) const
    {
        return ((BinaryExpression<T>::m_e1)->Eval())/
                ((BinaryExpression<T>::m_e2)->Eval());
    }

    virtual PExpression<T> accept(class ExprVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};

template <typename T>
class PowExpression : public BinaryExpression<T>
{
public:
    PowExpression(PExpression<T> e1, PExpression<T> e2)
    : BinaryExpression<T>(e1,e2)
    {}
    virtual std::pair<size_t,size_t> Size() const
    {
        return std::make_pair(
            BinaryExpression<T>::m_e1->Size().first,
            BinaryExpression<T>::m_e1->Size().second);
    }
    virtual PExpression<T> Clone() const
    {
        return PExpression<T>(new PowExpression(
            BinaryExpression<T>::m_e1->Clone(),
            BinaryExpression<T>::m_e2->Clone()));
    }
    virtual T Eval(void) const
    {
        return numeric_interface<T>::pow(BinaryExpression<T>::m_e1->Eval(),
                                         BinaryExpression<T>::m_e2->Eval() );
    }

    virtual PExpression<T> accept(class ExprVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};

template <typename T>
class FactExpression : public UnaryExpression<T>
{
public:
    FactExpression(PExpression<T> e) : UnaryExpression<T>(e) {}
    virtual std::pair<size_t,size_t> Size() const
    {
        return std::make_pair(1,1);
    }
    virtual PExpression<T> Clone() const
    {
        return PExpression<T>(new FactExpression(UnaryExpression<T>::m_e->Clone()));
    }
    virtual T Eval(void) const
    {
        return T(numeric_interface<T>::fact((UnaryExpression<T>::m_e)->Eval()));
    }

    virtual PExpression<T> accept(class ExprVisitor<T> &v) {
        return v.visit(this);
    }
protected:
};

template <typename T>
class ValExpression : public Expression<T>
{
public:
    ValExpression(const T& v) : Expression<T>(), m_value(v) {}
    virtual std::pair<size_t,size_t> Size() const
    {
        return std::make_pair(
                m_value.Size().first,
                m_value.Size().second);
    }
    virtual PExpression<T> Clone() const
    {
        return PExpression<T>(new ValExpression(m_value));
    }
    virtual T Eval(void) const
    {
        return m_value;
    }

    virtual PExpression<T> accept(class ExprVisitor<T> &v) {
        return v.visit(this);
    }
protected:
    T m_value;
};

template <typename T>
class MatExpression : public Expression<T>
{
public:
    MatExpression(size_t i, size_t j)
    : m_matrix(i,j,PExpression<T>(new EmptyExpression<T>))
    {}
    MatExpression(PExpression<T> e) : m_matrix(1,1)
    {
        m_matrix(1,1)=e;
    }
    MatExpression(std::vector<std::vector<PExpression<T> > >& mat) : m_matrix(mat)
    {
        for (size_t i = 1; i<=m_matrix.Size().first; ++i)
        {
            for (size_t j = 1; j<=m_matrix.Size().second; ++j)
            {
                if (m_matrix(i,j)==0)
                {
                    m_matrix(i,j).reset(new EmptyExpression<T>);
                }
                Expression<T>::m_params.insert(
                    Expression<T>::m_params.end(),
                    m_matrix(i,j)->Expression<T>::m_params.begin(),
                    m_matrix(i,j)->Expression<T>::m_params.end());
            }
        }
    }

    virtual std::pair<size_t,size_t> Size() const
    {
        size_t m=0, m1=0;
        size_t n=0, n1=0;
        for (size_t i = 1; i<=m_matrix.Size().first; ++i)
        {
            for (size_t j = 1; j<=m_matrix.Size().second; ++j)
            {
                n1+=m_matrix(i,j)->Size().second;
                m1=std::max(m_matrix(i,j)->Size().first,m1);
            }
            m+=m1;
            n=std::max(n,n1);
            n1=0;
            m1=0;
        }
        return std::make_pair(m,n);
    }


    virtual PExpression<T> Clone() const
    {
        MatExpression* mat = new MatExpression<T>(
            m_matrix.Size().first,
            m_matrix.Size().second);

        for (size_t i = 1; i<=m_matrix.Size().first; ++i)
        {
            for (size_t j = 1; j<=m_matrix.Size().second; ++j)
                mat->m_matrix(i,j)=m_matrix(i,j)->Clone();
        }
        mat->SetParameters(Expression<T>::m_params);
        PExpression<T> ret(mat);
        return ret;
    }
    virtual T Eval(void) const
    {
        T val(Size().first,Size().second);
        size_t m=0, m1=0;
        size_t n=0, n1=0;
        for (size_t i = 1; i<=m_matrix.Size().first; ++i)
        {
            for (size_t j = 1; j<=m_matrix.Size().second; ++j)
            {
                for (size_t k = 1; k<=m_matrix(i,j)->Size().first; ++k)
                {
                    for (size_t l = 1; l<=m_matrix(i,j)->Size().second; ++l)
                    {
                        val(m+k,n1+l)=m_matrix(i,j)->Eval()(k,l);
                    }
                }
                n1+=m_matrix(i,j)->Size().second;
                m1=std::max(m_matrix(i,j)->Size().first,m1);
            }
            m+=m1;
            n=std::max(n,n1);
            n1=0;
            m1=0;
        }
        return val;
    }

    virtual PExpression<T> accept(class ExprVisitor<T> &v) {
        return v.visit(this);
    }

    virtual ~MatExpression()
    {
        for (size_t i = 1; i<=m_matrix.Size().first; ++i)
        {
            for (size_t j = 1; j<=m_matrix.Size().second; ++j)
            {
                m_matrix(i,j).reset();
            }
        }
    }

    virtual size_t VirtualSize(void)
    {
        return m_matrix.Size().first*m_matrix.Size().second;
    }
    virtual PExpression<T> GetExpr(size_t i)
    {
        if (i<m_matrix.Size().first*m_matrix.Size().second)
            return m_matrix.get(i);
        else
            throw(std::logic_error("Out of matrix range."));
    }
protected:
    Matrix<PExpression<T> > m_matrix;
};

template <typename T>
class RefExpression : public Expression<T>
{
public:
    RefExpression(const std::string& name) : m_name(name)
    {
        Expression<T>::m_params=std::list<std::string>(1,m_name);
    }
    ~RefExpression() {}

    virtual std::pair<size_t,size_t> Size() const
    {
        return WorkSpManager<T>::Get()->GetExpr(m_name)->Size();
    }
    virtual PExpression<T> Clone() const
    {
        return PExpression<T>(new RefExpression(m_name));
    }
    virtual std::string Name()
    {
        return m_name;
    }
    virtual T Eval(void) const
    {
        return WorkSpManager<T>::Get()->GetExpr(m_name)->Eval();
    }
    virtual bool isRef()
    {
        return true;
    }

    virtual PExpression<T> accept(class ExprVisitor<T> &v) {
        return v.visit(this);
    }
protected:
    std::string m_name;
};

template <typename T>
class FuncExpression : public BinaryExpression<T>
{
public:
    FuncExpression(const std::string& name, PExpression<T> e1 = PExpression<T>(new  EmptyExpression<T>), PExpression<T> e2 = PExpression<T>(new  EmptyExpression<T>))
    : BinaryExpression<T>(e1,e2), m_name(name)
    {
        Expression<T>::m_params=std::list<std::string>(1,m_name);
    }
    virtual std::pair<size_t,size_t> Size() const
    {
        return WorkSpManager<T>::Get()->GetExpr(m_name)->Size();
    }
    virtual std::list<std::string> Parameters(void) const
    {
        return BinaryExpression<T>::m_e1->Parameters();
    }
    virtual PExpression<T> SubExpr() const
    {
        return BinaryExpression<T>::m_e2;
    }
    virtual PExpression<T> Clone() const
    {
        return PExpression<T>(new FuncExpression(
                m_name,
                BinaryExpression<T>::m_e1->Clone(),
                BinaryExpression<T>::m_e2->Clone()));
    }
    virtual std::string Name()
    {
        return m_name;
    }
    virtual T EvalParameters(void) const
    {
        return BinaryExpression<T>::m_e1->Eval();
    }
    virtual T Eval(void) const
    {
        T ret;//= 0;
        WorkSpManager<T>::Push();

        WorkSpManager<T>::Get()->GetFunc(m_name)->EvalParameters();
        WorkSpManager<T>::Get()->reset();
        BinaryExpression<T>::m_e1->Eval();
        if (!WorkSpManager<T>::Get()->modified())
        {
            std::list<std::string> params=WorkSpManager<T>::Get()->
                                                  GetFunc(m_name)->Parameters();
            size_t i = 0;
            std::list<std::string>::iterator it = params.begin();
            for(;
                (it!=params.end() && i<BinaryExpression<T>::m_e1->VirtualSize());
                ++it, ++i)
            {
                WorkSpManager<T>::Get()->SetFunc(*it,PExpression<T>(
                    new ValExpression<T>(
                        BinaryExpression<T>::m_e1->GetExpr(i)->Eval())));
            }
        }
        std::string subname = WorkSpManager<T>::Get()->
            GetFunc(m_name)->SubExpr()->Name();



        if (subname != "")
        {	
			if (!BinaryExpression<T>::m_e2->isEmpty())
			{
				int subval =  numeric_interface<T>::toInt(BinaryExpression<T>::m_e2->Eval());
				
				WorkSpManager<T>::Get()->
                SetFunc(subname,PExpression<T>(new FuncExpression<T>(subname)), PExpression<T>(new ValExpression<T>(T(subval))) );
				if (subval >= 0)
				{
					ret = WorkSpManager<T>::Get()->
						GetExpr(m_name, subval)->Eval();
				}
				else
				{
					ret = T();
				}
			}
			else
			{
				int subval = 0;
				WorkSpManager<T>::Get()->
					SetFunc(subname,PExpression<T>(new FuncExpression<T>(subname)), PExpression<T>(new ValExpression<T>(T(subval))) );
				T first =  WorkSpManager<T>::Get()->
					GetExpr(m_name, subval)->Eval();
				typedef typename numeric_interface_imp_types<T>::abs difference_type;
				difference_type difference = numeric_interface<difference_type>::zero();
				do
				{
					++subval;
					WorkSpManager<T>::Get()->
						SetFunc(subname,PExpression<T>(new FuncExpression<T>(subname)), PExpression<T>(new ValExpression<T>(T(subval))) );
					ret =  WorkSpManager<T>::Get()->
					GetExpr(m_name,subval)->Eval();
					difference = numeric_interface<T>::abs(first - ret);
					first = ret;
				}while(difference > _EXPRESION_EPSILON);
			}
        }
		else
		{
			if (!BinaryExpression<T>::m_e2->isEmpty())
			{
				 throw(std::logic_error("This expression has no defined subvalue."));
			}
			ret =  WorkSpManager<T>::Get()->
				GetExpr(m_name)->Eval();
		}
		
        WorkSpManager<T>::Pop();
        return ret;
    }

    virtual PExpression<T> accept(class ExprVisitor<T> &v) {
        return v.visit(this);
    }
protected:
    std::string m_name;
};


template <typename T>
class ParametersDefinition
{
public:
    ParametersDefinition() {}
    ParametersDefinition(
            const std::vector<std::string>& parameters_names,
            const ExprDict<T>& parameters_dict,
            const std::string& index_name,
            const int& a,
            const int& b
        ) :
            parameters_names_(parameters_names),
            parameters_dict_(parameters_dict),
            index_name_(index_name),
            a_(a),
            b_(b)
    { }
    ~ParametersDefinition() {}


protected:
    std::vector<std::string> parameters_names_;
    ExprDict<T> parameters_dict_;
    std::string index_name_;
    int a_;
    int b_;
};


#endif
