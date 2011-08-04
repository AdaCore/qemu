with Ada.Text_IO;

package body Target_Support is
   procedure Halt is
      procedure OS_Exit;
      pragma Import (C, OS_Exit, "exit");
   begin
      OS_Exit;
   end Halt;

   procedure New_Line renames Ada.Text_IO.New_Line;
   procedure Put (Item : Character) renames Ada.Text_IO.Put;
   procedure Put (Item : String) renames Ada.Text_IO.Put;
   procedure Put_Line (Item : String) renames Ada.Text_IO.Put_Line;
end Target_Support;
