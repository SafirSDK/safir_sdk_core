/******************************************************************************
*
* Copyright Saab AB, 2013-2022 (http://safirsdkcore.com)
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
#pragma once

#include <set>
#include <functional>
#include <algorithm>
#include <boost/random.hpp>
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <Safir/Utilities/Internal/SystemLog.h>
#include <Safir/Utilities/Internal/LowLevelLogger.h>
#include <Safir/Utilities/Internal/Id.h>
#include "Parameters.h"
#include "Node.h"
#include "Message.h"
#include "Resolver.h"
#include "Writer.h"

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4267)
#endif

#include <boost/asio/steady_timer.hpp>

#ifdef _MSC_VER
#pragma warning (pop)
#endif

namespace Safir
{
namespace Dob
{
namespace Internal
{
namespace Com
{
    template <class WriterType>
    class DiscovererBasic : private WriterType
    {
    public:

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4355)
#endif
        DiscovererBasic(boost::asio::io_context& ioContext,
                        const Node& me,
                        int fragmentSize,
                        const std::set<int64_t>& lightNodeTypes,
                        const std::function<void(const Node&)>& onNewNode)
            :WriterType(ioContext, Resolver::Protocol(me.unicastAddress))
            ,m_running(false)
            ,m_fragmentSize(static_cast<size_t>(fragmentSize))
            ,m_numberOfNodesPerNodeInfoMsg(static_cast<int>((m_fragmentSize-NodeInfoFixedSize)/NodeInfoPerNodeSize))
            ,m_lightNodeTypes(lightNodeTypes)
            ,m_seeds()
            ,m_nodes()
            ,m_reportedNodes()
            ,m_incompletedNodes()
            ,m_excludedNodes()
            ,m_strand(ioContext)
            ,m_me(me)
            ,m_thisNodeIsLightNode(IsLightNode(me.nodeTypeId))
            ,m_onNewNode(onNewNode)
            ,m_timer(ioContext)
            ,m_random(1000, 3000)
            ,m_logPrefix(Parameters::LogPrefix + "Discoverer - ")
        {
        }

#ifdef _MSC_VER
#pragma warning (pop)
#endif

        void Start() SAFIR_GCC_VISIBILITY_BUG_WORKAROUND
        {
            boost::asio::post(m_strand, [this]
            {
                m_running=true;
                m_timer.expires_after(std::chrono::milliseconds(m_random.Get()));
                m_timer.async_wait(boost::asio::bind_executor(m_strand, [this](const boost::system::error_code& error){OnTimeout(error);}));
            });
        }

        void Stop()
        {
            boost::asio::post(m_strand, [this]
            {
                m_running=false;
                m_timer.cancel();

                m_excludedNodes.clear();
                m_reportedNodes.clear();
                m_incompletedNodes.clear();
                m_seeds.clear();
                m_nodes.clear();

            });
        }

        void AddSeeds(const std::vector<std::string>& seeds)
        {
            boost::asio::post(m_strand, [this, seeds]
            {
                for (auto seed = seeds.cbegin(); seed != seeds.cend(); ++seed)
                {
                    AddSeed(*seed);
                }
            });
        }

        void HandleReceivedDiscover(const CommunicationMessage_Discover& msg)
        {
            if (m_thisNodeIsLightNode && IsLightNode(msg.from().node_type_id()))
            {
                std::ostringstream os;
                os << m_logPrefix.c_str() << "Received discover from " << msg.from().name() << " ["<<msg.from().node_id()<< "]. Should not happen since both are light nodes! Might be a configuration error, don't use lightNodes as seeds.";
                lllog(DiscovererLogLevel) << os.str().c_str() << std::endl;
                SEND_SYSTEM_LOG(Error, << os.str().c_str());
                throw std::logic_error(os.str());
            }
            else
            {
                lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Received discover from "<<msg.from().name().c_str()<<L" ["<<msg.from().node_id()<<L"]"<<std::endl;
            }

            boost::asio::post(m_strand, [this, msg]
            {
                if (!m_running)
                {
                    return;
                }

                if (msg.from().node_id()==m_me.nodeId)
                {
                    //discover from myself, remove seed
                    lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Received discover from myself. Remove from seed list"<<std::endl;
                    m_seeds.erase(msg.sent_to_id());
                    return;
                }

                if (IsExcluded(msg.from().node_id()))
                {
                    lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Received discover from node that has been excluded. NodeId: "<<msg.from().node_id()<<std::endl;
                    return;
                }

                if (IsNewNode(msg.from().node_id()))
                {
                    //new node
                    AddNewNode(msg.from(), false);
                    UpdateIncompleteNodes(msg.from().node_id(), 0, 0);  //setting numberOfPackets=0 indicates that we still havent got a nodeInfo, just a discover
                }

                SendNodeInfo(msg.from().node_id(), IsLightNode(msg.from().node_type_id()), msg.sent_to_id(), Resolver::StringToEndpoint(msg.from().control_address()));
            });
        }

        void HandleReceivedNodeInfo(const CommunicationMessage_NodeInfo& msg)
        {
            lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Received node info from "<<(msg.has_sent_from_node() ? msg.sent_from_node().name().c_str() : "<NotPresent>")<<", numNodes="<<msg.nodes().size()<<std::endl;

            boost::asio::post(m_strand, [this, msg]
            {
                if (!m_running)
                {
                    return;
                }

                if (m_thisNodeIsLightNode && IsLightNode(msg.sent_from_node().node_type_id()))
                {
                    std::ostringstream os;
                    os << m_logPrefix.c_str() << "Received NodeInfo from " << msg.sent_from_node().name() << " ["<<msg.sent_from_node().node_id()<< "]. Should not happen since both are light nodes! Might be a configuration error, don't use lightNodes as seeds.";
                    lllog(DiscovererLogLevel) << os.str().c_str() << std::endl;
                    SEND_SYSTEM_LOG(Error, << os.str().c_str());
                    throw std::logic_error(os.str());
                }

                //handle info about the sender
                if (IsExcluded(msg.sent_from_node().node_id()))
                {
                    lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Got nodeInfo from: "<<msg.sent_from_node().name().c_str()<<L" ["<<msg.sent_from_node().node_id()<<L"]. Node is excluded by this node, throw away!"<<std::endl;
                    return;
                }

                //we have now talked to this node, so it can be added as a real node and removed from seed and reported lists
                bool isSeed=m_seeds.erase(msg.sent_from_id())==1; //must use sentFromId and not sent_from_node().id since sentFromId is generated from address when seeding.
                m_reportedNodes.erase(msg.sent_from_node().node_id());

                if (IsNewNode(msg.sent_from_node().node_id()))
                {
                    AddNewNode(msg.sent_from_node(), isSeed);
                }
                else
                {
                    //maybe some other node reported this before we got this message. The mark it as seed in case it will be excluded sometime in future.
                    m_nodes.find(msg.sent_from_node().node_id())->second.isSeed=isSeed;
                }

                UpdateIncompleteNodes(msg.sent_from_node().node_id(), msg.number_of_packets(), msg.packet_number());

                //Add the rest to reportedNodes if not already a known node
                for (auto nit=msg.nodes().begin(); nit!=msg.nodes().end(); ++nit)
                {
                    if (nit->node_id()!=m_me.nodeId)
                    {
                        if (IsExcluded(nit->node_id()))
                        {
                            lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Reported node: "<<nit->name().c_str()<<L" ["<<nit->node_id()<<L"]. Node is excluded by this node, throw away!"<<std::endl;
                        }
                        else if (nit->node_id()==0 && nit->name()=="seed")
                        {
                            if (!IsKnownSeedAddress(nit->control_address()))
                            {
                                lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Received seed from other node that we did not already have as seed. Address to seed: "<<nit->control_address().c_str()<<std::endl;
                                AddSeed(nit->control_address());
                            }
                        }
                        else if (IsNewNode(nit->node_id()))
                        {
                            lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Reported node: "<<nit->name().c_str()<<L" ["<<nit->node_id()<<L"]"<<std::endl;
                            AddReportedNode(*nit);
                        }
                    }
                }
            });
        }

        void ExcludeNode(int64_t nodeId)
        {
            boost::asio::post(m_strand, [this, nodeId]
            {
                m_reportedNodes.erase(nodeId);
                m_incompletedNodes.erase(nodeId);

                auto it=m_nodes.find(nodeId);
                if (it!=m_nodes.end())
                {
                    if (m_thisNodeIsLightNode) // I am a lightNode
                    {
                        // Exclude other (always non-lightnodes) for a limited time.
                        ExcludeUntil timeLimit = std::chrono::steady_clock::now() + std::chrono::seconds(m_lightNodesExcludeTimeLimit);
                        m_excludedNodes.insert({nodeId, std::make_pair(timeLimit, it->second.isSeed ? it->second.controlAddress : "")});
                        lllog(DiscovererLogLevel) << m_logPrefix.c_str() << L"Exclude node for " << m_lightNodesExcludeTimeLimit << L" seconds, nodeId: " << nodeId <<std::endl;
                    }
                    else // I am NOT a lightNode.
                    {
                        // If the other node is NOT a lightNode, then exclude it forever.
                        // If the other node is a ligthNode we don't exlude it at all, just allow it to come back.
                        if (!IsLightNode(it->second.nodeTypeId))
                        {
                            m_excludedNodes.insert({nodeId, std::make_pair(boost::none, "")});
                            lllog(DiscovererLogLevel) << m_logPrefix.c_str() << L"Exclude node forever, nodeId: " << nodeId <<std::endl;
                        }

                        if (it->second.isSeed)
                        {
                            // If the node is a seed, we keep the address. The node can be restarted and then join again with the same ip address
                            // but with another nodeId.
                            AddSeed(it->second.controlAddress);
                        }
                    }

                    m_nodes.erase(it);
                }
            });
        }

        // This value is used when this node is a lightnode and it excludes a normal node. Then the normal node are not allowed to come back for the given number of seconds
        // For normal nodes, this has no effect.
        void SetExcludeNodeTimeLimit(unsigned int seconds)
        {
            boost::asio::post(m_strand, [this, seconds]
            {
                lllog(DiscovererLogLevel) << m_logPrefix.c_str() << L"SetExcludeNodeTimeLimit: " << seconds << L" sec " <<std::endl;
                m_lightNodesExcludeTimeLimit = seconds;
            });
        }


#ifndef SAFIR_TEST
    private:
#endif
        static const int DiscovererLogLevel=2;

        //Constant defining how many nodes that can be sent in a singel NodeInfo message without risking that fragmentSize is exceeded.
        //The stuff in CommunicationMessage+NodeInfo is less than 30 bytes plus sent_from_node, and each individual Node (also sent_from_node) is less than 100 bytes.
        //Hence this constant is calculated as (MaxFragmentSize-100-30)/100 = MaxNumberOfNodesPerMessage. (Assuming sent_from_node is set in every msg)
        static const int NodeInfoPerNodeSize=100;
        static const int NodeInfoFixedSize=30+NodeInfoPerNodeSize;

        bool m_running;
        const size_t m_fragmentSize;
        const int m_numberOfNodesPerNodeInfoMsg;
        std::set<int64_t> m_lightNodeTypes;

        NodeMap m_seeds; //id generated from ip:port
        NodeMap m_nodes; //known nodes
        NodeMap m_reportedNodes; //nodes only heard about from others, never talked to
        std::map<int64_t, std::vector<bool> > m_incompletedNodes; //talked to but still haven't received all node info from this node

        unsigned int m_lightNodesExcludeTimeLimit = 30; // seconds
        typedef boost::optional<std::chrono::steady_clock::time_point> ExcludeUntil;
        std::map<int64_t, std::pair<ExcludeUntil /*timeLimitedUntil*/, std::string /*seedIp*/>> m_excludedNodes;

        boost::asio::io_context::strand m_strand;
        Node m_me;
        bool m_thisNodeIsLightNode;
        std::function<void(const Node&)> m_onNewNode;
        boost::asio::steady_timer m_timer;
        Utilities::Random m_random;
        std::string m_logPrefix;

        bool IsExcluded(int64_t id) const {return m_excludedNodes.find(id)!=m_excludedNodes.cend();}
        bool IsLightNode(int64_t nodeType) const {return m_lightNodeTypes.find(nodeType)!=m_lightNodeTypes.cend();}

        void SendDiscover()
        {
            if (m_seeds.empty() && m_reportedNodes.empty() && m_incompletedNodes.empty())
            {
                return; // No receivers of discover messages.
            }

            //Compose a DiscoverMessage
            CommunicationMessage cm;
            cm.mutable_discover()->mutable_from()->set_node_id(m_me.nodeId);
            cm.mutable_discover()->mutable_from()->set_name(m_me.name);
            cm.mutable_discover()->mutable_from()->set_control_address(m_me.controlAddress);
            cm.mutable_discover()->mutable_from()->set_data_address(m_me.dataAddress);
            cm.mutable_discover()->mutable_from()->set_node_type_id(m_me.nodeTypeId);

            for (auto it=m_seeds.cbegin(); it!=m_seeds.cend(); ++it)
            {
                cm.mutable_discover()->set_sent_to_id(it->first);
                lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Send discover to seed: "<<it->second.controlAddress.c_str()<<std::endl;
                SendMessageTo(cm, Resolver::StringToEndpoint(it->second.controlAddress));
            }

            for (auto it=m_reportedNodes.cbegin(); it!=m_reportedNodes.cend(); ++it)
            {
                cm.mutable_discover()->set_sent_to_id(it->first);
                lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Send discover to node I've heard about: "<<it->second.name.c_str()<<L", id="<<it->second.nodeId<<std::endl;
                SendMessageTo(cm, Resolver::StringToEndpoint(it->second.controlAddress));
            }

            for (auto it=m_incompletedNodes.cbegin(); it!=m_incompletedNodes.cend(); ++it)
            {
                const auto nodeIt=m_nodes.find(it->first);
                assert(nodeIt!=m_nodes.cend());
                const Node& node=nodeIt->second;
                cm.mutable_discover()->set_sent_to_id(node.nodeId);
                lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Resend discover to node I havent got all nodeInfo from: "<<node.name.c_str()<<L", id="<<node.nodeId<<std::endl;
                SendMessageTo(cm, Resolver::StringToEndpoint(node.controlAddress));
            }
        }

        void SendNodeInfo(int64_t toId, bool receiverIsLightNode, int64_t fromId, const boost::asio::ip::udp::endpoint& toEndpoint)
        {
            //Compose a DiscoverMessage
            CommunicationMessage cm;
            cm.mutable_node_info()->set_sent_from_id(fromId);
            cm.mutable_node_info()->set_sent_to_id(toId);            

            //Add myself - if this node is a lightNode it will be the only information sent.
            CommunicationMessage_Node* me=cm.mutable_node_info()->mutable_sent_from_node();
            me->set_name(m_me.name);
            me->set_node_id(m_me.nodeId);
            me->set_control_address(m_me.controlAddress);
            me->set_data_address(m_me.dataAddress);
            me->set_node_type_id(m_me.nodeTypeId);

            // If this is a lightnode, only send the info about ourself.
            if (m_thisNodeIsLightNode)
            {
                // we are a lightnode, we dont send any info about other nodes nor our seeds
                cm.mutable_node_info()->set_number_of_packets(1);
                cm.mutable_node_info()->set_packet_number(0);
                SendMessageTo(cm, toEndpoint);
                return;
            }

            // If we get here we are not a lightnode.

            // Calculate the number of nodes to send as nodeInfo
            const int numberOfNodesToSend = static_cast<int>(m_seeds.size() + (receiverIsLightNode ?
                                                                               std::count_if(std::begin(m_nodes), std::end(m_nodes),[this](const auto& n)
                                                                               {
                                                                                   return !IsLightNode(n.second.nodeTypeId);
                                                                               })
                                                                               : m_nodes.size()));

            // Calculate the number of packages needed to send all nodes. We always have to send at least one package even if we dont have any other nodes to share.
            const int numberOfPackets = std::max(1, numberOfNodesToSend / m_numberOfNodesPerNodeInfoMsg + (numberOfNodesToSend % m_numberOfNodesPerNodeInfoMsg > 0 ? 1 : 0));

            cm.mutable_node_info()->set_number_of_packets(numberOfPackets);

            int packetNumber=0;

            //Add seeds
            for (auto seedIt=m_seeds.cbegin(); seedIt!=m_seeds.cend(); ++seedIt)
            {
                CommunicationMessage_Node* ptr=cm.mutable_node_info()->mutable_nodes()->Add();
                ptr->set_name(seedIt->second.name);
                ptr->set_node_id(0);
                ptr->set_control_address(seedIt->second.controlAddress);
                if (cm.node_info().nodes().size()==m_numberOfNodesPerNodeInfoMsg)
                {
                    cm.mutable_node_info()->set_packet_number(packetNumber);
                    SendMessageTo(cm, toEndpoint);
                    cm.mutable_node_info()->mutable_nodes()->Clear(); //clear nodes and continue fill up the same message
                    ++packetNumber;
                }
            }

            // Add all other nodes we have talked to and is not excluded. We dont send nodes that only have been reported, i.e we dont spread rumors
            // If the receiver is a lightNode we exclude other lightNodes from the NodeInfo result.
            for (auto nodeIt=m_nodes.cbegin(); nodeIt!=m_nodes.cend(); ++nodeIt)
            {
                const Node& node=nodeIt->second;
                if (receiverIsLightNode && IsLightNode(node.nodeTypeId))
                {
                    continue; // Dont send NodeInfo about lightNodes to a lightNode.
                }
                CommunicationMessage_Node* ptr=cm.mutable_node_info()->mutable_nodes()->Add();
                ptr->set_name(node.name);
                ptr->set_node_id(node.nodeId);
                ptr->set_node_type_id(node.nodeTypeId);
                ptr->set_control_address(node.controlAddress);
                ptr->set_data_address(node.dataAddress);

                if (cm.node_info().nodes().size()==m_numberOfNodesPerNodeInfoMsg)
                {
                    cm.mutable_node_info()->set_packet_number(packetNumber);
                    SendMessageTo(cm, toEndpoint);
                    cm.mutable_node_info()->mutable_nodes()->Clear(); //clear nodes and continue fill up the same message
                    ++packetNumber;
                }
            }

            // If no package has been sent or if the current package contains nodes that is not sent, then send the message.
            if (packetNumber == 0 || cm.node_info().nodes().size()>0)
            {
                cm.mutable_node_info()->set_packet_number(packetNumber);
                SendMessageTo(cm, toEndpoint);
                ++packetNumber;
            }

            lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Send "<<packetNumber<<" NodeInfo messages to id="<<toId<<std::endl;
        }

        void SendMessageTo(const CommunicationMessage& cm, const boost::asio::ip::udp::endpoint& toEndpoint)
        {
            const auto size=cm.ByteSizeLong();
            Safir::Utilities::Internal::SharedCharArray payload = Safir::Utilities::Internal::MakeSharedArray(size);
            google::protobuf::uint8* buf=reinterpret_cast<google::protobuf::uint8*>(const_cast<char*>(payload.get()));
            cm.SerializeWithCachedSizesToArray(buf);
            UserDataPtr ud(new UserData(m_me.nodeId, 0, ControlDataType, payload, static_cast<size_t>(size)));
            WriterType::SendTo(ud, toEndpoint);
        }

        bool IsNewNode(int64_t id) const
        {
            return m_nodes.find(id)==m_nodes.cend();
        }

        //incomplete nodes are nodes we have heard from but still haven't got all the nodeInfo messages from
        bool UpdateIncompleteNodes(int64_t id, size_t numberOfPackets, size_t packetNumber)
        {
            //note: numberOfPackets=0 means that we received a discover and maybe still havent got any nodeInfo at all from that node

            if (numberOfPackets==1 && packetNumber==0)
            {
                //this is the most common case, all of the nodes nodeInfo fits in one packet
                m_incompletedNodes.erase(id); //normally it shoulnt be in incomplete list, but it can happen if number of nodes decreased and now fits in one packet
                lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"All nodeInfo from "<<id<<L" have been received"<<std::endl;
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
                        lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"We haven't got all nodeInfo yet from "<<id<<std::endl;
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
                        lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"We still haven't got all nodeInfo from "<<id<<std::endl;
                        return false; //not completed yet since there are packets we havent got
                    }
                }

                //we get here if all have been received and node is completed
                m_incompletedNodes.erase(id);
                lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"All nodeInfo from "<<id<<L" have been received"<<std::endl;
                return true;
            }
            else //we have never talked to this node before
            {
                //fist time we talk to this node, the case when its immediately completed is handled first in this method (numberOfPackets=1 and packetNumber=0)
                //so if we get here we just received a Discover or a we still havent received all nodeInfo since it didn't fit in one packet
                lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Add incomplete node with id"<<id<<L". NumberOfPackets: "<<numberOfPackets<<L", packetNumber: "<<packetNumber<<std::endl;
                std::vector<bool> v(numberOfPackets, false);
                if (packetNumber<v.size())
                {
                    v[packetNumber]=true;
                }
                m_incompletedNodes.insert(std::make_pair(id, v));
                return false; //not completed
            }
        }

        void AddNewNode(const CommunicationMessage_Node& node, bool isSeed)
        {
            lllog(4)<< m_logPrefix.c_str() << L"Talked to new node "<<node.name().c_str()<<L" ["<<node.node_id()<<L"]"<<std::endl;
            //insert in node map
            Node n(node.name(), node.node_id(), node.node_type_id(), node.control_address(), node.data_address(), true);
            n.isSeed=isSeed;
            m_nodes.insert(std::make_pair(n.nodeId, n));

            m_reportedNodes.erase(n.nodeId); //now when we actually have talked to the node we also remove from reported nodes

            //notify listener
            m_onNewNode(n);
        }

        void AddReportedNode(const CommunicationMessage_Node& node)
        {
            lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Got report about node "<<node.name().c_str()<<L" ["<<node.node_id()<<L"]"<<std::endl;

            //insert in reported node map
            Node n(node.name(), node.node_id(), node.node_type_id(), node.control_address(), node.data_address(), true);
            m_reportedNodes.insert(std::make_pair(n.nodeId, n));
        }

        void AddSeed(const std::string& seed)
        {
            if (seed!=m_me.controlAddress) //avoid seeding ourself
            {
                uint64_t id=LlufId_Generate64(seed.c_str());
                Node s("seed", id, 0, seed, "", true);
                s.isSeed=true;
                m_seeds.insert(std::make_pair(id, s));
                lllog(DiscovererLogLevel)<< m_logPrefix.c_str() << L"Add seed "<<seed.c_str()<<std::endl;
            }
        }

        bool IsKnownSeedAddress(const std::string& seedAddress) const
        {
            for (auto vt = m_nodes.cbegin(); vt != m_nodes.cend(); ++vt)
            {
                if (vt->second.controlAddress==seedAddress)
                {
                    return true;
                }
            }
            return false;
        }

        void CheckTimeLimitedExclusions()
        {
            for (auto it = std::cbegin(m_excludedNodes); it != std::cend(m_excludedNodes);)
            {
                if (it->second.first.has_value() && it->second.first.value() < std::chrono::steady_clock::now())
                {
                    // time limit has elapsed
                    if (!it->second.second.empty())
                    {
                        // the excluded node has a seed address
                        AddSeed(it->second.second);
                    }
                    lllog(DiscovererLogLevel) << m_logPrefix.c_str() << L"Node " << it->first << L" is no longer excluded." << std::endl;
                    it = m_excludedNodes.erase(it);
                }
                else
                {
                    ++it;
                }
            }

        }

        void OnTimeout(const boost::system::error_code& error)
        {
            if (!m_running)
            {
                return;
            }

            if (!error)
            {
                CheckTimeLimitedExclusions();
                SendDiscover();

                //restart timer
                m_timer.expires_after(std::chrono::milliseconds(m_random.Get()));
                m_timer.async_wait(boost::asio::bind_executor(m_strand, [this](const boost::system::error_code& error){OnTimeout(error);}));
            }
        }
    };

    typedef DiscovererBasic< Writer<UserData> > Discoverer;
}
}
}
}
