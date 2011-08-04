with UART; use UART;
with Ada.Text_IO;

procedure Main is
   My_UART : UART.UART_Ref;
begin
   My_UART := new UART.UART_Device (16#ffff_ffff#, --  Vendor_Id
                                    16#aaaa_aaaa#, --  Device_Id
                                    16#8000_1000#, --  Base Address
                                    8032);         --  TCP Port
   --  Start the Device loop

   My_UART.Start;

   --  Now we are ready to receive connection from GNATemulator

   Ada.Text_IO.Put_Line ("Start Simulation");

   for Cnt in 1 .. 60 loop
      My_UART.UC.Put ("Send Message: " & Cnt'Img & ASCII.LF);
      delay 1.0;
   end loop;

   --  Abort the device loop

   My_UART.Kill;
end Main;
