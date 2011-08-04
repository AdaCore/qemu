pragma Warnings (Off);
with System.BB.Peripherals.LEON_3; use System.BB.Peripherals.LEON_3;
pragma Warnings (On);

package body Target_Support.Interrupt is

   procedure Enable_External_Interrupt is
   begin
      Interrupt_Mask := Interrupt_Mask or 2**4;
   end Enable_External_Interrupt;
end Target_Support.Interrupt;
