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

#ifndef __DOSE_CONNECTIONS_H__
#define __DOSE_CONNECTIONS_H__

#include <ace/OS_NS_unistd.h>
#include <Safir/Dob/Internal/InternalDefs.h>
#include <Safir/Dob/Internal/InternalExportDefs.h>
#include <Safir/Dob/Internal/SharedMemoryObject.h>
#include <Safir/Dob/Internal/Connection.h>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <Safir/Dob/Internal/ConnectRequest.h>

namespace Safir
{
namespace Dob
{
namespace Internal
{
    class DOSE_INTERNAL_API Connections:
        public SharedMemoryObject,
        private boost::noncopyable
    {
    private:
        //This is to make sure that only Instance can call the constructor even though the constructor
        //itself has to be public (limitation of boost::interprocess)
        struct private_constructor_t {};
    public:
        /**
         * Get the singleton instance.
         * Note that this is not a singleton in the usual sense of the word,
         * it is the single instance of something that resides in shared memory.
         */
        static Connections & Instance();

        //The constructor and destructor have to be public for the boost::interprocess internals to be able to call
        //them, but we can make the constructor "fake-private" by making it require a private type as argument.
        explicit Connections(private_constructor_t);
        ~Connections();

        /**
         * This is for applications waiting for "things" to happen to its connection.
         * This blocks forever until SignalIn for that connection has been called.
         * This should ONLY be used for waiting on your own connection. All other use will yield
         * undefined results...
         * Is intended to be called from the dispatch thread in dose_dll.
         */
        void WaitForConnectionSignal(const ConnectionId & connectionId);

        /**
         * This is for DoseMain to wait for events from connections.
         * This method will block forever.
         *
         * Note that all booleans may be set to true at once!
         *
         * @param connect [out] - An app is trying to connect or disconnect. Call Connections::HandleConnect
         * @param connection [out] - An app has signalled that it has done something that needs attention ...
         *                            (It has called it's SignalOut() method)
         */
        void WaitForDoseMainSignal(bool & connect, bool & connectionOut);

        /** For applications to connect to the DOB.
         * If the connectionName contains the string ";dose_main;" (ie the connectionNameCommonPart is "dose_main")
         *        it means that the connection is being made from within dose_main itself
         *        (for dose_mains own connection, not for connections from other nodes).
         *         No callbacks to a connectionConsumer will be made for this.
         **/
        void Connect(const std::string & connectionName,
                     const long context,
                     ConnectResult & result,
                     ConnectionPtr & connection);

        /** For applications to disconnect from the DOB. */
        void Disconnect(const ConnectionPtr & connection);

        class ConnectionConsumer
        {
        public:
            virtual ConnectResult CanAddConnection(const std::string & connectionName, const pid_t pid, const long context) = 0;
            virtual void HandleConnect(const ConnectionPtr & connection) = 0;
            virtual void HandleDisconnect(const ConnectionPtr & connection) = 0;
            virtual void DisconnectComplete() = 0;

            virtual ~ConnectionConsumer() {}
        };

        /**
         * Handle connects and disconnects.
         * Note that handles either one connect or disconnect per call.
         * It is illegal to call this method when there is noone trying to connect
         * (I.e when the WaitForDoseMainSignal hasnt said that there is a connect waiting)
         */
        void HandleConnect(ConnectionConsumer & connectionHandler);

        /**
         * This function is used to add connections from within dose_main.
         * Meant to be used for connections from other nodes.
         */
        void AddConnection(const std::string & connectionNameWithoutCouter,
                           const Typesystem::Int32 counter,
                           const long context,
                           const ConnectionId & id);


        /**
         * This function is used to remove connections from within dose_main.
         * Meant to be used for connections from other nodes or for connections
         * belonging to applications that terminate unexpectedly.
         */
        void RemoveConnection(const ConnectionPtr & connection);

        /**
         * Get a connection from a connection id.
         * It throws a SoftwareViolationException if connection is not found!
         */
        const ConnectionPtr GetConnection(const ConnectionId & connId) const;

        /**
         * Get a connection from a connection id.
         * Returns NULL if not found
         */
        const ConnectionPtr GetConnection(const ConnectionId & connId, std::nothrow_t) const;


        /**
         * Get a connection from a connection name.
         * Note that this is a fairly expensive operation, since the lock has to be held
         * while the table is searched for the right connection.
         * It throws a SoftwareViolationException if connection is not found!
         */
        const ConnectionPtr GetConnectionByName(const std::string & connectionName) const;

        /**
         * Get all connections from a pid.
         */
        void GetConnections(const pid_t  pid, std::vector<ConnectionPtr>& connections) const;

        /**
         * Get the name of a connection.
         * May return empty string if the connection does not exist (it may have just been removed)
         * The connection list is locked while this is called (reader lock)
         */
        const std::string GetConnectionName(const ConnectionId & connectionId) const;

        /**
         * For dose_main to call when it wants to start allowing applications to connect
         *
         * @param context The context to allow connects on (currently only -1 and 0 are allowed)
         */
        void AllowConnect(const long context);

        typedef boost::function<void(const ConnectionPtr &)> ConnectionOutEventHandler;

        //Note that this does not lock internal structures, since it must only be called from dose_main
        //we don't need to lock it (it is dose_main that modifies the structures)
        //TODO: Added a lock since there appears to be some problems in this area... GetConnectionInfo is not working correctly
        //and dose_main crashed with "could not find connection" when running multinode tests.
        void HandleConnectionOutEvents(const ConnectionOutEventHandler & handler);

        //look through all connections and see if there is a pending reg that is accepted
        bool IsPendingAccepted(const Typesystem::TypeId typeId, const Typesystem::HandlerId & handlerId) const;

        // Removes connection(s) from specified node.
        void RemoveConnectionFromNode(const NodeNumber node, const boost::function<void(const ConnectionPtr & connection)> & connectionFunc);

        //A reader lock on the connection vector will be taken during the looping and the callback!
        void ForEachConnection(const boost::function<void(const Connection & connection)> & connectionFunc) const;
        void ForEachConnectionPtr(const boost::function<void(const ConnectionPtr & connection)> & connectionFunc) const;

        // A reader lock will be taken during the callback.
        void ForSpecificConnection(const ConnectionId& connectionId,
                                   const boost::function<void(const ConnectionPtr & connection)> & connectionFunc) const;

    private:
        void AddToSignalHandling(Connection * const connection);
        void RemoveFromSignalHandling(const ConnectionPtr connection);


        void ConnectDoseMain(const std::string & connectionName,
                             const long context,
                             ConnectResult & result,
                             ConnectionPtr & connection);

        void DisconnectDoseMain(const ConnectionPtr& connection);

        typedef PairContainers<ConnectionId, ConnectionPtr>::map ConnectionTable;
        ConnectionTable m_connections;

        //these two vectors handle handle out-signals from the connections.
        //The positions in the vectors correspond to each other (i.e. the signal at position 20 in
        //m_connectionOutSignals represent the out signal of the app in position 20 in m_connectionOutIds).
        //They have a static size, and a connId of -1,-1 (default constructed ConnectionId) means that that "slot" is not occupied
        //THIS DATA STRUCTURE CAN NOT BE RESIZED!! EACH CONNECTION HOLDS A POINTER TO IT'S SIGNAL!
        //RESIZING/MOVING THE STRUCTURE WILL INVALIDATE THE POINTERS!
        //the reason that this is an int is that assigns of 0 and 1 should be atomic, and
        //that any compiler should have int be the wordsize for that processor.
        //TODO: make this volatile when boost makes it possible (submitted to boost as ticket #1634)
        Containers<int>::vector m_connectionOutSignals;
        Containers<ConnectionId>::vector m_connectionOutIds;
        size_t m_lastUsedSlot; //slot that marks the end of the region of used slots. optimization


        //Signal for when an application is trying to connect
        volatile int m_connectSignal;

        boost::interprocess::interprocess_semaphore m_connectSem;
        boost::interprocess::interprocess_semaphore m_connectMinusOneSem;
        boost::interprocess::interprocess_mutex m_connectLock;
        boost::interprocess::interprocess_semaphore m_connectResponseEvent;

        //lock for m_connections, m_connectionOutIds, and m_connectionOutSignals
        //Note that this is an upgradable lock!!!! Not a normal mutex!
        mutable boost::interprocess::interprocess_upgradable_mutex m_connectionTablesLock;

        ConnectRequest m_connectMessage;
        ConnectResponse m_connectResponse;

        Typesystem::Int32 m_connectionCounter;

        bool m_connectMinusOneSemSignalled;
        bool m_connectSemSignalled;
    };
}
}
}
#endif

