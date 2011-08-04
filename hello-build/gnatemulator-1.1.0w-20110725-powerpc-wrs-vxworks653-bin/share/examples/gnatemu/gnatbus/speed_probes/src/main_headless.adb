with Data_Container; use Data_Container;
with Device; use Device;
with Ada.Strings.Unbounded; use Ada.Strings.Unbounded;
with Ada.Text_IO; use Ada.Text_IO;
with Bus_Types; use Bus_Types;

procedure Main_HeadLess is
   Speed_Data   : aliased Data_Container_Record;
   Error_Data   : aliased Data_Container_Record;
   My_Device    : Device.Device_Ref;
   Cmd          : Character;
   Val          : Character;
   Prob         : Device.Probe_Range;

   function Char_To_Speed (Speed : Character) return Bus_Data;
   function Get_Probe_Id return Device.Probe_Range;

   function Char_To_Speed (Speed : Character) return Bus_Data is
   begin
      case Speed is
         when '0' =>
            return 0;
         when '1' =>
            return 10;
         when '2' =>
            return 20;
         when '3' =>
            return 30;
         when '4' =>
            return 40;
         when '5' =>
            return 50;
         when '6' =>
            return 60;
         when '7' =>
            return 70;
         when '8' =>
            return 80;
         when '9' =>
            return 90;
         when 'a' =>
            return 100;
         when others =>
            return 0;
      end case;
   end Char_To_Speed;

   function Get_Probe_Id return Device.Probe_Range is
      Index : Integer;
      Prob  : Character;
   begin
      loop
         Put_Line ("Probe nbr (" & Device.Probe_Range'First'Img &
                   " ..." & Device.Probe_Range'Last'Img & " )?");
         Put (":");
         Get (Prob);
         Index := Character'Pos (Prob) - Character'Pos ('0');
         if Index in Device.Probe_Range then
            return Device.Probe_Range (Index);
         else
            Put_Line ("Invalid number!");
         end if;
      end loop;

   end Get_Probe_Id;
begin

   Speed_Data := (Name => To_Unbounded_String ("Speed"), X => 0, Y => 0);
   Error_Data := (Name => To_Unbounded_String ("Error"), X => 0, Y => 0);

   --  Create the GNATbus device
   My_Device := new Device.Device_T (16#ffff_ffff#,  --  Vendor_Id
                                     16#aaaa_aaaa#,  --  Device_Id
                                     8032,           --  TCP Port
                                     16#8001_0000#,  --  Base Address
                                     Speed_Data'Unchecked_Access,
                                     Error_Data'Unchecked_Access);

   My_Device.Start;

   Cmd_Loop :
   loop
      Put_Line ("Menu:");
      Put_Line ("s) Set Speed");
      Put_Line ("e) Set Error");
      Put_Line ("p) Print");
      Put_Line ("q) Quit");
      Put ("?: ");
      Get (Cmd);
      New_Line;
      case Cmd is
         when 's' =>
            Prob := Get_Probe_Id;
            Put_Line ("Current speed:" &
                      Get_Speed (My_Device.Probes (Prob))'Img);
            Put_Line ("New speed?");
            Put_Line ("0) 0");
            Put_Line ("1) 10");
            Put_Line ("2) 20");
            Put_Line ("3) 30");
            Put_Line ("4) 40");
            Put_Line ("5) 50");
            Put_Line ("6) 60");
            Put_Line ("7) 70");
            Put_Line ("8) 80");
            Put_Line ("9) 90");
            Put_Line ("a) 100");
            Put (": ");
            Get (Val);
            Set_Speed (My_Device.Probes (Prob), Char_To_Speed (Val));
         when 'e' =>
            Prob := Get_Probe_Id;
            Put_Line ("   Current error:" &
                      Get_Error (My_Device.Probes (Prob))'Img);
            Put_Line ("   Error (0 or 1)?");
            Put ("   : ");
            Get (Val);
            Set_Error (My_Device.Probes (Prob),
                       (Character'Pos (Val) - Character'Pos ('0')) mod 1);
         when 'p' =>
            for Index in Device.Probe_Range loop
               Put_Line ("Probe" & Index'Img &
                         " speed:" & Get_Speed (My_Device.Probes (Index))'Img &
                         " error:" & Get_Error (My_Device.Probes (Index))'Img);
            end loop;
            Put_Line ("Computed Speed: " & Speed_Data.Y'Img);
            Put_Line ("Computed Error: " & Error_Data.Y'Img);
         when 'q' =>
            exit Cmd_Loop;
         when ASCII.CR | ASCII.LF =>
            null;
         when others =>
            Put_Line ("invalid choice");
      end case;
   end loop Cmd_Loop;

   My_Device.Kill;

end Main_HeadLess;
