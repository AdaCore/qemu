with Ada.Interrupts.Names; use Ada.Interrupts.Names;
with Ada.Interrupts; use Ada.Interrupts;
with System;

package Target_Support.Interrupt is

   External_Interrupt_Priority : constant System.Interrupt_Priority := Ada.Interrupts.Names.External_Interrupt_3_Priority;
   External_Interrupt          : constant Interrupt_ID              := Ada.Interrupts.Names.External_Interrupt_0;

   procedure Enable_External_Interrupt;
end Target_Support.Interrupt;
