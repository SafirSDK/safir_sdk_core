/******************************************************************************
*
* Copyright Saab AB, 2013 (http://safir.sourceforge.net)
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
const std::wstring regKey =
        L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\Safir";

// In order not to complicate our own build process (for example, we don't want to mess
// with the MessageCompiler) we use the resources provided by EventLogMessages.dll from the
// .Net framework.
const std::wstring eventMessageFile = L"C:\\Windows\\Microsoft.NET\\Framework\\v2.0.50727\\EventLogMessages.dll";

const wchar_t* eventMessageFileParamName = L"EventMessageFile";
const wchar_t* typesSupportedParamName = L"TypesSupported";
const DWORD typesSupported = (EVENTLOG_SUCCESS |
                              EVENTLOG_INFORMATION_TYPE |
                              EVENTLOG_WARNING_TYPE |
                              EVENTLOG_ERROR_TYPE);
const wchar_t* logSourceName = L"Safir";


//------------------------------------------
LSTATUS GetRegistryValue(HKEY hKey, const wchar_t* lpValueName, std::wstring& value)
{
    DWORD type = REG_NONE;
    DWORD size = 0;

    // get size of value
    LSTATUS res = RegQueryValueExW(hKey, lpValueName, NULL, &type, NULL, &size);
    if (res == ERROR_SUCCESS && ((type != REG_EXPAND_SZ && type != REG_SZ) || size / sizeof(wchar_t) > maxStringSize))
    {
        return ERROR_INVALID_DATA;
    }
    if (size == 0)
    {
        return res;
    }

    value.resize(size / sizeof(wchar_t));
    // get value
    res = RegQueryValueExW(hKey, lpValueName, NULL, &type, reinterpret_cast<LPBYTE>(&value[0]), &size);
    value.resize(std::wcslen(value.c_str())); // remove extra terminating zero

    return res;
}

//------------------------------------------
LSTATUS GetRegistryValue(HKEY hKey, const wchar_t* lpValueName, DWORD& value)
{
    DWORD type = REG_NONE;
    DWORD size = sizeof(value);
    LSTATUS res = RegQueryValueExW(hKey, lpValueName, NULL, &type, reinterpret_cast<LPBYTE>(&value), &size);
    if (res == ERROR_SUCCESS && type != REG_DWORD && type != REG_BINARY)
    {
        res = ERROR_INVALID_DATA;
    }
    return res;
}

} // anonymous namespace


WindowsLogger::WindowsLogger(const std::wstring& processName)
    : m_startupSynchronizer("LLUF_WINDOWS_LOGGING_INITIALIZATION"),
      m_sourceHandle(0),
      m_processName(processName)
{
    m_sourceHandle = RegisterEventSourceW(NULL, L"Safir");
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
    const std::wstring errTxt = 
        L"\n\nThis is a non-fatal error and the only issue is that\n"
        L"Event Logs generated by Safir will contain an annoying text\n"
        L"about a missing Event ID. The most likely cause is that the process\n"
        L"doesn't have write access to the registry. As an alternative\n"
        L"the necessary registry keys can be added prior to running the Safir system\n"
        L"using a provided reg file.\n"
        L"Check %SAFIR_RUNTIME%\\data\\text\\safir_event_log_registration.reg";

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
    LSTATUS res = RegCreateKeyExW(HKEY_LOCAL_MACHINE,
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
             L"The process " + m_processName + L" could not create registry key " + regKey + errTxt);
        return false;
    }

    // Shared pointer used as guard to close registry key when we leave scope
    boost::shared_ptr<void> hKeyGuard(hKey, RegCloseKey);

    // Set the module file name that contains event resources
    res = RegSetValueExW(hKey,
                         eventMessageFileParamName,
                         0,
                         REG_EXPAND_SZ,
                         reinterpret_cast<const BYTE*>(eventMessageFile.c_str()),
                         static_cast<DWORD>((eventMessageFile.length() + 1) * sizeof(wchar_t)));
    if (res != ERROR_SUCCESS)
    {
        Send(EVENTLOG_INFORMATION_TYPE,
             L"The process " + m_processName + L" could not create registry value " + eventMessageFileParamName + errTxt);
        return false;
    }

    // Set the supported event types
    DWORD eventTypes = typesSupported;
    res = RegSetValueExW(hKey,
                         typesSupportedParamName,
                         0,
                         REG_DWORD,
                         reinterpret_cast<LPBYTE>(&eventTypes),
                         static_cast<DWORD>(sizeof(eventTypes)));
    if (res != ERROR_SUCCESS)
    {
        Send(EVENTLOG_INFORMATION_TYPE,
             L"The process " + m_processName + L" could not create registry value " + typesSupportedParamName + errTxt);
        return false;
    }

    return true;
}

void WindowsLogger::Send(const WORD eventType, const std::wstring& log) const
{
    const wchar_t* logPtr = log.c_str();

    ReportEventW(m_sourceHandle,
                 eventType,
                 0,
                 0,
                 NULL,
                 1,
                 0,
                 &logPtr,
                 NULL);
}

bool WindowsLogger::RegistryIsInitialized() const
{
    // Open the key
    HKEY hKey = 0;
    LSTATUS res = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
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

    std::wstring msgFile;
    res = GetRegistryValue(hKey, eventMessageFileParamName, msgFile);
    if (res != ERROR_SUCCESS)
    {
        return false;
    }

    DWORD eventTypes = 0;
    res = GetRegistryValue(hKey, typesSupportedParamName, eventTypes);
    if (res != ERROR_SUCCESS)
    {
        return false;
    }

    return true;
}

}
}
}

#endif
