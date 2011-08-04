------------------------------------------------------------------------------
--                                                                          --
--                           B U S _ D E V I C E S                          --
--                                                                          --
--                                  S p e c                                 --
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

with Ada.Containers.Hashed_Maps;
with BusInterface; use BusInterface;
with Bus_Types;    use Bus_Types;

package Bus_Devices is

   type Bus_Device (Vendor_Id, Device_Id : Id;
                    Port                 : Integer)
      is abstract tagged limited private;

   type Bus_Device_Ref is access all Bus_Device'Class;

   type Device_Event_Callback_Access is
     access procedure (Device : Bus_Device_Ref; Expire_Time : Bus_Time);

   ----------------------
   -- Device Interface --
   ----------------------

   --  All methods in this section must be overridden

   procedure IO_Read
     (Self    : in out Bus_Device;
      Address : Bus_Address;
      Length  : Bus_Address;
      Value   : out Bus_Data) is abstract;

   procedure IO_Write
     (Self    : in out Bus_Device;
      Address : Bus_Address;
      Length  : Bus_Address;
      Value   : Bus_Data) is abstract;

   procedure Device_Setup (Self : in out Bus_Device) is abstract;

   procedure Device_Init (Self : in out Bus_Device) is abstract;

   procedure Device_Reset (Self : in out Bus_Device) is abstract;

   procedure Device_Exit (Self : in out Bus_Device) is abstract;

   --------------------
   -- Bus Operations --
   --------------------

   procedure Register_IO_Memory (Self            : in out Bus_Device;
                                 Address, Length : Bus_Address);

   procedure IRQ_Raise (Self : in out Bus_Device; Line : IRQ_Line);
   procedure IRQ_Lower (Self : in out Bus_Device; Line : IRQ_Line);
   procedure IRQ_Pulse (Self : in out Bus_Device; Line : IRQ_Line);

   function Get_Time (Self : Bus_Device) return Bus_Time;

   procedure Add_Event (Self   : in out Bus_Device;
                        Expire : Bus_Time;
                        Event  : Device_Event_Callback_Access);

   -------------------------
   -- Device task control --
   -------------------------

   procedure Start (Self : in out Bus_Device);
   --  Start the device

   procedure Stop (Self : in out Bus_Device);
   --  Send a stop signal and return

   procedure Wait_Termination  (Self : in out Bus_Device);
   --  Send a stop signal and wait until device termination

   procedure Kill (Self : in out Bus_Device);
   --  Kill the device

   ----------------------------
   -- Device Plug-in control --
   ----------------------------

   procedure Start_Plugin (Self : in out Bus_Device);

private

   -----------------------
   --  Device_Loop_Task --
   -----------------------

   --  This task will run the device loop in background

   task type Device_Loop_Task is
      entry Start (Device : Bus_Device_Ref);
      entry Wait_Stop;
   end Device_Loop_Task;

   type Device_Loop_Task_Access is access Device_Loop_Task;

   ---------------------
   -- Event container --
   ---------------------

   function Hash_Event_Id
     (Event_Id : Bus_Data) return Ada.Containers.Hash_Type;

   package Event_Container is new
     Ada.Containers.Hashed_Maps (Bus_Data,
                                 Device_Event_Callback_Access,
                                 Hash_Event_Id, "=", "=");

   type Bus_Device (Vendor_Id, Device_Id : Id;
                    Port                 : Integer)
      is abstract tagged limited record

         Internal_Device : Internal_Bus_Device;
         --  This is a pointer to the C Device struct. We don't need to access
         --  it but we have to pass it as the first arguement of
         --  bus_interface.h functions.

         Event_Id_Count : Bus_Data := 0;
         --  Provides a simple way to have unique event id for a device

         Event_Hash : Event_Container.Map;

         Loop_Task  : Device_Loop_Task_Access;

         Stop_Request : Boolean := False;
         --  When Stop_Request is True, the loop task will stop as soon as
         --  possible, i.e. at the next disconnection from bus master.

   end record;

   procedure Insert_Event (Self     : in out Bus_Device;
                           Event_Id : Bus_Data;
                           Event    : Device_Event_Callback_Access);

   procedure Delete_Event (Self : in out Bus_Device; Event_Id : Bus_Data);

   function Find_Event (Self : Bus_Device; Event_Id : Bus_Data)
                       return Device_Event_Callback_Access;

end Bus_Devices;
