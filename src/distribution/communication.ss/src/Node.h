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
#ifndef __SAFIR_DOB_COMMUNICATION_NODE_H__
#define __SAFIR_DOB_COMMUNICATION_NODE_H__

#include <map>
#include <set>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>

namespace Safir
{
namespace Dob
{
namespace Internal
{
namespace Com
{
    enum IpVersionType {V4, V6};

    class Node
    {
    public:
        Node(const std::string& name_,
             boost::int64_t nodeId_,
             boost::int64_t nodeTypeId_,
             const std::string& address_);

        static bool SplitAddress(const std::string& address, std::string& ip, unsigned short& port);
        static boost::asio::ip::udp::endpoint CreateEndpoint(const std::string& address,
                                                             IpVersionType& version,
                                                             unsigned short defaultPort=0);
        static boost::asio::ip::udp::endpoint CreateEndpoint(const std::string& address,
                                                             unsigned short defaultPort=0);

        //Getters and setters
        const std::string& Name() const {return m_name;}
        boost::int64_t Id() const {return m_id;}
        boost::int64_t NodeTypeId() const {return m_nodeTypeId;}
        const boost::asio::ip::udp::endpoint& Endpoint() const {return m_endpoint;}
        bool IsSystemNode() const {return m_systemNode;}
        void SetSystemNode(bool isSystemNode) {m_systemNode=isSystemNode;}
        const std::string& Address() const {return m_address;}
        IpVersionType IpVersion() const {return m_ipVersion;}

        boost::uint64_t& LastSentUnicastSeqNo() {return m_lastSentUnicastSeqNo;}
        std::string ToString() const;

    private:
        std::string m_name;
        boost::int64_t m_id;
        boost::int64_t m_nodeTypeId;
        std::string m_address;
        IpVersionType m_ipVersion;
        boost::asio::ip::udp::endpoint m_endpoint;
        bool m_systemNode;

        //Sequence numbers - wrap around not implemented. Will only work for 6 billion msg/sec in 100 years.
        boost::uint64_t m_lastSentUnicastSeqNo;     //last sent unicast msg to this node
    };

    typedef std::set<boost::int64_t> NodeIdSet;
    typedef std::vector<boost::int64_t> NodeIdVector;
    typedef std::map<boost::int64_t, Node> NodeMap;
}
}
}
}

#endif
