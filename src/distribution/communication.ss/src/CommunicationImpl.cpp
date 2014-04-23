/******************************************************************************
*
* Copyright Saab AB, 2013 (http://safir.sourceforge.net)
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
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include <Safir/Utilities/Internal/Id.h>
#include <Safir/Utilities/Internal/LowLevelLogger.h>
#include "Parameters.h"
#include "CommunicationImpl.h"
#include "Message.h"

namespace Safir
{
namespace Dob
{
namespace Internal
{
namespace Com
{
#ifdef _MSC_VER
#pragma warning (disable: 4355)
#endif
    CommunicationImpl::CommunicationImpl(const boost::shared_ptr<boost::asio::io_service>& ioService,
                                         const std::string& nodeName,
                                         boost::int64_t nodeId, //0 is not a valid id.
                                         boost::int64_t& nodeTypeId,
                                         const std::string& address,
                                         bool discovering,
                                         const NodeTypeMap& nodeTypes)
        :m_disableProtobufLogs()
        ,m_ioService(ioService)
        ,m_me(nodeName, nodeId, nodeTypeId, address)
        ,m_discovering(discovering)
        ,m_nodeTypes(nodeTypes)
        ,m_onNewNode()
        ,m_gotRecv()
        ,m_discoverer(m_ioService, m_me, [=](const Node& n){OnNewNode(n);})
        ,m_deliveryHandler(m_ioService, m_me)
        ,m_reader(ioService, m_me, m_nodeTypes[nodeTypeId]->MulticastAddress(),
                    [=](const char* d, size_t s){return OnRecv(d,s);},
                    [=](){return m_deliveryHandler.NumberOfUndeliveredMessages()<Parameters::MaxNumberOfUndelivered;})
    {
        if (nodeId==0)
        {
            throw std::invalid_argument("Safir.Communication ctor: Id=0 is reserved for internal usage and is not valid. You should consider using a random generated id.");
        }
        auto myNodeType=m_nodeTypes[nodeTypeId];
        lllog(2)<<L"COM: -------------------------------------------------"<<std::endl;
        lllog(2)<<L"COM: Communication started"<<std::endl;
        lllog(2)<<L"COM:     id:    "<<m_me.Id()<<std::endl;
        lllog(2)<<L"COM:     name:    "<<m_me.Name().c_str()<<std::endl;
        lllog(2)<<L"COM:     unicast: "<<m_me.Address().c_str()<<std::endl;
        lllog(2)<<L"COM:     multicast: "<<myNodeType->MulticastAddress().c_str()<<std::endl;
        lllog(2)<<L"COM:     using multicast: "<<std::boolalpha<<myNodeType->UseMulticast()<<std::dec<<std::endl;
        lllog(2)<<L"COM: -------------------------------------------------"<<std::endl;
    }
#ifdef _MSC_VER
#pragma warning (default: 4355)
#endif

    CommunicationImpl::~CommunicationImpl()
    {
    }

    void CommunicationImpl::SetNewNodeCallback(const NewNode& callback)
    {
        m_onNewNode=callback;
    }

    void CommunicationImpl::SetGotReceiveFromCallback(const GotReceiveFrom& callback)
    {
        m_reader.Strand().dispatch([=]
        {
            m_gotRecv=callback;
            m_deliveryHandler.SetGotRecvCallback(callback);
        });
    }

    void CommunicationImpl::SetRetransmitToCallback(const RetransmitTo& callback)
    {
        //m_ackedDataSender.SetRetransmitCallback(callback);
    }

    void CommunicationImpl::SetDataReceiver(const ReceiveData& callback, boost::int64_t dataTypeIdentifier)
    {
        m_reader.Strand().post([=]{m_deliveryHandler.SetReceiver(callback, dataTypeIdentifier);});
    }

    void CommunicationImpl::SetQueueNotFullCallback(const QueueNotFull& callback, int freePartThreshold)
    {
        //m_ackedDataSender.SetNotFullCallback(callback, freePartThreshold);
    }

    void CommunicationImpl::Start()
    {
        m_reader.Start();
        //m_heartbeatSender.Start();
        //m_ackedDataSender.Start();
        m_discoverer.Start();
    }

    void CommunicationImpl::Stop()
    {
        m_reader.Stop();
        //m_heartbeatSender.Stop();
        //m_ackedDataSender.Stop();
        m_discoverer.Stop();
    }

    void CommunicationImpl::IncludeNode(boost::int64_t id)
    {
        SetSystemNode(id, true);
    }

    void CommunicationImpl::ExcludeNode(boost::int64_t id)
    {
        SetSystemNode(id, false);
    }

    bool CommunicationImpl::SendToNode(boost::int64_t nodeId, const boost::shared_ptr<char[]>& data, size_t size, boost::int64_t dataTypeIdentifier)
    {
        //TODO
        //find node and send
        //return m_ackedDataSender.AddToSendQueue(toId, msg, size, dataTypeIdentifier);
        return true;
    }

    bool CommunicationImpl::SendToNodeType(boost::int64_t nodeTypeId, const boost::shared_ptr<char[]>& data, size_t size, boost::int64_t dataTypeIdentifier)
    {
        //TODO
        //find node type and send
        //return m_ackedDataSender.AddToSendQueue(0, msg, size, dataTypeIdentifier); //receiverId 0 means every system node
        return true;
    }

    size_t CommunicationImpl::NumberOfQueuedMessages(boost::int64_t nodeTypeId) const
    {
        //TODO
        //find node type
        // return m_ackedDataSender.SendQueueSize();
        return 0;
    }

    void CommunicationImpl::InjectSeeds(const std::vector<std::string>& seeds)
    {
        m_discoverer.AddSeeds(seeds);
    }

    void CommunicationImpl::SetSystemNode(boost::int64_t id, bool isSystemNode)
    {
        //We do post here to be sure the AddNode job will be executed before SetSystemNode. Otherwize we
        //risk losing a node.
        m_reader.Strand().post([=]
        {
            m_deliveryHandler.SetSystemNode(id, isSystemNode);
        });

        //m_ackedDataSender.SetSystemNode(id, isSystemNode);
        //m_heartbeatSender.SetSystemNode(id, isSystemNode);
    }

    void CommunicationImpl::OnNewNode(const Node& node)
    {
        lllog(6)<<L"COM: New node '"<<node.Name().c_str()<<L"' ["<<node.Id()<<L"]"<<std::endl;
        //m_ackedDataSender.AddNode(node);
        //m_heartbeatSender.AddNode(node);
        m_reader.Strand().dispatch([this, node]{m_deliveryHandler.AddNode(node);});

        //callback to host application
        m_onNewNode(node.Name(), node.Id(), node.NodeTypeId(), node.Address());
    }

    //returns true if it is ok to call OnRecv again, false if flooded with received messages
    bool CommunicationImpl::OnRecv(const char* data, size_t size)
    {
        //Always called from readStrand

        if (size<CommonHeaderSize)
        {
            lllog(4)<<L"COM: Received corrupt data"<<std::endl;
            return true; //corrupt message, return true means it is ok to receive another message
        }

        const CommonHeader* commonHeader=reinterpret_cast<const CommonHeader*>(data);
        if (commonHeader->senderId==m_me.Id())
        {
            //Received message from myself, return true means it is ok to receive another message
            return true;
        }

        switch (commonHeader->dataType)
        {
        case HeartbeatType:
        {
            const Node* senderNode=m_deliveryHandler.GetNode(commonHeader->senderId);
            if (senderNode!=nullptr && senderNode->IsSystemNode())
            {
                m_gotRecv(commonHeader->senderId);
                lllog(9)<<"COM: Heartbeat from "<<commonHeader->senderId<<std::endl;
            }
        }
            break;

        case AckType:
        {
            const Node* senderNode=m_deliveryHandler.GetNode(commonHeader->senderId);
            if (senderNode!=nullptr && senderNode->IsSystemNode())
            {
                m_gotRecv(commonHeader->senderId);
                const Ack* ack=reinterpret_cast<const Ack*>(data);
                //m_ackedDataSender.HandleAck(*ack);
            }
        }
            break;

        case ControlDataType:
        {
            if (size<MessageHeaderSize)
            {
                lllog(4)<<L"COM: Received corrupt ControlData"<<std::endl;
                return true; //corrupt message, return true means it is ok to receive another message
            }
            const MessageHeader* msgHeader=reinterpret_cast<const MessageHeader*>(data);
            const char* payload=data+MessageHeaderSize;
            ReceivedControlData(msgHeader, payload);
        }
            break;

        default: //some user defined type
        {
            //Application data
            if (size<MessageHeaderSize)
            {
                lllog(4)<<L"COM: Received corrupt ApplicationData"<<std::endl;
                return true; //corrupt message, return true means it is ok to receive another message
            }
            const MessageHeader* msgHeader=reinterpret_cast<const MessageHeader*>(data);
            const char* payload=data+MessageHeaderSize;
            m_deliveryHandler.ReceivedApplicationData(msgHeader, payload);
        }
            break;
        }

        return m_deliveryHandler.NumberOfUndeliveredMessages()<Parameters::MaxNumberOfUndelivered;
    }

    //Received internal Communication msg that shall not be immediately passed to application, i.e discover, nodeInfo etc.
    void CommunicationImpl::ReceivedControlData(const MessageHeader* header, const char* payload)
    {
        //Always called from readStrand

        //Protobuf message
        CommunicationMessage cm;
        bool parsedOk=cm.ParseFromArray(static_cast<const void*>(payload), static_cast<int>(header->fragmentContentSize));
        if (!parsedOk)
        {
            lllog(4)<<L"COM: Received message with valid header but corrupt data."<<std::endl;
            return;
        }

        switch (cm.message_type())
        {
        case CommunicationMessage_MessageType_NodeInfoMsg:
        {
            m_discoverer.HandleReceivedNodeInfo(cm.node_info());
        }
            break;

        case CommunicationMessage_MessageType_DiscoverMsg:
        {
            m_discoverer.HandleReceivedDiscover(cm.discover());
        }
            break;
        }
    }
}
}
}
}
