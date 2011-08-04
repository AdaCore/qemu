with SP_Handlers; use SP_Handlers;

with Gtk.Main, Gtk.Enums; use Gtk.Enums;

with Gdk.GC; use Gdk.GC;
with Gdk.Color; use Gdk.Color;
with Gdk.Window; use Gdk.Window;

package body Speed_Probe_Gtk is

   procedure Init_Gtk
     (SP_Gtk                 : Speed_Probe_Gtk_Ref;
      My_Device              : Device_Ref;
      Speed_Data, Error_Data : Data_Container_Ref) is
   begin
      Gtk.Main.Init;

      Gtk_New (SP_Gtk.Window);
      Set_Title (SP_Gtk.Window, "Speed probes");

      Window_Cb.Connect
        (SP_Gtk.Window, "destroy", Window_Cb.To_Marshaller (Destroy'Access));

      Set_Border_Width (SP_Gtk.Window, 10);

      Gtk_New_Vbox (SP_Gtk.Vbox, False, 5);
      Add (SP_Gtk.Window, SP_Gtk.Vbox);

      --  Create the canvas in which the plot will be drawn
      Gtk_New (SP_Gtk.Canvas, 1000, 300, 1.0);
      Pack_Start (SP_Gtk.Vbox, SP_Gtk.Canvas, True, True, 0);

      --  Create the plot
      Gtk_New (SP_Gtk.Plot);
      Get_Axis (SP_Gtk.Plot, Axis_Left).Axis_Set_Title ("Speed");
      Get_Axis (SP_Gtk.Plot, Axis_Left).Axis_Set_Ticks (0.2, 2);
      Get_Axis (SP_Gtk.Plot, Axis_Bottom).Axis_Set_Title ("Time (s)");
      Get_Axis (SP_Gtk.Plot, Axis_Bottom).Axis_Set_Ticks (0.2, 2);
      Axis_Set_Visible (Get_Axis (SP_Gtk.Plot, Axis_Top), False);
      Axis_Set_Visible (Get_Axis (SP_Gtk.Plot, Axis_Right), False);
      Axis_Hide_Title (Get_Axis (SP_Gtk.Plot, Axis_Top));
      Axis_Hide_Title (Get_Axis (SP_Gtk.Plot, Axis_Right));
      SP_Gtk.Plot.Legends_Move (1.0, 0.5);
      Gtk_New (SP_Gtk.Child, SP_Gtk.Plot);
      Put_Child (SP_Gtk.Canvas, SP_Gtk.Child, 0.15, 0.05, 0.8, 0.85);
      SP_Gtk.Plot.Show;

      --  Create Speed Data set
      Gtk_New (SP_Gtk.Speed_PData);
      SP_Gtk.Speed_PData.Show;
      SP_Gtk.Speed_PData.Set_Points (X  => Speed_Data.X,
                                     Y  => Speed_Data.Y,
                                     Dx => null,
                                     Dy => null);
      SP_Gtk.Speed_PData.Set_Legend ("Computed Speed");
      SP_Gtk.Plot.Add_Data (SP_Gtk.Speed_PData);

      --  Create Error Data set
      Gtk_New (SP_Gtk.Error_PData);
      SP_Gtk.Error_PData.Show;
      SP_Gtk.Error_PData.Set_Points (X  => Error_Data.X,
                                     Y  => Error_Data.Y,
                                     Dx => null,
                                     Dy => null);
      SP_Gtk.Error_PData.Set_Legend ("Error");
      SP_Gtk.Error_PData.Set_Line_Attributes (Style      => Line_Solid,
                                              Cap_Style  => Cap_Butt,
                                              Join_Style => Join_Bevel,
                                              Width      => 1.0,
                                              Color      => Parse ("red"));
      SP_Gtk.Plot.Add_Data (SP_Gtk.Error_PData);
      SP_Gtk.Plot.Autoscale;

      --  Create Probe controller
      for Index in Device.Probe_Range loop
         Gtk_New_Hbox (SP_Gtk.Hboxes (Index), False, 5);
         Pack_Start (SP_Gtk.Vbox, SP_Gtk.Hboxes (Index), False, False, 0);

         Gtk_New (SP_Gtk.Adjustments (Index), 0.0, 0.0, 100.0, 1.0, 1.0);
         Gtk_New_Hscale (SP_Gtk.Scales (Index), SP_Gtk.Adjustments (Index));
         Pack_Start (SP_Gtk.Hboxes (Index), SP_Gtk.Scales (Index),
                     True, True, 0);
         Adjustment_Cb.Connect
           (SP_Gtk.Adjustments (Index), "value_changed",
            Adjustment_Cb.To_Marshaller (Speed_Changed'Access),
            My_Device.Probes (Index)'Access);

         Gtk_New_With_Mnemonic (SP_Gtk.Errors (Index), "Error");
         Pack_Start (SP_Gtk.Hboxes (Index), SP_Gtk.Errors (Index),
                     False, False, 0);

         Toggle_Cb.Connect
           (SP_Gtk.Errors (Index), "toggled",
            Toggle_Cb.To_Marshaller (Toggled'Access),
            My_Device.Probes (Index)'Access);

      end loop;

      --  Create Interrupt button
      Gtk_New_With_Mnemonic (SP_Gtk.Interrupt, "Trigger interrupt");
      Pack_Start (SP_Gtk.Vbox, SP_Gtk.Interrupt, False, False, 0);
      Interrupt_Cb.Connect
        (SP_Gtk.Interrupt, "pressed",
         Interrupt_Cb.To_Marshaller (Trigger_Interrupt'Access), My_Device);

      Show_All (SP_Gtk.Window);

      SP_Gtk.Timeout :=
        Canvas_Sources.Timeout_Add (1000, Refresh_Canvas'Access, SP_Gtk);
   end Init_Gtk;

end Speed_Probe_Gtk;
