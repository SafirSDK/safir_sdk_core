/******************************************************************************
*
* Copyright Saab AB, 2007-2008 (http://www.safirsdk.com)
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
#include "Library.h"
#include <algorithm>
#include <Safir/Dob/NotOpenException.h>
#include <Safir/Dob/Typesystem/Exceptions.h>
#include <Safir/Application/BackdoorCommand.h>

#if _MSC_VER
#pragma warning(push)
#pragma warning (disable: 4702)
#endif
#include <boost/lexical_cast.hpp>
#if _MSC_VER
#pragma warning(pop)
#endif

#include <Safir/Dob/ConnectionAspectMisc.h>
#include <Safir/Dob/NodeParameters.h>
#include <Safir/Dob/OverflowException.h>
#include <Safir/Dob/ThisNodeParameters.h>
#include <Safir/Dob/Typesystem/Serialization.h>
#include <Safir/Utilities/Internal/LowLevelLogger.h>
#include <Safir/Utilities/Internal/PanicLogging.h>
#include <Safir/Utilities/ProcessInfo.h>
#include <Safir/Utilities/SystemLog.h>
#include <boost/bind.hpp>
#include <Safir/Utilities/CrashReporter.h>
#include <boost/regex.hpp>
#include <boost/tokenizer.hpp>
#include <iomanip>
#include <iostream>

//TODO get rid of these!
#ifdef GetMessage
#undef GetMessage
#endif

namespace Safir
{
namespace SwReports
{
namespace Internal
{
    using Safir::Dob::Typesystem::Utilities::ToUtf8;

    static const wchar_t* pingCmd = L"ping";
    static const wchar_t* helpCmd = L"help";

    typedef boost::tokenizer<boost::char_separator<wchar_t>,
                             std::wstring::const_iterator,
                             std::wstring> wtokenizer;

    const boost::char_separator<wchar_t> separator(L" ");

    boost::once_flag Library::SingletonHelper::m_onceFlag = BOOST_ONCE_INIT;
    boost::scoped_ptr<Library> Library::SingletonHelper::m_instance;

    void Library::SingletonHelper::Instantiate()
    {
        m_instance.reset(new Library());
    }

    Library & Library::Instance()
    {
        boost::call_once(SingletonHelper::m_onceFlag,boost::bind(SingletonHelper::Instantiate));
        return *SingletonHelper::m_instance;
    }


    Library::Library()
        : m_arguments(),
          m_prefixes(),
          m_prefixSearchLock(),
          m_backdoorConnection(),
          m_traceBufferLock(),
          m_traceBuffer(),
          m_prefixPending(true),
          m_logCreator(),
          m_systemLog()
    {
        std::wstring env;
        {
            char * cenv = getenv("FORCE_LOG");
            if (cenv != NULL)
            {
                env = Safir::Dob::Typesystem::Utilities::ToWstring(cenv);
            }
        }
        try
        {
            for (std::wstring::iterator it = env.begin();
                it != env.end(); ++it)
            {
                if (*it == '\"')
                {
                    *it = ' ';
                }
            }

            wtokenizer tokenizer(env,separator);

            //Tokenize the environment variable.

            std::vector<std::wstring> arguments;
            std::copy(tokenizer.begin(),tokenizer.end(),
                std::back_inserter(arguments));

            m_arguments = arguments;
        }
        catch (const std::exception & exc)
        {
            std::wcout << "Error while parsing FORCE_LOG environment variable '" <<
                env << "'.\nException description is '"<< exc.what() << "'.\nContinuing as if it was not set" << std::endl;
        }
    }

    Library::~Library() 
    {

    }

    void
    Library::StartTraceBackdoor(const std::wstring& connectionNameCommonPart,
                                const std::wstring& connectionNameInstancePart)
    {
        m_backdoorConnection.Attach(connectionNameCommonPart,
                                    connectionNameInstancePart);

        m_backdoorConnection.SubscribeMessage(Safir::Application::BackdoorCommand::ClassTypeId,
                                              Safir::Dob::Typesystem::ChannelId(),
                                              this);
    }

    void
    Library::StopTraceBackdoor()
    {
        if (!m_backdoorConnection.IsOpen())
        {
            // Connection has been closed.
            return; // *** RETURN ***
        }

        m_backdoorConnection.UnsubscribeMessage(Safir::Application::BackdoorCommand::ClassTypeId,
                                                Safir::Dob::Typesystem::ChannelId(),
                                                this);
    }


    Library::PrefixId
    Library::AddPrefix(const std::wstring & prefix)
    {

        boost::lock_guard<boost::recursive_mutex> lck(m_prefixSearchLock);
        Prefixes::iterator findIt = std::find(m_prefixes.begin(), m_prefixes.end(), prefix);
        if (findIt != m_prefixes.end())
        {
            return ToPrefixId(*findIt);
        }
        else
        {
            bool enabled = false;
            //Check if FORCE_LOG contains all or <prefix>
            if (find(m_arguments.begin(),m_arguments.end(),L"all") != m_arguments.end() ||
                find(m_arguments.begin(),m_arguments.end(),prefix) != m_arguments.end())
            {
                enabled = true;
            }
            m_prefixes.push_back(PrefixState(prefix,enabled));
            return ToPrefixId(m_prefixes.back());
        }
    }

    volatile bool * Library::GetPrefixStatePointer(const PrefixId prefixId)
    {
        return &ToPrefix(prefixId).m_isEnabled;
    }


    bool Library::IsEnabledPrefix(const PrefixId prefixId) const
    {
        return ToPrefix(prefixId).m_isEnabled;
    }

    void Library::EnablePrefix(const PrefixId prefixId, const bool enabled)
    {
        ToPrefix(prefixId).m_isEnabled = enabled;
    }

    // Private method that assumes the buffer lock is already taken.
    void
    Library::AddToTraceBuf(const PrefixId prefixId,
                           const wchar_t  ch)
    {
        if (m_prefixPending)
        {
            m_traceBuffer.append(ToPrefix(prefixId).m_prefix);
            m_traceBuffer.append(L": ");
            m_prefixPending = false;
        }
        if (ch == '\n')
        {
            m_prefixPending = true;
        }
        m_traceBuffer.push_back(ch);
    }

    void
    Library::Trace(const PrefixId prefixId,
                   const wchar_t ch)
    {
        boost::lock_guard<boost::mutex> lock(m_traceBufferLock);
        AddToTraceBuf(prefixId, ch);
    }

    void
    Library::TraceString(const PrefixId prefixId,
                         const std::wstring & str)
    {
        boost::lock_guard<boost::mutex> lock(m_traceBufferLock);
        std::for_each(str.begin(), str.end(), boost::bind(&Library::AddToTraceBuf,this,prefixId,_1));
    }

    /* todo remove
    void
    Library::TraceSync()
    {
        if (m_flushPending == 0)
        {
            m_flushPending = 1;
            //the functor will keep a reference to this shared_ptr, so we dont need to...
            boost::shared_ptr<boost::asio::deadline_timer> syncTimer
                (new boost::asio::deadline_timer(m_ioService,boost::posix_time::milliseconds(500)));
            syncTimer->async_wait(boost::bind(&Library::HandleTimeout,this,_1,syncTimer));
        }
    }
*/

    void
    Library::TraceFlush()
    {
        boost::lock_guard<boost::mutex> lock(m_traceBufferLock);

        if (m_traceBuffer.empty())
        {
            return;
        }

        // traces are always written to std out
        std::wcout << m_traceBuffer << std::flush;

        m_systemLog.Send(Safir::Utilities::SystemLog::Debug,
                         ToUtf8(m_traceBuffer));

        m_traceBuffer.clear();
    }

