/******************************************************************************
*
* Copyright Saab AB, 2007-2013,2015 (http://safir.sourceforge.net)
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
#pragma once

#include "Distribution.h"
#include "Node.h"
#include <Safir/Dob/Internal/InternalFwd.h>
#include <boost/noncopyable.hpp>

namespace Safir
{
namespace Dob
{
namespace Internal
{

    // Forward declarations
    namespace Com
    {
        class Communication;
    }

    class MessageHandler:
        private boost::noncopyable
    {
    public:
        explicit MessageHandler(Distribution& distribution);

        void DistributeMessages(const ConnectionPtr& connection);

    private:
        void DispatchMessage(size_t&                    numberDispatched,
                             const ConnectionPtr&       connection,
                             const DistributionData&    msg,
                             bool&                      exitDispatch,
                             bool&                      dontRemove);

        void TraverseMessageQueue(const ConnectionPtr& connection);

        void Send(const DistributionData& msg);

        Distribution&      m_distribution;
        const int64_t      m_dataTypeIdentifier;
    };
}
}
}
