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
with Dose_Test.Partner;
with Logger;
with Dose_Test.Action_Enum;
with Dose_Test.Dump;
with Dose_Test.Dump_Result;
with Dose_Test.Parameters;
with Safir.Dob.Entity_Proxies;
with Safir.Dob.Typesystem;
with Safir.Dob.Typesystem.Instance_Id;
with Safir.Dob.Typesystem.Channel_Id;
with Safir.Dob.Typesystem.Utilities; use Safir.Dob.Typesystem.Utilities;
with Safir.Dob.Instance_Id_Policy;
with Safir.Dob.Typesystem.Si_64;
with Safir.Dob.Error_Response;
with Safir.Dob.Typesystem.Object.Factory;
with Safir.Dob.Distribution_Channel_Parameters;
with Unchecked_Conversion;
with Interfaces.C;
with Ada.Strings.Wide_Fixed;
with Ada.Strings.Wide_Unbounded;
with Ada.Exceptions; use Ada.Exceptions;
with GNAT.Sockets; use GNAT.Sockets;
with GNAT.Command_Line;
with Text_IO; use Text_IO;
with Ada.Strings.Wide_Unbounded.Wide_Text_IO; use Ada.Strings.Wide_Unbounded.Wide_Text_IO;
with Ada.Text_IO;
with Ada.Command_Line;

pragma Warnings ("D"); -- turn off warnings for implicit dereference
pragma Warnings ("L"); -- turn off warnings for missing elaboration pragma

