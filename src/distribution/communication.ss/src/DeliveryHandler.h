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
#ifndef __SAFIR_DOB_COMMUNICATION_DELIVERY_HANDLER_H__
#define __SAFIR_DOB_COMMUNICATION_DELIVERY_HANDLER_H__

#include <atomic>
#include <boost/unordered_map.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/asio.hpp>
#include <Safir/Utilities/Internal/LowLevelLogger.h>
#include <Safir/Utilities/Internal/SystemLog.h>
#include "Parameters.h"
#include "Message.h"
#include "MessageQueue.h"
#include "Node.h"
#include "Writer.h"

namespace Safir
{
namespace Dob
{
namespace Internal
{
namespace Com
{
    typedef std::function<void(int64_t fromNodeId, int64_t fromNodeType, const boost::shared_ptr<char[]>& data, size_t size)> ReceiveData;
    typedef std::function<void(int64_t fromNodeId)> GotReceiveFrom;

    template <class WriterType>
    class DeliveryHandlerBasic : private WriterType
    {
    public:
        DeliveryHandlerBasic(boost::asio::io_service& ioService, int64_t myNodeId, int ipVersion)
            :WriterType(ioService, ipVersion)
            ,m_myId(myNodeId)
            ,m_deliverStrand(ioService)
            ,m_nodes()
            ,m_receivers()
            ,m_gotRecvFrom()
        {
            //TODO: use initialization instead. This is due to problems with VS2013
            m_numberOfUndeliveredMessages=0;
        }

        //Received data to be delivered up to the application. Everythin must be called from readStrand.
        void SetGotRecvCallback(const GotReceiveFrom& callback)
        {
            m_gotRecvFrom=callback;
        }

        void SetReceiver(const ReceiveData& callback, int64_t dataTypeIdentifier)
        {
            m_receivers.insert(std::make_pair(dataTypeIdentifier, callback));
        }

        //Handle received data and deliver to application if possible and sends ack back to sender.
        void ReceivedApplicationData(const MessageHeader* header, const char* payload)
        {
            //Always called from readStrand
            auto senderIt=m_nodes.find(header->commonHeader.senderId);

            if (senderIt==m_nodes.end() || !senderIt->second.node.systemNode)
            {
                lllog(4)<<L"COM: Received data from unknown node or a non system node with id="<<header->commonHeader.senderId<<std::endl;
                return;
            }

            lllog(8)<<L"COM: Received AppData from "<<header->commonHeader.senderId<<std::endl;
            m_gotRecvFrom(header->commonHeader.senderId); //report that we are receivinga intact data from the node

            bool sendAck=HandleMessage(header, payload, senderIt->second);
            Deliver(senderIt->second, header->sendMethod); //if something is now fully received, deliver it to application
            if (sendAck)
            {
                auto ackPtr=boost::make_shared<Ack>(m_myId, senderIt->second.channel[header->sendMethod].lastInSequence, header->sendMethod);
                WriterType::SendTo(ackPtr, senderIt->second.endpoint);
            }
        }

        //Add a node
        void AddNode(const Node& node)
        {
            //Always called from readStrand
            m_nodes.insert(std::make_pair(node.nodeId, NodeInfo(node)));
        }

        //Make node included. If excluded it is also removed.
        void IncludeNode(int64_t id)
        {
            const auto it=m_nodes.find(id);
            assert(it!=m_nodes.end());

            it->second.node.systemNode=true;
        }

        //Make node included or excluded. If excluded it is also removed.
        void RemoveNode(int64_t id)
        {
            m_nodes.erase(id);
        }

        const Node* GetNode(int64_t id) const
        {
            //Always called from readStrand
            auto it=m_nodes.find(id);
            if (it!=m_nodes.end())
            {
                return &it->second.node;
            }
            return nullptr;
        }

        uint32_t NumberOfUndeliveredMessages() const
        {
            return m_numberOfUndeliveredMessages;
        }

    private:
        typedef boost::unordered_map<int64_t, ReceiveData>  ReceiverMap;

        struct RecvData
        {
            bool free;
            uint64_t dataType;
            uint64_t sequenceNumber;
            uint16_t fragmentNumber;
            uint16_t numberOfFragments;
            boost::shared_ptr<char[]> data;
            size_t dataSize;
            uint32_t crc;

