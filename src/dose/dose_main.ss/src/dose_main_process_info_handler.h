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

#ifndef _dose_main_process_info_handler_h
#define _dose_main_process_info_handler_h

#include <Safir/Dob/Connection.h>
#include <Safir/Dob/Internal/Connections.h>
#include <Safir/Dob/ProcessInfo.h>
#include <Safir/Utilities/ProcessMonitor.h>
#include <vector>

namespace Safir
{
namespace Dob
{
namespace Internal
{
    class ExternNodeCommunication;

    class ProcessInfoHandler:
        public Safir::Dob::EntityHandler,
        private boost::noncopyable
    {
    public:
        // Constructor and Destructor
        ProcessInfoHandler();
        ~ProcessInfoHandler();

        void Init(const ExternNodeCommunication & ecom,
                  Safir::Utilities::ProcessMonitor& processMonitor);

        void ConnectionAdded(const ConnectionPtr & connection);
        void ConnectionRemoved(const ConnectionPtr & connection);

        void HandleProcessInfoEntityDelete(const DistributionData & request);

        //returns Success if it is possible to add a new connection to the given process,
        //otherwise an error code.
        ConnectResult CanAddConnectionFromProcess(const pid_t pid) const;

    private:
        virtual void OnRevokedRegistration(const Safir::Dob::Typesystem::TypeId    typeId,
                                           const Safir::Dob::Typesystem::HandlerId& handlerId);

        virtual void OnCreateRequest(const Safir::Dob::EntityRequestProxy entityRequestProxy,
                                     Safir::Dob::ResponseSenderPtr    responseSender);

        virtual void OnUpdateRequest(const Safir::Dob::EntityRequestProxy entityRequestProxy,
                                     Safir::Dob::ResponseSenderPtr    responseSender);

        virtual void OnDeleteRequest(const Safir::Dob::EntityRequestProxy entityRequestProxy,
                                     Safir::Dob::ResponseSenderPtr    responseSender);

        void AddOwnConnection();

        Safir::Dob::SecondaryConnection m_connection;

        Safir::Utilities::ProcessMonitor * m_processMonitor;
    };
}
}
}

#endif