package body Executor is

   package C renames Interfaces.C;

   procedure Inhibit_Outgoing_Traffic
     (Inhibit   : in  C.char;
      Success   : out C.char);
   pragma Import (C, Inhibit_Outgoing_Traffic, "InhibitOutgoingTraffic");

   procedure Inhibit_Outgoing_Traffic_Status (Is_Inhibited : out C.char);
   pragma Import (C, Inhibit_Outgoing_Traffic_Status, "InhibitOutgoingTrafficStatus");

   task Action_Reader is
      entry Run (Multicast_Nic : in String);
      entry Stop;
   end Action_Reader;
   Socket : Socket_Type;

   procedure Run is
      use Ada.Strings.Wide_Unbounded;
   begin
      MainLoop.Run (GNAT.Command_Line.Get_Argument); -- program instance
      if Safir.Dob.Distribution_Channel_Parameters.Distribution_Channels (0).Ref.Multicast_Address.Get_Val = "127.0.0.1" then
         Put_Line ("System appears to be Standalone, not listening for multicasted test actions");
      else
         Action_Reader.Run (GNAT.Command_Line.Get_Argument);  -- NIC to be used for multicast reception
      end if;
   end Run;


   procedure On_Do_Dispatch
     (Self : in out Dispatcher) is
   begin
      MainLoop.Dispatch (Self.Conn);
   end On_Do_Dispatch;

   task body Action_Reader is
      use Ada.Strings.Wide_Unbounded;
      use Ada.Streams;
      -- Port and address must correspond to what is used by the test sequencer
      Port : constant Port_Type := 31789;
      Multicast_Addr : constant Inet_Addr_Type := Inet_Addr (To_Utf_8 (Dose_Test.Parameters.Test_Multicast_Address));

      Local_Addr : constant Sock_Addr_Type := (Family => Family_Inet, Addr => Any_Inet_Addr, Port => Port);


      Item : Ada.Streams.Stream_Element_Array (1 .. 65000);
      Last : Ada.Streams.Stream_Element_Offset;
      type Stream_Element_Ptr is access all Ada.Streams.Stream_Element;
      Buf_Ptr : constant Stream_Element_Ptr := Item (1)'Access;

      function To_Char_Star is new Unchecked_Conversion (Stream_Element_Ptr, Safir.Dob.Typesystem.Char_Star);

      Action : Dose_Test.Action.Smart_Pointer;

   begin
      if Safir.Dob.Distribution_Channel_Parameters.Distribution_Channels (0).Ref.Multicast_Address.Get_Val /= "127.0.0.1" then
         accept Run (Multicast_Nic : in String) do

            Create_Socket (Socket, Family_Inet, Socket_Datagram);

            Set_Socket_Option
               (Socket,
                Socket_Level,
                (Reuse_Address, True));

            -- Needed to set the port
            Bind_Socket (Socket, Local_Addr);

            if Multicast_Nic'Length = 0 then
               Set_Socket_Option
                  (Socket,
                   IP_Protocol_For_IP_Level,
                   (Add_Membership, Multicast_Addr, Any_Inet_Addr));

               Put_Line ("NIC is not set ... listen on default interface.");

            else
               Set_Socket_Option
                  (Socket,
                   IP_Protocol_For_IP_Level,
                   (Add_Membership, Multicast_Addr, Inet_Addr (Multicast_Nic)));

               Put_Line ("Used NIC: " & Multicast_Nic);
            end if;

         end Run;

         loop
            begin
               Receive_Socket (Socket, Item, Last);
               if Last = Item'First - 1 then
                  exit;
               end if;

               Action := Dose_Test.Action.Smart_Pointer (Safir.Dob.Typesystem.Object.Factory.Create_Object
                                                            (Blob => To_Char_Star (Buf_Ptr)));

               MainLoop.Handle_Action (Action);
            exception
               when Socket_Error =>
                  exit;
            end;
         end loop;
         accept Stop;
      end if;
   exception when E : others =>
         Ada.Text_IO.Put_Line ("Exception in Action_Reader task: " & Exception_Name (E));
         Ada.Text_IO.Put_Line (Exception_Information (E));
   end Action_Reader;

   task body MainLoop is
      The_Executor : aliased Executor_Type;
      use Ada.Strings.Wide_Unbounded;
   begin
      Put_Line ("Starting");

      The_Executor.Identifier := Ada.Strings.Wide_Unbounded.To_Unbounded_Wide_String ("ada");
      The_Executor.Control_Connection_Name := The_Executor.Identifier & "_control";
      The_Executor.Test_Connection_Name := Ada.Strings.Wide_Unbounded.
        To_Unbounded_Wide_String ("partner_test_connection");

      accept Run (Arg : in String) do
         The_Executor.Instance := Safir.Dob.Typesystem.Int_64'Value (Arg);
         The_Executor.Instance_String := Ada.Strings.Wide_Unbounded.To_Unbounded_Wide_String
           (Ada.Strings.Wide_Fixed.Trim
              (Source => Safir.Dob.Typesystem.Int_64'Wide_Image (The_Executor.Instance),
               Side => Ada.Strings.Both));
         The_Executor.Partner_Entity_Id :=
           Safir.Dob.Typesystem.Entity_Id.Create_Entity_Id
           (Dose_Test.Partner.Class_Type_Id,
            Safir.Dob.Typesystem.Instance_Id.Create_Instance_Id (The_Executor.Instance));
      end Run;

      The_Executor.Control_Connection.Open (The_Executor.Control_Connection_Name,
                                            The_Executor.Instance_String,
                                            0,
                                            The_Executor'Access,
                                            The_Executor.Control_Dispatcher'Access);

      The_Executor.Control_Connection.Subscribe_Message
        (Dose_Test.Action.Class_Type_Id,
         Safir.Dob.Typesystem.Channel_Id.Create_Channel_Id (The_Executor.Instance),
         The_Executor'Access);

      The_Executor.Control_Connection.Subscribe_Message
        (Dose_Test.Action.Class_Type_Id,
         Safir.Dob.Typesystem.Channel_Id.Default_Channel,
         The_Executor'Access);

      Put_Line (The_Executor.Identifier & ":" & Safir.Dob.Typesystem.Int_64'Wide_Image (The_Executor.Instance) & " Started");
      declare
         Connection_To_Dispatch : Connection;
         use type Consumers.Consumer_Access;
      begin
         while not The_Executor.Is_Done loop
            select
               accept Dispatch (Conn : in Connection) do
                  Connection_To_Dispatch := Conn;
               end Dispatch;

               case Connection_To_Dispatch is
                  when Control =>
                     The_Executor.Control_Connection.Dispatch;
                  when Test =>
                     if The_Executor.Dispatch_Test_Connection and The_Executor.Is_Active then
                        The_Executor.Execute_Callback_Actions (Safir.Dob.Callback_Id.On_Do_Dispatch);

                        for I in The_Executor.The_Consumers'Range loop
                           if The_Executor.The_Consumers (I) /= null then
                              The_Executor.The_Consumers (I).Execute_Callback_Actions (Safir.Dob.Callback_Id.On_Do_Dispatch);
                           end if;
                        end loop;

                        The_Executor.Test_Connection.Dispatch;
                     end if;
               end case;

            or
               accept Handle_Action (Action : in Dose_Test.Action.Smart_Pointer) do
                  The_Executor.Handle_Action (Action);
               end Handle_Action;

            or
               delay 0.08;

            end select;
         end loop;

      end;

      if Safir.Dob.Distribution_Channel_Parameters.Distribution_Channels (0).Ref.Multicast_Address.Get_Val /= "127.0.0.1" then
         -- TODO: use a selector in the action_reader task instead of
         -- using this slightly undocumented way of getting receive_socket
         -- to break from blocking
         begin
            Shutdown_Socket (Socket);
         exception
            when Socket_Error =>
               null;
         end;
         Action_Reader.Stop;
      end if;

      --  let the testconnection dispatch too.
      select
         accept Dispatch (Conn : in Connection) do
            pragma Unreferenced (Conn);
            null;
         end Dispatch;
      or
         delay 0.1;
      end select;

      Logger.Put_Line ("Exiting");

      Ada.Command_Line.Set_Exit_Status (0);
   exception
      when E : others =>
         Logger.Put ("Exception in main loop: ");
         Logger.Put_Line (Safir.Dob.Typesystem.Utilities.From_Utf_8 (Ada.Exceptions.Exception_Name (E)));
         Logger.Put_Line (Safir.Dob.Typesystem.Utilities.From_Utf_8 (Ada.Exceptions.Exception_Information (E)));
   end MainLoop;

   overriding
   procedure On_Stop_Order
     (Self : in out Executor_Type) is
   begin
      Logger.Put_Line ("Got stop order");
      Self.Execute_Callback_Actions (Safir.Dob.Callback_Id.On_Stop_Order);
      Self.Is_Done := True;
   end On_Stop_Order;

   overriding
   procedure On_Message
     (Self    : in out Executor_Type;
      Message_Proxy : in Safir.Dob.Message_Proxies.Message_Proxy) is

      Action : constant Dose_Test.Action.Smart_Pointer :=
                 Dose_Test.Action.Smart_Pointer
                   (Safir.Dob.Message_Proxies.Get_Message (Message_Proxy));
   begin
      Self.Execute_Callback_Actions (Safir.Dob.Callback_Id.On_Message);
      Self.Handle_Action (Action);
   end On_Message;

   overriding
   procedure On_Revoked_Registration
     (Self       : in out Executor_Type;
      Type_Id    : in Safir.Dob.Typesystem.Type_Id;
      Handler_Id : in Safir.Dob.Typesystem.Handler_Id.Handler_Id_Type) is
      pragma Unreferenced (Type_Id, Handler_Id);
   begin
      if Self.Is_Active then
         Put_Line ("Deactivating");

         Self.Test_Connection.Close;

         Self.Control_Connection.Unregister_Handler
           (Safir.Dob.Typesystem.Entity_Id.Get_Type_Id (Self.Partner_Entity_Id),
            Safir.Dob.Typesystem.Handler_Id.Create_Handler_Id (Self.Instance));
         Self.Control_Connection.Unregister_Handler
           (Dose_Test.Dump.Class_Type_Id,
            Safir.Dob.Typesystem.Handler_Id.Create_Handler_Id (Self.Instance));

         Self.Is_Active := False;
      end if;
   end On_Revoked_Registration;

   overriding
   procedure On_Create_Request
    (Self                 : in out Executor_Type;
     Entity_Request_Proxy : in Safir.Dob.Entity_Request_Proxies.Entity_Request_Proxy;
     Response_Sender      : in Safir.Dob.Response_Senders.Response_Sender) is

      pragma Unreferenced (Self, Entity_Request_Proxy);

      Response : constant Safir.Dob.Error_Response.Smart_Pointer :=
                   Safir.Dob.Error_Response.Create;
   begin
      Response_Sender.Send (Response);
   end On_Create_Request;

   overriding
   procedure On_Update_Request
    (Self                 : in out Executor_Type;
     Entity_Request_Proxy : in Safir.Dob.Entity_Request_Proxies.Entity_Request_Proxy;
     Response_Sender      : in Safir.Dob.Response_Senders.Response_Sender) is

      pragma Unreferenced (Self, Entity_Request_Proxy);

      Response : constant Safir.Dob.Error_Response.Smart_Pointer :=
                   Safir.Dob.Error_Response.Create;
   begin
      Response_Sender.Send (Response);
   end On_Update_Request;

   overriding
   procedure On_Delete_Request
    (Self                 : in out Executor_Type;
     Entity_Request_Proxy : in Safir.Dob.Entity_Request_Proxies.Entity_Request_Proxy;
     Response_Sender      : in Safir.Dob.Response_Senders.Response_Sender) is

      pragma Unreferenced (Self, Entity_Request_Proxy);

      Response : constant Safir.Dob.Error_Response.Smart_Pointer :=
                   Safir.Dob.Error_Response.Create;
   begin
      Response_Sender.Send (Response);
   end On_Delete_Request;

   overriding
   procedure On_Service_Request
    (Self                  : in out Executor_Type;
     Service_Request_Proxy : in Safir.Dob.Service_Request_Proxies.Service_Request_Proxy;
     Response_Sender       : in Safir.Dob.Response_Senders.Response_Sender) is

      pragma Unreferenced (Self, Service_Request_Proxy);

      Result : constant Dose_Test.Dump_Result.Smart_Pointer := Dose_Test.Dump_Result.Create;
   begin
      Result.Ref.Result.Set_Val (Logger.Get_String);
      Logger.Clear;

      Response_Sender.Send (Result);
   end On_Service_Request;


   function "=" (Left, Right : in Dose_Test.Action.Smart_Pointer) return Boolean is
      pragma Unreferenced (Left, Right);
   begin
      return False;
   end "=";

   procedure Handle_Action (Self : in out Executor_Type;
                            Action : in Dose_Test.Action.Smart_Pointer) is
      use type Safir.Dob.Typesystem.Channel_Id.Channel_Id_Type;
   begin

      if not Action.Ref.Partner.Is_Null and then
        Action.Ref.Partner.Get_Val /= Safir.Dob.Typesystem.Channel_Id.Create_Channel_Id (Self.Instance) then
         return;
      end  if;

      if Action.Ref.Consumer.Is_Null then
         -- No consumer set, meant for the executor.
         if Action.Ref.Action_Callback.Is_Null then
            -- It is a normal action.
            Self.Execute_Action (Action);
         elsif Self.Is_Active then
            Self.Add_Callback_Action (Action);
         end if;
      elsif Self.Is_Active then
         declare
            Consumer : constant Consumers.Consumer_Access :=
              Self.The_Consumers (Integer (Action.Ref.Consumer.Get_Val));
         begin
            if Action.Ref.Action_Callback.Is_Null then
               Consumer.Execute_Action (Action);
            else
               Consumer.Add_Callback_Action (Action);
            end if;
         end;
      end if;
   end Handle_Action;

   procedure Execute_Action
     (Self   : in out Executor_Type;
      Action : in Dose_Test.Action.Smart_Pointer) is

      use type Ada.Strings.Wide_Unbounded.Unbounded_Wide_String;
      use type Safir.Dob.Typesystem.Int_32;
      Partner : Dose_Test.Partner.Smart_Pointer;
      Entity_Proxy : Safir.Dob.Entity_Proxies.Entity_Proxy;
   begin
      case Action.Ref.Action_Kind.Get_Val is

         when Dose_Test.Action_Enum.Activate =>
            if Action.Ref.Identifier.Get_Val = Self.Identifier then
               Self.Default_Context := Action.Ref.Context.Get_Val;
               Put_Line ("Activating (default context is " & Safir.Dob.Typesystem.Int_32'Image (Self.Default_Context) & ")");
               if not Self.Is_Active then

                  Self.Control_Connection.Register_Entity_Handler
                    (Safir.Dob.Typesystem.Entity_Id.Get_Type_Id (Self.Partner_Entity_Id),
                     Safir.Dob.Typesystem.Handler_Id.Create_Handler_Id (Self.Instance),
                     Safir.Dob.Instance_Id_Policy.Handler_Decides_Instance_Id,
                     Self'Access);

                  Self.Control_Connection.Register_Service_Handler
                    (Dose_Test.Dump.Class_Type_Id,
                     Safir.Dob.Typesystem.Handler_Id.Create_Handler_Id (Self.Instance),
                     Self'Access);

                  Partner := Dose_Test.Partner.Create;
                  Partner.Ref.Incarnation.Set_Val (0);
                  Partner.Ref.Identifier.Set_Val (Self.Identifier);

                  Self.Control_Connection.Set_All
                    (Partner,
                     Safir.Dob.Typesystem.Entity_Id.Get_Instance_Id (Self.Partner_Entity_Id),
                     Safir.Dob.Typesystem.Handler_Id.Create_Handler_Id (Self.Instance));

                  Self.Is_Active := True;
               end if;
            end if;

         when Dose_Test.Action_Enum.Deactivate =>
            if Action.Ref.Identifier.Get_Val = Self.Identifier then
               Put_Line ("Deactivating");

               Self.Test_Connection.Close;
               Self.Control_Connection.Delete
                 (Self.Partner_Entity_Id,
                  Safir.Dob.Typesystem.Handler_Id.Create_Handler_Id (Self.Instance));
               Self.Control_Connection.Unregister_Handler
                 (Safir.Dob.Typesystem.Entity_Id.Get_Type_Id (Self.Partner_Entity_Id),
                  Safir.Dob.Typesystem.Handler_Id.Create_Handler_Id (Self.Instance));
               Self.Control_Connection.Unregister_Handler
                 (Dose_Test.Dump.Class_Type_Id,
                  Safir.Dob.Typesystem.Handler_Id.Create_Handler_Id (Self.Instance));
               Self.Is_Active := False;
            end if;

         when Dose_Test.Action_Enum.Reset =>
            if Self.Is_Active then
               Self.Test_Connection.Close;
               Self.Test_Connection.Open
                 (Self.Test_Connection_Name,
                  Self.Instance_String,
                  Self.Default_Context,
                  null,
                  Self.Test_Dispatcher'Access);

               Entity_Proxy := Self.Control_Connection.Read (Self.Partner_Entity_Id);
               Partner := Dose_Test.Partner.Smart_Pointer (Entity_Proxy.Get_Entity);
               Partner.Ref.Incarnation.Set_Val (Partner.Ref.Incarnation.Get_Val + 1);
               Self.Control_Connection.Set_Changes
                 (Partner,
                  Safir.Dob.Typesystem.Entity_Id.Get_Instance_Id (Self.Partner_Entity_Id),
                  Safir.Dob.Typesystem.Handler_Id.Create_Handler_Id (Self.Instance));

               for Index in Self.The_Consumers'Range loop
                  Consumers.Destroy (Self.The_Consumers (Index));
                  Self.The_Consumers (Index) := Consumers.Create (Index, Self.Test_Connection_Name, Self.Instance_String);
               end loop;

               for Index in Safir.Dob.Callback_Id.Enumeration loop
                  Self.Callback_Actions (Index).Clear;
               end loop;
            end if;

         when Dose_Test.Action_Enum.Open =>
            if Self.Is_Active then
               declare
                  Context : Safir.Dob.Typesystem.Int_32 := Self.Default_Context;
                  Conn_Name : Ada.Strings.Wide_Unbounded.Unbounded_Wide_String :=
                     Self.Test_Connection_Name;
               begin
                  if not Action.Ref.Context.Is_Null then
                     Context := Action.Ref.Context.Get_Val;
                  end if;

                  if not Action.Ref.Connection_Name.Is_Null then
                     Conn_Name := Action.Ref.Connection_Name.Get_Val;
                  end if;

                  Self.Test_Connection.Open
                    (Conn_Name,
                     Self.Instance_String,
                     Context,
                     null,
                     Self.Test_Dispatcher'Access);
               end;

            end if;

         when Dose_Test.Action_Enum.Close =>
            if Self.Is_Active then
               Self.Test_Connection.Close;
            end if;

         when Dose_Test.Action_Enum.Inhibit_Dispatch =>
            if Self.Is_Active then
               Self.Dispatch_Test_Connection := not Action.Ref.Inhibit.Get_Val;
               Logger.Put_Line ("InhibitDispatch set to " & Boolean'Wide_Image (Self.Dispatch_Test_Connection));
            end if;

         when Dose_Test.Action_Enum.Inhibit_Outgoing_Traffic =>
            if Self.Is_Active then
               declare
                  Success : C.char;
                  Inhibit : constant C.char := C.char'Val (Boolean'Pos (Action.Ref.Inhibit.Get_Val));
                  Is_Inhibited : C.char;
               begin
                  Inhibit_Outgoing_Traffic (Inhibit, Success);
                  Inhibit_Outgoing_Traffic_Status (Is_Inhibited);
                  Logger.Put_Line ("InhibitOutgoingTraffic set to" & Integer'Wide_Image (C.char'Pos (Is_Inhibited)));
               end;
            end if;

         when Dose_Test.Action_Enum.Print =>
            if Self.Is_Active then
               Logger.Put_Line (Action.Ref.Print_String.Get_Val);
            end if;

         when Dose_Test.Action_Enum.Reset_Callback_Actions =>
            for Index in Safir.Dob.Callback_Id.Enumeration loop
               Self.Callback_Actions (Index).Clear;
            end loop;

         when Dose_Test.Action_Enum.Sleep =>
            if Self.Is_Active then
               Put_Line ("Sleeping " &
                         Safir.Dob.Typesystem.Si_64.Second'Image (Action.Ref.Sleep_Duration.Get_Val) &
                         " seconds");
               delay Duration (Action.Ref.Sleep_Duration.Get_Val);
            end if;


         when Dose_Test.Action_Enum.Check_References |
              Dose_Test.Action_Enum.Close_And_Check_References |
              Dose_Test.Action_Enum.Run_Garbage_Collector =>
            -- These actions are for garbage collected languages only.
            null;

         when others =>
            Logger.Put_Line ("Got unexpected action " &
                             Dose_Test.Action_Enum.Enumeration'Wide_Image (Action.Ref.Action_Kind.Get_Val));

      end case;

   end Execute_Action;


   procedure Add_Callback_Action
     (Self   : in out Executor_Type;
      Action : in Dose_Test.Action.Smart_Pointer) is
   begin
      Self.Callback_Actions (Action.Ref.Action_Callback.Get_Val).Append (Action);
   end Add_Callback_Action;

   procedure Execute_Callback_Actions
     (Self     : in out Executor_Type;
      Callback : in     Safir.Dob.Callback_Id.Enumeration) is

      procedure Execute (Position : in Action_Vectors.Cursor) is
      begin
         Execute_Action (Self, Action_Vectors.Element (Position));
      end Execute;

   begin
      Self.Callback_Actions (Callback).Iterate (Execute'Access);
   end Execute_Callback_Actions;

end Executor;
