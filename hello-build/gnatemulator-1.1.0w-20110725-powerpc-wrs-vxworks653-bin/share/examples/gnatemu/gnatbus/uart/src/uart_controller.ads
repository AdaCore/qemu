with Ada.Containers.Doubly_Linked_Lists;
with Bus_Devices;  use Bus_Devices;
with Bus_Types;  use Bus_Types;

package UART_Controller is

   -----------------------------
   -- UART registers and FIFO --
   -----------------------------

   package Bus_Data_Container is new
     Ada.Containers.Doubly_Linked_Lists (Bus_Data);

   type Reserved_30 is array (0 .. 29) of Boolean;
   for Reserved_30'Size use 30;
   pragma Pack (Reserved_30);

   type UART_CTRL_Register is record
      Enable_Interrupt : Boolean;
      Data_To_Read     : Boolean;
      Reserved         : Reserved_30;
   end record;

   for UART_CTRL_Register use record
      Enable_Interrupt at 0 range 00 .. 00;
      Data_To_Read     at 0 range 01 .. 01;
      Reserved         at 0 range 02 .. 31;
   end record;

   for UART_CTRL_Register'Size use 32;

   protected type UART_Control is

      procedure Reset;
      function  Get_CTRL return Bus_Data;
      procedure Set_CTRL (CTRL_In : Bus_Data);
      procedure Pop_DATA (DATA_Out : out Bus_Data);
      procedure Put (C : Character);
      procedure Put (S : String);
      procedure Set_Device (Device_In : Bus_Device_Ref);

      private

         CTRL : UART_CTRL_Register := (Enable_Interrupt => False,
                                       Data_To_Read     => False,
                                       Reserved         => (others => False));
         DATA : Bus_Data_Container.List;

         Device : Bus_Device_Ref;
   end UART_Control;

end UART_Controller;
