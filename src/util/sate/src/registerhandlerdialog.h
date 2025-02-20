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
#pragma once

#include <QDialog>
#include <Safir/Dob/InstanceIdPolicy.h>
#include <Safir/Dob/Typesystem/HandlerId.h>

namespace Ui {
class RegisterHandlerDialog;
}

class RegisterHandlerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterHandlerDialog(QWidget *parent);
    ~RegisterHandlerDialog();

    // For Entities use allFieldsVisible=true, for serviceReq false
    void Show(int64_t typeId, bool allFieldsVisible = true);

    int64_t TypeId() const;
    Safir::Dob::Typesystem::HandlerId Handler() const;
    bool Pending() const;
    bool InjectionHandler() const;
    Safir::Dob::InstanceIdPolicy::Enumeration InstancePolicy() const;

private:
    Ui::RegisterHandlerDialog *ui;
    int64_t m_typeId = 0;
};