            RecvData()
                :free(true)
                ,dataType(0)
                ,sequenceNumber(0)
                ,fragmentNumber(0)
                ,numberOfFragments(0)
                ,data()
                ,dataSize(0)
                ,crc(0)
            {
            }

            void Clear()
            {
                free=true;
                data.reset();
                dataSize=0;
            }
        };

        struct Channel
        {
            uint64_t lastInSequence; //last sequence number that was moved out of the queue. seq(queue[0])-1
            CircularArray<RecvData> queue;
            Channel()
                :lastInSequence(0)
                ,queue(Parameters::ReceiverWindowSize)
            {
            }
        };

        struct NodeInfo
        {
            Node node;
            std::vector<Channel> channel;
            boost::asio::ip::udp::endpoint endpoint;

            NodeInfo(const Node& node_)
                :node(node_)
                ,channel(2) //two channels, one for each sendMethod
                ,endpoint(Utilities::CreateEndpoint(node.unicastAddress))
            {
            }
        };
        typedef std::map<int64_t, NodeInfo> NodeInfoMap;

        int64_t m_myId;
        boost::asio::strand m_deliverStrand; //for delivering data to application
        std::atomic_uint m_numberOfUndeliveredMessages;

        NodeInfoMap m_nodes;
        ReceiverMap m_receivers;
        GotReceiveFrom m_gotRecvFrom;

        void Insert(const MessageHeader* header, const char* payload, NodeInfo& ni)
        {
            Channel& ch=ni.channel[header->sendMethod];
            size_t currentIndex, firstIndex, lastIndex;
            CalculateIndices(ch.lastInSequence, header, currentIndex, firstIndex, lastIndex);
            RecvData& recvData=ch.queue[currentIndex];
            if (!recvData.free)
            {
                if (recvData.sequenceNumber==header->sequenceNumber)
                {
                    //duplicate, just throw away
                    lllog(8)<<L"COM: Recv duplicated message ahead. Seq: "<<header->sequenceNumber<<std::endl;
                    return;
                }
                else
                {
                    //Programming error, should never happen
                    std::ostringstream os;
                    os<<"COM: Logical error detected. We received msg with seqNo="<<header->sequenceNumber<<" and calculated queue index to "
                     <<currentIndex<<". But that index is already occupied with seqNo="<<recvData.sequenceNumber<<". Last received seqNo in non-broken sequence was "
                    <<ch.lastInSequence<<". Current sequenceNumbers in queue is (start with index=0): [";
                    for (size_t i=0; i<ch.queue.Size(); ++i)
                    {
                        const auto& item=ch.queue[i];
                        if (item.free)
                            os<<"X  ";
                        else
                            os<<ch.queue[i].sequenceNumber<<"  ";
                    }
                    os<<"]";
                    SEND_SYSTEM_LOG(Error, <<os.str().c_str());

                    throw std::logic_error(os.str());
                    return;
                }
            }
            recvData.free=false;
            recvData.numberOfFragments=header->numberOfFragments;
            recvData.fragmentNumber=header->fragmentNumber;
            recvData.sequenceNumber=header->sequenceNumber;
            recvData.dataType=header->commonHeader.dataType;
            recvData.crc=header->crc;

            //Check if buffer is not created for us. In the case the message is fragmented
            //another fragment may already have arrived and the buffer is created.
            if (!recvData.data)
            {
                recvData.data.reset(new char[header->totalContentSize]);
                recvData.dataSize=header->totalContentSize;

                //copy buffer to other indices if it shall be shared among many fragments
                uint16_t tmpFragNo=0;
                for (size_t i=firstIndex; i<currentIndex; ++i)
                {
                    ch.queue[i].data=recvData.data;
                    ch.queue[i].dataSize=recvData.dataSize;
                    ch.queue[i].numberOfFragments=recvData.numberOfFragments;
                    ch.queue[i].fragmentNumber=tmpFragNo++;
                }
                for (size_t i=currentIndex+1; i<=lastIndex; ++i)
                {
                    ch.queue[i].data=recvData.data;
                    ch.queue[i].dataSize=recvData.dataSize;
                    ch.queue[i].numberOfFragments=recvData.numberOfFragments;
                    ch.queue[i].fragmentNumber=++tmpFragNo;
                }
            }

            //copy data to correct offset in buffer depending on fragment number
            char* dest=recvData.data.get()+header->fragmentOffset;
            memcpy(dest, payload, header->fragmentContentSize);
        }

