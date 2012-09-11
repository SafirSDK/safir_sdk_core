/******************************************************************************
*
* Copyright Saab Systems AB, 2012 (http://www.safirsdk.com)
*
* Created by: Lars Hagstr�m / lars@foldspace.nu
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
#include <Safir/Utilities/CrashReporter.h>
#include <iostream>
void callback(const char* const dumpPath)
{
    std::wcout << "callback with dumpPath = '" << dumpPath << "'" << std::endl;
}


int main()
{
    Safir::Utilities::CrashReporter::Start();
    Safir::Utilities::CrashReporter::RegisterCallback(callback);
    
    int* foo = NULL;
    *foo = 10;
    Safir::Utilities::CrashReporter::Stop();
    return 0;
}


