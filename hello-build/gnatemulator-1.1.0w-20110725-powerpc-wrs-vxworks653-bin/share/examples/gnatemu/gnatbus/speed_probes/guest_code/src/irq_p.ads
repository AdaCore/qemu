with Target_Support.Interrupt; use Target_Support.Interrupt;

package IRQ_P is

   protected IRQ is
      pragma Interrupt_Priority (External_Interrupt_Priority);

      procedure Signal;
      pragma Attach_Handler (Signal, External_Interrupt);

   end IRQ;

end IRQ_P;