        void ForceInsert(const MessageHeader* header, const char* payload, NodeInfo& ni)
        {
            Channel& ch=ni.channel[header->sendMethod];
            ch.lastInSequence=header->sequenceNumber-1; //we pretend this was exactly what we expected to get
            //Clear the queue, should not be necessary as long as we dont let excluded nodes come back.
            for (size_t i=0; i<Parameters::ReceiverWindowSize; ++i)
            {
                ch.queue[i].Clear();
            }

            //Call the normal insert
            Insert(header, payload, ni);
        }

        bool HandleMessage(const MessageHeader* header, const char* payload, NodeInfo& ni)
        {
            lllog(8)<<L"COM: recv from: "<<ni.node.nodeId<<L", sendMethod: "<<
                      (header->sendMethod==SingleReceiverSendMethod ? L"Unicast" : L"Multicast")<<
                      L", seq: "<<header->sequenceNumber<<std::endl;

            Channel& ch=ni.channel[header->sendMethod];

            if (ch.lastInSequence==0) //first time we receive anything, we accept any seqNo and start counting from there
            {
                if (header->fragmentNumber==0) //we cannot start in the middle of a fragmented message
                {
                    ForceInsert(header, payload, ni);
                }
                //else we must wait for beginning of a new message before we start
            }
            else if (header->sequenceNumber<=ch.lastInSequence)
            {
                lllog(8)<<L"COM: Recv duplicated message in order. Seq: "<<header->sequenceNumber<<L" from node "<<ni.node.name.c_str()<<std::endl;
            }
            else if (header->sequenceNumber<ch.lastInSequence+Parameters::ReceiverWindowSize)
            {
                //The Normal case:
                //This is something within our receive window, maybe it is out of order but in that case the gaps will eventually be filled in
                //when sender retransmits non-acked messages.
                Insert(header, payload, ni);
            }
            else //lost messages, got something too far ahead
            {
                //This can occur if the senders send window i bigger than the receivers receive window. If everything is working as expected we can just ignore this message.
                //Sooner or later the sender must retransmit all non-acked messages, and in time this message will come into our receive window.
                lllog(8)<<L"COM: Received Seq: "<<header->sequenceNumber<<" wich means that we have lost a message. LastInSequence="<<ch.lastInSequence<<std::endl;

                std::wcout<<L"COM: recv from: "<<ni.node.name.c_str()<<L", sendMethod: "<<
                                      (header->sendMethod==SingleReceiverSendMethod ? L"Unicast" : L"Multicast")<<
                                      L", seq: "<<header->sequenceNumber<<std::endl;
                std::wcout<<L"COM: Received Seq: "<<header->sequenceNumber<<" wich means that we have lost a message. LastInSequence="<<ch.lastInSequence<<std::endl;
                return false; //this message is not handled, dont send ack
            }

            return true; // all messages except lost message when the received is too far ahead of us, must be acked.
        }

        void CalculateIndices(uint64_t lastSeq, const MessageHeader* header,
                              size_t& currentIndex, size_t& firstIndex, size_t& lastIndex) const
        {
            currentIndex=static_cast<size_t>(header->sequenceNumber-lastSeq-1);
            int first=static_cast<int>(currentIndex)-header->fragmentNumber;
            firstIndex=static_cast<size_t>( std::max(0, first) );
            lastIndex=std::min(Parameters::ReceiverWindowSize-1, currentIndex+header->numberOfFragments-header->fragmentNumber-1);
        }

