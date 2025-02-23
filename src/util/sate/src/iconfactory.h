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

#include "typesystemrepository.h"
#include <QIcon>
#include <map>

class IconFactory
{
public:

    enum Modifier {None, Register, Pending, Subscribe, SubscribeRecursive};

    static QIcon GetNamespaceIcon();
    static QIcon GetEnumIcon();
    static QIcon GetSearchIcon();
    static QIcon GetIcon(TypesystemRepository::DobBaseClass baseClass, Modifier modifier = IconFactory::None);

private:
    static std::map<QString, QIcon> m_icons;
};
