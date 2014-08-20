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
#ifndef __SAFIR_DOB_COMMUNICATION_READER_H__
#define __SAFIR_DOB_COMMUNICATION_READER_H__

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/chrono.hpp>
#include <Safir/Utilities/Internal/LowLevelLogger.h>
#include <Safir/Utilities/Internal/SystemLog.h>
#include "Parameters.h"
#include "Node.h"

namespace Safir
{
namespace Dob
{
namespace Internal
{
namespace Com
{
    /**
     * The DataReceiver class is responsible for receiving data on both unicast and multicast.
     * All received messages are passed to onRecv-callback. DataReceiver is unaware of sequenceNumbers and fragments.
     * If callback onRecv is returning false it means that there is maximum number of messages waiting to be retrieved
     * by the application and in that case DataReceiver will sleep until callback isReceiverReady is returning true again.
     */
    template <class ReaderType>
    class DataReceiverType : private ReaderType
    {
    public:

        DataReceiverType(boost::asio::io_service& ioService,
                         const std::string& unicastAddress,
                         const std::string& multicastAddress,
                         const std::function<bool(const char*, size_t)>& onRecv,
                         const std::function<bool(void)>& isReceiverIsReady)
            :m_strand(ioService)
            ,m_timer(ioService, boost::chrono::milliseconds(10))
            ,m_onRecv(onRecv)
            ,m_isReceiverReady(isReceiverIsReady)
            ,m_running(false)
        {
            int unicastIpVersion;
            auto unicastEndpoint=Utilities::CreateEndpoint(unicastAddress, unicastIpVersion);

            m_socket.reset(new boost::asio::ip::udp::socket(ioService));
            m_socket->open(unicastEndpoint.protocol());

            if (!multicastAddress.empty())
            {
                //using multicast
                int multicastIpVersion=0;
                auto mcEndpoint=Utilities::CreateEndpoint(multicastAddress, multicastIpVersion);
                if (multicastIpVersion!=unicastIpVersion)
                {
                    throw std::logic_error("Unicast address and multicast address is not in same format (IPv4 and IPv6)");
                }
                m_multicastSocket.reset(new boost::asio::ip::udp::socket(ioService));
                m_multicastSocket->open(mcEndpoint.protocol());
                m_multicastSocket->set_option(boost::asio::ip::udp::socket::reuse_address(true));
                m_multicastSocket->set_option(boost::asio::ip::multicast::enable_loopback(true));
                m_multicastSocket->set_option(boost::asio::ip::multicast::join_group(mcEndpoint.address()));
                BindSocket(m_multicastSocket, multicastIpVersion, mcEndpoint.port());
            }

            BindSocket(m_socket, unicastIpVersion, unicastEndpoint.port());
        }

        void Start()
        {
            m_strand.dispatch([=]
            {
                m_running=true;
                AsyncReceive(m_bufferUnicast, m_socket.get());
                if (m_multicastSocket)
                {
                    AsyncReceive(m_bufferMulticast, m_multicastSocket.get());
                }
            });
        }

        void Stop()
        {
            m_strand.dispatch([=]
            {
                m_running=false;
                m_timer.cancel();

                if (m_socket->is_open())
                {
                    m_socket->close();
                }
                if (m_multicastSocket && m_multicastSocket->is_open())
                {
                    m_multicastSocket->close();
                }
            });
        }

        boost::asio::io_service::strand& Strand() {return m_strand;}

#ifndef SAFIR_TEST
    private:
#endif
        boost::asio::io_service::strand m_strand;
        boost::asio::steady_timer m_timer;
        std::function<bool(const char*, size_t)> m_onRecv;
        std::function<bool(void)> m_isReceiverReady;
        boost::shared_ptr<boost::asio::ip::udp::socket> m_socket;
        boost::shared_ptr<boost::asio::ip::udp::socket> m_multicastSocket;
        bool m_running;

        char m_bufferUnicast[Parameters::ReceiveBufferSize];
        char m_bufferMulticast[Parameters::ReceiveBufferSize];

        void AsyncReceive(char* buf, boost::asio::ip::udp::socket* socket)
        {
            ReaderType::AsyncReceive(buf,
                                     Parameters::ReceiveBufferSize,
                                     socket,
                                     m_strand.wrap([=](const boost::system::error_code& error, size_t bytesRecv)
                                     {
                                         HandleReceive(error, bytesRecv, buf, socket);
                                     }));
        }

        void HandleReceive(const boost::system::error_code& error, size_t bytesRecv, char* buf, boost::asio::ip::udp::socket* socket)
        {
            if (!m_running)
            {
                return;
            }

            if (!error)
            {
                if (m_onRecv(buf, bytesRecv))
                {
                    //receiver is keeping up with our pace, continue to read incoming messages
                    AsyncReceive(buf, socket);
                }
                else
                {
                    // we must wait for a while before delivering more messages
                    lllog(7)<<"COM: Reader has to wait for application to handle delivered messages"<<std::endl;
                    SetWakeUpTimer(buf, socket);
                }
            }
            else
            {
                std::cout<<"Read failed, error "<<error.message().c_str()<<std::endl;
                SEND_SYSTEM_LOG(Error, <<"Read failed, error "<<error.message().c_str());
            }
        }

        void BindSocket(boost::shared_ptr<boost::asio::ip::udp::socket>& socket, int ipv, unsigned short port)
        {
            if (ipv==4)
            {
                boost::asio::ip::udp::endpoint listenEp(boost::asio::ip::address_v4::from_string("0.0.0.0"), port);
                socket->bind(listenEp);
            }
            else
            {
                boost::asio::ip::udp::endpoint listenEp(boost::asio::ip::address_v6::from_string("0.0.0.0"), port);
                socket->bind(listenEp);
            }
        }

        void SetWakeUpTimer(char* buf, boost::asio::ip::udp::socket* socket)
        {
            m_timer.expires_from_now(boost::chrono::milliseconds(10));
            m_timer.async_wait(m_strand.wrap([=](const boost::system::error_code& error){WakeUpAfterSleep(error, buf, socket);}));
        }

        void WakeUpAfterSleep(const boost::system::error_code& /*error*/, char* buf, boost::asio::ip::udp::socket* socket)
        {
            if (!m_running)
            {
                return;
            }

            if (m_isReceiverReady())
            {
                lllog(7)<<"COM: Reader wakes up from sleep and starts receiving again"<<std::endl;
                AsyncReceive(buf, socket);
            }
            else
            {
                lllog(7)<<"COM: Reader wakes up but must go back to sleep until application is ready"<<std::endl;
                SetWakeUpTimer(buf, socket);
            }
        }
    };

    struct SocketReader
    {
        void AsyncReceive(char* buf,
                          size_t bufSize,
                          boost::asio::ip::udp::socket* socket,
                          const boost::function< void(const boost::system::error_code&, size_t) >& completionHandler)
        {
            socket->async_receive(boost::asio::buffer(buf, bufSize), completionHandler);
        }
    };

    typedef DataReceiverType<SocketReader> DataReceiver;
}
}
}
}

#endif
