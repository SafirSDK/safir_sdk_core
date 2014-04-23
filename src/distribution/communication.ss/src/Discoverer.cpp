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
#include <Safir/Utilities/Internal/Id.h>
#include "Discoverer.h"
#include "Message.h"

namespace Safir
{
namespace Dob
{
namespace Internal
{
namespace Com
{
    static const int DiscovererLogLevel=9;

#ifdef _MSC_VER
#pragma warning (disable: 4355)
#endif
    Discoverer::Discoverer(const boost::shared_ptr<boost::asio::io_service>& ioService,
                           const Node& me,
                           const boost::function<void(const UserDataPtr&, const boost::asio::ip::udp::endpoint&)>& sendTo,
                           const boost::function<void(const Node&)>& onNewNode)
        :m_seeds()
        ,m_nodes()
        ,m_reportedNodes()
        ,m_incompletedNodes()
        ,m_strand(*ioService)
        ,m_myNodeId(me.Id())
        ,m_myNodeName(me.Name())
        ,m_myAddress(me.Address())
        ,m_myNodeTypeId(me.NodeTypeId())
        ,m_sendTo(sendTo)
        ,m_onNewNode(onNewNode)
        ,m_timer(*ioService)
        ,m_randomGenerator(static_cast<boost::uint32_t>(me.Id()))
    {
    }
#ifdef _MSC_VER
#pragma warning (default: 4355)
#endif

    void Discoverer::Start()
    {
        m_strand.dispatch([=]
        {
            m_running=true;
            m_timer.expires_from_now(boost::chrono::milliseconds(Random(0, 1000)));
            m_timer.async_wait(m_strand.wrap([=](const boost::system::error_code& error){OnTimeout(error);}));
        });
    }

    void Discoverer::Stop()
    {
        m_strand.dispatch([=]
        {
            m_running=false;
            m_timer.cancel();
        });
    }

    void Discoverer::AddSeed(const std::string& seed)
    {
        if (seed!=m_myAddress)
        {
            boost::uint64_t id=LlufId_Generate64(seed.c_str());
            NodeInfo s(id, 0, "seed", seed);
            this->m_seeds.insert(std::make_pair(id, s));
            lllog(DiscovererLogLevel)<<L"COM: Add seed "<<seed.c_str()<<std::endl;
        }
        //else trying to seed myself

    }

    void Discoverer::AddSeeds(const std::vector<std::string>& seeds)
    {
        m_strand.dispatch([=]
        {
            for (auto& seed : seeds)
            {
                AddSeed(seed);
            }
        });
    }

    void Discoverer::AddNewNode(const CommunicationMessage_Node& node)
    {
        lllog(4)<<L"COM: Discoverer talked to new node "<<node.name().c_str()<<L" ["<<node.node_id()<<L"]"<<std::endl;
        //insert in node map
        NodeInfo ni(node.node_id(), node.node_type_id(), node.name(), node.unicast_address());
        m_nodes.insert(std::make_pair(ni.nodeId, ni));

        m_reportedNodes.erase(ni.nodeId); //now when we actually have talked to the node we also remove from reported nodes

        //notify listener
        Node n( node.name(), node.node_id(), node.node_type_id(), node.unicast_address());
        m_onNewNode(n);
    }

    void Discoverer::AddReportedNode(const CommunicationMessage_Node& node)
    {
        lllog(DiscovererLogLevel)<<L"COM: Got report about node "<<node.name().c_str()<<L" ["<<node.node_id()<<L"]"<<std::endl;

        //insert in reported node map
        NodeInfo ni(node.node_id(), node.node_type_id(), node.name(), node.unicast_address());
        m_reportedNodes.insert(std::make_pair(ni.nodeId, ni));
    }

