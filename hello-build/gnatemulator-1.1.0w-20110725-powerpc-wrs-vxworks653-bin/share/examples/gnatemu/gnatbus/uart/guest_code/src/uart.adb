with Ada.Real_Time;

with Target_Support; use Target_Support;
with Target_Support.Interrupt;

package body uart is

   use type Ada.Real_Time.Time_Span;

   ---------------
   -- Uart Ctrl --
   ---------------

   type Reserved_30 is array (0 .. 29) of Boolean;
   for Reserved_30'Size use 30;
   pragma Pack (Reserved_30);

   type UART_CTRL_Register is
      record
         Enable_Interrupt : Boolean;
         Data_To_Read     : Boolean;
         Reserved         : Reserved_30;
      end record;

   for UART_CTRL_Register use
      record
         Reserved         at 0 range 00 .. 29;
         Data_To_Read     at 0 range 30 .. 30;
         Enable_Interrupt at 0 range 31 .. 31;
      end record;

   for UART_CTRL_Register'Size use 32;

   pragma Suppress_Initialization (UART_CTRL_Register);

   UART_CTRL : UART_CTRL_Register;
   pragma Atomic (UART_CTRL);
   for UART_CTRL'Address use System'To_Address (16#8000_1000#);

   type Register is mod 2**32;
   for Register'Size use 32;
   pragma Suppress_Initialization (Register);

   UART_DATA : Character;
   pragma Atomic (UART_DATA);
   for UART_DATA'Address use System'To_Address (16#8000_1004#);

   task body T1 is
   begin

      --  Mask Interrupt

      Target_Support.Interrupt.Enable_External_Interrupt;

      --  Enable Interrupt in UART controller

      UART_CTRL.Enable_Interrupt := True;

      delay until Ada.Real_Time.Time_Last;
   end T1;

   protected body UART is
      procedure Signal is
      begin
         while UART_CTRL.Data_To_Read Loop
            Put (UART_DATA);
         end loop;
      end Signal;
   end UART;

end uart;
