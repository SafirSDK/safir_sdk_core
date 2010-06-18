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

#include <iostream>
#include <Safir/Dob/Typesystem/Operations.h>
#include <Safir/Dob/Connection.h>
#include <Safir/Dob/Consumer.h>
#include "TestCaseReader.h"
#include "dose_test_sequencer.h"

#if defined _MSC_VER
  #pragma warning (push)
  #pragma warning (disable : 4127 4512 4702 4267)
#endif

#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <ace/Thread.h>
#include <boost/random.hpp>
#include <boost/filesystem/path.hpp>

#if defined _MSC_VER
  #pragma warning (pop)
#endif

#include <ace/Reactor.h>
#include <ace/OS_NS_unistd.h>

namespace po = boost::program_options;


class StopHandler : public Safir::Dob::StopHandler
{
    virtual void OnStopOrder()
    {
        std::wcout << "Got stop order!" <<std::endl;
        exit(0);
    }
};


class Dispatcher:
    public Safir::Dob::Dispatcher,
    public ACE_Event_Handler,
    private boost::noncopyable
{
public:
    explicit Dispatcher(Safir::Dob::Connection & connection):m_connection(connection) {}
private:
    virtual void OnDoDispatch() {ACE_Reactor::instance()->notify(this);}
    virtual int handle_exception(ACE_HANDLE){m_connection.Dispatch(); return 0;}

    Safir::Dob::Connection & m_connection;
};



std::string GetRandomLanguage()
{
#if 1
    static const char * languages[] = {"cpp", "ada", "java", "dotnet"};
    return languages[rand()%4];
#else
    static const char * languages[] = {"cpp", "dotnet", "java"};
    return languages[rand()%3];
#endif
}

int GetRandomContext()
{
    return rand()%2;
}

void DumpAction(const DoseTest::ActionConstPtr currentAction) {

    if (!currentAction->Partner().IsNull())
    {
        std::wcout << "P" << currentAction->Partner().GetVal();
    }
    if (!currentAction->Consumer().IsNull())
    {
        std::wcout << "," << "C" << currentAction->Consumer().GetVal();
    }
    if (!currentAction->ActionKind().IsNull())
    {
        if (!currentAction->Partner().IsNull())
        {
            std::wcout << ",";
        }

        std::wcout << DoseTest::ActionEnum::ToString(currentAction->ActionKind().GetVal()) << "(";
        bool first = true;

        if (!currentAction->Context().IsNull())
        {
            if (!first)
                std::wcout << ",";
            first = false;
            std::wcout  << currentAction->Context().GetVal();
        }        

        if (!currentAction->Channel().IsNull())
        {
            if (!first)
                std::wcout << ",";
            first = false;
            std::wcout << currentAction->Channel().GetVal().ToString();
        }
        if (!currentAction->Handler().IsNull())
        {
            if (!first)
                std::wcout << ",";
            first = false;
            std::wcout << currentAction->Handler().GetVal().ToString();
        }
        if (!currentAction->TypeId().IsNull())
        {
            if (!first)
                std::wcout << ",";
            first = false;
            const std::wstring name = Safir::Dob::Typesystem::Operations::GetName(currentAction->TypeId().GetVal());
            const std::wstring::size_type index = name.find_last_of('.');
            std::wcout << name.substr(index + 1, name.length() - index);
        }
        std::wcout << ")";
    }

    std::wcout << std::endl; 
}

