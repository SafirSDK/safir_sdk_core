/******************************************************************************
*
* Copyright Saab AB, 2005-2008 (http://www.safirsdk.com)
* 
* Created by: Joel Ottosson / stjoot
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

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace Sate
{
    public partial class DobUnitDetailsForm : Form
    {
        Int64 typeId;
        public DobUnitDetailsForm(Int64 typeId)//, Safir.Dob.Typesystem.InstanceId instanceId)
        {
            InitializeComponent();
            //objId = oid;
                 this.typeId = typeId;
                  //this.instanceId = instanceId;
            //if (oid.Instance==Safir.Dob.Typesystem.Constants.WHOLE_CLASS)
            {
                ClassDetails();
            }
#if STSYLI
            else
            {
                ObjectDetails();
            }
#endif
        }

#if STSYLI
        private void ObjectDetails()
        {
            //window text
#if STSYLI
            Text = "Details [object " + Safir.Dob.Typesystem.Operations.GetName(objId.TypeId) + " : " + objId.Instance + "]";
#endif
            //size

            //reg owner

            //subscribers           

        }
#endif

        private void ClassDetails()
        {
            //window text
            Text = "Details [class " + Safir.Dob.Typesystem.Operations.GetName(typeId) + "]";

            //number of instances

            //owner

            //subscribers

        }
    }
}
