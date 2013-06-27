/******************************************************************************
*
* Copyright Saab AB, 2013 (http://www.safirsdk.com)
*
* Created by: Anders Widén
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
#ifdef _MSC_VER

#include "LogWin32.h"

#include <windows.h>
#include <psapi.h>
#include <vector>
#include <string>

#include <boost/shared_ptr.hpp>

namespace Safir
{
namespace Utilities
{
namespace Internal
{

namespace
{

// Some constants
const DWORD maxStringSize = 64u * 1024u;
const std::string regKey =
        "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\Safir";
const std::string eventMessageFile32 = "C:\\Windows\\Microsoft.NET\\Framework\\v2.0.50727\\EventLogMessages.dll";
const std::string eventMessageFile64 = "C:\\Windows\\Microsoft.NET\\Framework64\\v2.0.50727\\EventLogMessages.dll";
const char* eventMessageFileParamName = "EventMessageFile";
const char* typesSupportedParamName = "TypesSupported";
const DWORD typesSupported = (EVENTLOG_SUCCESS |
                              EVENTLOG_INFORMATION_TYPE |
                              EVENTLOG_WARNING_TYPE |
                              EVENTLOG_ERROR_TYPE);
const char* logSourceName ="Safir";

bool Is64Bit()
{
#pragma warning(disable:4127) //Get rid of warning that this if-expression is constant (comparing two constants)

    if (sizeof(void*) > 4)
    {
        return true;
    }
    else
    {
        return false;
    }
#pragma warning(default:4127)
}

//------------------------------------------
LSTATUS GetRegistryValue(HKEY hKey, const char* lpValueName, std::string& value)
{
    DWORD type = REG_NONE;
    DWORD size = 0;
    LSTATUS res = RegQueryValueExA(hKey, lpValueName, NULL, &type, NULL, &size);
    if (res == ERROR_SUCCESS && ((type != REG_EXPAND_SZ && type != REG_SZ) || size > maxStringSize))
    {
        return ERROR_INVALID_DATA;
    }
    if (size == 0)
    {
        return res;
    }

    value.resize(size);
    res = RegQueryValueExA(hKey, lpValueName, NULL, &type, reinterpret_cast<LPBYTE>(&value[0]), &size);
    value.resize(std::strlen(value.c_str())); // remove extra terminating zero

    return res;
}

//------------------------------------------
LSTATUS GetRegistryValue(HKEY hKey, const char* lpValueName, DWORD& value)
{
    DWORD type = REG_NONE;
    DWORD size = sizeof(value);
    LSTATUS res = RegQueryValueExA(hKey, lpValueName, NULL, &type, reinterpret_cast<LPBYTE>(&value), &size);
    if (res == ERROR_SUCCESS && type != REG_DWORD && type != REG_BINARY)
    {
        res = ERROR_INVALID_DATA;
    }
    return res;
}

} // anonymous namespace


WindowsLogger::WindowsLogger(const std::string& processName)
    : m_startupSynchronizer("LLUF_WINDOWS_LOGGING_INITIALIZATION"),
      m_sourceHandle(0),
      m_processName(processName),
      m_eventMessageFile()
{
    // In order not to complicate our own build process (for example, we don't want to mess
    // with the MessageCompiler) we use the resources provided by EventLogMessages.dll from the
    // .Net framework.
    if (Is64Bit())
    {
        m_eventMessageFile = eventMessageFile64;
    }
    else
    {
        m_eventMessageFile = eventMessageFile64;
    }

    m_sourceHandle = RegisterEventSourceA(NULL, "Safir");
    if (!m_sourceHandle)
    {
        throw std::logic_error("Could not register event source");
    }

    m_startupSynchronizer.Start(this);
}

WindowsLogger::~WindowsLogger()
{
    DeregisterEventSource(m_sourceHandle);
}

//------------------------------------------
// Implementation of synchronization callbacks
//
void WindowsLogger::Create()
{
    AddRegistryEntries();
}

void WindowsLogger::Use()
{
    // No action
}

void WindowsLogger::Destroy()
{
    // no action
}

//------------------------------------------
bool WindowsLogger::AddRegistryEntries() const
{
    //TODO
    const std::string errTxt = "\n\nThis is a non-fatal error and the only issue is that\n"
                               "Event Logs generated by Safir will contain an annoying text\n"
                               "about a missing Event ID. The most likely cause is that the process\n"
                               "doesn't have write access to the registry. As an alternative\n"
                               "the necessary registry keys can be added prior to running the Safir system\n"
                               "using a provided reg file. Check $SAFIR_RUNTIME\\????";

    // First check the registry keys and values in read-only mode.
    // This allow us to avoid to elevate permissions to modify the registry
    // when we dont need to.
    if (RegistryIsInitialized())
    {
        return true;
    }

    // Create or open the key
    HKEY hKey = 0;
    DWORD disposition = 0;
    LSTATUS res = RegCreateKeyExA(HKEY_LOCAL_MACHINE,
                                  regKey.c_str(),
                                  0,
                                  NULL,
                                  REG_OPTION_NON_VOLATILE,
                                  KEY_WRITE,
                                  NULL,
                                  &hKey,
                                  &disposition);
    if (res != ERROR_SUCCESS)
    {
        Send(EVENTLOG_INFORMATION_TYPE,
             "The process " + m_processName + " could not create registry key " + regKey + errTxt);
        return false;
    }

    // Shared pointer used as guard to close registry key when we leave scope
    boost::shared_ptr<void> hKeyGuard(hKey, RegCloseKey);

    // Set the module file name that contains event resources
    res = RegSetValueExA(hKey,
                         eventMessageFileParamName,
                         0,
                         REG_EXPAND_SZ,
                         reinterpret_cast<LPBYTE>(const_cast<char*>(m_eventMessageFile.c_str())),
                         static_cast<DWORD>(m_eventMessageFile.size() + 1));
    if (res != ERROR_SUCCESS)
    {
        Send(EVENTLOG_INFORMATION_TYPE,
             "The process " + m_processName + " could not create registry value " + eventMessageFileParamName + errTxt);
        return false;
    }

    // Set the supported event types
    DWORD eventTypes = typesSupported;
    res = RegSetValueExA(hKey,
                         typesSupportedParamName,
                         0,
                         REG_DWORD,
                         reinterpret_cast<LPBYTE>(&eventTypes),
                         static_cast<DWORD>(sizeof(eventTypes)));
    if (res != ERROR_SUCCESS)
    {
        Send(EVENTLOG_INFORMATION_TYPE,
             "The process " + m_processName + " could not create registry value " + typesSupportedParamName + errTxt);
        return false;
    }

    return true;
}

void WindowsLogger::Send(const WORD eventType, const std::string& log) const
{
    const char* message = log.c_str();

    ReportEventA(m_sourceHandle,
                 eventType,
                 0,
                 0,
                 NULL,
                 1,
                 0,
                 &message,
                 NULL);
}

bool WindowsLogger::RegistryIsInitialized() const
{
    // Open the key
    HKEY hKey = 0;
    LSTATUS res = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                                regKey.c_str(),
                                REG_OPTION_NON_VOLATILE,
                                KEY_READ,
                                &hKey);
    if (res != ERROR_SUCCESS)
    {
        return false;
    }

    // Shared pointer used as guard to close registry key when we leave scope
    boost::shared_ptr<void> hKeyGuard(hKey, RegCloseKey);

    std::string msgFile;
    res = GetRegistryValue(hKey, eventMessageFileParamName, msgFile);
    if (res != ERROR_SUCCESS || msgFile != m_eventMessageFile)
    {
        return false;
    }

    DWORD eventTypes = 0;
    res = GetRegistryValue(hKey, typesSupportedParamName, eventTypes);
    if (res != ERROR_SUCCESS || eventTypes != typesSupported)
    {
        return false;
    }

    return true;
}

}
}
}

#endif