    //incomplete nodes are nodes we have heard from but still haven't got all the nodeInfo messages from
    bool Discoverer::UpdateIncompleteNodes(boost::int64_t id, size_t numberOfPackets, size_t packetNumber)
    {
        if (numberOfPackets==1 && packetNumber==0)
        {
            //this is the most common case, all of the nodes nodeInfo fits in one packet
            m_incompletedNodes.erase(id); //normally it shoulnt be in incomplete list, but it can happen if number of nodes decreased and now fits in one packet
            lllog(DiscovererLogLevel)<<L"COM: All nodeInfo from "<<id<<L" have been received"<<std::endl;
            return true;
        }

        auto it=m_incompletedNodes.find(id);
        if (it!=m_incompletedNodes.end()) //have we have talked to this node before
        {
            std::vector<bool>& v=it->second;
            if (v.empty())
            {
                if (numberOfPackets>0)
                {
                    //this is first time we receive nodeInfo from this node. Previously we had just received a Discover
                    v.resize(numberOfPackets, false);
                }
                else
                {
                    //if v is empty we still havent got any nodeInfo at all and is not completed. The case when we just receives Discover but no NodeInfo
                    lllog(DiscovererLogLevel)<<L"COM: We haven't got all nodeInfo yet from "<<id<<std::endl;
                    return false;
                }
            }

            if (packetNumber<v.size())
            {
                //packet is within range, set as received
                v[packetNumber]=true;
            }

            if (numberOfPackets<v.size())
            {
                //number of packets has decreased. We handle this by setting out-of-range packets to received(=true)
                for (size_t i=numberOfPackets; i<v.size(); ++i)
                {
                    v[i]=true;
                }
            }

            for (auto valIt=v.begin(); valIt!=v.end(); ++valIt)
            {
                if (!(*valIt))
                {
                    lllog(DiscovererLogLevel)<<L"COM: We still haven't got all nodeInfo from "<<id<<std::endl;
                    return false; //not completed yet since there are packets we havent got
                }
            }

            //we get here if all have been received and node is completed
            m_incompletedNodes.erase(id);
            lllog(DiscovererLogLevel)<<L"COM: All nodeInfo from "<<id<<L" have been received"<<std::endl;
            return true;
        }
        else //we have never talked to this node before
        {
            //fist time we talk to this node, the case when its immediately completed is handled first in this method (numberOfPackets=1 and packetNumber=0)
            //so if we get here we just received a Discover or a we still havent received all nodeInfo since it didn't fit in one packet
            std::vector<bool> v(numberOfPackets, false);
            if (packetNumber<v.size())
            {
                v[packetNumber]=true;
            }
            m_incompletedNodes.insert(std::make_pair(id, v));
            return false; //not completed
        }
    }

    void Discoverer::HandleReceivedDiscover(const CommunicationMessage_Discover& msg)
    {
        lllog(DiscovererLogLevel)<<L"COM: Received discover from "<<msg.from().name().c_str()<<" ["<<msg.from().node_id()<<"]"<<std::endl;
        m_strand.dispatch([=]
        {
            if (!m_running)
            {
                return;
            }

            if (msg.from().node_id()==m_myNodeId)
            {
                //discover from myself, remove seed
                lllog(DiscovererLogLevel)<<L"COM: Received discover from myself. Remove from seed list"<<std::endl;
                m_seeds.erase(msg.sent_to_id());
                return;
            }

            if (IsNewNode(msg.from().node_id()))
            {
                //new node
                AddNewNode(msg.from());
                UpdateIncompleteNodes(msg.from().node_id(), 0, 0);
            }

            SendNodeInfo(msg.from().node_id(), msg.sent_to_id(), Utilities::CreateEndpoint(msg.from().unicast_address()));
        });
    }

