/******************************************************************************
*
* Copyright Saab AB, 2024 (http://safirsdkcore.com)
*
* Created by: Joel Ottosson
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
#include "typesystemcontextmenuhandler.h"
#include "registerhandlerdialog.h"
#include <QPoint>
#include <QCursor>
#include <QMenu>
#include <QDebug>
#include <QMessageBox>

TypesystemContextMenuHandler::TypesystemContextMenuHandler(DobHandler* dob, QTreeView* parent)
    : QObject(parent)
    , m_dob(dob)
    , m_treeView(parent)
    , m_registerDlg(new RegisterHandlerDialog(parent))
    , m_subscribeDlg(new SubscribeDialog(parent))
{
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QTreeView::customContextMenuRequested, [this](const QPoint&){
        auto ix = m_treeView->currentIndex();
        if (ix.isValid())
        {
            auto tid = ix.data(TypesystemRepository::DobTypeIdRole);
            auto bc = ix.data(TypesystemRepository::DobBaseClassRole);

            if (tid.isValid())
            {
                if (bc.isValid())
                {
                    // A class type
                    CreateContextMenu(tid.toLongLong(), static_cast<TypesystemRepository::DobBaseClass>(bc.toInt()));
                }
                else
                {
                    // Valid typeId but no baseClass, must be an Enum
                    QMenu menu;
                    menu.setToolTipsVisible(true);
                    int64_t typeId = tid.toLongLong();
                    auto* openDouFile = new QAction(tr("Open dou-file"), &menu);
                    openDouFile->setToolTip(tr("Open the dou file of this Enum type."));
                    menu.addAction(openDouFile);
                    connect(openDouFile, &QAction::triggered, this, [this, typeId]{ emit OpenDouFile(typeId);});
                    menu.exec(m_treeView->cursor().pos());
                }
            }
        }
    });

    // Handle registration dialog
    connect(m_registerDlg, &QDialog::accepted, this, [this]
    {
        auto typeId = m_registerDlg->TypeId();
        if (TypesystemRepository::Instance().GetClass(typeId)->dobBaseClass == TypesystemRepository::Entity)
        {
            m_dob->RegisterEntityHandler(typeId, m_registerDlg->Handler(), m_registerDlg->InstancePolicy(), m_registerDlg->Pending(), m_registerDlg->InjectionHandler());
        }
        else
        {
            m_dob->RegisterServiceHandler(typeId, m_registerDlg->Handler(), m_registerDlg->Pending());
        }
    } );

    // Handle subscription dialog
    connect(m_subscribeDlg, &QDialog::accepted, this, [this]
    {
        auto typeId = m_subscribeDlg->TypeId();
        if (TypesystemRepository::Instance().GetClass(typeId)->dobBaseClass == TypesystemRepository::Message)
        {
            emit OpenMessageInstanceViewer(typeId, m_subscribeDlg->Channel(), m_subscribeDlg->IncludeSubclasses());
            m_dob->SubscribeMessage(typeId, m_subscribeDlg->Channel(), m_subscribeDlg->IncludeSubclasses());
        }
    } );
}

void TypesystemContextMenuHandler::CreateContextMenu(int64_t typeId, TypesystemRepository::DobBaseClass baseClass)
{
    QMenu menu;
    menu.setToolTipsVisible(true);

    //create all the actions
    auto* subscribeEntity = new QAction(tr("Subscribe"), &menu);
    auto* subscribeEntityRecursive = new QAction(tr("Subscribe recursive"), &menu);
    auto* openObjectEditor = new QAction(tr("Edit new instance"), &menu);
    auto* registerDefaultEntityHandler = new QAction(tr("Register default handler"), &menu);
    auto* registerEntityHandlerEllipsis = new QAction(tr("Register handler..."), &menu);
    auto* unregisterAllHandlers = new QAction(tr("Unregister handlers"), &menu);
    auto* unsubscribeEntity = new QAction(tr("Unsubscribe"), &menu);
    auto* subscribeMessage = new QAction(tr("Subscribe"), &menu);
    auto* subscribeMessageEllipsis = new QAction(tr("Subscribe..."), &menu);
    auto* unsubscribeMessage = new QAction(tr("Unsubscribe"), &menu);
    auto* registerDefaultServiceHandler = new QAction(tr("Register default handler"), &menu);
    auto* registerServiceHandlerEllipsis = new QAction(tr("Register handler..."), &menu);
    auto* subscribeServiceRegistrations = new  QAction(tr("Subscribe registration"), &menu);
    auto* unsubscribeServiceRegistrations = new  QAction(tr("Unsubscribe registration"), &menu);
    auto* openDouFile = new QAction(tr("Open dou-file"), &menu);

    //set up tooltips
    subscribeEntity->setToolTip(tr("Subscribe to this entity type and open a view that displays all instances of it.\n"
                                   "Also subscribes to registrations.\n"
                                   "Shortcut: double click"));
    subscribeEntityRecursive->setToolTip(tr("Subscribe to this entity type and all its subclasses, and open a view "
                                            "that displays all instances of it.\n"
                                            "Shortcut: shift+double click"));
    openObjectEditor->setToolTip(tr("Open a new empty instance of this type in an object edit view.\n"
                                    "Shortcut: ctrl+double click"));

    registerDefaultEntityHandler->setToolTip(tr("Register the DEFAULT_HANDLER for this entity type as a\n"
                                                "non-pending, non-injection, RequestorDecidesInstanceId handler."));
    registerEntityHandlerEllipsis->setToolTip(tr("Register a handler for this entity type. Opens a dialog to allow\n"
                                                 "different options to be selected."));
    unregisterAllHandlers->setToolTip(tr("Unregister all handlers for this type."));
    unsubscribeEntity->setToolTip(tr("Unsubscribe to this entity and all subclasses."));
    subscribeMessage->setToolTip(tr("Subscribe to all channels of this message and all its subclasses."));
    subscribeMessageEllipsis->setToolTip(tr("Subscribe to this message type. Opens a dialog to allow\n"
                                            "different options to be selected."));
    unsubscribeMessage->setToolTip(tr("Unsubscribe all channels of this message and its subclasses."));
    registerDefaultServiceHandler->setToolTip(tr("Register the DEFAULT_HANDLER for this service type, as a\n"
                                                 "non-pending handler."));
    registerServiceHandlerEllipsis->setToolTip(tr("Register a handler for this service type. Opens a dialog to allow\n"
                                                  "different options to be selected."));
    subscribeServiceRegistrations->setToolTip(tr("Subscribe to registrations of this type and all its subclasses."));
    unsubscribeServiceRegistrations->setToolTip(tr("Unsubscribe to registrations of this type and all its subclasses."));
    openDouFile->setToolTip(tr("Open the dou file of this type."));

    //Create the menu
    switch(baseClass) {
    case TypesystemRepository::Entity:
        {
            menu.addAction(openObjectEditor);
            menu.addAction(registerDefaultEntityHandler);
            menu.addAction(registerEntityHandlerEllipsis);
            menu.addAction(unregisterAllHandlers);
            menu.addSeparator();
            menu.addAction(subscribeEntity);
            menu.addAction(subscribeEntityRecursive);
            menu.addAction(unsubscribeEntity);
            menu.addSeparator();
            menu.addAction(openDouFile);
        }
        break;
    case TypesystemRepository::Message:
        {
            menu.addAction(openObjectEditor);
            menu.addAction(subscribeMessage);
            menu.addAction(subscribeMessageEllipsis);
            menu.addAction(unsubscribeMessage);
            menu.addSeparator();
            menu.addAction(openDouFile);
        }
        break;
    case TypesystemRepository::Service:
        {
            menu.addAction(openObjectEditor);
            menu.addAction(registerDefaultServiceHandler);
            menu.addAction(registerServiceHandlerEllipsis);
            menu.addAction(unregisterAllHandlers);
            menu.addSeparator();
            menu.addAction(subscribeServiceRegistrations);
            menu.addAction(unsubscribeServiceRegistrations);
            menu.addSeparator();
            menu.addAction(openDouFile);
        }
        break;

    case TypesystemRepository::Response:
    case TypesystemRepository::Parametrization:
    case TypesystemRepository::Item:
    case TypesystemRepository::Struct:
    case TypesystemRepository::Object:
        {
            menu.addAction(openObjectEditor);

            menu.addSeparator();

            menu.addAction(openDouFile);
        }
        break;
    }


    //execute the menu and run the actions
    auto* chosenAction = menu.exec(m_treeView->cursor().pos());

    if (chosenAction == subscribeEntity)
    {
        emit OpenEntityInstanceViewer(typeId, false);
        m_dob->SubscribeEntity(typeId, Safir::Dob::Typesystem::InstanceId(), false);
    }
    else if (chosenAction == subscribeEntityRecursive)
    {
        emit OpenEntityInstanceViewer(typeId, true);
        m_dob->SubscribeEntity(typeId, Safir::Dob::Typesystem::InstanceId(), true);
    }
    else if (chosenAction == openObjectEditor)
    {
        emit OpenObjectEdit(typeId);
    }
    else if (chosenAction == registerDefaultEntityHandler)
    {
        m_dob->RegisterEntityHandler(typeId,
                                     sdt::HandlerId(),
                                     Safir::Dob::InstanceIdPolicy::RequestorDecidesInstanceId,
                                     false,
                                     false);
    }
    else if (chosenAction == registerEntityHandlerEllipsis ||
             chosenAction == registerServiceHandlerEllipsis)
    {
        m_registerDlg->Show(typeId);
    }
    else if (chosenAction == unregisterAllHandlers)
    {
        m_dob->Unregister(typeId);
    }
    else if (chosenAction == unsubscribeEntity)
    {
        m_dob->UnsubscribeRegistrations(typeId);
        m_dob->UnsubscribeEntity(typeId);
    }
    else if (chosenAction == subscribeMessage)
    {
        emit OpenMessageInstanceViewer(typeId, sdt::ChannelId::ALL_CHANNELS, false);
        m_dob->SubscribeMessage(typeId, sdt::ChannelId::ALL_CHANNELS, false);
    }
    else if (chosenAction == subscribeMessageEllipsis)
    {
        m_subscribeDlg->Show(typeId);
    }
    else if (chosenAction == unsubscribeMessage)
    {
        m_dob->UnsubscribeMessage(typeId);
    }
    else if (chosenAction == registerDefaultServiceHandler)
    {
        m_dob->RegisterServiceHandler(typeId, sdt::HandlerId(), false);
    }
    else if (chosenAction == subscribeServiceRegistrations)
    {
        m_dob->SubscribeRegistrations(typeId, sdt::HandlerId::ALL_HANDLERS, true);
    }
    else if (chosenAction == unsubscribeServiceRegistrations)
    {
        m_dob->UnsubscribeRegistrations(typeId);
    }
    else if (chosenAction == openDouFile)
    {
        emit OpenDouFile(typeId);
    }
}
