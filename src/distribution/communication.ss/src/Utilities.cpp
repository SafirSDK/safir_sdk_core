/******************************************************************************
*
* Copyright Saab AB, 2014 (http://safir.sourceforge.net)
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
#include "Utilities.h"

namespace Safir
{
namespace Dob
{
namespace Internal
{
namespace Com
{
namespace  Utilities
{
    bool SplitAddress(const std::string& address, std::string& ip, unsigned short& port)
    {
        size_t startPortSearch=address.find_last_of(']'); //if ip6, start search after address end
        if (startPortSearch==address.npos)
        {
            startPortSearch=0; //not found, then we search from beginning
        }
        size_t index=address.find_first_of(':', startPortSearch);

        if (index==address.npos)
        {
            ip=address;
            return false; //no port found
        }

        ip=address.substr(0, index);
        try
        {
            port=boost::lexical_cast<unsigned short>(address.substr(index+1));
        }
        catch (const boost::bad_lexical_cast&)
        {
            return false;
        }

        return true;

    }

    boost::asio::ip::udp::endpoint CreateEndpoint(const std::string& address, int& ipVersion)
    {
        std::string addr;
        unsigned short port=0;
        if (!SplitAddress(address, addr, port))
        {
            throw std::logic_error("Failed to parse '"+address+"' as an udp endpoint with port_number on form <ip>:<port>");
        }

        boost::system::error_code ec;
        boost::asio::ip::address_v4 a4=boost::asio::ip::address_v4::from_string(addr, ec);
        if (!ec) //ip v4 address
        {
            ipVersion=4;
            return boost::asio::ip::udp::endpoint(a4, port);
        }

        boost::asio::ip::address_v6 a6=boost::asio::ip::address_v6::from_string(addr, ec);
        if (!ec) //ip v6 address
        {
            ipVersion=6;
            return boost::asio::ip::udp::endpoint(a6, port);
        }

        throw std::logic_error("Failed to parse '"+address+"' as an udp endpoint.");
    }

    boost::asio::ip::udp::endpoint CreateEndpoint(const std::string& address)
    {
        int dummy;
        return CreateEndpoint(address, dummy);
    }

    boost::asio::ip::basic_endpoint::protocol_type Protocol(int p)
    {
        if (p==4)
        {
            return boost::asio::ip::udp::v4();
        }
        else if (p==6)
        {
            return boost::asio::ip::udp::v6();
        }
        throw std::logic_error("Invalid ip protocol. IPv4 and IPv6 supported.");
    }

}
}
}
}
}
