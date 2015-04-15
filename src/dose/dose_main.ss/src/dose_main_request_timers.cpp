/******************************************************************************
*
* Copyright Saab AB, 2007-2013 (http://safir.sourceforge.net)
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

#if 0 //stewart
#include "dose_main_request_timers.h"

namespace Safir
{
namespace Dob
{
namespace Internal
{
    TimerId RequestTimers::m_localReqTimerId;
    TimerId RequestTimers::m_externalReqTimerId;

    bool RequestTimerInfo::operator<(const RequestTimerInfo& rhs) const
    {
        if (m_connectionIdentifier < rhs.m_connectionIdentifier)
        {
            return true;
        }
        else if (rhs.m_connectionIdentifier < m_connectionIdentifier)
        {
            return false;
        }

        if (m_requestId.GetCounter() < rhs.m_requestId.GetCounter())
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}
}
}
#endif
