/******************************************************************************
*
* Copyright Saab AB, 2012-2013 (http://safir.sourceforge.net)
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
#ifndef __DIAGNOSTICS_H__
#define __DIAGNOSTICS_H__
#include "Safir/Databases/Odbc/Defs.h"
#include "Safir/Databases/Odbc/Internal/InternalDefs.h"
#include <string>
#include <utility>

namespace Safir
{
namespace Databases
{
namespace Odbc
{

typedef std::pair<std::wstring,std::wstring> StateMessagePair;

std::pair<std::wstring,std::wstring> GetDiagRec(const SQLSMALLINT handleType,
                                                const SQLHANDLE handle);
    

}
}
}

#endif

