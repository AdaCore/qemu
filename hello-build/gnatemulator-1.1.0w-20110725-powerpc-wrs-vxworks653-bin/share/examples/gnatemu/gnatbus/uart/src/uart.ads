with Bus_Devices; use Bus_Devices;
with Bus_Types;   use Bus_Types;

with UART_Controller; use UART_Controller;

package UART is

   ----------------
   -- The Device --
   ----------------

   type UART_Device (Vendor_Id, Device_Id : Id;
                     Base_Address         : Bus_Address;
                     Port                 : Integer)
      is new Bus_Device (Vendor_Id, Device_Id, Port) with record

         UC : UART_Control;
   end record;

   type UART_Ref is access all UART_Device'Class;

   ----------------------
   -- Device callbacks --
   ----------------------

   overriding procedure IO_Read (Self    : in out UART_Device;
                                 Address : Bus_Address;
                                 Length  : Bus_Address;
                                 Value   : out Bus_Data);

   overriding procedure IO_Write (Self    : in out UART_Device;
                                  Address : Bus_Address;
                                  Length  : Bus_Address;
                                  Value   : Bus_Data);

   overriding procedure Device_Setup (Self : in out UART_Device);

   overriding procedure Device_Init (Self : in out UART_Device);

   overriding procedure Device_Reset (Self : in out UART_Device);

   overriding procedure Device_Exit (Self : in out UART_Device);

end UART;
