/******************************************************************************
*
* Copyright Saab AB, 2006-2008 (http://www.safirsdk.com)
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

#ifndef __DOSE_TEST_CPP_NODE_H__
#define __DOSE_TEST_CPP_NODE_H__

#include <boost/noncopyable.hpp>
#include <Safir/Dob/Connection.h>
#include <Safir/Dob/Internal/Atomic.h>
#include <DoseTest/Action.h>
#include <Safir/Dob/ErrorResponse.h>
#include "Consumer.h"
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

class Dispatcher:
    public Safir::Dob::Dispatcher,
    private boost::noncopyable
{
public:
    Dispatcher(const boost::function<void(void)> & dispatchCallback,
               boost::asio::io_service & ioService)
        : m_dispatchCallback(dispatchCallback)
        , m_isNotified(0)
        , m_ioService(ioService)
    {}

private:
    virtual void OnDoDispatch()
    {
        if (m_isNotified == 0)
        {
            m_isNotified = 1;
            m_ioService.post(boost::bind(&Dispatcher::Dispatch,this));
        }
    }
    virtual void Dispatch()
    {
        m_isNotified = 0;
        m_dispatchCallback();
    }

    const boost::function<void(void)> m_dispatchCallback;
    Safir::Dob::Internal::AtomicUint32 m_isNotified;
    boost::asio::io_service & m_ioService;
};

class ActionReader:
    private boost::noncopyable
{
public:
    ActionReader(const boost::function<void (DoseTest::ActionPtr)> & handleActionCallback,
                 const std::string& listenAddressStr,
                 boost::asio::io_service& ioService);

private:
    void HandleData(const boost::system::error_code& error,
                    const size_t bytes_recvd);
    boost::asio::ip::udp::socket m_socket;
    boost::asio::ip::udp::endpoint m_senderEndpoint;
    Safir::Dob::Typesystem::BinarySerialization m_buffer;

    const boost::function<void (DoseTest::ActionPtr)> m_handleActionCallback;
};


class Executor:
    public Safir::Dob::StopHandler,
    public Safir::Dob::MessageSubscriber,
    public Safir::Dob::EntityHandler,
    public Safir::Dob::ServiceHandler,
    private boost::noncopyable
{
public:
    Executor(const std::vector<std::string> & commandLine);


    void Run();

private:

    virtual void OnStopOrder();
    virtual void OnMessage(const Safir::Dob::MessageProxy messageProxy);

    virtual void OnRevokedRegistration(const Safir::Dob::Typesystem::TypeId     typeId,
                                       const Safir::Dob::Typesystem::HandlerId& handlerId);

    virtual void OnCreateRequest(const Safir::Dob::EntityRequestProxy entityRequestProxy,
                                 Safir::Dob::ResponseSenderPtr        responseSender)
    {responseSender->Send(Safir::Dob::ErrorResponse::Create());}

    virtual void OnUpdateRequest(const Safir::Dob::EntityRequestProxy entityRequestProxy,
                                 Safir::Dob::ResponseSenderPtr        responseSender)
    {responseSender->Send(Safir::Dob::ErrorResponse::Create());}

    virtual void OnDeleteRequest(const Safir::Dob::EntityRequestProxy entityRequestProxy,
                                 Safir::Dob::ResponseSenderPtr        responseSender)
    {responseSender->Send(Safir::Dob::ErrorResponse::Create());}

    virtual void OnServiceRequest(const Safir::Dob::ServiceRequestProxy serviceRequestProxy,
                                  Safir::Dob::ResponseSenderPtr         responseSender);

    void HandleAction(DoseTest::ActionPtr action);

    void ExecuteAction(DoseTest::ActionPtr action);
    void AddCallbackAction(DoseTest::ActionPtr action);
    void ExecuteCallbackActions(const Safir::Dob::CallbackId::Enumeration callback);

    void DispatchControlConnection();
    void DispatchTestConnection();

    const std::wstring m_identifier;
    const int m_instance;
    const std::wstring m_instanceString;
    const std::wstring m_controlConnectionName;
    const std::wstring m_testConnectionName;

    boost::asio::io_service m_ioService;

    const Safir::Dob::Typesystem::EntityId m_partnerEntityId;

    bool m_isDone;
    bool m_isActive;

    std::vector<boost::shared_ptr<Consumer> > m_consumers;

    Safir::Dob::Connection m_controlConnection;
    Safir::Dob::Connection m_testConnection;
    bool m_dispatchTestConnection;

    Dispatcher m_testDispatcher;
    Dispatcher m_controlDispatcher;

    ActionReader m_actionReader;

    typedef std::vector<DoseTest::ActionPtr> Actions;
    typedef std::vector<Actions> CallbackActions;
    std::vector<std::vector<DoseTest::ActionPtr> > m_callbackActions;

    int m_defaultContext;
};

typedef boost::shared_ptr<Executor> ExecutorPtr;

#endif
