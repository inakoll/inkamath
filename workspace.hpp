#ifndef H_WORKSPACE
#define H_WORKSPACE

#include <iostream>
#include <algorithm> // for_each (deleter,copy)
#include <map>
#include <string>
#include <list>
#include <deque>
#include <utility> // pair
#include <memory>

#include "sequence.hpp"


#define PExpression		std::shared_ptr<Expression<T> >
#define PFuncExpression	std::shared_ptr<FuncExpression<T> >

template <typename T>
class WorkSpManager;

template <typename T>
class Expression;

template <typename T>
class EmptyExpression;

template <typename T>
class ValExpression;

template <typename T>
class RefExpression;

template <typename T>
class FuncExpression;

#ifdef _MSC_VER
	#pragma warning(disable : 4512) // functor ExprCopy can't have an assignation operator
#endif

template <typename T>
struct ExprCopy
{
    std::map<std::string, PExpression >& m_map;
    ExprCopy(std::map<std::string, PExpression >& map) : m_map(map) {}
    void operator()(std::pair<std::string, PExpression > p)
    {
        m_map.insert(std::pair<std::string, PExpression >(p.first,p.second->Clone()));
    }
};

template <typename T>
struct FuncExprCopy
{
    std::map<std::string, PFuncExpression >& m_map;
    FuncExprCopy(std::map<std::string, PFuncExpression >& map) : m_map(map) {}
    void operator()(std::pair<std::string, PFuncExpression > p)
    {
		PFuncExpression i = std::dynamic_pointer_cast<FuncExpression<T> >(p.second->Clone());
        m_map.insert(std::pair<std::string, PFuncExpression >(p.first, i));
    }
};

template <typename T>
class WorkSp
{
public:
    WorkSp();
    ~WorkSp();
    WorkSp(const WorkSp&);
    WorkSp(const std::list<std::string>&);
    WorkSp operator=(const WorkSp& w);
    const PExpression GetExpr(const std::string&);
    const PExpression GetExpr(const std::string&, size_t);
    const PExpression SetExpr(const std::string&,  PExpression);
    void SetSub(const std::string& name, size_t i, PExpression exp);
    const PExpression GetFunc(const std::string&);
	const PExpression SetFunc(const std::string&, PExpression);
    const PExpression SetFunc(const std::string&,  PExpression, PExpression);
    void EvalAllExpr(void);

    static void ExprDeleter(std::pair<std::string, PExpression > p)
    {
        p.second.reset();
    }
     static void ExprEval(std::pair<std::string, PExpression > p) 
	{
		T ret = 0;
		if(p.second) 
			ret = p.second->Eval();
		ExprDeleter(p);
		p.second.reset(new ValExpression<T>(ret));
	}

    bool empty()
    {
        return m_emap.empty();
    }

    void reset()
    {
        m_modified=false;
    }

    bool modified()
    {
        return m_modified;
    }

    std::list<std::string> toStringList() const;

    friend class WorkSpManager<T>;
protected:
    std::map<std::string,PExpression >		m_emap;
    std::map<std::string,PFuncExpression >	m_fmap;
    std::map<std::string,Sequence<T> >		m_smap;

    bool m_modified;

private:



};

template <typename T>
WorkSp<T>::WorkSp() : m_modified(false)
{}

template <typename T>
WorkSp<T>::WorkSp(const WorkSp& wks) : m_modified(false)
{
    std::for_each(wks.m_emap.begin(),wks.m_emap.end(),ExprCopy<T>(m_emap));
    std::for_each(wks.m_fmap.begin(),wks.m_fmap.end(),FuncExprCopy<T>(m_fmap));
    m_smap = wks.m_smap;
}

template <typename T>
WorkSp<T>::WorkSp(const std::list<std::string>& ls) : m_modified(false)
{
    for (typename std::list<std::string>::const_iterator it = ls.begin(); it!=ls.end(); ++it)
    {
        SetExpr(*it,new ValExpression<T>(0));
    }
}
template <typename T>
WorkSp<T> WorkSp<T>::operator=(const WorkSp<T>& wks)
{
    std::for_each(m_emap.begin(),m_emap.end(),ExprDeleter);
    m_emap.clear();
    std::for_each(wks.m_emap.begin(),wks.m_emap.end(),ExprCopy<T>(m_emap));

    std::for_each(m_fmap.begin(),m_fmap.end(),ExprDeleter);
    m_fmap.clear();
    std::for_each(wks.m_fmap.begin(),wks.m_fmap.end(),ExprCopy<T>(m_fmap));

    m_smap.clear();
    m_smap = wks.m_smap;

    return *this;
}


