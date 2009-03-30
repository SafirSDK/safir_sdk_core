/******************************************************************************
*
* Copyright Saab AB, 2007-2008 (http://www.safirsdk.com)
*
* Created by: Jonas Thor / stjth
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
#ifndef __PROCESS_MONITOR_H__
#define __PROCESS_MONITOR_H__

#include <ace/OS.h>
#include <boost/function.hpp>

#if defined _MSC_VER
  #ifdef LLUF_UTILS_EXPORTS
    #define LLUF_UTILS_API __declspec(dllexport)
  #else
    #define LLUF_UTILS_API __declspec(dllimport)
    #ifdef _DEBUG
      #pragma comment( lib, "lluf_utilsd.lib" )
    #else
      #pragma comment( lib, "lluf_utils.lib" )
    #endif
  #endif
#elif defined __GNUC__
  #define LLUF_UTILS_API
  #define __cdecl
#endif


namespace Safir
{
namespace Utilities
{
    class ProcessMonitorImpl;

    class LLUF_UTILS_API ProcessMonitor 
    {
    public:
        typedef boost::function<void(const pid_t & pid)> OnTerminateCb;

        ProcessMonitor();
        ~ProcessMonitor();

        /**
         * Init the ProcessMonitor.
         *
         * This method must be called first thing and before any call to StartMonitorPid or StopMonitorPid.
         *
         * @param [in] callback The function to be called when a monitored process exists. Note that this function
         *                      is executed in ProcessMonitor's own thread.
         */
        void Init(const OnTerminateCb& callback);
        

        /**
         * Start monitor the given PID.
         *
         * @param [in] pid  The PID which we want to monitor.
         */
        void StartMonitorPid(const pid_t pid);

        /**
         * Stop monitor the given PID.
         *
         * @param [in] pid  The PID which we want to stop monitor.
         */
        void StopMonitorPid(const pid_t pid);
        
    private:
        
        ProcessMonitorImpl* m_impl;
    };

}
}

#endif

