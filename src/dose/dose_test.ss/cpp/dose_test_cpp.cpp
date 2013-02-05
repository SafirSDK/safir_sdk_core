/******************************************************************************
*
* Copyright Saab AB, 2006-2008 (http://www.safirsdk.com)
*
* Created by: Lars Hagström / stlrha
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

#include <iostream>
#include "Executor.h"
#include "Logger.h"
#include <Safir/SwReports/SwReport.h>



int main(int argc, char* argv[])
{
    Safir::SwReports::SwReportStarter starter;

    const std::vector<std::string> arguments(&argv[0],&argv[argc]);

    std::wcout << "Starting" << std::endl;
    try
    {
        Executor app(arguments);
        app.Run();
    }
    catch(std::exception & e)
    {
        lout << "Caught std::exception! Contents of exception is:" << std::endl
                   << e.what()<<std::endl;
    }
    catch (...)
    {
        lout << "Caught ... exception!" << std::endl;
    }

    std::wcout << "Exiting" << std::endl;

    return 0;
}

