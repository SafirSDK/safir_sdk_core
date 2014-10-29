/******************************************************************************
*
* Copyright Saab AB, 2013 (http://safir.sourceforge.net)
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
//need this for setResizeMode
#define QT_DISABLE_DEPRECATED_BEFORE 0x000000

#include "common_header.h"
#include "RawStatisticsPage.h"
#include <iostream>
#include <set>
#include <boost/make_shared.hpp>
namespace
{
    enum TableColumn
    {
        COLUMN_NAME,
        COLUMN_ID,
        COLUMN_NODE_TYPE,
        COLUMN_CONTROL_ADDRESS,
        COLUMN_DATA_ADDRESS,
        COLUMN_CONTROL_RECEIVE_COUNT,
        COLUMN_CONTROL_RETRANSMIT_COUNT,
        COLUMN_DATA_RECEIVE_COUNT,
        COLUMN_DATA_RETRANSMIT_COUNT,

        NUM_COLUMNS
    };

    void SetDead(QTableWidget* table, const int row)
    {
        if (table->item(row, COLUMN_NAME)->background() != QColor(200,200,200))
        {
            //std::wcout << "Set row " << row << " to dead" << std::endl;
            for (int column = 0; column < NUM_COLUMNS; ++column)
            {
                table->item(row, column)->setBackground(QColor(200,200,200));
                table->item(row, column)->setToolTip("Node is dead");
            }
        }
    }

    template <class T>
    void SetText(T* widget, const std::string& str_)
    {
        const QString str = QString::fromUtf8(str_.c_str());
        if (widget->text() != str)
        {
            widget->setText(str);
        }

    }

    template <class T>
    void SetText(T* widget, const char* str_)
    {
        const QString str = QString::fromUtf8(str_);
        if (widget->text() != str)
        {
            widget->setText(str);
        }
    }

    template <class T, class U>
    void SetText(T* widget, const U number)
    {
        const QString str = QString::number(number);
        if (widget->text() != str)
        {
            widget->setText(str);
        }
    }


    std::map<boost::int64_t, int> GetAllIds(const Safir::Dob::Internal::SP::RawStatistics& statistics)
    {
        std::map<boost::int64_t,int> ids;
        const int size = statistics.Size();
        for (int i = 0; i < size; ++i)
        {
            ids.insert(std::make_pair(statistics.Id(i),i));
        }
        return ids;
    }

}

RawStatisticsPage::RawStatisticsPage(QWidget* /*parent*/)
    : m_systemPicture(Safir::Dob::Internal::SP::subscriber_tag,
                      m_ioService)
{
    m_systemPicture.StartRawSubscription([this](const Safir::Dob::Internal::SP::RawStatistics& data)
                                         {UpdatedStatistics(data);});

    setupUi(this);
    connect(localTable, SIGNAL(itemSelectionChanged()), this, SLOT(LocalTableSelectionChanged()));

    localTable->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    localTable->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    localTable->sortItems(COLUMN_NAME);

    remoteTable->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    remoteTable->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    remoteTable->sortItems(COLUMN_NAME);

    tableSplitter->setStretchFactor(0,2);
    tableSplitter->setStretchFactor(1,1);

    connect(&m_ioServicePollTimer,SIGNAL(timeout()), this, SLOT(PollIoService()));
    m_ioServicePollTimer.start(100);
    PollIoService();
}


void RawStatisticsPage::closeEvent(QCloseEvent* event)
{
    m_systemPicture.Stop();

    //run the ioservice until all SystemPicture stuff is completed.
    m_ioService.reset();
    m_ioService.run();

    event->accept();
}


void RawStatisticsPage::UpdatedStatistics(const Safir::Dob::Internal::SP::RawStatistics& data)
{
    m_statistics = data;

    SetText(name,data.Name());
    SetText(address,data.ControlAddress());
    SetText(id,data.Id());

    localTable->setSortingEnabled(false);
    UpdateLocalTable();
    localTable->setSortingEnabled(true);

    remoteTable->setSortingEnabled(false);
    UpdateRemoteTable();
    remoteTable->setSortingEnabled(true);
}


