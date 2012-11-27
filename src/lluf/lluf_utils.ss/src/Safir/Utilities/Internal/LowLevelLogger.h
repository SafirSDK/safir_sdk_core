/******************************************************************************
*
* Copyright Saab AB, 2007-2012 (http://www.safirsdk.com)
*
* Created by: Lars Hagström / lars@foldspace.nu
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
#ifndef __LLUF_LOW_LEVEL_LOGGER_NEW_H__
#define __LLUF_LOW_LEVEL_LOGGER_NEW_H__

#include <Safir/Utilities/Internal/UtilsExportDefs.h>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/once.hpp>
#include <boost/thread/mutex.hpp>
#include <ostream>

/**
  * This is a utility for logging to file that is _only_ intended for
  * use by low level parts of the Safir system. All other applications should
  * use some other mechanism for logging.
  *
  * Just use lllog like you would any ostream
  *   lllog(1) << "hello world 1"<<std::endl;
  *   lllog(5) << 123 << std::endl;
  * The argument to lllog itself is the logging level, where high means verbose.
  * Level is a value between 1 and 9 (no checks are made for this, but any other
  * values may cause unexpected behavior).
  */
#define lllog(level) if (Safir::Utilities::Internal::Internal::LowLevelLogger::Instance().LogLevel() < level) ; \
                     else if(boost::unique_lock<boost::mutex> lck_fjki34 = \
                             boost::unique_lock<boost::mutex>(Safir::Utilities::Internal::Internal::LowLevelLogger::Instance().m_lock)) ; \
                     else Safir::Utilities::Internal::Internal::LowLevelLogger::Instance()
#define lllout lllog(7)
#define lllerr if(boost::unique_lock<boost::mutex> lck_fjki34 = \
                  boost::unique_lock<boost::mutex>(Safir::Utilities::Internal::Internal::LowLevelLogger::Instance().m_lock)) ; \
               else Safir::Utilities::Internal::Internal::LowLevelLogger::Instance()

namespace Safir
{
namespace Utilities
{
namespace Internal
{
    //this is all the hidden magic implementation
    namespace Internal
    {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4275)
#pragma warning(disable: 4251)
#endif
        class LLUF_UTILS_API LowLevelLogger :
            public std::wostream,
            private boost::noncopyable
        {
        public:
            static LowLevelLogger & Instance();
            
            inline int LogLevel() const 
            {
                if (m_pLogLevel == NULL) 
                {
                    return 0;
                }
                else
                {
                    return *m_pLogLevel;
                }
            }
            
            //this lock needs to be taken before logging to the logger!
            boost::mutex m_lock;
        private:
            /** Constructor*/
            explicit LowLevelLogger();

            /** Destructor */
            ~LowLevelLogger();

            /**
             * This class is here to ensure that only the Instance method can get at the 
             * instance, so as to be sure that boost call_once is used correctly.
             * Also makes it easier to grep for singletons in the code, if all 
             * singletons use the same construction and helper-name.
             */
            struct SingletonHelper
            {
            private:
                friend LowLevelLogger& LowLevelLogger::Instance();
                
                static LowLevelLogger& Instance();
                static boost::once_flag m_onceFlag;
            };

        class Impl;
        class LoggingImpl;
        class FallbackImpl;
        boost::shared_ptr<Impl> m_impl;

        const int* m_pLogLevel;            
        };
#ifdef _MSC_VER
#pragma warning (pop)
#endif
    }
}
}
}

#endif
