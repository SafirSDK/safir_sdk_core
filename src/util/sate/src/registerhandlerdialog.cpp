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
#include "registerhandlerdialog.h"
#include "ui_registerhandlerdialog.h"

#include <Safir/Dob/Typesystem/Operations.h>


RegisterHandlerDialog::RegisterHandlerDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterHandlerDialog)
{
    ui->setupUi(this);
    setModal(true);
    this->setFixedSize(this->width(),this->height());
}

RegisterHandlerDialog::~RegisterHandlerDialog()
{
    delete ui;
}

void RegisterHandlerDialog::Show(int64_t typeId, bool allFieldsVisible)
{
    m_typeId = typeId;
    ui->nameLabel->setText(QString::fromStdWString(Safir::Dob::Typesystem::Operations::GetName(typeId)));
    ui->requestorRb->setChecked(false);
    ui->handlerRb->setChecked(true);
    ui->pendingCb->setChecked(false);
    ui->injectionCb->setChecked(false);
    ui->handlerIdEdit->setText("DEFAULT_HANDLER");

    // Only show fields needed to register a service
    ui->instanceIdPolicyGroupBox->setVisible(allFieldsVisible);
    ui->injectionCb->setVisible(allFieldsVisible);

    show();
}

int64_t RegisterHandlerDialog::TypeId() const
{
    return m_typeId;
}

Safir::Dob::Typesystem::HandlerId RegisterHandlerDialog::Handler() const
{
    auto h = ui->handlerIdEdit->text();
    if (h.isEmpty())
    {
        return Safir::Dob::Typesystem::HandlerId();
    }

    bool ok;
    int64_t num = h.toLongLong(&ok, 10);
    return ok ? Safir::Dob::Typesystem::HandlerId(num) : Safir::Dob::Typesystem::HandlerId(h.toStdWString());
}

bool RegisterHandlerDialog::Pending() const
{
    return ui->pendingCb->isChecked();

}
bool RegisterHandlerDialog::InjectionHandler() const
{
    return ui->injectionCb->isChecked();
}

Safir::Dob::InstanceIdPolicy::Enumeration RegisterHandlerDialog::InstancePolicy() const
{
    return ui->requestorRb->isChecked() ? Safir::Dob::InstanceIdPolicy::RequestorDecidesInstanceId : Safir::Dob::InstanceIdPolicy::HandlerDecidesInstanceId;
}
