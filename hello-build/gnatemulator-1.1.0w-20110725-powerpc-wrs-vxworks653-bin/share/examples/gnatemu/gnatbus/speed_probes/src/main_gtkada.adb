with Data_Container; use Data_Container;
with Glib; use Glib;
with Device; use Device;
with Speed_Probe_Gtk; use Speed_Probe_Gtk;
with Gtk; use Gtk;
with Gtk.Main;

procedure Main_GtkAda is
   Speed_Data   : aliased Data_Container_Record;
   Error_Data   : aliased Data_Container_Record;

   My_Device    : Device.Device_Ref;

   Sp_Gtk : aliased Speed_Probe_Gtk_Record;
begin

   Speed_Data.X := new Gdouble_Array (1 .. 50);
   Speed_Data.Y := new Gdouble_Array (1 .. 50);
   Error_Data.X := new Gdouble_Array (1 .. 50);
   Error_Data.Y := new Gdouble_Array (1 .. 50);

   --  Create the GNATbus device
   My_Device := new Device.Device_T (16#ffff_ffff#,  --  Vendor_Id
                                     16#aaaa_aaaa#,  --  Device_Id
                                     8032,           --  TCP Port
                                     16#8001_0000#,  --  Base Address
                                     Speed_Data'Unchecked_Access,
                                     Error_Data'Unchecked_Access);

   Init_Gtk (Sp_Gtk'Unchecked_Access,
             My_Device,
             Speed_Data'Unchecked_Access,
             Error_Data'Unchecked_Access);

   My_Device.Start;
   Gtk.Main.Main;
   My_Device.Kill;
end Main_GtkAda;