void DumpTestcaseDetailedData(const int first,
                              const int last)
{

    for (int testcaseNo = first; testcaseNo <= last; ++testcaseNo)
    {
        DoseTest::Items::TestCaseConstPtr tc = TestCaseReader::Instance().GetTestCase(testcaseNo);

        if (tc == NULL)
        {
            if (first == last)
            {
                std::wcout << std::endl << "TESTCASE " << testcaseNo << "  (null)" << std::endl; 
            }
            continue;
        }

        std::wcout << std::endl << "---------------------------------------------------------" << std::endl;
        std::wcout << std::endl << "TESTCASE " << testcaseNo << std::endl;
        std::wcout << "Description: " << tc->Description().GetVal() << std::endl;
        std::wcout  << "Expectation: " << tc->Expectation().GetVal() << std::endl;

        std::wcout << std::endl << "Setup actions:" << std::endl; 
        for (int i = 0; i < DoseTest::Items::TestCase::TestCaseSetupActionsArraySize(); ++i)
        {
            if (tc->TestCaseSetupActions()[i].IsNull())
            {
                continue;
            }

            DumpAction(tc->TestCaseSetupActions()[i].GetPtr());
        }


        std::wcout << std::endl << "Test actions:" << std::endl; 
        for (int i = 0; i < DoseTest::Items::TestCase::TestActionsArraySize(); ++i)
        {
            if (tc->TestActions()[i].IsNull())
            {
                continue;
            }

            DumpAction(tc->TestActions()[i].GetPtr());
        }
    }

}

void DumpTestcaseData(const int first,
                      const int last,
                      const bool descriptions,
                      const bool expectations)
{
    for (int i = first; i <= last; ++i)
    {
        DoseTest::Items::TestCaseConstPtr tc = TestCaseReader::Instance().GetTestCase(i);

        if (tc == NULL)
        {
            continue;
        }

        std::wcout << std::endl << "TESTCASE " << i;
        std::wcout << std::endl;
        if (descriptions)
        {
            std::wcout << "Description: " << tc->Description().GetVal() << std::endl;
        }
        if (expectations)
        {
            std::wcout  << "Expectation: " << tc->Expectation().GetVal() << std::endl;
        }
    }
}

const std::string GetTestcaseDir()
{
    const char * SAFIR_RUNTIME = getenv("SAFIR_RUNTIME");
    boost::filesystem::path path = SAFIR_RUNTIME;
    path /= "data";
    path /= "text";
    path /= "dose_test";
    path /= "testcases";
    return path.string();
}


struct CommandLineResults
{
    int first;
    int last;
    Languages languages;
    int repeats;
    bool randomLanguages;
    std::string testcaseDirectory;
    bool noTimeout;
    int testcaseNo;
    int context;
};

const CommandLineResults & HandleCommandLine(int argc, char* argv[])
{
    try
    {
        static CommandLineResults results;

        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "show help message")
            ("testcase-directory,d",po::value<std::string>(&results.testcaseDirectory),"directory that contains the test cases")
            ("descriptions","show descriptions")
            ("expectations", "show expectations")
            ("description", po::value<int>(&results.testcaseNo), "Detailed description of the given testcase (-1 = use first/last args)")
            ("repeats", po::value<int>(&results.repeats)->default_value(1), "number of times to run tests")
            ("languages,l", po::value<std::vector<std::string> >(&results.languages)->multitoken()->default_value(Languages(3,"cpp"),"cpp cpp cpp"), "choose languages to run, e.g.\n--languages cpp ada java or \n--languages random")
            ("first", po::value<int>(&results.first)->default_value(0), "first testcase")
            ("last", po::value<int>(&results.last)->default_value(9999), "last testcase")
            ("no-timeout", "Do not time out and exit if a partner does not respond for a long time")
            ("context", po::value<int>(&results.context)->default_value(0), "default context for partner test connection (-1 for random)");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::ostringstream ostr;
            ostr << desc;
            std::wcout << ostr.str().c_str() << std::endl;
            exit(0);
        }
        
        TestCaseReader::Initialize(results.testcaseDirectory);

        if (results.testcaseDirectory.empty())
        {
            results.testcaseDirectory = GetTestcaseDir();
        }

        const bool descriptions = vm.count("descriptions") != 0;
        const bool expectations = vm.count("expectations") != 0;
        const bool description = vm.count("description") != 0;

        if (descriptions || expectations)
        {
            DumpTestcaseData(results.first,results.last,descriptions,expectations);
            exit(0);
        }

        if (description)
        {
            if (results.testcaseNo < 0){
                DumpTestcaseDetailedData(results.first,results.last);
            }
            else
            {
                DumpTestcaseDetailedData(results.testcaseNo, results.testcaseNo);
            }
            exit(0);
        }

        results.noTimeout = vm.count("no-timeout") != 0;

        results.randomLanguages = results.languages.size() == 1 && results.languages[0] == "random";

        if (results.randomLanguages)
        {
            std::wcout << "Using random languages" << std::endl;
            results.languages.resize(3,"");
        }
        else if (results.languages.size() != 3)
        {
            std::wcout << "Need 3 languages or 'random'"<<std::endl;
            exit(0);
        }

        std::wcout << "Testcase range is " << results.first << " to " << results.last << std::endl;
        return results;
    }
    catch (const std::exception & e)
    {
        std::wcout << "Got exception while parsing command line: "<< std::endl
            <<e.what() <<std::endl;
        exit(-1);
    }

}


