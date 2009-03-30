-------------------------------------------------------------------------------
--
--  Copyright Saab AB, 2005-2008 (http://www.safirsdk.com)
--
--  Created by: Erik Adolfsson / sterad
--
-------------------------------------------------------------------------------
--
--  This file is part of Safir SDK Core.
--
--  Safir SDK Core is free software: you can redistribute it and/or modify
--  it under the terms of version 3 of the GNU General Public License as
--  published by the Free Software Foundation.
--
--  Safir SDK Core is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU General Public License for more details.
--
--  You should have received a copy of the GNU General Public License
--  along with Safir SDK Core.  If not, see <http://www.gnu.org/licenses/>.
--
-------------------------------------------------------------------------------
with Dose_Pkg;
with Safir.Dob.Typesystem;
with Safir.Test.TimeConversion;

procedure Douf_Time_Test_Program_receive is
   objId : Safir.Dob.Typesystem.ObjectId;
begin

   Dose_Pkg.Start;

   objId.TypeId := Safir.Test.TimeConversion.ClassId;
   objId.Instance := Safir.Dob.Typesystem.WHOLE_CLASS;

   Dose_Pkg.Subscribe_Entity (objId.TypeId, objId.Instance);

   while True loop
      delay 5.0;
   end loop;

end Douf_Time_Test_Program_Receive;


