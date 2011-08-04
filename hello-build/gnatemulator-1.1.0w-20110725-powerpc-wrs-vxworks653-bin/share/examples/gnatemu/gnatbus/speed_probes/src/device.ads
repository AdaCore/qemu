with Bus_Devices; use Bus_Devices;
with Bus_Types;   use Bus_Types;
with Data_Container; use Data_Container;

package Device is

   subtype Probe_Range is Integer range 1 .. 3;

   type Probe_Interface is private;

   type Probe_Interface_Ref is access all Probe_Interface;

   procedure Set_Speed (Probe : in out Probe_Interface; Val : Bus_Data);
   function  Get_Speed (Probe :        Probe_Interface) return Bus_Data;

   procedure Set_Error (Probe : in out Probe_Interface; Val : Bus_Data);
   function  Get_Error (Probe :        Probe_Interface) return Bus_Data;

   type Probe_Array is array (Probe_Range) of aliased Probe_Interface;

   subtype Registers_Area is
     Bus_Address range 0 .. (Probe_Range'Range_Length * 8);

   ----------------
   -- The Device --
   ----------------

   type Device_T (Vendor_Id, Device_Id   : Id;
                  Port                   : Integer;
                  Base_Address           : Bus_Address;
                  Speed_Data, Error_Data : Data_Container_Ref)
      is new Bus_Device (Vendor_Id, Device_Id, Port) with record

      Probes    : Probe_Array;
   end record;

   type Device_Ref is access all Device_T'Class;

   ----------------------
   -- Device callbacks --
   ----------------------

   overriding procedure IO_Read (Self    : in out Device_T;
                                 Address : Bus_Address;
                                 Length  : Bus_Address;
                                 Value   : out Bus_Data);

   overriding procedure IO_Write (Self    : in out Device_T;
                                  Address : Bus_Address;
                                  Length  : Bus_Address;
                                  Value   : Bus_Data);

   overriding procedure Device_Setup (Self : in out Device_T);

   overriding procedure Device_Init (Self : in out Device_T);

   overriding procedure Device_Reset (Self : in out Device_T);

   overriding procedure Device_Exit (Self : in out Device_T);

private

   type Probe_Interface is record
      Speed      : Bus_Data := 0;
      Error_Flag : Bus_Data := 0;
   end record;

end Device;
