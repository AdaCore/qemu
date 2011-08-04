with Gtk.Extra.Plot_Data; use Gtk.Extra.Plot_Data;
with Bus_Types; use Bus_Types;

package Data_Container is
   type Data_Container_Record is record
      X : Gdouble_Array_Access;
      Y : Gdouble_Array_Access;
   end record;

   type Data_Container_Ref is access all Data_Container_Record;

   procedure Push (Data : Data_Container_Ref; Y : Bus_Data; X : Bus_Time);
   procedure Reset (Data : Data_Container_Ref);

end Data_Container;
