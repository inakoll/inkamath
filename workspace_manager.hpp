#ifndef H_WORKSPACE_MANAGER
#define H_WORKSPACE_MANAGER

#include <map>
#include <utility>
#include <deque>
#include <stack>
#include <string>
#include <memory>

#include "workspace.hpp"
#include "stdworkspace.hpp"

#define PWorkSp std::shared_ptr<WorkSp<T> >

template <typename T>
class WorkSpManager
{
public:
    static PWorkSp Get()
    {
        return m_stack.top();
    }

    PWorkSp Workspace()
    {
        return m_stack.top();
    }

    static PWorkSp Pop()
    {
		PWorkSp ret;
        if (m_stack.size() != 0) 
		{
			m_stack.pop();
			ret = m_stack.top();
		}
		else
		{
			ret = PStdWks;
		}
		return ret;
    }

    static PWorkSp PushNew(PWorkSp tmp)
    {
        if (tmp)
        {
            m_stack.push(tmp);
        }
        return m_stack.top();
    }

    static PWorkSp Push()
    {
        if (m_stack.top())
        {
            PWorkSp tmp(new WorkSp<T>(*m_stack.top()));
            m_stack.push(tmp);
        }
        return m_stack.top();
    }

protected:
    WorkSpManager();
    virtual ~WorkSpManager();

	static PWorkSp PStdWks;
    static std::stack<PWorkSp > m_stack;

};

template<typename T>
PWorkSp WorkSpManager<T>::PStdWks = PWorkSp(new stdworkspace<T>);

template<typename T>
std::stack<PWorkSp > WorkSpManager<T>::m_stack = std::stack<PWorkSp >(std::deque<PWorkSp> (1,WorkSpManager<T>::PStdWks) );

#undef PWorkSp

#endif // H_WORKSPACE_MANAGER