template <typename T>
WorkSp<T>::~WorkSp()
{
    for_each(m_emap.begin(),m_emap.end(),ExprDeleter);
    for_each(m_fmap.begin(),m_fmap.end(),ExprDeleter);
}

template <typename T>
const PExpression WorkSp<T>::GetExpr(const std::string& name)
{
    if (m_emap.count(name) == 0)
    {
        m_emap[name].reset(new ValExpression<T>(T(0)));
    }
    PExpression ret(m_emap[name]);
    return ret;
}

template <typename T>
const PExpression WorkSp<T>::GetExpr(const std::string& name, size_t i)
{
    T val;
    if (m_emap.count(name) == 0)
    {
        m_emap[name].reset(new ValExpression<T>(T(0)));
    }
    if (!m_smap[name].Get(i,val))
    {
        for (size_t j = m_smap[name].Size(); j<=i; ++j)
        {
            std::string subname = WorkSpManager<T>::Get()->GetFunc(name)->SubExpr()->Name();
            WorkSpManager<T>::Get()->SetFunc(subname,PExpression(new FuncExpression<T>(subname)),
																  PExpression(new ValExpression<T>(T(j))));
            val = m_emap[name]->Eval();
            m_smap[name].Set(j,val);
            //std::cout << name << "[" << j << "]=" << val << std::endl;
        }
        m_smap[name].Clear();
    }
    PExpression ret(new ValExpression<T>(val));
    return ret;
}

template <typename T>
const PExpression WorkSp<T>::SetExpr(const std::string& name, PExpression exp)
{
    m_emap[name].reset();
    m_modified = true;
    if (exp)
    {
        m_emap[name]=exp;
        //std::cout << "Set : " << name << " to " << exp->Eval() << std::endl;
    }
    else
    {
        std::cout << "Bad Set of : " << name << std::endl;
    }
    return m_emap[name];
}

template <typename T>
void WorkSp<T>::SetSub(const std::string& name, size_t i, PExpression exp)
{
    // Assure que l'expression existe dorénavant dans les maps
    WorkSpManager<T>::Get()->GetExpr(name);
    WorkSpManager<T>::Get()->GetFunc(name);

    m_modified = true;
    if (exp)
    {
        m_smap[name].Set(i,exp->Eval());
        //std::cout << "Set : " << name << " to " << exp->Eval() << std::endl;
    }
    else
    {
        std::cout << "Bad Set of : " << name << std::endl;
    }
    return;
}

template <typename T>
const PExpression WorkSp<T>::GetFunc(const std::string& name)
{
    if (m_fmap.count(name) == 0)
    {
		m_fmap[name].reset(new FuncExpression<T>(name, PExpression(new EmptyExpression<T>), PExpression(new EmptyExpression<T>))); 
        m_emap[name].reset(new ValExpression<T>(T(0)));
    }
    PExpression ret(m_fmap[name]);
    return ret;
}

template <typename T>
const PExpression WorkSp<T>::SetFunc(const std::string& name, PExpression funcExp, PExpression valExp)
{
    m_fmap[name].reset();
    m_modified = true;
    if (funcExp)
    {
		m_fmap[name] = std::dynamic_pointer_cast<FuncExpression<T> >(funcExp);
		//std::cout << "Set : " << name << " to " << exp->Eval() << std::endl;
    }
    else
    {
        std::cout << "Bad Set of : " << name << std::endl;
    }
	SetExpr(name, valExp);
    return m_fmap[name];
}

template <typename T>
const PExpression WorkSp<T>::SetFunc(const std::string& name, PExpression valExp)
{
    m_fmap[name].reset();
	m_fmap[name] = PFuncExpression(new FuncExpression<T>(name, PExpression(new EmptyExpression<T>),PExpression(new EmptyExpression<T>) ) );
    m_modified = true;
	SetExpr(name, valExp);
    return m_fmap[name];
}

template <typename T>
void WorkSp<T>::EvalAllExpr(void)
{
//    for_each(m_map.begin(),m_map.end(),ExprEval);
}

template <typename T>
std::list<std::string> WorkSp<T>::toStringList() const
{
    std::list<std::string> ls;
    for (typename std::map<std::string,PExpression >::iterator it =m_emap.begin(); it !=m_emap.end(); ++it)
    {
        ls.push_back(it->first);
    }
    return ls;
}

template <typename T>
bool isRecursive(const std::string& name)
{
    return false;
}

#undef PExpression
#undef PFuncExpression

#endif