    void Discoverer::HandleReceivedNodeInfo(const CommunicationMessage_NodeInfo& msg)
    {
        lllog(DiscovererLogLevel)<<L"COM: Received node info from "<<(msg.has_sent_from_node() ? msg.sent_from_node().name().c_str() : "<NotPresent>")<<", numNodes="<<msg.nodes().size()<<std::endl;

        m_strand.dispatch([=]
        {
            if (!m_running)
            {
                return;
            }

            //handle info about the sender
            if (msg.has_sent_from_node())
            {
                //we have now talked to this node, so it can be added as a real node and removed from seed and reported lists
                m_seeds.erase(msg.sent_from_id()); //must use sentFromId and not sent_from_node().id since sentFromId is generated from address when seeding.
                m_reportedNodes.erase(msg.sent_from_node().node_id());

                if (IsNewNode(msg.sent_from_node().node_id()))
                {
                    AddNewNode(msg.sent_from_node());
                }

                UpdateIncompleteNodes(msg.sent_from_node().node_id(), msg.number_of_packets(), msg.packet_number());
            }

            //Add the rest to reportedNodes if not already a known node
            for (auto nit=msg.nodes().begin(); nit!=msg.nodes().end(); ++nit)
            {
                if (nit->node_id()!=m_myNodeId)
                {
                    if (nit->node_id()==0 && nit->name()=="seed")
                    {
                        lllog(DiscovererLogLevel)<<L"COM: Received seed from other node. Address to seed: "<<nit->unicast_address().c_str()<<std::endl;
                        AddSeed(nit->unicast_address());
                    }
                    else if (IsNewNode(nit->node_id()))
                    {
                        lllog(DiscovererLogLevel)<<L"COM: Reported node: "<<nit->name().c_str()<<L" ["<<nit->node_id()<<L"]"<<std::endl;
                        AddReportedNode(*nit);
                        //TODO: If not very busy, we can send discover right away instead of waiting for timer.
                    }
                }
            }
        });
    }

    void Discoverer::SendDiscover()
    {
        //Compose a DiscoverMessage
        CommunicationMessage cm;
        cm.set_message_type(CommunicationMessage_MessageType_DiscoverMsg);

        cm.mutable_discover()->mutable_from()->set_node_id(m_myNodeId);
        cm.mutable_discover()->mutable_from()->set_name(m_myNodeName);
        cm.mutable_discover()->mutable_from()->set_unicast_address(m_myAddress);
        cm.mutable_discover()->mutable_from()->set_node_type_id(m_myNodeTypeId);

        for (auto it=m_seeds.cbegin(); it!=m_seeds.cend(); ++it)
        {
            cm.mutable_discover()->set_sent_to_id(it->first);
            lllog(DiscovererLogLevel)<<L"Send discover to seed: "<<it->second.unicastAddress.c_str()<<std::endl;
            SendMessageTo(cm, Utilities::CreateEndpoint(it->second.unicastAddress));
        }

        for (auto it=m_reportedNodes.cbegin(); it!=m_reportedNodes.cend(); ++it)
        {
            cm.mutable_discover()->set_sent_to_id(it->first);
            lllog(DiscovererLogLevel)<<L"Send discover to node I've heard about: "<<it->second.name.c_str()<<L", id="<<it->second.nodeId<<std::endl;
            SendMessageTo(cm, Utilities::CreateEndpoint(it->second.unicastAddress));
        }

        for (auto it=m_incompletedNodes.cbegin(); it!=m_incompletedNodes.cend(); ++it)
        {
            const auto nodeIt=m_nodes.find(it->first);
            assert(nodeIt!=m_nodes.cend());
            const NodeInfo& node=nodeIt->second;
            cm.mutable_discover()->set_sent_to_id(node.nodeId);
            lllog(DiscovererLogLevel)<<L"Resend discover to node I havent got all nodeInfo from: "<<node.name.c_str()<<L", id="<<node.nodeId<<std::endl;
            SendMessageTo(cm, Utilities::CreateEndpoint(node.unicastAddress));
        }
    }

