/******************************************************************************
*
* Copyright Saab AB, 2007-2013 (http://safir.sourceforge.net)
*
* Created by: Lars Hagström / stlrha
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

#ifndef _dose_main_connection_handler_h
#define _dose_main_connection_handler_h

#include <boost/noncopyable.hpp>
#include <Safir/Dob/Internal/InternalFwd.h>
#include <Safir/Dob/Internal/DistributionData.h>
#include <deque>

namespace Safir
{
namespace Dob
{
namespace Internal
{
    //forward declarations:
#if 0 //stewart
    class ExternNodeCommunication;
#endif
    class ProcessInfoHandler;
    class RequestHandler;
    class PendingRegistrationHandler;
    class NodeHandler;
    class PersistHandler;

    class ConnectionHandler:
        private boost::noncopyable
    {
    public:
        ConnectionHandler();
        ~ConnectionHandler();

        void Init(
#if 0 //stewart
                  ExternNodeCommunication & ecom,
#endif
                  ProcessInfoHandler & processInfoHandler,
                  RequestHandler & requesthandler,
                  PendingRegistrationHandler & prh,
                  NodeHandler & nh,
                  PersistHandler & persistHandler);

        void MaybeSignalConnectSemaphore();

        void HandleConnect(const ConnectionPtr & connection);
        void HandleDisconnect(const ConnectionPtr & connection);

        bool HandleUnsent();

        void HandleConnectFromDoseCom(const DistributionData & connectMsg);
        void HandleDisconnectFromDoseCom(const DistributionData & disconnectMsg);

    private:
#if 0 //stewart
        ExternNodeCommunication * m_ecom;
#endif
        ProcessInfoHandler * m_processInfoHandler;
        RequestHandler * m_requestHandler;
        PendingRegistrationHandler * m_pendingRegistrationHandler;
        NodeHandler * m_nodeHandler;
        PersistHandler * m_persistHandler;

        std::deque<DistributionData> m_unsent;

        bool m_connectSemHasBeenSignalled;
    };
}
}
}

#endif

