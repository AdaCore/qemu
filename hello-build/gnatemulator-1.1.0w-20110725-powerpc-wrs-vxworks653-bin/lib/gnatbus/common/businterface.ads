------------------------------------------------------------------------------
--                                                                          --
--                          B U S I N T E R F A C E                         --
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

with Bus_Types; use Bus_Types;
with Interfaces.C;

package BusInterface is

   type Internal_Bus_Device is access Integer;
   type Opaque_Ptr          is access Integer;

   --------------------------------
   -- Callbacks types definition --
   --------------------------------

   type IO_Read_Access is access function
     (Opaque  : Opaque_Ptr;
      Address : Bus_Address;
      Length  : Bus_Address)
     return Bus_Data;
   pragma Convention (C, IO_Read_Access);

   type IO_Write_Access is access procedure
     (Opaque  : Opaque_Ptr;
      Address : Bus_Address;
      Length  : Bus_Address;
      Value   : Bus_Data);
   pragma Convention (C, IO_Write_Access);

   type Device_Init_Access is access procedure (Opaque : Opaque_Ptr);
   pragma Convention (C, Device_Init_Access);

   type Device_Reset_Access is access procedure (Opaque : Opaque_Ptr);
   pragma Convention (C, Device_Reset_Access);

   type Device_Exit_Access is access procedure (Opaque : Opaque_Ptr);
   pragma Convention (C, Device_Exit_Access);

   type Event_Callback_Access is access procedure (Opaque      : Opaque_Ptr;
                                                   Event_Id    : Bus_Data;
                                                   Expire_Time : Bus_Time);
   pragma Convention (C, Event_Callback_Access);

   --  Import Bus interface from C implementation

   function Allocate_Device return Internal_Bus_Device;
   pragma Import (C, Allocate_Device, "allocate_device");

   procedure Cleanup_Device (Dev : Internal_Bus_Device);
   pragma Import (C, Cleanup_Device, "cleanup_device");

   procedure Set_Device_Info (Device               : Internal_Bus_Device;
                              Opaque               : Opaque_Ptr;
                              Vendor_Id, Device_Id : Id;
                              Name, Description    : String);

   procedure Register_IO_Memory (Device          : Internal_Bus_Device;
                                 Address, Length : Bus_Address);
   pragma Import (C, Register_IO_Memory, "register_io_memory");

   procedure Register_Callbacks (Device       : Internal_Bus_Device;
                                 IO_Read      : IO_Read_Access;
                                 IO_Write     : IO_Write_Access;
                                 Device_Init  : Device_Init_Access;
                                 Device_Reset : Device_Reset_Access;
                                 Device_Exit  : Device_Exit_Access);
   pragma Import (C, Register_Callbacks, "register_callbacks");

   function Register_Device (Device : Internal_Bus_Device;
                             Port    : Bus_Port) return Interfaces.C.int;
   pragma Import (C, Register_Device, "register_device");

   procedure Device_Loop (Device : Internal_Bus_Device);
   pragma Import (C, Device_Loop, "device_loop");

   procedure IRQ_Raise (Device : Internal_Bus_Device; Line : IRQ_Line);
   pragma Import (C, IRQ_Raise, "IRQ_raise");
   procedure IRQ_Lower (Device : Internal_Bus_Device; Line : IRQ_Line);
   pragma Import (C, IRQ_Lower, "IRQ_lower");
   procedure IRQ_Pulse (Device : Internal_Bus_Device; Line : IRQ_Line);
   pragma Import (C, IRQ_Pulse, "IRQ_pulse");

   function Get_Time (Device : Internal_Bus_Device) return Bus_Time;
   pragma Import (C, Get_Time, "get_time");

   procedure Add_Event (Device   : Internal_Bus_Device;
                        Expire   : Bus_Time;
                        Event_Id : Bus_Data;
                        Event    : Event_Callback_Access);
   pragma Import (C, Add_Event, "add_event");

end BusInterface;
