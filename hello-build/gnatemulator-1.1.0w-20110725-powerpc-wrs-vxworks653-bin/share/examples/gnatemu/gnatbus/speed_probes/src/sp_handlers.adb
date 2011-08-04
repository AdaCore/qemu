with Gtk.Main;
with Bus_Types; use Bus_Types;
with Glib; use Glib;

package body SP_Handlers is

   procedure Speed_Changed
     (Adjustment : access Gtk.Adjustment.Gtk_Adjustment_Record'Class;
      Probe      :        Probe_Interface_Ref)
   is
   begin
      Set_Speed (Probe.all, Bus_Data (Adjustment.Get_Value));
   end Speed_Changed;

   procedure Toggled
     (Button : access Gtk.Toggle_Button.Gtk_Toggle_Button_Record'Class;
      Probe  :        Probe_Interface_Ref)
   is
   begin
      if Button.Get_Active then
         Set_Error (Probe.all, 1);
      else
         Set_Error (Probe.all, 0);
      end if;
   end Toggled;

   procedure Destroy (Window : access Gtk.Window.Gtk_Window_Record'Class) is
      pragma Unreferenced (Window);
   begin
      Gtk.Main.Main_Quit;
   end Destroy;

   function Refresh_Canvas (SP_Gtk : Speed_Probe_Gtk_Ref) return Boolean is
   begin
      SP_Gtk.Plot.Autoscale;
      SP_Gtk.Canvas.Paint;
      SP_Gtk.Canvas.Refresh;
      return True;
   end Refresh_Canvas;

   procedure Trigger_Interrupt
     (Button : access Gtk.Button.Gtk_Button_Record'Class;
      Dev    :        Device_Ref) is
      pragma Unreferenced (Button);
   begin
      Dev.IRQ_Pulse (4);
   end Trigger_Interrupt;

end SP_Handlers;