void RawStatisticsPage::UpdateLocalTable()
{
    //starts off containing all ids in the statistics message
    //but in the first loop we remove all that we already have in the table
    auto ids = GetAllIds(m_statistics);

    //start by removing rows that are no longer in statistics
    //and updating existing rows
    for (int row = localTable->rowCount() - 1; row >= 0; --row)
    {
        const qlonglong id = localTable->item(row, COLUMN_ID)->text().toLongLong();

        const auto findIt = ids.find(id);
        if (findIt == ids.end())
        {
            localTable->removeRow(row);
        }
        else
        {
            SetText(localTable->item(row,COLUMN_CONTROL_RECEIVE_COUNT),
                    m_statistics.ControlReceiveCount(findIt->second));

            SetText(localTable->item(row,COLUMN_CONTROL_RETRANSMIT_COUNT),
                    m_statistics.ControlRetransmitCount(findIt->second));

            SetText(localTable->item(row,COLUMN_DATA_RECEIVE_COUNT),
                    m_statistics.DataReceiveCount(findIt->second));

            SetText(localTable->item(row,COLUMN_DATA_RETRANSMIT_COUNT),
                    m_statistics.DataRetransmitCount(findIt->second));

            ids.erase(id);

            if (m_statistics.IsDead(findIt->second))
            {
                SetDead(localTable, row);
            }
        }
    }

    //add new ids
    for(auto it = ids.begin(); it != ids.end(); ++it)
    {
        const int row = localTable->rowCount();
        localTable->insertRow(row);
        localTable->setItem(row,
                            COLUMN_NAME,
                            new QTableWidgetItem(QString::fromUtf8(m_statistics.Name(it->second).c_str())));
        localTable->setItem(row,
                            COLUMN_ID,
                            new QTableWidgetItem(QString::number(it->first)));
        localTable->setItem(row,
                            COLUMN_NODE_TYPE,
                            new QTableWidgetItem(QString::number(m_statistics.NodeTypeId(it->second))));
        localTable->setItem(row,
                            COLUMN_CONTROL_ADDRESS,
                            new QTableWidgetItem(QString::fromUtf8(m_statistics.ControlAddress(it->second).c_str())));
        localTable->setItem(row,
                            COLUMN_DATA_ADDRESS,
                            new QTableWidgetItem(QString::fromUtf8(m_statistics.DataAddress(it->second).c_str())));

        localTable->setItem(row,
                             COLUMN_CONTROL_RECEIVE_COUNT,
                             new QTableWidgetItem(QString::number(m_statistics.ControlReceiveCount(it->second))));
        localTable->setItem(row,
                             COLUMN_CONTROL_RETRANSMIT_COUNT,
                             new QTableWidgetItem(QString::number(m_statistics.ControlRetransmitCount(it->second))));

        localTable->setItem(row,
                             COLUMN_DATA_RECEIVE_COUNT,
                             new QTableWidgetItem(QString::number(m_statistics.DataReceiveCount(it->second))));
        localTable->setItem(row,
                             COLUMN_DATA_RETRANSMIT_COUNT,
                             new QTableWidgetItem(QString::number(m_statistics.DataRetransmitCount(it->second))));

        for (int column = 0; column < NUM_COLUMNS; ++column)
        {
            localTable->item(row, column)->setToolTip("Node is alive");
        }

        if (m_statistics.IsDead(it->second))
        {
            SetDead(localTable,row);
        }
    }
}


void RawStatisticsPage::LocalTableSelectionChanged()
{
    if (m_statistics.Valid())
    {
        //remoteTable->clear();
        remoteTable->setSortingEnabled(false);
        UpdateRemoteTable();
        remoteTable->setSortingEnabled(true);

    }
}

