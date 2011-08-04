pragma Warnings (Off);
with System.BB.Peripherals.LEON; use System.BB.Peripherals.LEON;
pragma Warnings (On);

package body Target_Support.Interrupt is

   procedure Enable_External_Interrupt is
   begin
      Interrupt_Mask_And_Priority.External_0 := True;
   end Enable_External_Interrupt;
end Target_Support.Interrupt;
