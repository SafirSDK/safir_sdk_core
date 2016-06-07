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
#pragma once

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4267)
#endif

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#ifdef _MSC_VER
#pragma warning (pop)
#endif

class PingHandler : public boost::enable_shared_from_this<PingHandler>
{
public:
    PingHandler(boost::asio::strand& strand, int interval, const boost::function<void()>& sendPing)
        :m_running(false)
        ,m_strand(strand)
        ,m_sendPing(sendPing)
        ,m_pingInterval(interval)
        ,m_lastSendTime(boost::chrono::steady_clock::now())
        ,m_timer(strand.get_io_service())
    {
    }

    void Start()
    {
        m_strand.dispatch([=]
        {
            m_running=true;
            m_timer.expires_from_now(m_pingInterval);
            auto self(shared_from_this());
            m_timer.async_wait(m_strand.wrap([self](const boost::system::error_code&){self->OnTimeout();}));
        });
    }

    void Stop()
    {
        //always called from strand
        if (m_running)
        {
            m_running = false;
            m_timer.cancel();
        }
    }

    void Update()
    {
        //always called from strand
        m_lastSendTime = boost::chrono::steady_clock::now();
    }

private:
    bool m_running;
    boost::asio::io_service::strand& m_strand;
    boost::function<void()> m_sendPing;
    boost::chrono::seconds m_pingInterval;
    boost::chrono::steady_clock::time_point m_lastSendTime;
    boost::asio::steady_timer m_timer;

    void OnTimeout()
    {
        if (m_running)
        {
            auto durationSinceSend=boost::chrono::steady_clock::now()-m_lastSendTime;
            if (durationSinceSend>m_pingInterval)
            {
                m_sendPing();
                m_lastSendTime=boost::chrono::steady_clock::now();
                m_timer.expires_from_now(m_pingInterval);
            }
            else
            {
                m_timer.expires_from_now(m_pingInterval-durationSinceSend);
            }

            auto self(shared_from_this());
            m_timer.async_wait(m_strand.wrap([self](const boost::system::error_code&){self->OnTimeout();}));
        }
    }
};
