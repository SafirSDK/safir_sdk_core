/******************************************************************************
*
* Copyright Saab AB, 2013 (http://safir.sourceforge.net)
*
* Created by: Lars Hagström / lars.hagstrom@consoden.se
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
#ifndef __STATE_PUBLISHER_REMOTE_H__
#define __STATE_PUBLISHER_REMOTE_H__

#include <Safir/Dob/Internal/Communication.h>
#include <Safir/Utilities/Internal/Id.h>
#include "Coordinator.h"

namespace Safir
{
namespace Dob
{
namespace Internal
{
namespace SP
{
    using Safir::Utilities::Internal::AsioPeriodicTimer;

    class StatePublisherRemote
    {
    public:
        StatePublisherRemote(const boost::shared_ptr<boost::asio::io_service>& ioService,
                             const boost::shared_ptr<Com::Communication>& communication,
                             const std::map<boost::int64_t, NodeType>& nodeTypes,
                             const char* const senderId,
                             const boost::shared_ptr<Coordinator>& coordinator)
            : m_communication(communication)
            , m_senderId(LlufId_Generate64(senderId))
            , m_nodeTypes(nodeTypes)
            , m_coordinator(coordinator)
            , m_publishTimer(AsioPeriodicTimer::Create(*ioService, 
                                                       boost::chrono::seconds(1),
                                                       [this](const boost::system::error_code& error)
                                                       {
                                                           Publish(error);
                                                       }))
        {
            m_publishTimer->Start();
        }

        void Stop()
        {
            m_publishTimer->Stop();
        }

    private:
        void Publish(const boost::system::error_code& error)
        {
            if (error)
            {
                SEND_SYSTEM_LOG(Alert,
                                << "Unexpected error in StatePublisherRemote::Publish: " << error);
                throw std::logic_error("Unexpected error in StatePublisherRemote::Publish");
            }

            //Only publish if we're elected
            if (!m_coordinator->IsElected())
            {
                return;
            }

            lllog(8) << "Publishing state statistics to other nodes" << std::endl;
            
#ifdef CHECK_CRC
            const int crcBytes = sizeof(int);
#else
            const int crcBytes = 0;
#endif
            
            m_coordinator->PerformOnStateMessage([this,crcBytes](const boost::shared_ptr<char[]>& data, const size_t size)
                                                 {
#ifdef CHECK_CRC
                                                     const int crc = GetCrc32(data.get(), size - crcBytes);
                                                     memcpy(data.get() + size - crcBytes, &crc, sizeof(int));
#endif

                                                     for (auto it: m_nodeTypes)
                                                     {
                                                         m_communication->SendToNodeType(it.second.id, data, size, m_senderId);
                                                     }
                                                 },
                                                 crcBytes);
        }

        const boost::shared_ptr<Com::Communication> m_communication;
        const boost::uint64_t m_senderId;
        const std::map<boost::int64_t, NodeType> m_nodeTypes;
        const boost::shared_ptr<Coordinator> m_coordinator;
        boost::shared_ptr<Safir::Utilities::Internal::AsioPeriodicTimer> m_publishTimer;
    };
}
}
}
}

#endif

