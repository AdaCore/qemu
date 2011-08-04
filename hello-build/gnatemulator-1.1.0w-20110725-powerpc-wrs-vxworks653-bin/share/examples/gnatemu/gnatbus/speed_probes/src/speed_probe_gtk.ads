with Gtk.Window, Gtk.Adjustment;
use Gtk.Window, Gtk.Adjustment;

with Device; use Device;
with Gtk.Box; use Gtk.Box;
with Gtk.Scale; use Gtk.Scale;
with Gtk.Toggle_Button; use Gtk.Toggle_Button;
with Gtk.Extra.Plot; use Gtk.Extra.Plot;
with Gtk.Extra.Plot_Canvas; use Gtk.Extra.Plot_Canvas;
with Gtk.Extra.Plot_Data; use Gtk.Extra.Plot_Data;
with Glib; use Glib;
with Gtk.Extra.Plot_Canvas.Plot; use Gtk.Extra.Plot_Canvas.Plot;
with Glib.Main; use Glib.Main;
with Gtk.Button; use Gtk.Button;
with Data_Container; use Data_Container;

package Speed_Probe_Gtk is

   type Adjustment_Array is array (Device.Probe_Range) of Gtk_Adjustment;
   type Scale_Array      is array (Device.Probe_Range) of Gtk_Hscale;
   type Error_Array      is array (Device.Probe_Range) of Gtk_Toggle_Button;
   type Hboxe_Array      is array (Device.Probe_Range) of Gtk_Hbox;

   type Speed_Probe_Gtk_Record is record
      Window       : Gtk_Window;
      Vbox         : Gtk_Vbox;
      Adjustments  : Adjustment_Array;
      Scales       : Scale_Array;
      Errors       : Error_Array;
      Hboxes       : Hboxe_Array;
      Interrupt    : Gtk_Button;
      Plot         : Gtk_Plot;
      Canvas       : Gtk_Plot_Canvas;
      Child        : Gtk_Plot_Canvas_Plot;
      Speed_PData  : Gtk_Plot_Data;
      Error_PData  : Gtk_Plot_Data;
      Timeout      : G_Source_Id;
   end record;

   type Speed_Probe_Gtk_Ref is access all Speed_Probe_Gtk_Record;

   procedure Init_Gtk
     (SP_Gtk                 : Speed_Probe_Gtk_Ref;
      My_Device              : Device_Ref;
      Speed_Data, Error_Data : Data_Container_Ref);

end Speed_Probe_Gtk;
