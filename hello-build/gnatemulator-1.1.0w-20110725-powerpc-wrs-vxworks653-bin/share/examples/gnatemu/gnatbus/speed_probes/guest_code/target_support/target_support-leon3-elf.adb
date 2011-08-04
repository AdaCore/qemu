with System.Machine_Code; use System.Machine_Code;

pragma Warnings (Off);
with System.BB.Serial_Output;
pragma Warnings (On);

package body Target_Support is
   procedure Halt is
   begin
      Asm ("mov 1, %%g1" & ASCII.LF & ASCII.HT & "ta 0",
           No_Output_Operands, No_Input_Operands, "g1", True);
   end Halt;

   procedure New_Line renames System.BB.Serial_Output.New_Line;
   procedure Put (Item : Character) renames System.BB.Serial_Output.Put;
   procedure Put (Item : String) renames System.BB.Serial_Output.Put;
   procedure Put_Line (Item : String) renames System.BB.Serial_Output.Put_Line;
end Target_Support;
