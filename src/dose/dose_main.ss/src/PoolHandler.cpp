/******************************************************************************
*
* Copyright Consoden AB, 2015 (http://www.consoden.se)
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
#include <boost/make_shared.hpp>
#include <Safir/Utilities/Internal/Id.h>
#include <Safir/Dob/Typesystem/Operations.h>
#include <Safir/Dob/Service.h>
#include <Safir/Dob/Entity.h>
#include <Safir/Dob/Internal/DistributionData.h>
#include <Safir/Dob/Internal/StateDeleter.h>
#include <Safir/Dob/Internal/Connections.h>
#include <Safir/Dob/Internal/ServiceTypes.h>
#include <Safir/Dob/Internal/EntityTypes.h>

#include "PoolHandler.h"

namespace Safir
{
namespace Dob
{
namespace Internal
{
    PoolHandler::PoolHandler(boost::asio::io_service::strand& strand,
                             Distribution& distribution,
                             const std::function<void(int64_t)>& checkPendingReg)
        :m_strand(strand)
        ,m_communication(distribution.GetCommunication())
    {
        auto injectNode=[=](const std::string&, int64_t id, int64_t nt, const std::string&)
        {
            m_strand.dispatch([=]{m_nodes.insert({id, nt});});
        };

        auto excludeNode=[=](int64_t id, int64_t)
        {
            m_strand.dispatch([=]
            {
                if (m_nodes.erase(id)>0)
                {
                    if (m_poolDistributionRequests)
                    {
                        m_poolDistributionRequests->ReceivedPoolDistributionCompleteFrom(id);
                    }

                    //TODO: cancel ongoing pool distributions to this node
                }
            });
        };

        //subscribe for included and excluded nodes
        distribution.SubscribeNodeEvents(injectNode, excludeNode);

        //set data receiver for pool distribution information
        m_communication.SetDataReceiver([=](int64_t fromNodeId, int64_t fromNodeType, const char *data, size_t size)
                                        {
                                            OnPoolDistributionInfo(fromNodeId, fromNodeType, data, size);
                                        },
                                        PoolDistributionInfoDataTypeId,
                                        [=](size_t s){return new char[s];});

        //set data receiver for registration states
        m_communication.SetDataReceiver([=](int64_t fromNodeId, int64_t fromNodeType, const char *data, size_t size)
                                        {
                                            OnRegistrationState(fromNodeId, fromNodeType, data, size);
                                        },
                                        RegistrationStateDataTypeId,
                                        [=](size_t s){return DistributionData::NewData(s);});

        //set data receiver for entity states
        m_communication.SetDataReceiver([=](int64_t fromNodeId, int64_t fromNodeType, const char *data, size_t size)
                                        {
                                            OnEntityState(fromNodeId, fromNodeType, data, size);
                                        },
                                        EntityStateDataTypeId,
                                        [=](size_t s){return DistributionData::NewData(s);});

        //create one StateDistributor per nodeType
        for (auto& nt : distribution.GetNodeTypeConfiguration().nodeTypesParam)
        {
            auto sd=std::unique_ptr<StateDistributor<Com::Communication> >(new StateDistributor<Com::Communication>(nt.id, m_communication, m_strand, checkPendingReg));
            m_stateDistributors.emplace(nt.id, std::move(sd));
        }
    }

    void PoolHandler::Start(const std::function<void()>& poolDistributionComplete)
    {
        m_strand.dispatch([=]
        {
            //request pool distributions
            m_poolDistributionRequests=std::unique_ptr<PoolDistributionRequestor<Com::Communication> >(
                        new PoolDistributionRequestor<Com::Communication>(m_strand, m_communication, m_nodes,
                                                    [=]{poolDistributionComplete();
                                                    m_strand.post([=]{m_poolDistributionRequests.reset();});}));

            //start distributing states to other nodes
            for (auto& vt : m_stateDistributors)
            {
                vt.second->Start();
            }
        });
    }

    void PoolHandler::Stop()
    {
        m_strand.dispatch([=]
        {
            //We are stopping so we dont care about receiving pool distributions anymore
            if (m_poolDistributionRequests)
            {
                for (auto& vt : m_nodes)
                {
                    m_poolDistributionRequests->ReceivedPoolDistributionCompleteFrom(vt.first);
                }
            }

            //TODO: stop poolDistrbutions

            //stop distributing states to others
            for (auto& vt : m_stateDistributors)
            {
                vt.second->Stop();
            }
        });
    }

    void PoolHandler::OnPoolDistributionInfo(int64_t fromNodeId, int64_t /*fromNodeType*/, const char *data, size_t /*size*/)
    {
        const PoolDistributionInfo pdInfo=*reinterpret_cast<const PoolDistributionInfo*>(data);
        delete[] data;

        m_strand.dispatch([=]
        {
            switch (pdInfo)
            {
            case PoolDistributionInfo::RequestPd:
            {
                //start new pool distribution to node

            }
                break;

            case PoolDistributionInfo::PdComplete:
            {
                if (m_poolDistributionRequests)
                {
                    m_poolDistributionRequests->ReceivedPoolDistributionCompleteFrom(fromNodeId);
                }
            }

            default:
                break;
            }
        });
    }

    void PoolHandler::OnRegistrationState(int64_t /*fromNodeId*/, int64_t /*fromNodeType*/, const char* data, size_t /*size*/)
    {
        const auto state=DistributionData::ConstConstructor(new_data_tag, data);
        DistributionData::DropReference(data);

        if (state.GetType()!=DistributionData::RegistrationState)
        {
            throw std::logic_error("PoolHandler::OnRegistrationState received DistributionData that is not a RegistrationState");
        }

        if (state.IsRegistered()) //is a registration state
        {
            const ConnectionId senderId=state.GetSenderId();

            ENSURE(senderId.m_id != -1, << "Registration states are expected to have ConnectionId != -1! Reg for type "
                   << Safir::Dob::Typesystem::Operations::GetName(state.GetTypeId()));

            const ConnectionPtr connection = Connections::Instance().GetConnection(senderId, std::nothrow);

            if (connection!=nullptr)
            {
                if (Typesystem::Operations::IsOfType(state.GetTypeId(),Safir::Dob::Service::ClassTypeId))
                {
                    ServiceTypes::Instance().RemoteSetRegistrationState(connection,state);
                }
                else
                {
                    EntityTypes::Instance().RemoteSetRegistrationState(connection,state);
                }
            }
        }
        else //is an unregistration state
        {
            ENSURE(state.GetSenderId().m_id == -1, << "Unregistration states are expected to have ConnectionId == -1! Unreg for type "
                   << Safir::Dob::Typesystem::Operations::GetName(state.GetTypeId()));

            //unregistration states bypass all waitingstates handling and go straight
            //into shared memory
            if (Typesystem::Operations::IsOfType(state.GetTypeId(),Safir::Dob::Service::ClassTypeId))
            {
                ServiceTypes::Instance().RemoteSetRegistrationState(ConnectionPtr(), state);
            }
            else
            {
                EntityTypes::Instance().RemoteSetRegistrationState(ConnectionPtr(), state);
            }

            //TODO
            //m_pendingRegistrationHandler->CheckForPending(state.GetTypeId());
        }

    }

    void PoolHandler::OnEntityState(int64_t /*fromNodeId*/, int64_t /*fromNodeType*/, const char* data, size_t /*size*/)
    {
        const auto state=DistributionData::ConstConstructor(new_data_tag, data);
        DistributionData::DropReference(data);

        if (state.GetType()!=DistributionData::EntityState)
        {
            throw std::logic_error("PoolHandler::OnEntityState received DistributionData that is not a EntityState");
        }

        switch (state.GetEntityStateKind())
        {
        case DistributionData::Ghost:
        {
            ENSURE(state.GetSenderId().m_id == -1, << "Ghost states are expected to have ConnectionId == -1! Ghost for "
                   << state.GetEntityId());

            EntityTypes::Instance().RemoteSetRealEntityState(ConnectionPtr(), // Null connection for ghosts
                                                             state);
        }
            break;
        case DistributionData::Injection:
        {
            ENSURE(state.GetSenderId().m_id == -1, << "Injection states are expected to have ConnectionId == -1! Injection for "
                   << state.GetEntityId());
            EntityTypes::Instance().RemoteSetInjectionEntityState(state);
        }
            break;
        case DistributionData::Real:
        {
            if (state.IsCreated()) //handle created and delete states differently
            {
                const ConnectionId senderId=state.GetSenderId();

                ENSURE(senderId.m_id != -1, << "Entity states are expected to have ConnectionId != -1! State for "
                       << state.GetEntityId());

                const ConnectionPtr connection = Connections::Instance().GetConnection(senderId, std::nothrow);
                if (connection==nullptr)
                {
                    throw std::logic_error("Received acked entity with an unknown connection");
                }
                else
                {
                    EntityTypes::Instance().RemoteSetRealEntityState(connection, state);
                }
            }
            else
            {
                ENSURE(state.GetSenderId().m_id == -1, << "Delete states are expected to have ConnectionId == -1! Delete for "
                       << state.GetEntityId());

                EntityTypes::Instance().RemoteSetDeleteEntityState(state);
            }
        }
            break;
        }
    }
}
}
}