int main(int argc, char* argv[])
{
    //random numbers are used in command line handling and inside the sequencer, so seed the generator here.
    srand(static_cast<unsigned int>(time(NULL)));

    const CommandLineResults & commandLine = HandleCommandLine(argc,argv);


    try
    {
        const std::wstring nameCommonPart = L"Sequencer";
        const std::wstring nameInstancePart = L"";

        StopHandler stopHandler;

        Safir::Dob::Connection connection;
        Dispatcher dispatcher(connection);
        std::wcout << nameCommonPart.c_str() <<  nameInstancePart.c_str() << ": Started" <<std::endl;

        connection.Open(nameCommonPart,
                        nameInstancePart,
                        0, // Context
                        &stopHandler,
                        &dispatcher);

        //take ownership of the reactor:
        ACE_Reactor::instance()->owner(ACE_Thread::self());

        for (int i = 0; i < commandLine.repeats; ++i)
        {
            std::wcout << "Test run " << i << ", Languages: ";
            Languages languages = commandLine.languages;
            int context = commandLine.context;

            if (commandLine.randomLanguages)
            {
                for (Languages::iterator it = languages.begin();
                     it != languages.end(); ++it)
                {
                    *it = GetRandomLanguage();
                }
            }
            for (Languages::iterator it = languages.begin();
                it != languages.end(); ++it)
            {
                std::wcout << it->c_str() << " ";
            }

            if (commandLine.context == -1)
            {
                // random context
                context = GetRandomContext();
            }

            std::wcout << "Context: " << context;

            std::wcout << std::endl;

            Sequencer sequencer(commandLine.first, commandLine.last, languages, commandLine.noTimeout, context);
            while (!sequencer.IsFinished())
            {
                ACE_Time_Value time(ACE_Time_Value(0,150000)); //150ms
                ACE_Reactor::instance()->run_reactor_event_loop(time);
                sequencer.Tick();
            }
            ACE_OS::sleep(1);
            sequencer.GetTestResults(i);
            while (!sequencer.GotTestResults())
            {
                ACE_Time_Value time(ACE_Time_Value(0,100000)); //100ms
                ACE_Reactor::instance()->run_reactor_event_loop(time);
            }


            while (!sequencer.DeactivateAll())
            {
                ACE_Time_Value time(ACE_Time_Value(0,100000)); //100ms
                ACE_Reactor::instance()->run_reactor_event_loop(time);
            }


            std::wcout << "\n" << std::endl; //two newlines
        }

        connection.Close();
        std::wcout << "End" << std::endl;
    }
    catch(std::exception & e)
    {
        std::wcout << "Caught std::exception! Contents of exception is:" << std::endl
                   << e.what()<<std::endl;
    }
    catch (...)
    {
        std::wcout << "Caught ... exception!" << std::endl;
    }

    return 0;
}

