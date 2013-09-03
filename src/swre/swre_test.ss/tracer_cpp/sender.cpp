/******************************************************************************
*
* Copyright Saab AB, 2011 (http://www.safirsdk.com)
*
* Created by: Lars Hagstrom
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
#include <Safir/Application/Tracer.h>
#include <Safir/Dob/Typesystem/Utilities.h>
#include <boost/thread.hpp>
#include <iostream>

int main(int /*argc*/, char* argv[])
{
    Safir::Application::Tracer debug(L"test");
    debug.Enable(true);
    debug << "blahonga" << std::endl;
    debug << "blahonga" << std::endl;
    debug << "blahonga" << std::endl;

    boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    debug << "blahonga" << std::endl;
    boost::this_thread::sleep(boost::posix_time::seconds(2));
    debug << "blahonga" << std::endl;

    return 0;
}