/* todo
    inline void
    Library::StartThread()
    {
        if (m_thread == boost::thread())
        {
            boost::lock_guard<boost::mutex> lock(m_threadStartingLock);
            if (m_thread == boost::thread())
            {
                m_thread = boost::thread(boost::bind(&Library::Run,this));
                m_threadId = m_thread.get_id();
            }
        }
    }
*/

    void Library::CrashFunc(const char* const dumpPath)
    {
        std::ostringstream ostr;
        ostr << "An application has crashed! A dump was generated to:\n" 
             << dumpPath;
        std::wcerr << ostr.str().c_str() << std::endl;
        Safir::Utilities::Internal::PanicLogging::Log(ostr.str());
    }

    void
    Library::StartCrashReporting()
    {
        Safir::Utilities::CrashReporter::RegisterCallback(CrashFunc);
        Safir::Utilities::CrashReporter::Start();
    }

    void
    Library::StopCrashReporting()
    {
        Safir::Utilities::CrashReporter::Stop();
    }

/* TODO remove
    void
    Library::StopInternal()
    {
        //We only let one call to Stop try to do the join. otherwise who knows what will happen...
        if (m_thread != boost::thread())
        {
            boost::lock_guard<boost::mutex> lock(m_threadStartingLock);
            if (m_thread != boost::thread())
            {
                m_ioService.stop();
                m_thread.join();
                m_thread = boost::thread();
                m_threadId = boost::thread::id();
            }
        }
    }
    */


