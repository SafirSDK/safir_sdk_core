/******************************************************************************
*
* Copyright Saab AB, 2006-2008 (http://www.safirsdk.com)
* 
* Created by: Lars Hagstr�m / stlrha
*
*******************************************************************************
*
* This file is part of Safir SDK Core.
*
* Safir SDK Core is free software: you can redistribute it and/or modify
* it under the terms of version 3 of the GNU General Public License as
* published by the Free Software Foundation.
*
* Safir SDK Core is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Safir SDK Core.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************************/

#include "dots_exception_keeper.h"
#include <ace/Guard_T.h>
#include <Safir/Utilities/Internal/LowLevelLogger.h>
#include <sstream>
#include <iostream>


namespace Safir
{
namespace Dob
{
namespace Typesystem
{
namespace Internal
{
    ExceptionKeeper * volatile ExceptionKeeper::m_instance = NULL;
    ACE_Thread_Mutex ExceptionKeeper::m_instantiationLock;// = ACE_Thread_Mutex();

    //
    //Instance
    //
    ExceptionKeeper & ExceptionKeeper::Instance()
    {
        if (m_instance == NULL)
        {
            ACE_Guard<ACE_Thread_Mutex> lck(m_instantiationLock);

            if (m_instance == NULL)
            {
                m_instance = new ExceptionKeeper();
            }
        }
        return *m_instance;
    }

    ExceptionKeeper::ExceptionKeeper()
    {

    }

    ExceptionKeeper::~ExceptionKeeper()
    {

    }


    ExceptionKeeper::ExceptionData &
    ExceptionKeeper::GetDataForCurrentThread() const
    {
        ACE_Guard<ACE_Thread_Mutex> lck(m_lock);
        const ACE_thread_t threadId = ACE_Thread::self();
        ThreadExceptionTable::iterator it = m_threadExceptionTable.find(threadId);
        if (it == m_threadExceptionTable.end()) //not found
        {
            it = m_threadExceptionTable.insert(std::make_pair(threadId,ExceptionData())).first;
        }
        return it->second;
    }

    void
    ExceptionKeeper::Set(const DotsC_TypeId exceptionId, const std::string & description)
    {
        ExceptionData & exc = GetDataForCurrentThread();

        lllout << "ExceptionKeeper::Set: id = " << exceptionId << " description = " << description.c_str() <<std::endl;
        if (exc.isSet)
        {
            std::wostringstream ostr;
            ostr << "ExceptionKeeper::Set: Trying to set new exception data over old (and uncleared) exception data" << std::endl
                << "Old = (" << exc.exceptionId << ", " << exc.description.c_str() << ")" <<std::endl
                << "New = (" << exceptionId << ", " << description.c_str() << ")" <<std::endl;
            ostr << "This means that there is something wrong with the exception handling of a library you are using"<< std::endl
                << "Please contact your nearest DOB developer if you have no idea what this means, otherwise contact the"
                << " developer of the library you suspect has the error"<<std::endl;

            lllout << ostr.str();
            std::wcout << ostr.str();
        }
        exc.exceptionId = exceptionId;
        exc.description = description;
        exc.isSet = true;
    }

    void
    ExceptionKeeper::AppendDescription(const std::string & moreDescription)
    {
        lllout << "ExceptionKeeper::AppendDescription: moreDescription = " << moreDescription.c_str() << std::endl;
        ExceptionData & exc = GetDataForCurrentThread();
        if (!exc.isSet)
        {
            std::wostringstream ostr;
            ostr << "ExceptionKeeper::AppendDescription: Called even though there is no exception set!!!)" <<std::endl;
            ostr << "This means that there is something wrong with the exception handling of a library you are using"<< std::endl
                << "Please contact your nearest DOB developer if you have no idea what this means, otherwise contact the"
                << " developer of the library you suspect has the error"<<std::endl;
            lllout << ostr.str();
            std::wcout << ostr.str();
        }
        exc.description.append("\n-------------- More Description -------------\n");
        exc.description.append(moreDescription);
    }

    bool
    ExceptionKeeper::IsSet() const
    {
        return GetDataForCurrentThread().isSet;
    }

    bool
    ExceptionKeeper::GetAndClear(DotsC_TypeId & exceptionId, std::string & description)
    {
        ExceptionData & exc = GetDataForCurrentThread();
        if (!exc.isSet)
        {
            lllout << "ExceptionKeeper::GetAndClear: when no exception is set!"<<std::endl;
            return false;
        }
        else
        {
            exceptionId = exc.exceptionId ;
            description = exc.description;
            lllout << "ExceptionKeeper::GetAndClear: id = " << exceptionId << " description = " << description.c_str() <<std::endl;

            exc.isSet = false;
            return true;
        }
    }

    void
    ExceptionKeeper::Peek(DotsC_TypeId & exceptionId, std::string & description) const
    {
        ExceptionData & exc = GetDataForCurrentThread();
        if (!exc.isSet)
        {
            lllout << "ExceptionKeeper::Peek: when no exception is set!"<<std::endl;

            std::wostringstream ostr;
            ostr << "ExceptionKeeper::Peek: Called even though there is no exception set!!!)" <<std::endl;
            ostr << "This means that there is something wrong with the exception handling of a library you are using"<< std::endl
                << "Please contact your nearest DOB developer if you have no idea what this means, otherwise contact the"
                << " developer of the library you suspect has the error"<<std::endl;
            lllout << ostr.str();
            std::wcout << ostr.str();
            exceptionId = 0;
            description.clear();
        }
        else
        {
            exceptionId = exc.exceptionId ;
            description = exc.description;
            lllout << "ExceptionKeeper::Peek: id = " << exceptionId << " description = " << description.c_str() <<std::endl;
        }
    }

    void
    ExceptionKeeper::Clear()
    {
        ExceptionData & exc = GetDataForCurrentThread();
        if (!exc.isSet)
        {
            std::wostringstream ostr;
            ostr << "ExceptionKeeper::Clear: Called even though there is no exception set!!!)" <<std::endl;
            ostr << "This means that there is something wrong with the exception handling of a library you are using"<< std::endl
                << "Please contact your nearest DOB developer if you have no idea what this means, otherwise contact the"
                << " developer of the library you suspect has the error"<<std::endl;
            lllout << ostr.str();
            std::wcout << ostr.str();
        }
        else
        {
            exc.isSet = false;
        }
    }

}
}
}
}
