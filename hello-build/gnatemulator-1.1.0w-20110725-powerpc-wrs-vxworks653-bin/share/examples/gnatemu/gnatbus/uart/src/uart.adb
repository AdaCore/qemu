with Ada.Text_IO;

package body UART is

   -------------
   -- IO_Read --
   -------------

   procedure IO_Read (Self    : in out UART_Device;
                      Address : Bus_Address;
                      Length  : Bus_Address;
                      Value   : out Bus_Data) is

      pragma Unreferenced (Length);
   begin
      Ada.Text_IO.Put_Line ("Read @ " & Address'Img);

      --  case statement on the relative address

      case Address - Self.Base_Address is
         when 0 =>
            --  Return value of the control register
            Value := Self.UC.Get_CTRL;

         when 4 =>
            --  Pop a byte from FIFO queue
            Self.UC.Pop_DATA (Value);

         when others =>
            Ada.Text_IO.Put_Line ("Read unknown register:" & Address'Img);
            Value := 0;
      end case;
   end IO_Read;

   --------------
   -- IO_Write --
   --------------

   procedure IO_Write (Self    : in out UART_Device;
                       Address : Bus_Address;
                       Length  : Bus_Address;
                       Value   : Bus_Data) is

      pragma Unreferenced (Length);
   begin
      Ada.Text_IO.Put_Line ("Write @ " & Address'Img);

      --  case statement on the relative address

      case Address - Self.Base_Address is
         when 0 =>
            --  Set Control register value
            Self.UC.Set_CTRL (Value);

         when others =>
            Ada.Text_IO.Put_Line ("Write unknown register:" & Address'Img);
      end case;
   end IO_Write;

   -----------------
   -- Device_Init --
   -----------------

   procedure Device_Init (Self : in out UART_Device) is
      pragma Unreferenced (Self);
   begin
      Ada.Text_IO.Put_Line ("Device_Init");
   end Device_Init;

   ------------------
   -- Device_Reset --
   ------------------

   procedure Device_Reset (Self : in out UART_Device) is
   begin
      Ada.Text_IO.Put_Line ("Device_Reset");

      --  Send the reset signal to the UART_Control

      Self.UC.Reset;
   end Device_Reset;

   -----------------
   -- Device_Exit --
   -----------------

   procedure Device_Exit (Self : in out UART_Device) is
      pragma Unreferenced (Self);
   begin
      Ada.Text_IO.Put_Line ("Device_Exit");
   end Device_Exit;

   ------------------
   -- Device_Setup --
   ------------------

   procedure Device_Setup (Self : in out UART_Device) is
   begin
      Ada.Text_IO.Put_Line ("Device_Setup");

      --  Register the only I/O area: 8 bytes at base address to match the two
      --  registers.

      Self.Register_IO_Memory (Self.Base_Address, 8);

      --  Set UART_Device access in the UART_Control protected object

      Self.UC.Set_Device (Self'Unchecked_Access);
   end Device_Setup;

end UART;
