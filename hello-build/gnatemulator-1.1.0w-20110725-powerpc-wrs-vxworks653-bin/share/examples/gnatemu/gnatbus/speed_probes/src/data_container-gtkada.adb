with Glib; use Glib;

package body Data_Container is
   procedure Push (Data : Data_Container_Ref; Y : Bus_Data; X : Bus_Time) is
   begin
      for Index in Data.Y'First .. Data.Y'Last - 1 loop
         Data.Y (Index) := Data.Y (Index + 1);
         Data.X (Index) := Data.X (Index + 1);
      end loop;

      Data.Y (Data.Y'Last) := Gdouble (Y);
      Data.X (Data.X'Last) := Gdouble (X) / 1000000000.0;
   end Push;

   procedure Reset (Data : Data_Container_Ref) is
   begin
      for Index in Data.Y'First .. Data.Y'Last loop
         Data.Y (Index) := 0.0;
         Data.X (Index) := 0.0;
      end loop;
   end Reset;

end Data_Container;
