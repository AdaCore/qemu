pragma Warnings (Off);
with System.BB.Peripherals.ERC32;
pragma Warnings (On);

package body Target_Support.Interrupt is

   package BPR renames System.BB.Peripherals.ERC32;

   procedure Enable_External_Interrupt is
      --  MEC registers must be accesses as a whole.

      Interrupt_Mask_Auxiliary  : BPR.Interrupt_Mask_Register :=
        BPR.Interrupt_Mask;
   begin
      Interrupt_Mask_Auxiliary.UART_A_Ready := False;
      BPR.Interrupt_Mask := Interrupt_Mask_Auxiliary;
   end Enable_External_Interrupt;
end Target_Support.Interrupt;
