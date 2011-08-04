with Target_Support; use Target_Support;

package body IRQ_P is

   protected body IRQ is
      procedure Signal is
      begin
         Put_Line ("From Guest: Interrupt handled");
      end Signal;

   end IRQ;

end IRQ_P;

