/******************************************************************************
*
* Copyright Saab AB, 2007-2008 (http://www.safirsdk.com)
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

#include <Safir/Dob/Typesystem/LibraryExceptions.h>
#include <Safir/Dob/Typesystem/Utilities.h>
#include <Safir/Dob/Typesystem/Internal/Kernel.h>
#include <Safir/Dob/Typesystem/Exceptions.h>

//disable warnings in ace
#if defined _MSC_VER
  #pragma warning (push)
  #pragma warning (disable : 4267 4244)
#endif

#include <ace/Guard_T.h>
#include <ace/Recursive_Thread_Mutex.h>

//and enable the warnings again
#if defined _MSC_VER
  #pragma warning (pop)
#endif

#include <sstream>
#include <Safir/Utilities/Internal/LowLevelLogger.h>

namespace Safir
{
namespace Dob
{
namespace Typesystem
{
    LibraryExceptions * volatile LibraryExceptions::m_pInstance = NULL;

    // -----------------------------------------------------------
    LibraryExceptions &
    LibraryExceptions::Instance()
    {
        if (!m_pInstance)
        {
            static ACE_Recursive_Thread_Mutex instantiationLock;
            ACE_Guard<ACE_Recursive_Thread_Mutex> lck(instantiationLock);
            if (!m_pInstance)
            {
                m_pInstance = new LibraryExceptions();
            }
        }
        return *m_pInstance;
    }

    // -----------------------------------------------------------
    LibraryExceptions::LibraryExceptions()
    {

    }

    // -----------------------------------------------------------
    LibraryExceptions::~LibraryExceptions()
    {

    }

    // -----------------------------------------------------------
    void LibraryExceptions::Set(const FundamentalException & exception)
    {
        Set(static_cast<const Internal::CommonExceptionBase &>(exception));
    }


    // -----------------------------------------------------------
    void LibraryExceptions::Set(const Exception & exception)
    {
        Set(static_cast<const Internal::CommonExceptionBase &>(exception));
    }

    // -----------------------------------------------------------
    void LibraryExceptions::Set(const Internal::CommonExceptionBase & exception)
    {
        DotsC_SetException(exception.GetTypeId(),
                           Utilities::ToUtf8(exception.GetExceptionInfo()).c_str());
    }

    // -----------------------------------------------------------
    void LibraryExceptions::Set(const std::exception & exception)
    {
        DotsC_SetException(0,exception.what());
    }


    // -----------------------------------------------------------
    void LibraryExceptions::SetUnknown()
    {
        DotsC_SetException(0,"Unknown exception (caught as ...)");
    }


    // -----------------------------------------------------------
    void LibraryExceptions::AppendDescription(const std::wstring & moreDescription)
    {
        DotsC_AppendExceptionDescription(Utilities::ToUtf8(moreDescription).c_str());
    }

    // -----------------------------------------------------------
    /*    void LibraryExceptions::Set(const TypeId exceptionId, const std::wstring & description)
    {
        DotsC_SetException(exceptionId,Utilities::ToUtf8(description).c_str());
        }*/

    static const char * err1 = "Failed to copy the exception string in UnknownException constructor";
    static const char * err2 = "Failed to extract the c_str from the std::string in what()";

    class UnknownException:
        public std::exception
    {
    public:
        UnknownException(const std::string & what) throw()
        {
            try
            {
                m_what = what;
            }
            catch (...)
            {

            }
        }

        ~UnknownException() throw() {}

        const char * what() const throw ()
        {
            try
            {
                if (m_what.empty())
                {
                    return err1;
                }
                return m_what.c_str();
            }
            catch (...)
            {
                return err2;
            }
        }
    private:
        std::string m_what;
    };

    // -----------------------------------------------------------
    void LibraryExceptions::Throw()
    {
        bool wasSet;
        TypeId exceptionId;
        char * description;
        DotsC_BytePointerDeleter deleter;
        DotsC_GetAndClearException(exceptionId,description,deleter,wasSet);
        if (wasSet)
        {
            if (exceptionId == 0)
            {
                std::string desc = description;
                deleter(description);
                throw UnknownException(desc.c_str());
            }
            else
            {
                std::wstring desc = Utilities::ToWstring(description);
                deleter(description);
                CallbackMap::const_iterator it = m_CallbackMap.find(exceptionId);
                if (it == m_CallbackMap.end())
                {
                    std::wostringstream ostr;
                    ostr << "LibraryExceptions::Throw was called when an exception that was not registered in the exception-factory was set in dots_kernel." << std::endl
                         << "exceptionId = " << exceptionId << ", description = '" << desc << "'." << std::endl
                         << "Please report this to your nearest DOB developer!";
                    lllout << ostr.str() << std::endl;
                    throw SoftwareViolationException(ostr.str(),__WFILE__,__LINE__);
                }
                //invoke the function
                it->second(desc);
            }
        }
        else
        {
            throw SoftwareViolationException(L"There was no exception set when LibraryExceptions::Throw was called!",__WFILE__,__LINE__);
        }
    }


}
}
}
