//-*- coding: utf-8 -*-
/******************************************************************************
 *
 * Copyright Saab AB, 2009-2015 (http://safirsdkcore.com)
 *
 * Created by: Joel Ottosson / joot
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
package com.saabgroup.safir.dob.typesystem;

public class EntityIdSequenceContainer
extends ValueSequenceContainer<EntityId> {

    public EntityIdSequenceContainer() {
        super();
    }
    
    public EntityIdSequenceContainer(EntityIdSequenceContainer other) {
        super(other);
    }
    
    /**
     * @see com.saabgroup.safir.dob.typesystem.ValueSequenceContainer#clone()
     */
    @Override
    public EntityIdSequenceContainer clone() {
        return new EntityIdSequenceContainer(this);
    }
}
