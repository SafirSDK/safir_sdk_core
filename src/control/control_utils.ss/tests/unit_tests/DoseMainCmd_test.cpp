/******************************************************************************
*
* Copyright Saab AB, 2014 (http://safir.sourceforge.net)
* Copyright Consoden AB, 2014 (http://www.consoden.se)
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
#include <Safir/Dob/Internal/DoseMainCmd.h>
#include <boost/thread.hpp>

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4267)
#endif

#include <boost/asio.hpp>

#ifdef _MSC_VER
#pragma warning (pop)
#endif

#define BOOST_TEST_MODULE DoseMainCmdTest
#include <boost/test/unit_test.hpp>

using namespace Safir::Dob::Internal::Control;


BOOST_AUTO_TEST_CASE( send_inject_node )
{
    boost::asio::io_service pubIoService;
    boost::asio::io_service subIoService;

    boost::shared_ptr<boost::asio::io_service::work> pubWork (new boost::asio::io_service::work(pubIoService));
    boost::shared_ptr<boost::asio::io_service::work> subWork (new boost::asio::io_service::work(subIoService));

    boost::thread_group threads;
    threads.create_thread([&pubIoService](){pubIoService.run();});
    threads.create_thread([&subIoService](){subIoService.run();});

    bool injectOwnNodeCmdReceived = false;
    bool injectNodeCmdReceived = false;

    DoseMainCmdSender cmdSender(pubIoService,

                                [&cmdSender]()
                                {
                                    // cmd receiver has connected

                                    cmdSender.InjectOwnNode(12345,
                                                            "Kalle",
                                                            54321,
                                                            121212,
                                                            "192.168.211.10");

                                    cmdSender.InjectNode(54321,
                                                         "Olle",
                                                         99999,
                                                         88888,
                                                         "192.168.213.55");

                                    cmdSender.StopDoseMain(45);

                                });

    DoseMainCmdReceiver cmdReceiver(subIoService,

                                    // Inject own node callback
                                    [&injectOwnNodeCmdReceived](int64_t requestId,
                                       const std::string& nodeName,
                                       int64_t nodeId,
                                       int64_t nodeTypeId,
                                       const std::string& dataAddress)
                                    {
                                        injectOwnNodeCmdReceived = true;

                                        BOOST_CHECK(requestId == 12345);
                                        BOOST_CHECK(nodeName == "Kalle");
                                        BOOST_CHECK(nodeId == 54321);
                                        BOOST_CHECK(nodeTypeId == 121212);
                                        BOOST_CHECK(dataAddress == "192.168.211.10");
                                    },

                                    // Inject node callback
                                    [&injectNodeCmdReceived](int64_t requestId,
                                       const std::string& nodeName,
                                       int64_t nodeId,
                                       int64_t nodeTypeId,
                                       const std::string& dataAddress)
                                    {
                                        injectNodeCmdReceived = true;

                                        BOOST_CHECK(requestId == 54321);
                                        BOOST_CHECK(nodeName == "Olle");
                                        BOOST_CHECK(nodeId == 99999);
                                        BOOST_CHECK(nodeTypeId == 88888);
                                        BOOST_CHECK(dataAddress == "192.168.213.55");
                                    },

                                    // Stop dose_main
                                    [&cmdSender, &cmdReceiver, &pubWork, &subWork,
                                     &injectOwnNodeCmdReceived, &injectNodeCmdReceived](int64_t requestId)
                                    {
                                        BOOST_CHECK(requestId == 45);
                                        BOOST_CHECK(injectOwnNodeCmdReceived);
                                        BOOST_CHECK(injectNodeCmdReceived);

                                        cmdSender.Stop();
                                        cmdReceiver.Stop();
                                        pubWork.reset();
                                        subWork.reset();
                                    }

                                    );
    cmdSender.Start();
    cmdReceiver.Start();

    threads.join_all();

}