void RawStatisticsPage::UpdateRemoteTable()
{
    const auto selection = localTable->selectedItems();
    if (selection.empty())
    {
        while(remoteTable->rowCount() > 0)
        {
            remoteTable->removeRow(0);
        }
    }
    else
    {
        const int row = selection[0]->row();
        const qlonglong id = localTable->item(row, COLUMN_ID)->text().toLongLong();

        const int size = m_statistics.Size();
        for (int i = 0; i < size; ++i)
        {
            if (id == m_statistics.Id(i))
            {
                if(!m_statistics.HasRemoteStatistics(i))
                {
                    remoteTable->setRowCount(0);
                    remoteTable->setEnabled(false);
                    remoteTable->setToolTip("There is no remote data for the selected node");
                    return;
                }
                remoteTable->setEnabled(true);
                remoteTable->setToolTip("");

                const auto statistics = m_statistics.RemoteStatistics(i);

                //starts off containing all ids in the statistics message
                //but in the first loop we remove all that we already have in the table
                auto ids = GetAllIds(statistics);

                //start by removing rows that are no longer in statistics
                //and updating existing rows
                for (int row = remoteTable->rowCount() - 1; row >= 0; --row)
                {
                    const qlonglong id = remoteTable->item(row, COLUMN_ID)->text().toLongLong();

                    const auto findIt = ids.find(id);
                    if (findIt == ids.end())
                    {
                        remoteTable->removeRow(row);
                    }
                    else
                    {
                        SetText(remoteTable->item(row,COLUMN_CONTROL_RECEIVE_COUNT),
                                statistics.ControlReceiveCount(findIt->second));

                        SetText(remoteTable->item(row,COLUMN_CONTROL_RETRANSMIT_COUNT),
                                statistics.ControlRetransmitCount(findIt->second));

                        SetText(remoteTable->item(row,COLUMN_DATA_RECEIVE_COUNT),
                                statistics.DataReceiveCount(findIt->second));

                        SetText(remoteTable->item(row,COLUMN_DATA_RETRANSMIT_COUNT),
                                statistics.DataRetransmitCount(findIt->second));

                        ids.erase(id);

                        if (statistics.IsDead(findIt->second))
                        {
                            SetDead(remoteTable, row);
                        }
                    }
                }

                //add new ids
                for(auto it = ids.begin(); it != ids.end(); ++it)
                {
                    const int row = remoteTable->rowCount();
                    remoteTable->insertRow(row);
                    remoteTable->setItem(row,
                                         COLUMN_NAME,
                                         new QTableWidgetItem(QString::fromUtf8(statistics.Name(it->second).c_str())));
                    remoteTable->setItem(row,
                                         COLUMN_ID,
                                         new QTableWidgetItem(QString::number(it->first)));
                    remoteTable->setItem(row,
                                         COLUMN_NODE_TYPE,
                                         new QTableWidgetItem(QString::number(statistics.NodeTypeId(it->second))));
                    remoteTable->setItem(row,
                                         COLUMN_CONTROL_ADDRESS,
                                         new QTableWidgetItem(QString::fromUtf8(statistics.ControlAddress(it->second).c_str())));
                    remoteTable->setItem(row,
                                         COLUMN_DATA_ADDRESS,
                                         new QTableWidgetItem(QString::fromUtf8(statistics.DataAddress(it->second).c_str())));

                    remoteTable->setItem(row,
                                         COLUMN_CONTROL_RECEIVE_COUNT,
                                         new QTableWidgetItem(QString::number(statistics.ControlReceiveCount(it->second))));
                    remoteTable->setItem(row,
                                         COLUMN_CONTROL_RETRANSMIT_COUNT,
                                         new QTableWidgetItem(QString::number(statistics.ControlRetransmitCount(it->second))));
                    remoteTable->setItem(row,
                                         COLUMN_DATA_RECEIVE_COUNT,
                                         new QTableWidgetItem(QString::number(statistics.DataReceiveCount(it->second))));
                    remoteTable->setItem(row,
                                         COLUMN_DATA_RETRANSMIT_COUNT,
                                         new QTableWidgetItem(QString::number(statistics.DataRetransmitCount(it->second))));

                    for (int column = 0; column < NUM_COLUMNS; ++column)
                    {
                        remoteTable->item(row, column)->setToolTip("Node is alive");
                    }


                    if (statistics.IsDead(it->second))
                    {
                        SetDead(remoteTable,row);
                    }
                }
            }
        }

    }
}

void RawStatisticsPage::PollIoService()
{
    m_ioService.poll_one();
    m_ioService.reset();
}
