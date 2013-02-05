/******************************************************************************
*
* Copyright Saab AB, 2009 (http://www.safirsdk.com)
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

#include "dots_exception_description.h"

namespace Safir
{
namespace Dob
{
namespace Typesystem
{
namespace Internal
{
    ExceptionDescription::ExceptionDescription(const std::string & name,
                                               const TypeId typeId,
                                               const ExceptionDescriptionConstPtr baseClass,
                                               AllocationHelper & allocHelper):
        m_name(name.begin(),name.end(),allocHelper.GetAllocator<char>()),
        m_typeId(typeId),
        m_baseClass(baseClass)
    {
        
    }

    ExceptionDescription::~ExceptionDescription()
    {

    }

}
}
}
}

