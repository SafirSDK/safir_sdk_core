/******************************************************************************
*
* Copyright Saab AB, 2014 (http://safir.sourceforge.net)
*
* Created by: Joel Ottosson / joel.ottosson@consoden.se
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
#include "NodeType.h"
#include "Utilities.h"

namespace Safir
{
namespace Dob
{
namespace Internal
{
namespace Com
{
    NodeType::NodeType(boost::int64_t id, const std::string &name, const std::string &multicastAddr, int heartbeatInterval, int retryTimeout)
        :m_id(id)
        ,m_name(name)
        ,m_multicastAddress(multicastAddr)
        ,m_heartbeatInterval(heartbeatInterval)
        ,m_retryTimeout(retryTimeout)
        ,m_multicastEndpoint()
    {
        if (IsMulticastEnabled())
        {
            m_multicastEndpoint=Utilities::CreateEndpoint(multicastAddr);
        }
    }
}
}
}
}
