--  with Ada.Text_IO; use Ada.Text_IO;

package body Data_Container is
   procedure Push (Data : Data_Container_Ref; Y : Bus_Data; X : Bus_Time) is
   begin
      Data.X := X;
      Data.Y := Y;
      --  Put_Line ("Recieved " & To_String (Data.Name) & " value: " & Y'Img);
   end Push;

   procedure Reset (Data : Data_Container_Ref) is
   begin
      Data.X := 0;
      Data.Y := 0;
   end Reset;

end Data_Container;
