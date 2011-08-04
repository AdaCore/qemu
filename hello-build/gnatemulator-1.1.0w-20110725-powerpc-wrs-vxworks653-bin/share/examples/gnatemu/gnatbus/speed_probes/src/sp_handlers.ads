with Gtk.Adjustment, Gtk.Window, Gtk.Handlers, Gtk.Toggle_Button;
with Device; use Device;
with Glib.Main; use Glib.Main;
with Gtk.Button;
with Speed_Probe_Gtk; use Speed_Probe_Gtk;

package SP_Handlers is

   package Adjustment_Cb is new Gtk.Handlers.User_Callback
     (Gtk.Adjustment.Gtk_Adjustment_Record, Probe_Interface_Ref);

   procedure Speed_Changed
     (Adjustment : access Gtk.Adjustment.Gtk_Adjustment_Record'Class;
      Probe      :        Probe_Interface_Ref);

   package Window_Cb is new Gtk.Handlers.Callback
     (Gtk.Window.Gtk_Window_Record);

   procedure Destroy (Window : access Gtk.Window.Gtk_Window_Record'Class);

   package Toggle_Cb is new Gtk.Handlers.User_Callback
     (Gtk.Toggle_Button.Gtk_Toggle_Button_Record, Probe_Interface_Ref);

   procedure Toggled
     (Button : access Gtk.Toggle_Button.Gtk_Toggle_Button_Record'Class;
      Probe  :        Probe_Interface_Ref);

   package Canvas_Sources is new
     Glib.Main.Generic_Sources (Speed_Probe_Gtk_Ref);

   function Refresh_Canvas (SP_Gtk : Speed_Probe_Gtk_Ref) return Boolean;

   package Interrupt_Cb is new Gtk.Handlers.User_Callback
     (Gtk.Button.Gtk_Button_Record, Device_Ref);

   procedure Trigger_Interrupt
     (Button : access Gtk.Button.Gtk_Button_Record'Class;
      Dev    :        Device_Ref);

end SP_Handlers;
