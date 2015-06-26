/******************************************************************************
*
* Copyright Saab AB, 2014 (http://safir.sourceforge.net)
*
* Created by: Anders Widén / anders.widen@consoden.se
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
#include <Safir/Dob/Internal/Communication.h>
#include <Safir/Utilities/ProcessInfo.h>
#include <Safir/Utilities/Internal/SystemLog.h>
#include <Safir/Utilities/Internal/LowLevelLogger.h>
#include <Safir/Utilities/Internal/MakeUnique.h>
#include <Safir/Utilities/CrashReporter.h>
#include <Safir/Dob/Internal/SystemPicture.h>
#include <Safir/Utilities/Internal/Id.h>
#include <iostream>
#include <map>
#include <deque>
#include <atomic>

//disable warnings in boost
#if defined _MSC_VER
#  pragma warning (push)
#  pragma warning (disable : 4100)
#  pragma warning (disable : 4267)
#endif

#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/thread.hpp>

#if defined _MSC_VER
#  pragma warning (pop)
#endif


std::wostream& operator<<(std::wostream& out, const std::string& str)
{
    return out << str.c_str();
}

std::wostream& operator<<(std::wostream& out, const boost::program_options::options_description& opt)
{
    std::ostringstream ostr;
    ostr << opt;
    return out << ostr.str().c_str();
}

class ProgramOptions
{
public:
    ProgramOptions(int argc, char* argv[])
        : parseOk(false)
    {
        using namespace boost::program_options;
        options_description options("Options");
        options.add_options()
            ("help,h", "show help message")
            ("data-address,d",
             value<std::string>(&dataAddress)->default_value("127.0.0.1:40000"),
             "Address and port of the data channel")
            ("name,n",
             value<std::string>(&name)->default_value("<not set>", ""),
             "A nice name for the node, for presentation purposes only")
            ("force-id",
             value<int64_t>(&id)->default_value(LlufId_GenerateRandom64(), ""),
             "Override the automatically generated node id. For debugging/testing purposes only.")
            ("suicide-trigger",
             value<std::string>(&suicideTrigger),
             "This node should exit gracefully a time after the node specified in suicideTrigger has exited");


        variables_map vm;

        try
        {
            store(command_line_parser(argc, argv).
                  options(options).run(), vm);
            notify(vm);
        }
        catch (const std::exception& exc)
        {
            std::wcerr << "Error parsing command line: " << exc.what() << "\n" << std::endl;
            ShowHelp(options);
            return;
        }

        if (vm.count("help"))
        {
            ShowHelp(options);
            return;
        }

        std::wcout << "Got suicide trigger '" << suicideTrigger << "'" << std::endl;

        parseOk = true;
    }

    bool parseOk;

    std::string dataAddress;
    int64_t id;
    std::string name;
    std::string suicideTrigger;
private:
    static void ShowHelp(const boost::program_options::options_description& desc)
    {
        std::wcerr << std::boolalpha
                   << "Usage: dose_main_stub [OPTIONS]\n"
                   << desc << "\n"
                   << std::endl;
}

};

class SystemStateHandler
{
public:
    SystemStateHandler(boost::asio::io_service& ioService,
                       const int64_t id,
                       Safir::Dob::Internal::Com::Communication& communication,
                       const std::string& suicideTrigger)
        : m_strand(ioService)
        , m_injectedNodes(1,id) //consider ourselves already injected
        , m_communication(communication)
        , m_trigger(suicideTrigger)
        , m_suicideTimer(ioService)
        , m_suicideScheduled(false)
    {

    }

    //this must be called before io_services are started
    void SetStopHandler(const std::function<void()>& fcn)
    {
        m_stopHandler = fcn;
    }

    void NewState(const Safir::Dob::Internal::SP::SystemState& data)
    {
        m_strand.dispatch([this,data]
                          {
                              //std::wcout << "Got new SystemState:\n" << data << std::endl;

                              CheckState(data);
                              InjectNodes(data);
                              ConsiderSuicide(data);

                              m_states.push_front(data);
                              while (m_states.size() > 10)
                              {
                                  m_states.pop_back();
                              }
                          });
    }

private:
    void InjectNodes(const Safir::Dob::Internal::SP::SystemState& data)
    {
        for (int i = 0; i < data.Size(); ++i)
        {
            //don't inject dead or already injected nodes
            if (data.IsDead(i) || m_injectedNodes.find(data.Id(i)) != m_injectedNodes.end())
            {
                continue;
            }

            std::wcout << "Injecting node " << data.Name(i) << "(" << data.Id(i)
                     << ") of type " << data.NodeTypeId(i)
                     << " with address " << data.DataAddress(i) << std::endl;

            m_communication.InjectNode(data.Name(i),
                                       data.Id(i),
                                       data.NodeTypeId(i),
                                       data.DataAddress(i));

            m_injectedNodes.insert(data.Id(i));
        }
    }

    void CheckState(const Safir::Dob::Internal::SP::SystemState& data)
    {
        if (!m_states.empty())
        {
            const Safir::Dob::Internal::SP::SystemState last = m_states.front();

            //find nodes that have disappeared
            for (int i = 0; i < last.Size(); ++i)
            {
                bool found = false;
                for (int j = 0; j < data.Size(); ++j)
                {
                    if (last.Id(i) == data.Id(j))
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    std::wcout << "Node " << last.Id(i) << " could not be found in current SS\n"
                             << "Last: \n" << last << "\nCurrent:\n"<< data <<std::endl;
                    throw std::logic_error("Node has gone missing!");
                }
            }

            //find nodes that have been resurrected
            for (int i = 0; i < data.Size(); ++i)
            {
                //only interested in live nodes
                if (data.IsDead(i))
                {
                    return;
                }

                for (int j = 0; j < last.Size(); ++j)
                {
                    if (last.Id(j) == data.Id(i))
                    {
                        if (last.IsDead(j))
                        {
                            std::wcout << "Node " << data.Id(i) << " was dead in last state!\n"
                                     << "Last: \n" << last << "\nCurrent:\n"<< data <<std::endl;
                            throw std::logic_error("Node has been resurrected!");

                        }
                        break;
                    }
                }
            }

            //find duplicates
            std::set<int64_t> ids;
            for (int i = 0; i < data.Size(); ++i)
            {
                if (!ids.insert(data.Id(i)).second)
                {
                    std::wcout << "Node " << data.Id(i) << " is duplicated in the System State!\n"
                             << "State:\n"<< data <<std::endl;
                    throw std::logic_error("Duplicate node in state!");
                }
            }

        }

    }

    void ConsiderSuicide(const Safir::Dob::Internal::SP::SystemState& data)
    {
        if (m_states.empty() || m_trigger.empty() || m_suicideScheduled)
        {
            return;
        }

        //std::wcout << "Considering suicide" << std::endl;

        bool triggered = false;

        //find nodes that have died
        for (int i = 0; i < data.Size(); ++i)
        {
            //look for our trigger node
            if (data.Name(i) != m_trigger)
            {
                continue;
            }

            //we're only interested if it is dead
            if (!data.IsDead(i))
            {
                const auto res = m_triggerHistory.insert(std::make_pair(data.Id(i),false));
                if (res.first->second)
                {
                    throw std::logic_error("Node status changed from dead to alive!");
                }
                continue;
            }

            //std::wcout << "Found dead trigger node " << data.Id(i) << std::endl;

            //check if it was not known about
            const auto findIt = m_triggerHistory.find(data.Id(i));
            if (findIt == m_triggerHistory.end())
            {
                //std::wcout << " most probably from a previous cycle, ignore it." << std::endl;
                m_triggerHistory.insert(std::make_pair(data.Id(i),true));
                continue;
            }

            //if it wasnt previously dead
            if (!findIt->second)
            {
                //std::wcout << " was not previously dead, triggering" << std::endl;
                findIt->second = true; //now known to be dead
                triggered = true;
                break;
            }
        }

        if (triggered)
        {
            std::wcout << "My trigger node (" << m_trigger << "), has died, will schedule a suicide" << std::endl;

            m_suicideTimer.expires_from_now(boost::chrono::seconds(10));
            m_suicideTimer.async_wait([this](const boost::system::error_code& error)
                                      {
                                          if (!error)
                                          {
                                              std::wcout << "Committing suicide!" << std::endl;
                                              m_stopHandler();
                                          }
                                      });
            m_suicideScheduled = true;
        }

    }

    boost::asio::io_service::strand m_strand;
    std::set<int64_t> m_injectedNodes;
    Safir::Dob::Internal::Com::Communication& m_communication;

    std::deque<Safir::Dob::Internal::SP::SystemState> m_states;

    const std::string m_trigger;
    std::map<int64_t, bool> m_triggerHistory;

    boost::asio::steady_timer m_suicideTimer;
    bool m_suicideScheduled;
    std::function<void()> m_stopHandler;
};

int main(int argc, char * argv[])
{
    std::wcout << "Pid: " << Safir::Utilities::ProcessInfo::GetPid() << std::endl;
    //ensure call to CrashReporter::Stop at application exit
    boost::shared_ptr<void> crGuard(static_cast<void*>(0),
                                    [](void*){Safir::Utilities::CrashReporter::Stop();});
    Safir::Utilities::CrashReporter::Start();

    const ProgramOptions options(argc, argv);
    if (!options.parseOk)
    {
        return 1;
    }

    boost::asio::io_service ioService;
    std::atomic<bool> m_stop(false);

    // Make some work to stop io_service from exiting.
    auto work = Safir::make_unique<boost::asio::io_service::work>(ioService);

    std::vector<Safir::Dob::Internal::Com::NodeTypeDefinition> commNodeTypes;
    std::map<int64_t, Safir::Dob::Internal::SP::NodeType> spNodeTypes;

    commNodeTypes.push_back(Safir::Dob::Internal::Com::NodeTypeDefinition(1,
                                                                          "NodeTypeA",
                                                                          "", //no multicast
                                                                          "", //no multicast
                                                                          1000,
                                                                          20,
                                                                          15));

    spNodeTypes.insert(std::make_pair(1,
                                      Safir::Dob::Internal::SP::NodeType(1,
                                                                         "NodeTypeA",
                                                                         false,
                                                                         boost::chrono::milliseconds(1000),
                                                                         15,
                                                                         boost::chrono::milliseconds(20))));

    commNodeTypes.push_back(Safir::Dob::Internal::Com::NodeTypeDefinition(2,
                                                                          "NodeTypeB",
                                                                          "", //no multicast
                                                                          "", //no multicast
                                                                          2000,
                                                                          50,
                                                                          8));

    spNodeTypes.insert(std::make_pair(2,
                                      Safir::Dob::Internal::SP::NodeType(2,
                                                                         "NodeTypeB",
                                                                         false,
                                                                         boost::chrono::milliseconds(2000),
                                                                         8,
                                                                         boost::chrono::milliseconds(50))));



    Safir::Dob::Internal::Com::Communication communication(Safir::Dob::Internal::Com::dataModeTag,
                                                           ioService,
                                                           options.name,
                                                           options.id,
                                                           1,
                                                           options.dataAddress,
                                                           commNodeTypes);




    Safir::Dob::Internal::SP::SystemPicture sp(Safir::Dob::Internal::SP::slave_tag,
                                               ioService,
                                               communication,
                                               options.name,
                                               options.id,
                                               1,
                                               std::move(spNodeTypes));

    SystemStateHandler ssh(ioService, options.id, communication, options.suicideTrigger);

    // Start subscription to system state changes from SP
    sp.StartStateSubscription([&ssh](const Safir::Dob::Internal::SP::SystemState& data){ssh.NewState(data);});

    communication.SetDataReceiver([](const int64_t /*fromNodeId*/,
                                     const int64_t /*fromNodeType*/,
                                     const char* data_,
                                     const size_t size)
                                  {
                                      const boost::shared_ptr<const char[]> data(data_);
                                      if (size != 10000)
                                      {
                                          throw std::logic_error("Received incorrectly sized data!");
                                      }
                                      for (size_t i = 0; i < size; ++i)
                                      {
                                          if (data[i] != 3)
                                          {
                                              throw std::logic_error("Received corrupt data!");
                                          }
                                      }

                                  },
                                  1000100222,
                                  [](size_t size){return new char[size];});

    communication.Start();


    boost::asio::steady_timer sendTimer(ioService);
    bool timerStopped = false;

    const std::function<void(const boost::system::error_code& error)> send =
        [&communication, &sendTimer, &timerStopped, &send](const boost::system::error_code& error)
        {
            if (error || timerStopped)
            {
                return;
            }

            const size_t size = 10000;
            const boost::shared_ptr<char[]> data(new char[size]);
            memset(data.get(), 3, size);
            //send the data to both node types.
            communication.Send(0,1,data,size,1000100222,true);
            communication.Send(0,2,data,size,1000100222,true);

            sendTimer.expires_from_now(boost::chrono::milliseconds(1000));
            sendTimer.async_wait(send);
        };

    sendTimer.expires_from_now(boost::chrono::milliseconds(1000));
    sendTimer.async_wait(send);


    boost::asio::signal_set signalSet(ioService);

