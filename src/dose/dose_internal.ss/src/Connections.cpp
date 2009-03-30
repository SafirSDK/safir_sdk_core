/******************************************************************************
*
* Copyright Saab AB, 2007-2008 (http://www.safirsdk.com)
*
* Created by: Lars Hagstr�m / stlrha
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

#include <Safir/Dob/Internal/Connections.h>
#include <Safir/Dob/Typesystem/Internal/InternalUtils.h>
#include <Safir/Dob/Typesystem/Operations.h>
#include <Safir/Dob/ProcessInfo.h>
#include <Safir/Dob/ThisNodeParameters.h>
#include <Safir/Utilities/Internal/LowLevelLogger.h>
#include <boost/interprocess/sync/upgradable_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include "Signals.h"
#include "ExitHandler.h"

#include <vector>

namespace Safir
{
namespace Dob
{
namespace Internal
{
    static const int MAX_NUM_CONNECTIONS =
        Safir::Dob::ProcessInfo::MaxNumberOfInstances() *
        Safir::Dob::ProcessInfo::ConnectionNamesArraySize();

    Connections & Connections::Instance()
    {
        //find_or_construct has good synchronization guarantees,
        //so we dont need extra locking here.
        static Connections * instance = GetSharedMemory().find_or_construct<Connections>("CONNECTIONS")(private_constructor_t());
        return *instance;
    }


    Connections::Connections(private_constructor_t):
        m_connectionOutSignals(MAX_NUM_CONNECTIONS,0),
        m_connectionOutIds(MAX_NUM_CONNECTIONS), //default constructed (-1,-1)
        m_lastUsedSlot(0),
        m_connectSignal(0),
        m_connectSem(0),
        m_connectMinusOneSem(0),
        m_connectResponseEvent(0),
        m_connectionCounter(0),
        m_connectMinusOneSemSignalled(false),
        m_connectSemSignalled(false)
    {

    }

    Connections::~Connections()
    {

    }

    void Connections::WaitForDoseMainSignal(bool & connect, bool & connectionOut)
    {
        connect = false;
        connectionOut = false;
        Signals::Instance().WaitForConnectOrOut();
        //get the events
        if (m_connectSignal != 0)
        {
            connect = true;
            m_connectSignal = 0;
        }

        //we could loop through the connection signals, but currently we just assume that we might as well
        //tell dose_main to do it for us even if no signals have been set....
        connectionOut = true;
        //std::wcout << "WaitForDoseMainSignal: connect = " << std::boolalpha << connect << ", connectionOut = " << connectionOut << std::endl;
    }

    void Connections::WaitForConnectionSignal(const ConnectionId & connectionId)
    {
        Signals::Instance().WaitForConnectionSignal(connectionId);
    }



    void Connections::Connect(const std::string & connectionName,
                              const long context,
                              ConnectResult & result,
                              ConnectionPtr & connection)
    {
        if (connectionName.length() > MAX_CONNECTION_NAME_LENGTH)
        {
            std::wostringstream ostr;
            ostr << "Connection name is too long '" <<
                connectionName.c_str() << "'";
            throw Safir::Dob::Typesystem::IllegalValueException(ostr.str(),__WFILE__,__LINE__);
        }

        //is this a connect from within dose_main's main thread? if so, bypass normal connection handling
        if (connectionName.find(";dose_main;") != connectionName.npos)
        {
            ConnectDoseMain(connectionName,context,result,connection);
            return;
        }

        //guard against connections before dose_main has said that it is ok to start connecting.
        if (context == -1)
        {
            lllout << "Waiting on m_connectMinusOneSem" << std::endl;
            m_connectMinusOneSem.wait();
            m_connectMinusOneSem.post();
        }
        else
        {
            lllout << "Waiting on m_connectSem" << std::endl;
            m_connectSem.wait();
            m_connectSem.post();
        }

        namespace bi = boost::interprocess;

        bi::scoped_lock<bi::interprocess_mutex> lck(m_connectLock);

        const pid_t pid = ACE_OS::getpid();

        m_connectMessage.Set(connect_tag, connectionName, context, pid);

        m_connectSignal = 1;
        Signals::Instance().SignalConnectOrOut();
        //wait for response
        m_connectResponseEvent.wait();

        m_connectResponse.GetAndClear(connect_tag, result, connection);

        ENSURE(result == Success ||
               (result != Success && connection == NULL), <<"failed to insert connection!!");

        ENSURE(result != Undefined, << "Connect response was not set correctly!!");
    }

    void Connections::ConnectDoseMain(const std::string & connectionName,
                                      const long context,
                                      ConnectResult & result,
                                      ConnectionPtr & connection)
    {
        lllout << "Handling a Connect for one of dose_mains own connections. name = " << connectionName.c_str() << std::endl;

        connection = NULL;
        result = Success;
        boost::interprocess::upgradable_lock<boost::interprocess::interprocess_upgradable_mutex> rlock(m_connectionTablesLock);

        bool foundName = false;
        for (ConnectionTable::const_iterator it = m_connections.begin();
             it != m_connections.end(); ++it)
        {
            if (it->second->NameWithoutCounter() == connectionName)
            {
                foundName = true;
                break;
            }
        }

        if (foundName)
        {
            lllout << "Connection already exists. name = " << connectionName.c_str() << std::endl;

            result = ConnectionNameAlreadyExists;
            return;
        }
        else
        {
            const pid_t pid = ACE_OS::getpid();

            //upgrade the mutex
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_upgradable_mutex> wlock(move(rlock));
            connection = GetSharedMemory().construct<Connection>
                (boost::interprocess::anonymous_instance)
                (connectionName, m_connectionCounter++, Safir::Dob::ThisNodeParameters::NodeNumber(), context, pid);

            const bool success = m_connections.insert
                (std::make_pair(connection->Id(),ConnectionPtr(connection))).second;

            ENSURE(success, << "ConnectDoseMain: Failed to insert connection in map! name = " << connectionName.c_str());

            AddToSignalHandling(connection.get());

            //We don't call the connect handler, since we assume that dose_main is able to
            //do the bookkeeping for it's own connection by itself.
        }
    }

    void Connections::Disconnect(const ConnectionPtr & connection)
    {
        ENSURE(connection != NULL, << "Cannot disconnect a NULL connection");

        std::string connectionName(connection->NameWithoutCounter());

        //is this a disconnect from within dose_main? if so, bypass normal disconnection handling
        if (connectionName.find(";dose_main;") != connectionName.npos)
        {
            DisconnectDoseMain(connection);
            return;
        }

        namespace bi = boost::interprocess;

        bi::scoped_lock<bi::interprocess_mutex> lck(m_connectLock,bi::defer_lock);

        //we only 'poll' the lock so that we can still exit if it is dose_main exiting
        //that is causing this disconnect. (It is not enough to check it just once, since there
        //may be multiple connections in one app, and all may not get the Died flag set before
        //the app starts exiting.)
        while (!lck)
        {
            if (connection->IsDead())
            {
                lllout << "dose_main appears to have died, so we disconnect quietly, without waiting for acks etc." << std::endl;
                return;
            }
            lck.try_lock();
            if (!lck)
            {
                ACE_OS::sleep(ACE_Time_Value(0,1000)); //1 millisecond
            }
        }

        m_connectMessage.Set(disconnect_tag, connection);

        m_connectSignal = 1;
        Signals::Instance().SignalConnectOrOut();

        //wait for response
        m_connectResponseEvent.wait();
        //TODO: check for existance of dose_main somehow here, then we can
        //stop waiting if dose_main exits.

        ConnectResult result;
        m_connectResponse.GetAndClear(disconnect_tag, result);
        ENSURE(result == Success, << "Disconnect should always succeed!!!");
    }

    void Connections::DisconnectDoseMain(const ConnectionPtr& connection)
    {
        lllout << "Handling a Disconnect for one of dose_mains own connections. name = " << connection->NameWithCounter() << std::endl;

        boost::interprocess::scoped_lock<boost::interprocess::interprocess_upgradable_mutex> wlock(m_connectionTablesLock);

        RemoveFromSignalHandling(connection);
        m_connections.erase(connection->Id());

        GetSharedMemory().destroy_ptr(connection.get());
    }


    void Connections::HandleConnect(ConnectionConsumer & connectionHandler)
    {
        if (m_connectMessage.IsConnect())
        {
            lllout << "Handling a Connect" << std::endl;

            std::string connectionName;
            long context;
            int pid;

            m_connectMessage.GetAndClear(connect_tag, connectionName, context, pid);

            boost::interprocess::upgradable_lock<boost::interprocess::interprocess_upgradable_mutex> rlock(m_connectionTablesLock);

            bool foundName = false;
            for (ConnectionTable::const_iterator it = m_connections.begin();
                 it != m_connections.end(); ++it)
            {
                if (it->second->NameWithoutCounter() == connectionName)
                {
                    foundName = true;
                    break;
                }
            }

            if (foundName)
            {
                lllout << "Connection with name '" << connectionName.c_str() << "' already exists, returning an error code" << std::endl;
                m_connectResponse.Set(connect_tag,ConnectionNameAlreadyExists,ConnectionPtr(/*NULL*/));
            }
            else
            {
                const ConnectResult result = connectionHandler.CanAddConnection(connectionName,pid,context);
                if (result != Success)
                {
                    m_connectResponse.Set(connect_tag, result, ConnectionPtr(/*NULL*/));
                }
                else
                {
                    Connection * connection = NULL;
                    {
                        //upgrade the mutex
                        boost::interprocess::scoped_lock<boost::interprocess::interprocess_upgradable_mutex> wlock(move(rlock));

                        connection = GetSharedMemory().construct<Connection>(boost::interprocess::anonymous_instance)
                            (connectionName, m_connectionCounter++, Safir::Dob::ThisNodeParameters::NodeNumber(), context, pid);

                        const bool success = m_connections.insert(std::make_pair(connection->Id(),ConnectionPtr(connection))).second;

                        ENSURE(success, << "HandleConnect: Failed to insert connection in map! name = " << connectionName.c_str());

                        AddToSignalHandling(connection);
                    }

                    ExitHandler::Instance().AddFunction(connection, &Connection::Died);
                    ExitHandler::Instance().AddFunction(connection, &Connection::SendStopOrder);

                    //call the connect handler
                    connectionHandler.HandleConnect(connection);

                    m_connectResponse.Set(connect_tag, Success, connection);
                }
            }

        }
        else //is a disconnect message
        {
            lllout << "Handling a Disconnect" << std::endl;
            ConnectionPtr connection;
            m_connectMessage.GetAndClear(disconnect_tag, connection);

            lllout << "ConnectionHandler::HandleDisconnect: Disconnecting " << connection->Id() << std::endl;
            connectionHandler.HandleDisconnect(connection);

            {
                boost::interprocess::scoped_lock<boost::interprocess::interprocess_upgradable_mutex> wlock(m_connectionTablesLock);

                RemoveFromSignalHandling(connection);
                m_connections.erase(connection->Id());

                GetSharedMemory().destroy_ptr(connection.get());
            }

            m_connectResponse.Set(disconnect_tag, Success);
            ExitHandler::Instance().RemoveConnection(connection);

            connectionHandler.DisconnectComplete();
        }
        //lllout << "Signalling the connector that the response is available" << std::endl;
        m_connectResponseEvent.post();
    }

    void Connections::AddConnection(const std::string & connectionName,
                                    const Typesystem::Int32 counter,
                                    const long context,
                                    const ConnectionId & id)
    {
        lllout << "Adding a connection (from other node?) name = '" << connectionName.c_str()
               << "', context = " << context
               << ", id = " << id << std::endl;

        boost::interprocess::upgradable_lock<boost::interprocess::interprocess_upgradable_mutex> rlock(m_connectionTablesLock);

        ConnectionTable::const_iterator findIt = m_connections.find(id);

        if (findIt != m_connections.end())
        {
            // The connection already exists on this node.

            lllout << "Connection already exists on this node!!!! name = " << connectionName.c_str()
                << ", id = " << id << std::endl;

            return;
        }


        //upgrade the mutex
        boost::interprocess::scoped_lock<boost::interprocess::interprocess_upgradable_mutex> wlock(move(rlock));

        //remote connections have pid = -1
        Connection * connection = GetSharedMemory().construct<Connection>(boost::interprocess::anonymous_instance)
            (connectionName, counter, id.m_node, context, -1);

        const bool success = m_connections.insert(std::make_pair(connection->Id(),ConnectionPtr(connection))).second;

        ENSURE(success, << "AddConnection: Failed to insert connection in map! name = " << connectionName.c_str());
    }


    void Connections::RemoveConnection(const ConnectionPtr & connection)
    {
        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_upgradable_mutex> wlock(m_connectionTablesLock);

            if (connection->IsLocal())
            {
                RemoveFromSignalHandling(connection);
            }
            m_connections.erase(connection->Id());

            GetSharedMemory().destroy_ptr(connection.get());
        }

        ExitHandler::Instance().RemoveConnection(connection);
    }


    void Connections::AllowConnect(const long context)
    {
        lllout << "Signalling to allow connections on context " << context  << std::endl;
        if (context == -1)
        {
            if (m_connectMinusOneSemSignalled)
            {
                lllout << "  already signalled, ignoring call"  << std::endl;
            }
            else
            {
                m_connectMinusOneSem.post();
            }
        }
        else
        {
            ENSURE(context == 0, << "Only contexts -1 and 0 are allowed");
            if (m_connectSemSignalled)
            {
                lllout << "  already signalled, ignoring call"  << std::endl;
            }
            else
            {
                m_connectSem.post();
            }
        }
    }

    void Connections::AddToSignalHandling(Connection * const connection)
    {
        //find a free slot
        Containers<ConnectionId>::vector::iterator findIt = std::find(m_connectionOutIds.begin(),m_connectionOutIds.end(),ConnectionId());
        ENSURE (findIt != m_connectionOutIds.end(), << "No free slot was found in the connection handler arrays!");

        *findIt = connection->Id();
        const size_t index = std::distance(m_connectionOutIds.begin(),findIt);
        m_connectionOutSignals[index] = 0;
        connection->SetOutSignal(&m_connectionOutSignals[index]);

        m_lastUsedSlot = std::max(m_lastUsedSlot,index);
    }


    void Connections::RemoveFromSignalHandling(const ConnectionPtr connection)
    {
        Containers<ConnectionId>::vector::iterator findIt =
            std::find(m_connectionOutIds.begin(),
                      m_connectionOutIds.end(),
                      connection->Id());

        ENSURE(findIt != m_connectionOutIds.end(), << "Could not find connection " << connection->Id() << " to remove from signalling vector!");

        //set the signal to false and remove the connection from the vector
        const size_t index = std::distance(m_connectionOutIds.begin(),findIt);
        m_connectionOutSignals[index] = false;
        *findIt = ConnectionId();

        //if it was the last slot that we're no longer using we need to find
        //the last one that is in use now.
        if (index == m_lastUsedSlot)
        {
            size_t i = index;
            while(m_connectionOutIds[i] == ConnectionId() && i > 0)
            {
                --i;
            }
            m_lastUsedSlot = i;
        }
    }



    void Connections::HandleConnectionOutEvents(const ConnectionOutEventHandler & handler)
    {
        //        boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(m_connectionTablesLock);

        //Workaround to fix ticket #629

        //above line has been "temporarily" commented out to work around the fact that the
        //interprocess_upgradable_mutex is semi-recursive. It allows a recursive lock, as long
        //as no writer has queued itself on getting the exclusive lock after the first reader
        //has been taken but before the second reader is taken.

        //the lock can be removed here since all writes to the table occur from
        //the same thread in dose_main, which is also the only caller of this function.

        const Containers<int>::vector::iterator end = m_connectionOutSignals.begin() + m_lastUsedSlot + 1;
        for (Containers<int>::vector::iterator it = m_connectionOutSignals.begin();
             it != end; ++it)
        {
            volatile int & signal = *it;
            if (signal != 0)
            {
                signal = 0;

                const ConnectionId & connId = m_connectionOutIds[std::distance(m_connectionOutSignals.begin(), it)];

                //                std::wcout << "Handling event from connection " << connId << std::endl;
                if (connId == ConnectionId()) //compare with dummy-connection
                {
                    lllout << "A signal was set for a slot that has no valid connection id! Resetting it!" << std::endl;
                    std::wcout << "A signal was set for a slot that has no valid connection id! Resetting it!" << std::endl;
                }
                else
                {
                    ConnectionTable::const_iterator findIt = m_connections.find(connId);
                    ENSURE(findIt != m_connections.end(), << "Connections::HandleConnectionOutEvents: Failed to find connection! connectionId = " << connId);
                    handler(findIt->second);
                }
            }

        }
    }

    bool
    Connections::IsPendingAccepted(const Typesystem::TypeId typeId, const Typesystem::HandlerId & handlerId) const
    {
        boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(m_connectionTablesLock);
        for (ConnectionTable::const_iterator it = m_connections.begin();
             it != m_connections.end(); ++it)
        {
            if (it->second->IsPendingAccepted(typeId,handlerId))
            {
                return true;
            }
        }
        return false;
    }

    const ConnectionPtr
    Connections::GetConnection(const ConnectionId & connId) const
    {
        boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(m_connectionTablesLock);
        ConnectionTable::const_iterator findIt = m_connections.find(connId);
        ENSURE(findIt != m_connections.end(), << "Connections::GetConnection: Failed to find connection! connectionId = " << connId);
        return findIt->second;
    }

    const ConnectionPtr
    Connections::GetConnection(const ConnectionId & connId, std::nothrow_t) const
    {
        boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(m_connectionTablesLock);
        ConnectionTable::const_iterator findIt = m_connections.find(connId);
        if (findIt != m_connections.end())
        {
            return findIt->second;
        }
        else
        {
            return NULL;
        }
    }

    const ConnectionPtr
    Connections::GetConnectionByName(const std::string & connectionName) const
    {
        boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(m_connectionTablesLock);
        for (ConnectionTable::const_iterator it = m_connections.begin();
            it != m_connections.end(); ++it)
        {
            if (it->second->NameWithCounter() == connectionName)
            {
                return it->second;
            }
        }
        ENSURE(false, << "Failed to find connection by name! connectionName = " << connectionName.c_str());
        return NULL; //Keep compiler happy
    }

    void
    Connections::GetConnections(const pid_t  pid, std::vector<ConnectionPtr>& connections) const
    {
        boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(m_connectionTablesLock);

        connections.clear();

        for(ConnectionTable::const_iterator it = m_connections.begin();
            it != m_connections.end();
            ++it)
        {
            if ((*it).second->Pid() == pid)
            {
                connections.push_back((*it).second);
            }
        }
    }

    const std::string Connections::GetConnectionName(const ConnectionId & connectionId) const
    {
        boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(m_connectionTablesLock);
        ConnectionTable::const_iterator findIt = m_connections.find(connectionId);

        if (findIt != m_connections.end())
        {
            return findIt->second->NameWithCounter();
        }
        else
        {
            return std::string();
        }
    }

    void Connections::RemoveConnectionFromNode(const NodeNumber node, const boost::function<void(const ConnectionPtr & connection)> & connectionFunc)
    {
        std::vector<ConnectionPtr> removeConnections;

        // Remove from m_connections and store in removeConnections
        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_upgradable_mutex> wlock(m_connectionTablesLock);

            ConnectionTable::const_iterator it = m_connections.begin();

            while(it != m_connections.end())
            {
                ConnectionTable::const_iterator removeIt = it;
                ++it;

                if(removeIt->first.m_node == node)
                {
                    removeConnections.push_back(removeIt->second);
                    m_connections.erase(removeIt);
                }
            }
        }


        // Now loop over the connection to be removed and do the work
        for(std::vector<ConnectionPtr>::const_iterator it = removeConnections.begin();
            it != removeConnections.end();
            ++it)
        {
            ConnectionPtr removeConnection = (*it);

            connectionFunc(removeConnection);

            if (removeConnection->IsLocal())
            {
                RemoveFromSignalHandling(removeConnection);
            }

            GetSharedMemory().destroy_ptr(removeConnection.get());

            ExitHandler::Instance().RemoveConnection(removeConnection);
        }
    }


    void Connections::ForEachConnection(const boost::function<void(const Connection & connection)> & connectionFunc) const
    {
        boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(m_connectionTablesLock);
        for (ConnectionTable::const_iterator it = m_connections.begin();
             it != m_connections.end(); ++it)
        {
            connectionFunc(*it->second);
        }
    }

    void Connections::ForEachConnectionPtr(const boost::function<void(const ConnectionPtr & connection)> & connectionFunc) const
    {
        boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(m_connectionTablesLock);
        for (ConnectionTable::const_iterator it = m_connections.begin();
             it != m_connections.end(); ++it)
        {
            connectionFunc(it->second);
        }
    }

    void Connections::ForSpecificConnection(const ConnectionId& connectionId,
                                            const boost::function<void(const ConnectionPtr & connection)> & connectionFunc) const
    {
        boost::interprocess::sharable_lock<boost::interprocess::interprocess_upgradable_mutex> lock(m_connectionTablesLock);

        ConnectionTable::const_iterator it = m_connections.find(connectionId);

        if (it != m_connections.end())
        {
            connectionFunc(it->second);
        }
    }
}
}
}
