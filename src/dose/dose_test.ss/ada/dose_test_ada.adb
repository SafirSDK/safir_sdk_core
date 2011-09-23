-------------------------------------------------------------------------------
--
--  Copyright Saab AB, 2009 (http://www.safirsdk.com)
--
--  Created by: Anders Wid�n / stawi
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
with Ada.Exceptions;
with Executor;
with Logger;
with Safir.Dob.Typesystem.Utilities;

procedure Dose_Test_Ada is
begin
   Executor.Run;
exception
   when E : others =>
      Logger.Put ("Caught some exception: ");
      Logger.Put_Line (Safir.Dob.Typesystem.Utilities.From_Utf_8 (Ada.Exceptions.Exception_Name (E)));
end Dose_Test_Ada;
