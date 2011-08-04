------------------------------------------------------------------------------
--                                                                          --
--                           B U S _ D E V I C E S                          --
--                                                                          --
--                                  B o d y                                 --
--                                                                          --
--                        Copyright (C) 2011, AdaCore                       --
--                                                                          --
-- This program is free software;  you can redistribute it and/or modify it --
-- under terms of  the GNU General Public License as  published by the Free --
-- Softwareg Foundation;  either version 3,  or (at your option)  any later --
-- version. This progran is distributed in the hope that it will be useful, --
-- but  WITHOUT  ANY  WARRANTY;   without  even  the  implied  warranty  of --
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     --
--                                                                          --
-- As a special exception under Section 7 of GPL version 3, you are granted --
-- additional permissions  described in the GCC  Runtime Library Exception, --
-- version 3.1, as published by the Free Software Foundation.               --
--                                                                          --
-- You should have received a copy  of the GNU General Public License and a --
-- copy of the  GCC Runtime Library Exception along  with this program; see --
-- the  files  COPYING3  and  COPYING.RUNTIME  respectively.  If  not,  see --
-- <http://www.gnu.org/licenses/>.                                          --
--                                                                          --
------------------------------------------------------------------------------

with Ada.Unchecked_Conversion;
with Interfaces.C; use Interfaces.C;
with Ada.Task_Identification;