    void Discoverer::SendNodeInfo(boost::int64_t toId, boost::int64_t fromId, const boost::asio::ip::udp::endpoint& toEndpoint)
    {
        //This method must always be called from within writeStrand
        const int totalNumberOfNodes=static_cast<int>(m_seeds.size()+m_nodes.size());
        const int numberOfPackets=totalNumberOfNodes/NumberOfNodesPerNodeInfoMsg+(totalNumberOfNodes%NumberOfNodesPerNodeInfoMsg>0 ? 1 : 0);

        //Compose a DiscoverMessage
        CommunicationMessage cm;
        cm.set_message_type(CommunicationMessage_MessageType_NodeInfoMsg);

        cm.mutable_node_info()->set_sent_from_id(fromId);
        cm.mutable_node_info()->set_sent_to_id(toId);
        cm.mutable_node_info()->set_number_of_packets(numberOfPackets);

        //Add myself
        CommunicationMessage_Node* me=cm.mutable_node_info()->mutable_sent_from_node();
        me->set_name(m_myNodeName);
        me->set_node_id(m_myNodeId);
        me->set_unicast_address(m_myAddress);
        me->set_node_type_id(m_myNodeTypeId);

        int packetNumber=0;
        //Add seeds
        for (auto seedIt=m_seeds.cbegin(); seedIt!=m_seeds.cend(); ++seedIt)
        {
            CommunicationMessage_Node* ptr=cm.mutable_node_info()->mutable_nodes()->Add();
            ptr->set_name(seedIt->second.name);
            ptr->set_node_id(0);
            ptr->set_unicast_address(seedIt->second.unicastAddress);
            if (cm.node_info().nodes().size()==NumberOfNodesPerNodeInfoMsg)
            {
                cm.mutable_node_info()->set_packet_number(packetNumber);
                SendMessageTo(cm, toEndpoint);
                cm.mutable_node_info()->mutable_nodes()->Clear(); //clear nodes and continue fill up the same message
                ++packetNumber;
            }
        }

        //Add all other nodes we have talked to. We dont send nodes that only have been reported, i.e we dont spread rumors
        for (auto nodeIt=m_nodes.cbegin(); nodeIt!=m_nodes.cend(); ++nodeIt)
        {
            const Discoverer::NodeInfo& node=nodeIt->second;
            CommunicationMessage_Node* ptr=cm.mutable_node_info()->mutable_nodes()->Add();
            ptr->set_name(node.name);
            ptr->set_node_id(node.nodeId);
            ptr->set_node_type_id(node.nodeTypeId);
            ptr->set_unicast_address(node.unicastAddress);

            if (cm.node_info().nodes().size()==NumberOfNodesPerNodeInfoMsg)
            {
                cm.mutable_node_info()->set_packet_number(packetNumber);
                SendMessageTo(cm, toEndpoint);
                cm.mutable_node_info()->mutable_nodes()->Clear(); //clear nodes and continue fill up the same message
                ++packetNumber;
            }
        }

        if (cm.node_info().nodes().size()>0)
        {
            cm.mutable_node_info()->set_packet_number(packetNumber);
            SendMessageTo(cm, toEndpoint);
            ++packetNumber;
        }

        assert(packetNumber==numberOfPackets);

        lllog(DiscovererLogLevel)<<"Send "<<packetNumber<<" NodeInfo messages to id="<<toId<<std::endl;
    }

    void Discoverer::SendMessageTo(const CommunicationMessage& cm, const boost::asio::ip::udp::endpoint& toEndpoint)
    {
        int size=cm.ByteSize();
        boost::shared_ptr<char[]> payload(new char[size]);
        google::protobuf::uint8* buf=reinterpret_cast<google::protobuf::uint8*>(const_cast<char*>(payload.get()));
        cm.SerializeWithCachedSizesToArray(buf);
        UserDataPtr ud(new UserData(m_myNodeId, ControlDataType, payload, static_cast<size_t>(size)));
        m_sendTo(ud, toEndpoint);
    }

    void Discoverer::OnTimeout(const boost::system::error_code& error)
    {
        if (!m_running)
        {
            return;
        }

        if (!error)
        {
            SendDiscover();

            //restart timer
            m_timer.expires_from_now(boost::chrono::milliseconds(Random(500, 3000)));
            m_timer.async_wait(m_strand.wrap([=](const boost::system::error_code& error){OnTimeout(error);}));
        }
    }
}
}
}
}
