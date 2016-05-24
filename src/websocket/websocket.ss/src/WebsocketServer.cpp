/******************************************************************************
*
* Copyright Saab AB, 2016 (http://safirsdkcore.com)
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
#include <signal.h>
#include <boost/make_shared.hpp>
#include <Safir/Websocket/Parameters.h>
#include "WebsocketServer.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4100)
#endif

#include <boost/thread.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace ws = Safir::Websocket;

WebsocketServer::WebsocketServer(boost::asio::io_service& ioService)
    :m_server()
    ,m_ioService(ioService)
    ,m_work(new boost::asio::io_service::work(m_ioService))
    ,m_connections()
    ,m_signals(m_ioService, SIGINT, SIGABRT, SIGTERM)
    ,m_dobConnection()
    ,m_dobDispatcher(m_dobConnection, m_ioService)
{
}

void WebsocketServer::Run()
{    
    m_signals.async_wait([=](const boost::system::error_code&, int /*signal*/){Terminate();});

    lllog(5)<<"Wait for DOB to let us open a connection..."<<std::endl;
    m_dobConnection.Open(L"safir_websocket", L"", 0, this, &m_dobDispatcher);

    // Initialize ASIO
    m_server.init_asio(&m_ioService);

    //try to disable all logging from websocketpp, seems like info is still logging
    m_server.set_access_channels(websocketpp::log::alevel::none);

    m_server.set_open_handler([=](websocketpp::connection_hdl hdl)
    {
        auto con=boost::make_shared<RemoteClient>(m_server, m_ioService, hdl, [=](const RemoteClient* con){OnConnectionClosed(con);});
        m_connections.insert(con);
        lllog(5)<<"Server: new connection added: "<<con->ToString().c_str()<<std::endl;
        PrintConnections();
    });

    //Set up listening port
    //TODO: report error if port number is out of valid range?
    m_server.listen(static_cast<uint16_t>(ws::Parameters::Port()));

    // Start the server accept loop
    m_server.start_accept();

    lllog(5)<<"Running ws server on port "<<ws::Parameters::Port()<<std::endl;
}

void WebsocketServer::Terminate()
{
    //At the moment websocketpp is logging info when stop_listening is called and that is normal behavious
    //according to https://github.com/zaphoyd/websocketpp/issues/498
    //Cant figure out how to disable the info logging

    lllog(5)<<"safir_websocket is starting to shut down..."<<std::endl;
    m_server.stop_listening();
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
    {
        (*it)->Close();
    }

    if (m_dobConnection.IsOpen())
    {
        m_dobConnection.Close();
    }

    m_work.reset();
    m_ioService.stop();

    lllog(5)<<"all connections closed..."<<std::endl;
}

void WebsocketServer::OnConnectionClosed(const RemoteClient* con)
{
    auto it=std::find_if(m_connections.begin(), m_connections.end(), [&](const boost::shared_ptr<RemoteClient>& p){return p.get()==con;});

    if (it!=m_connections.end())
    {
        lllog(5)<<"Connection closed: "<<con->ToString().c_str()<<std::endl;
        m_connections.erase(it);
    }
    else
    {
        lllog(5)<<"Cllosed connection was not found."<<std::endl;
    }

    PrintConnections();
}

void WebsocketServer::OnStopOrder()
{
    Terminate();
}

void WebsocketServer::PrintConnections() const
{
    lllog(5)<<"----- Connections -----"<<std::endl;
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
    {
        lllog(5)<<(*it)->ToString().c_str()<<std::endl;
    }
}

