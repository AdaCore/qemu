with Ada.Interrupts.Names; use Ada.Interrupts.Names;
with Ada.Interrupts; use Ada.Interrupts;
with System;

package Target_Support.Interrupt is

   External_Interrupt_Priority : constant System.Interrupt_Priority := UART_A_Ready_Priority;
   External_Interrupt          : constant Interrupt_ID              := UART_A_Ready;

   procedure Enable_External_Interrupt;
end Target_Support.Interrupt;
