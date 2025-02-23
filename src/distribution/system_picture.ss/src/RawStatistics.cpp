/******************************************************************************
*
* Copyright Saab AB, 2013 (http://safirsdkcore.com)
*
* Created by: Lars Hagström / lars.hagstrom@consoden.se
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
#include <Safir/Dob/Internal/RawStatistics.h>
#include <boost/noncopyable.hpp>
#include <Safir/Utilities/Internal/LowLevelLogger.h>
#include "MessageWrapperCreators.h"
#include <stdexcept>

#ifdef _MSC_VER
#  pragma warning (push)
#  pragma warning (disable: 4127)
#  pragma warning (disable: 4100)
#  pragma warning (disable: 4244)
#endif

#include "RawStatisticsMessage.pb.h"

#ifdef _MSC_VER
#  pragma warning (pop)
#endif


namespace
{
    void PrintMessage(const Safir::Dob::Internal::SP::RawStatisticsMessage& msg,
                      std::wostream& out,
                      const unsigned int level = 0)
    {
        if (level > 1)
        {
            throw std::logic_error("Too many levels in RawStatisticsMessage!");
        }

        const std::wstring indent = level == 0 ? L"" : L"    ";

        if (level == 0)
        {
            out << indent << msg.name().c_str() << "@" << msg.control_address().c_str() << " (" << msg.id() << "), "
                << "\n          Eid: " << (msg.has_election_id() ? msg.election_id() : 0)
                << "\n          Iid: " << (msg.has_incarnation_id() ? msg.incarnation_id() : 0);
        }
        else
        {
            out << "\n        " << indent << "Eid: " << (msg.has_election_id() ? msg.election_id() : 0);
            out << "\n        " << indent << "Iid: " << (msg.has_incarnation_id() ? msg.incarnation_id() : 0);
        }

        for (int i = 0; i < msg.node_info_size(); ++i)
        {
            const auto& node = msg.node_info(i);
            out << "\n" << indent;
            if (node.is_dead())
            {
                out << "   [";
            }
            else
            {
                out << "    ";
            }
            out << node.name().c_str()
                       << "@" << node.control_address().c_str()
                       << " (" << node.id() << ")";
            if (node.is_dead())
            {
                out << "]";
                if (node.is_resurrecting())
                {
                    out << "R";
                }
            }
            else
            {
                out << "\n" << indent << "        "
                    <<"ctrlRecv = " << node.control_receive_count() << ", "
                    <<"ctrlDup = " << node.control_duplicate_count() << ", "
                    <<"ctrlRetry = " << node.control_retransmit_count() << ", "
                    <<"dataRecv = " << node.data_receive_count() << ", "
                    <<"dataDup = " << node.data_duplicate_count() << ", "
                    <<"dataRetry = " << node.data_retransmit_count();
            }

            if (node.has_remote_statistics())
            {
                PrintMessage(node.remote_statistics(), out, level + 1);
            }
            else if (level == 0 && !node.is_dead())
            {
                out << indent << "\n        [No remote statistics received yet]";
            }
        }

        if (level == 0)
        {
            out << "\nMore Dead Nodes: ";
            for (int i = 0; i < msg.more_dead_nodes_size(); ++i)
            {
                out << msg.more_dead_nodes(i) << " ";
            }
        }

        out << std::flush;
    }
}

namespace Safir
{
namespace Dob
{
namespace Internal
{
namespace SP
{

    class RawStatistics::Impl
        : private boost::noncopyable
    {
    public:
        Impl(const RawStatisticsMessage& message,
             std::shared_ptr<const RawStatisticsMessage> owner)
            : m_message(message)
            , m_owner(std::move(owner))
        {

        }

        const std::string& Name() const
        {
            return m_message.name();
        }

        int64_t Id() const
        {
            return m_message.id();
        }

        int64_t NodeTypeId() const
        {
            return m_message.node_type_id();
        }

        const std::string& ControlAddress() const
        {
            return m_message.control_address();
        }

        const std::string& DataAddress() const
        {
            return m_message.data_address();
        }

        int64_t ElectionId() const
        {
            if (m_message.has_election_id())
            {
                return m_message.election_id();
            }
            else
            {
                return 0;
            }
        }

        int64_t IncarnationId() const
        {
            if (m_message.has_incarnation_id())
            {
                return m_message.incarnation_id();
            }
            else
            {
                return 0;
            }
        }

        int Size() const
        {
            return m_message.node_info_size();
        }

        const std::string& Name(const int index) const
        {
            return m_message.node_info(index).name();
        }

        int64_t Id(const int index) const
        {
            return m_message.node_info(index).id();
        }

        int64_t NodeTypeId(const int index) const
        {
            return m_message.node_info(index).node_type_id();
        }

        const std::string& ControlAddress(const int index) const
        {
            return m_message.node_info(index).control_address();
        }

        const std::string& DataAddress(const int index) const
        {
            return m_message.node_info(index).data_address();
        }

        bool IsDead(const int index) const
        {
            return m_message.node_info(index).is_dead();
        }

        bool IsResurrecting(const int index) const
        {
            return m_message.node_info(index).is_resurrecting();
        }

        uint32_t ControlReceiveCount(const int index) const
        {
            return m_message.node_info(index).control_receive_count();
        }

        uint32_t ControlDuplicateCount(const int index) const
        {
            return m_message.node_info(index).control_duplicate_count();
        }

        uint32_t ControlRetransmitCount(const int index) const
        {
            return m_message.node_info(index).control_retransmit_count();
        }

        uint32_t DataReceiveCount(const int index) const
        {
            return m_message.node_info(index).data_receive_count();
        }

        uint32_t DataDuplicateCount(const int index) const
        {
            return m_message.node_info(index).data_duplicate_count();
        }

        uint32_t DataRetransmitCount(const int index) const
        {
            return m_message.node_info(index).data_retransmit_count();
        }

        bool IsEveryoneElseDead() const
        {
            for(const auto& ni: m_message.node_info())
            {
                if (!ni.is_dead() && Id() != ni.id())
                {
                    lllog(4) << "SP: IsEveryoneElseDead: Node " << ni.id() << " is not dead" << std::endl;
                    return false;
                }
            }
            lllog(4) << "SP: IsEveryoneElseDead: Yes, everyone is dead!" << std::endl;
            return true;
        }


        bool HasRemoteStatistics(const int index) const
        {
            return m_message.node_info(index).has_remote_statistics();
        }

        RawStatistics RemoteStatistics(const int index) const
        {
            if (m_message.node_info(index).has_remote_statistics())
            {
                return RawStatistics(std::make_shared<Impl>(m_message.node_info(index).remote_statistics(), m_owner));
            }
            else
            {
                throw std::logic_error("No remote statistics available!");
            }
        }

        int MoreDeadNodesSize() const
        {
            return m_message.more_dead_nodes_size();
        }

        int64_t MoreDeadNodes(const int i) const
        {
            return m_message.more_dead_nodes(i);
        }

        void Print(std::wostream& out) const
        {
            PrintMessage(m_message, out);
        }

    private:
        friend class RawStatisticsCreator;

        static RawStatistics Create(std::unique_ptr<RawStatisticsMessage> message)
        {
            std::shared_ptr<RawStatisticsMessage> msg(std::move(message));
            return RawStatistics(std::make_shared<Impl>(*msg.get(), msg));
        }

        const RawStatisticsMessage& m_message;
        const std::shared_ptr<const RawStatisticsMessage> m_owner;
    };

    RawStatistics::RawStatistics() {}

    const std::string& RawStatistics::Name() const {return m_impl->Name();}
    int64_t RawStatistics::Id() const {return m_impl->Id();}
    int64_t RawStatistics::NodeTypeId() const {return m_impl->NodeTypeId();}
    const std::string& RawStatistics::ControlAddress() const {return m_impl->ControlAddress();}
    const std::string& RawStatistics::DataAddress() const {return m_impl->DataAddress();}

    int64_t RawStatistics::ElectionId() const {return m_impl->ElectionId();}
    int64_t RawStatistics::IncarnationId() const {return m_impl->IncarnationId();}

    int RawStatistics::Size() const {return m_impl->Size();}

    const std::string& RawStatistics::Name(const int index) const {return m_impl->Name(index);}
    int64_t RawStatistics::Id(const int index) const {return m_impl->Id(index);}
    int64_t RawStatistics::NodeTypeId(const int index) const {return m_impl->NodeTypeId(index);}
    const std::string& RawStatistics::ControlAddress(const int index) const {return m_impl->ControlAddress(index);}
    const std::string& RawStatistics::DataAddress(const int index) const {return m_impl->DataAddress(index);}

    bool RawStatistics::IsDead(const int index) const {return m_impl->IsDead(index);}
    bool RawStatistics::IsResurrecting(const int index) const {return m_impl->IsResurrecting(index);}
    uint32_t RawStatistics::ControlReceiveCount(const int index) const {return m_impl->ControlReceiveCount(index);}
    uint32_t RawStatistics::ControlDuplicateCount(const int index) const {return m_impl->ControlDuplicateCount(index);}
    uint32_t RawStatistics::ControlRetransmitCount(const int index) const {return m_impl->ControlRetransmitCount(index);}
    uint32_t RawStatistics::DataReceiveCount(const int index) const {return m_impl->DataReceiveCount(index);}
    uint32_t RawStatistics::DataDuplicateCount(const int index) const {return m_impl->DataDuplicateCount(index);}
    uint32_t RawStatistics::DataRetransmitCount(const int index) const {return m_impl->DataRetransmitCount(index);}

    bool RawStatistics::IsEveryoneElseDead() const {return m_impl->IsEveryoneElseDead();}

    bool RawStatistics::HasRemoteStatistics(const int index) const {return m_impl->HasRemoteStatistics(index);}
    RawStatistics RawStatistics::RemoteStatistics(const int index) const {return m_impl->RemoteStatistics(index);}

    int RawStatistics::MoreDeadNodesSize() const {return m_impl->MoreDeadNodesSize();}
    int64_t RawStatistics::MoreDeadNodes(const int i) const {return m_impl->MoreDeadNodes(i);}

    void RawStatistics::Print(std::wostream& out) const {m_impl->Print(out);}

    RawStatistics RawStatisticsCreator::Create(std::unique_ptr<RawStatisticsMessage> message)
    {
        return RawStatistics::Impl::Create(std::move(message));
    }
}
}
}
}