#if defined (_WIN32)
    signalSet.add(SIGABRT);
    signalSet.add(SIGBREAK);
    signalSet.add(SIGINT);
    signalSet.add(SIGTERM);
#else
    signalSet.add(SIGQUIT);
    signalSet.add(SIGINT);
    signalSet.add(SIGTERM);
#endif

    const auto stopFcn = [&sp, &communication, &sendTimer, &timerStopped, &work, &signalSet, &m_stop]
    {
        m_stop = true;
        std::wcout << "Stopping SystemPicture" << std::endl;
        sp.Stop();
        std::wcout << "Stopping Communication" << std::endl;
        communication.Stop();
        std::wcout << "Stopping sendTimer" << std::endl;
        timerStopped = true;
        sendTimer.cancel();
        std::wcout << "resetting work" << std::endl;
        work.reset();
        std::wcout << "Cancelling signalSet" << std::endl;
        signalSet.cancel();
    };

    signalSet.async_wait([stopFcn, &m_stop](const boost::system::error_code& error,
                                   const int signal_number)
                         {
                             if (m_stop)
                             {
                                 return;
                             }

                             if (!!error) //fix for ws2012 warning
                             {
                                 SEND_SYSTEM_LOG(Error,
                                                 << "Got a signals error: " << error);
                             }

                             std::wcout << "Got signal " << signal_number << std::endl;
                             stopFcn();
                         }
                         );


    ssh.SetStopHandler(stopFcn);

    std::atomic<bool> success(true);

    const auto run = [&ioService,&success]
        {
            try
            {
                ioService.run();
                std::wcout << "Thread exiting" << std::endl;
                return;
            }
            catch (const std::exception & exc)
            {
                SEND_SYSTEM_LOG(Alert,
                                << "Caught 'std::exception' exception from io_service.run(): "
                                << "  '" << exc.what() << "'.");
                success.exchange(false);
            }
            catch (...)
            {
                SEND_SYSTEM_LOG(Alert,
                                << "Caught '...' exception from io_service.run().");
                success.exchange(false);
            }
        };

    std::wcout << "Launching io_service" << std::endl;

    boost::thread_group threads;
    for (int i = 0; i < 2; ++i)
    {
        threads.create_thread(run);
    }

    run();

    threads.join_all();

    std::wcout << "Exiting..." << std::endl;
    return success ? 0 : 1;
}
