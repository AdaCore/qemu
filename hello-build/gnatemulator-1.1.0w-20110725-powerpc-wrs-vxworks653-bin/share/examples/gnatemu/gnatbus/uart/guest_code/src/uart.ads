with System;
with Target_Support.Interrupt; use Target_Support.Interrupt;

package uart is

   task T1 is
      pragma Priority (System.Priority'Last);
      pragma Storage_Size (80 * 1024);
   end T1;

   protected UART is
      pragma Interrupt_Priority (External_Interrupt_Priority);

      procedure Signal;
      pragma Attach_Handler (Signal, External_Interrupt);

   end UART;

end uart;