package body Bus_Devices is

   use Event_Container;

   procedure Device_Init_Callback (Opaque : Opaque_Ptr);
   pragma Convention (C, Device_Init_Callback);

   procedure Device_Reset_Callback (Opaque : Opaque_Ptr);
   pragma Convention (C, Device_Reset_Callback);

   procedure Device_Exit_Callback (Opaque : Opaque_Ptr);
   pragma Convention (C, Device_Exit_Callback);

   procedure Internal_Event_Callback (Opaque      : Opaque_Ptr;
                                      Event_Id    : Bus_Data;
                                      Expire_Time : Bus_Time);
   pragma Convention (C, Internal_Event_Callback);

   function Opaque_Ptr_To_BD_Ref is new Ada.Unchecked_Conversion
     (Opaque_Ptr, Bus_Device_Ref);

   function BD_Ref_To_Opaque_Ptr is new Ada.Unchecked_Conversion
     (Bus_Device_Ref, Opaque_Ptr);

   ---------------
   -- Add_Event --
   ---------------

   procedure Add_Event (Self   : in out Bus_Device;
                        Expire : Bus_Time;
                        Event  : Device_Event_Callback_Access) is

      Event_Id : constant Bus_Data := Self.Event_Id_Count;
   begin
      Self.Event_Id_Count := Self.Event_Id_Count + 1;

      Self.Insert_Event (Event_Id, Event);
      BusInterface.Add_Event (Self.Internal_Device,
                              Expire,
                              Event_Id,
                              Internal_Event_Callback'Access);
   end Add_Event;

   --------------------------
   -- Device_Exit_Callback --
   --------------------------

   procedure Device_Exit_Callback (Opaque : Opaque_Ptr) is
      BD : constant Bus_Device_Ref := Opaque_Ptr_To_BD_Ref (Opaque);
   begin
      BD.Device_Exit;
   end Device_Exit_Callback;

   --------------------------
   -- Device_Init_Callback --
   --------------------------

   procedure Device_Init_Callback (Opaque : Opaque_Ptr) is
      BD : constant Bus_Device_Ref := Opaque_Ptr_To_BD_Ref (Opaque);
   begin
      BD.Device_Init;
   end Device_Init_Callback;

   --------------
   -- Get_Time --
   --------------

   function Get_Time (Self : Bus_Device) return Bus_Time is
   begin
      return BusInterface.Get_Time (Self.Internal_Device);
   end Get_Time;

   -------------------
   -- Hash_Event_Id --
   -------------------

   function Hash_Event_Id
     (Event_Id : Bus_Data) return Ada.Containers.Hash_Type is
   begin
      return Ada.Containers.Hash_Type (Event_Id);
   end Hash_Event_Id;

   ---------------
   -- IRQ_Lower --
   ---------------

   procedure IRQ_Lower (Self : in out Bus_Device; Line : IRQ_Line) is
   begin
      BusInterface.IRQ_Lower (Self.Internal_Device, Line);
   end IRQ_Lower;

   ---------------
   -- Callbacks --
   ---------------

   function IO_Read_Callback
     (Opaque  : Opaque_Ptr;
      Address : Bus_Address;
      Length  : Bus_Address)
     return Bus_Data;
   pragma Convention (C, IO_Read_Callback);

   procedure  IO_Write_Callback
     (Opaque  : Opaque_Ptr;
      Address : Bus_Address;
      Length  : Bus_Address;
      Value   : Bus_Data);
   pragma Convention (C, IO_Write_Callback);

   ------------------------
   -- Register_IO_Memory --
   ------------------------

   procedure Register_IO_Memory (Self            : in out Bus_Device;
                                 Address, Length : Bus_Address) is
   begin
      BusInterface.Register_IO_Memory (Self.Internal_Device, Address, Length);
   end Register_IO_Memory;

   -----------
   -- Start --
   -----------

   procedure Start (Self : in out Bus_Device) is
   begin
      Self.Internal_Device := BusInterface.Allocate_Device;

      BusInterface.Set_Device_Info
        (Self.Internal_Device,
         BD_Ref_To_Opaque_Ptr (Self'Unchecked_Access),
         Self.Vendor_Id,
         Self.Device_Id,
         "TEMP NAME",
         "TEMP DESC");

      Bus_Device'Class (Self).Device_Setup;

      BusInterface.Register_Callbacks (Self.Internal_Device,
                                       IO_Read_Callback'Access,
                                       IO_Write_Callback'Access,
                                       Device_Init_Callback'Access,
                                       Device_Reset_Callback'Access,
                                       Device_Exit_Callback'Access);

      Self.Loop_Task := new Device_Loop_Task;
      Self.Loop_Task.Start (Self'Unchecked_Access);
   end Start;

   ----------
   -- Stop --
   ----------

   procedure Stop (Self : in out Bus_Device) is
   begin
      Self.Stop_Request := True;
   end Stop;

   ----------------------
   -- Wait_Termination --
   ----------------------

   procedure Wait_Termination (Self : in out Bus_Device) is
   begin

      if Ada.Task_Identification.Is_Terminated (Self.Loop_Task'Identity) then
         return;
      end if;

      Self.Stop;
      Self.Loop_Task.Wait_Stop;
   end Wait_Termination;

   ----------
   -- Kill --
   ----------

   procedure Kill (Self : in out Bus_Device) is
   begin
      Ada.Task_Identification.Abort_Task (Self.Loop_Task'Identity);
      Cleanup_Device (Self.Internal_Device);
   end Kill;

   ------------------
   -- Start_Plugin --
   ------------------

   procedure Start_Plugin (Self : in out Bus_Device) is
      error : Interfaces.C.int;
      pragma Unreferenced (error);
   begin
      Self.Internal_Device := BusInterface.Allocate_Device;

      BusInterface.Set_Device_Info
        (Self.Internal_Device,
         BD_Ref_To_Opaque_Ptr (Self'Unchecked_Access),
         Self.Vendor_Id,
         Self.Device_Id,
         "TEMP NAME",
         "TEMP DESC");

      Bus_Device'Class (Self).Device_Setup;

      BusInterface.Register_Callbacks (Self.Internal_Device,
                                       IO_Read_Callback'Access,
                                       IO_Write_Callback'Access,
                                       Device_Init_Callback'Access,
                                       Device_Reset_Callback'Access,
                                       Device_Exit_Callback'Access);

      error := BusInterface.Register_Device (Self.Internal_Device,
                                            Bus_Port (Self.Port));
   end Start_Plugin;

   ----------------------
   -- Device_Loop_Task --
   ----------------------

   task body Device_Loop_Task is
      My_Device : Bus_Device_Ref;
   begin
      accept Start (Device : Bus_Device_Ref) do
         My_Device := Device;
      end Start;

      while not My_Device.Stop_Request loop
         if BusInterface.Register_Device (My_Device.all.Internal_Device,
                                          Bus_Port (My_Device.all.Port)) = 0
         then
            --  The device is connected
            BusInterface.Device_Loop (My_Device.all.Internal_Device);
            Cleanup_Device (My_Device.all.Internal_Device);
         else
            --  Connection failure
            My_Device.Stop_Request := True;
         end if;
      end loop;

      select
         accept Wait_Stop;
      else
         null;
      end select;
   end Device_Loop_Task;

   ---------------
   -- IRQ_Raise --
   ---------------

   procedure IRQ_Raise (Self : in out Bus_Device; Line : IRQ_Line) is
   begin
      BusInterface.IRQ_Raise (Self.Internal_Device, Line);
   end IRQ_Raise;

   ---------------
   -- IRQ_Pulse --
   ---------------

   procedure IRQ_Pulse (Self : in out Bus_Device; Line : IRQ_Line) is
   begin
      BusInterface.IRQ_Pulse (Self.Internal_Device, Line);
   end IRQ_Pulse;

   ------------------
   -- Insert_Event --
   ------------------

   procedure Insert_Event (Self     : in out Bus_Device;
                           Event_Id : Bus_Data;
                           Event    : Device_Event_Callback_Access) is
   begin
      Self.Event_Hash.Insert (Event_Id, Event);
   end Insert_Event;

   ------------------
   -- Delete_Event --
   ------------------

   procedure Delete_Event (Self : in out Bus_Device; Event_Id : Bus_Data) is
   begin
      Self.Event_Hash.Delete (Event_Id);
   end Delete_Event;

   ----------------
   -- Find_Event --
   ----------------

   function Find_Event (Self : Bus_Device; Event_Id : Bus_Data)
                       return Device_Event_Callback_Access is

      Cursor : Event_Container.Cursor;
   begin
      Cursor := Self.Event_Hash.Find (Event_Id);

      if not Has_Element (Cursor) then
         return null;
      else
         return Element (Cursor);
      end if;
   end Find_Event;

   -----------------------------
   -- Internal_Event_Callback --
   -----------------------------

   procedure Internal_Event_Callback (Opaque      : Opaque_Ptr;
                                      Event_Id    : Bus_Data;
                                      Expire_Time : Bus_Time) is

      BD : constant Bus_Device_Ref := Opaque_Ptr_To_BD_Ref (Opaque);
      My_Event : Device_Event_Callback_Access;
   begin
      My_Event := BD.Find_Event (Event_Id);

      if My_Event /= null then
         BD.Delete_Event (Event_Id);
         My_Event (BD, Expire_Time);
      end if;

   end Internal_Event_Callback;

   ----------------------
   -- IO_Read_Callback --
   ----------------------

   function IO_Read_Callback (Opaque  : Opaque_Ptr;
                              Address : Bus_Address;
                              Length  : Bus_Address) return Bus_Data is

      BD    : constant Bus_Device_Ref := Opaque_Ptr_To_BD_Ref (Opaque);
      Value : Bus_Data;
   begin
      BD.IO_Read (Address, Length, Value);
      return Value;
   end IO_Read_Callback;

   -----------------------
   -- IO_Write_Callback --
   -----------------------

   procedure IO_Write_Callback (Opaque  : Opaque_Ptr;
                                Address : Bus_Address;
                                Length  : Bus_Address;
                                Value   : Bus_Data) is

      BD : constant Bus_Device_Ref := Opaque_Ptr_To_BD_Ref (Opaque);
   begin
      BD.IO_Write (Address, Length, Value);
   end IO_Write_Callback;

   ---------------------------
   -- Device_Reset_Callback --
   ---------------------------

   procedure Device_Reset_Callback (Opaque : Opaque_Ptr) is
      BD : constant Bus_Device_Ref := Opaque_Ptr_To_BD_Ref (Opaque);
   begin
      BD.Device_Reset;
   end Device_Reset_Callback;

end Bus_Devices;
