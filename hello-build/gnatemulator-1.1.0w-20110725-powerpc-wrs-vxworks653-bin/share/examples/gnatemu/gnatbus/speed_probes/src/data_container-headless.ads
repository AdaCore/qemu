with Bus_Types; use Bus_Types;
with Ada.Strings.Unbounded; use Ada.Strings.Unbounded;

package Data_Container is

   type Data_Container_Record is record
      Name : Unbounded_String;
      X    : Bus_Time;
      Y    : Bus_Data;
   end record;

   type Data_Container_Ref is access all Data_Container_Record;

   procedure Push (Data : Data_Container_Ref; Y : Bus_Data; X : Bus_Time);
   procedure Reset (Data : Data_Container_Ref);

end Data_Container;