        void Deliver(NodeInfo& ni, uint16_t sendMethod)
        {
            Channel& ch=ni.channel[sendMethod];

            for (size_t i=0; i<ch.queue.Size(); ++i)
            {
                RecvData& rd=ch.queue[0];
                if (!rd.free)
                {
                    assert(rd.sequenceNumber==ch.lastInSequence+1);
                    ch.lastInSequence=rd.sequenceNumber;
                    if (rd.fragmentNumber+1==rd.numberOfFragments)
                    {
                        //check crc
                        uint32_t crc=CalculateCrc32(rd.data.get(), rd.dataSize);
                        if (crc!=rd.crc)
                        {
                            std::ostringstream os;
                            os<<"COM: Received data from node "<<ni.node.name<<" with bad CRC. Got "<<crc<<" but expected "<<rd.crc<<std::endl;
                            os<<"Fragments: "<<rd.numberOfFragments<<", no: "<<rd.fragmentNumber<<std::endl;
                            os<<"Seq: "<<rd.sequenceNumber<<std::endl;
                            os<<"DataType: "<<rd.dataType<<std::endl;
                            os<<"Size: "<<rd.dataSize<<std::endl;
                            SEND_SYSTEM_LOG(Error, <<os.str().c_str());
                            //hexdump(rd.data.get(), 0, rd.dataSize);
                            throw std::logic_error(os.str());
                        }

                        //last fragment has been received, we can deliver this message to application
                        auto fromId=ni.node.nodeId;
                        auto fromNodeType=ni.node.nodeTypeId;
                        auto dataPtr=rd.data;
                        auto dataSize=rd.dataSize;
                        auto dataType=rd.dataType;
                        //auto seqNo=rd.sequenceNumber;


                        m_numberOfUndeliveredMessages++;

                        m_deliverStrand.post([=]
                        {
                            auto recvIt=m_receivers.find(dataType); //m_receivers shall be safe to use inside m_deliverStrand since it is not supposed to be modified after start
                            if (recvIt!=m_receivers.end())
                            {
                                //unsigned int val=*reinterpret_cast<const unsigned int*>(dataPtr.get());
                                //std::cout<<"deliver "<<seqNo<<", val="<<val<<std::endl;
                                recvIt->second(fromId, fromNodeType, dataPtr, dataSize);
                            }
                            else
                            {
                                std::ostringstream os;
                                os<<"COM: Received data from node "<<fromId<<" that has no registered receiver. DataTypeIdentifier: "<<dataType<<std::endl;
                                SEND_SYSTEM_LOG(Error, <<os.str().c_str());
                                throw std::logic_error(os.str());
                            }
                            m_numberOfUndeliveredMessages--;
                        });
                    }

                    //Check if queue contains fragmented message with more fragments than recvWindowSize, needs special handling
                    const RecvData& lastInQueue=ch.queue[ch.queue.Size()-1];
                    if (lastInQueue.dataSize>0 && lastInQueue.fragmentNumber<lastInQueue.numberOfFragments-1)
                    {
                        //lastInQueue has allocated buffer and is not the last fragment of the message
                        //remember if windowSize=1 then lastInQueue==rd, so we have to copy data before clearing rd if we need to to
                        boost::shared_ptr<char[]> data=lastInQueue.data;
                        size_t dataSize=lastInQueue.dataSize;
                        uint16_t numberOfFragments=lastInQueue.numberOfFragments;
                        uint16_t fragmentNumber=lastInQueue.fragmentNumber+1;

                        rd.Clear();
                        ch.queue.Step();

                        RecvData& nextFragment=ch.queue[ch.queue.Size()-1];
                        nextFragment.data=data;
                        nextFragment.dataSize=dataSize;
                        nextFragment.numberOfFragments=numberOfFragments;
                        nextFragment.fragmentNumber=fragmentNumber;
                    }
                    else //the normal case. Step up queue and the last item will be free without allocated buffer
                    {
                        rd.Clear(); //mark as free and release reference to data
                        ch.queue.Step(); //step queue so index zero (first in queue) is the next item
                    }

                }
                else
                {
                    //nothing more in order
                    break;
                }
            }
        }

        inline boost::shared_ptr<char[]> MakePtr(const char* data, size_t size)
        {
            boost::shared_ptr<char[]> ptr=boost::make_shared<char[]>(size);
            memcpy(ptr.get(), data, size);
            return ptr;
        }
    };

    typedef DeliveryHandlerBasic< Writer<Ack> > DeliveryHandler;
}
}
}
}

#endif
