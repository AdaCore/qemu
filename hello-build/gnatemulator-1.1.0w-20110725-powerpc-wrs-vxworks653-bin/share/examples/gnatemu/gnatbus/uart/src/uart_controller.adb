with Ada.Unchecked_Conversion;

package body UART_Controller is

   function UART_CTRL_To_Bus_Data is new Ada.Unchecked_Conversion
     (UART_CTRL_Register, Bus_Data);

   function Bus_Data_To_UART_CTRL is new Ada.Unchecked_Conversion
     (Bus_Data, UART_CTRL_Register);

   protected body UART_Control is

      -----------
      -- Reset --
      -----------

      procedure Reset is
      begin
         CTRL := (Enable_Interrupt => False,
                  Data_To_Read     => False,
                  Reserved         => (others => False));
      end Reset;

      --------------
      -- Get_CTRL --
      --------------

      function Get_CTRL return Bus_Data is
      begin
         return UART_CTRL_To_Bus_Data (CTRL);
      end Get_CTRL;

      --------------
      -- Set_CTRL --
      --------------

      procedure Set_CTRL (CTRL_In : Bus_Data) is
      begin
         CTRL := Bus_Data_To_UART_CTRL (CTRL_In);
      end Set_CTRL;

      --------------
      -- Pop_DATA --
      --------------

      procedure Pop_DATA (DATA_Out : out Bus_Data) is
      begin
         if Bus_Data_Container.Is_Empty (DATA) then
            DATA_Out := 0;
            return;
         end if;

         DATA_Out := Bus_Data_Container.First_Element (DATA);
         Bus_Data_Container.Delete_First (DATA);

         CTRL.Data_To_Read := not Bus_Data_Container.Is_Empty (DATA);

      end Pop_DATA;

      ---------
      -- Put --
      ---------

      procedure Put (S : String) is
      begin
         for Index in S'Range loop
            Put (S (Index));
         end loop;
      end Put;

      ---------
      -- Put --
      ---------

      procedure Put (C : Character) is
      begin

         Bus_Data_Container.Append (DATA, Character'Pos (C));
         CTRL.Data_To_Read := True;

         if CTRL.Enable_Interrupt and then Device /= null then
            Device.IRQ_Pulse (4);
         end if;
      end Put;

      ----------------
      -- Set_Device --
      ----------------

      procedure Set_Device (Device_In : Bus_Device_Ref) is
      begin
         Device := Device_In;
      end Set_Device;

   end UART_Control;

end UART_Controller;
