with Ada.Text_IO; use Ada.Text_IO;

package body Device is

   ---------------------
   -- Probe_Interface --
   ---------------------

   procedure Set_Speed (Probe : in out Probe_Interface; Val : Bus_Data) is
   begin
      Probe.Speed := Val;
   end Set_Speed;

   function  Get_Speed (Probe :        Probe_Interface) return Bus_Data is
   begin
      return Probe.Speed;
   end Get_Speed;

   procedure Set_Error (Probe : in out Probe_Interface; Val : Bus_Data) is
   begin
      Probe.Error_Flag := Val;
   end Set_Error;

   function  Get_Error (Probe :        Probe_Interface) return Bus_Data is
   begin
      return Probe.Error_Flag;
   end Get_Error;

   -----------------
   -- Device_Exit --
   -----------------

   overriding procedure Device_Exit (Self : in out Device_T) is
      pragma Unreferenced (Self);
   begin
      null;
   end Device_Exit;

   -----------------
   -- Device_Init --
   -----------------

   overriding procedure Device_Init (Self : in out Device_T) is
      pragma Unreferenced (Self);
   begin
      null;
   end Device_Init;

   ------------------
   -- Device_Reset --
   ------------------

   overriding procedure Device_Reset (Self : in out Device_T) is
   begin
      Reset (Self.Speed_Data);
      Reset (Self.Error_Data);
   end Device_Reset;

   ------------------
   -- Device_Setup --
   ------------------

   overriding procedure Device_Setup (Self : in out Device_T) is
   begin
      Self.Register_IO_Memory (Self.Base_Address, Registers_Area'Range_Length);
   end Device_Setup;

   -------------
   -- IO_Read --
   -------------

   --  --------------------- 0
   --  |  Speed_Mesured 1  |
   --  --------------------- 4
   --  |    Error_Flag 1   |
   --  --------------------- 8 * (n - 1)
   --  |  Speed_Mesured n  |
   --  --------------------- 8 * (n - 1) + 4
   --  |    Error_Flag n   |
   --  ---------------------
   --  .                   .
   --  .                   .
   --  .                   .
   --  --------------------- Probe_Range'Range_Length * 8

   overriding procedure IO_Read
     (Self    : in out Device_T;
      Address :        Bus_Address;
      Length  :        Bus_Address;
      Value   :    out Bus_Data) is

      Probe_Index      : Probe_Range;
      Rel_Addr         : constant Bus_Address := Address - Self.Base_Address;
   begin

      if Length /= 4 or else Rel_Addr mod 4 /= 0 then
         Put_Line ("Invalid read: Only handle aligned reads of 4 bytes");
         Value := 0;
         return;
      end if;

      if Rel_Addr not in Registers_Area then
         Put_Line ("The address is beyond the last register");
         Value := 0;
         return;
      end if;

      Probe_Index := Probe_Range'Val (Rel_Addr / 8)
        + Probe_Range'Pos  (Probe_Range'First);

      if Rel_Addr mod 8 = 0 then
         Value := Self.Probes (Probe_Index).Speed;
      elsif Rel_Addr mod 8 = 4 then
         Value := Self.Probes (Probe_Index).Error_Flag;
      end if;
   end IO_Read;

   --------------
   -- IO_Write --
   --------------

   --  --------------------- 0
   --  |   Computed Speed  |
   --  --------------------- 4
   --  |   Computed Error  |
   --  --------------------- 8

   overriding procedure IO_Write
     (Self    : in out Device_T;
      Address :        Bus_Address;
      Length  :        Bus_Address;
      Value   :        Bus_Data) is

      Rel_Addr : constant Bus_Address := Address - Self.Base_Address;
   begin
      if Rel_Addr = 0 and then Length = 4 then
         Push (Self.Speed_Data, Value, Self.Get_Time);
      end if;

      if Rel_Addr = 4 and then Length = 4 then
         Push (Self.Error_Data, Value, Self.Get_Time);
      end if;
   end IO_Write;

end Device;