/* TODO
    //The swre library thread uses context 0 to connect to the dob. The strange looking negative number
    //is a way to indicate that this is a connection with special privileges.
    const Safir::Dob::Typesystem::Int32 SWRE_LIBRARY_THREAD_CONTEXT = -1000000;

    void
    Library::Run()
    {
        try
        {
            m_connection.reset(new Safir::Dob::Connection());
            Dispatcher dispatcher(*m_connection,m_ioService);

            std::wstring connName = m_programName;
            if (connName.empty())
            {
                Safir::Utilities::ProcessInfo proc(Safir::Utilities::ProcessInfo::GetPid());
                connName = Safir::Dob::Typesystem::Utilities::ToWstring(proc.GetProcessName());
            }
            m_connection->Open(connName,
                               boost::lexical_cast<std::wstring>(Safir::Utilities::ProcessInfo::GetPid()),
                               SWRE_LIBRARY_THREAD_CONTEXT,
                               NULL, 
                               &dispatcher);

            StartBackdoor();
            boost::asio::io_service::work keepRunning(m_ioService);
            m_ioService.run();
            HandleExit();
            m_connection->Close();
            m_connection.reset();
        }
        catch (const std::exception & exc)
        {
            std::wcout << "SwreLibrary caught an unexpected exception!" <<std::endl
                       << "Please report this error to the Safir System Kernel team!" <<std::endl
                       << exc.what() <<std::endl;
        }
        catch (...)
        {
            std::wcout << "SwreLibrary caught an unexpected ... exception!" <<std::endl
                       << "Please report this error to the Safir System Kernel team!" <<std::endl;
        }
        if (m_crashed != 0)
        {
            Safir::Utilities::CrashReporter::Stop();
        }
    }
*/
    void
    Library::OnMessage(const Safir::Dob::MessageProxy messageProxy)
    {
        const Safir::Dob::MessagePtr message = messageProxy.GetMessage();
        try
        {
            const boost::wregex::flag_type regExpFlags = boost::regex::perl | boost::regex::icase;

            const Safir::Application::BackdoorCommandConstPtr cmd =
                boost::static_pointer_cast<Safir::Application::BackdoorCommand>(message);

            if (!cmd->NodeName().IsNull())
            {
                if (!boost::regex_search(Safir::Dob::NodeParameters::Nodes(Safir::Dob::ThisNodeParameters::NodeNumber())->NodeName().GetVal(),
                                         boost::wregex(cmd->NodeName().GetVal(), regExpFlags)))
                {
                    // Node name doesn't match
                    return;  // *** RETURN ***
                }
            }

            if (!cmd->ConnectionName().IsNull())
            {
                Safir::Dob::SecondaryConnection conn;
                conn.Attach();
                Safir::Dob::ConnectionAspectMisc connectionAspectMisc(conn);
                if (!boost::regex_search(connectionAspectMisc.GetConnectionName(), boost::wregex(cmd->ConnectionName().GetVal(), regExpFlags)))

                   // AIWI TODO varför behövs denna koll?
                    //&&
                    //!boost::regex_search(m_programName, boost::wregex(cmd->ConnectionName().GetVal(), regExpFlags)))
                {
                    // Connection name doesn't match
                    return;  // *** RETURN ***
                }
            }

            if (cmd->Command().IsNull())
            {
                // No command given
                return;  // *** RETURN ***
            }

            // Ok, it seems that this PI-command is for this application

            wtokenizer tokenizer(cmd->Command().GetVal(),separator);


            //copy the tokens into a vector

            //Convert the strings to wstrings
            std::vector<std::wstring> cmdTokens;
            std::copy(tokenizer.begin(),tokenizer.end(),std::back_inserter(cmdTokens));

            if (!cmdTokens.empty())
            {
                if (cmdTokens[0] == pingCmd)
                {
                    // It's a 'ping' command.

                    std::wcout << L"Tracer Ping reply" << std::endl;

                    SendProgramInfoReport(L"Tracer Ping reply");

                    return; // *** RETURN ***
                }
                else if (cmdTokens[0] == helpCmd)
                {
                    std::wstring help = GetHelpText();

                    std::wcout << help << std::endl;

                    SendProgramInfoReport(help);

                    return; // *** RETURN ***
                }
            }

            // Let the subclass handle the command
            HandleCommand(cmdTokens);

        }
        catch (const boost::bad_expression& /* e*/ )
        {
            // An invalid regular expression was used, skip this command
            return;  // *** RETURN ***
        }
    }

    void
    Library::HandleCommand(const std::vector<std::wstring>& cmdTokens)
    {
        if (cmdTokens.size() != 2)
        {
            return;
        }

        boost::lock_guard<boost::recursive_mutex> lck(m_prefixSearchLock);
        for (Prefixes::iterator it = m_prefixes.begin();
             it != m_prefixes.end(); ++it)
        {
            if (cmdTokens[0] == it->m_prefix || cmdTokens[0] == L"all")
            {
                if (cmdTokens[1] == L"on")
                {
                    TraceString(ToPrefixId(*it), L": Turning logging on\n");
                    it->m_isEnabled = true;
                }
                else if (cmdTokens[1] == L"off")
                {
                    TraceString(ToPrefixId(*it), L": Turning logging off\n");
                    it->m_isEnabled = false;
                }
                else
                {
                    TraceString(ToPrefixId(*it), std::wstring(L"Got unrecognized command '") + cmdTokens[1] + L"'\n");
                }
            }
        }
        TraceFlush();
    }

    std::wstring
    Library::GetHelpText()
    {
        std::wostringstream out;
        out << "Trace logger supports the following commands: " << std::endl;
        out << "  " << std::setw(10) << "all" << " on/off - Turn logging of all prefices on or off" << std::endl;
        boost::lock_guard<boost::recursive_mutex> lck(m_prefixSearchLock);
        for (Prefixes::iterator it = m_prefixes.begin();
             it != m_prefixes.end(); ++it)
        {
            out << "  " << std::setw (10) << it->m_prefix << " on/off - Turn logging of this prefix on or off. Currently "<<
                (it->m_isEnabled?"on":"off") <<std::endl;
        }

        return out.str();
    }


    Library::PrefixState &
    Library::ToPrefix(const PrefixId prefixId)
    {
        return *reinterpret_cast<PrefixState*>(prefixId);
    }

    Library::PrefixId
    Library::ToPrefixId(PrefixState & prefix)
    {
        return reinterpret_cast<PrefixId>(&prefix);
    }

    void
    Library::SendFatalErrorReport(const std::wstring& errorCode,
                                  const std::wstring& location,
                                  const std::wstring& text)
    {
        m_systemLog.Send(Safir::Utilities::SystemLog::Critical,
                         ToUtf8(m_logCreator.CreateFatalErrorLog(errorCode,
                                                                 location,
                                                                 text)));
    }

    void
    Library::SendErrorReport(const std::wstring& errorCode,
                             const std::wstring& location,
                             const std::wstring& text)
    {
        m_systemLog.Send(Safir::Utilities::SystemLog::Error,
                         Safir::Dob::Typesystem::Utilities::ToUtf8
                         (m_logCreator.CreateErrorLog(errorCode,
                                                      location,
                                                      text)));
    }

    void
    Library::SendResourceReport(const std::wstring& resourceId,
                                const bool          allocated,
                                const std::wstring& text)
    {
        m_systemLog.Send(Safir::Utilities::SystemLog::Warning,
                         Safir::Dob::Typesystem::Utilities::ToUtf8
                         (m_logCreator.CreateResourceLog(resourceId,
                                                         allocated,
                                                         text)));
    }

    void
    Library::SendProgrammingErrorReport(const std::wstring & errorCode,
                                        const std::wstring & location,
                                        const std::wstring & text)
    {
        m_systemLog.Send(Safir::Utilities::SystemLog::Critical,
                         Safir::Dob::Typesystem::Utilities::ToUtf8
                         (m_logCreator.CreateProgrammingErrorLog(errorCode,
                                                                 location,
                                                                 text)));
    }

    void
    Library::SendProgramInfoReport(const std::wstring & text)
    {
        m_systemLog.Send(Safir::Utilities::SystemLog::Debug,
                         ToUtf8(m_logCreator.CreateProgramInfoLog(text)));
    }
}
}
}
