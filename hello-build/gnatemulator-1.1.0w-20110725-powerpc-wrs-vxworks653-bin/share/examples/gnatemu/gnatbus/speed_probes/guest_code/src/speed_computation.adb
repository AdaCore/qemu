with Speed_Computation; use Speed_Computation;

package body Speed_Computation is

   -------------------
   -- Compute_Speed --
   -------------------

   procedure Compute_Speed
     (Probes          : Probe_Array;
      Speed           : out Float;
      General_Error   : out Boolean)
   is
      function Within_Margin (Left, Right : Probe_Interface) return Boolean;

      function Within_Margin (Left, Right : Probe_Interface) return Boolean is
      begin
         if Left.Error_Flag or else Right.Error_Flag then
            return False;
         else
            return abs (Left.Speed_Measured - Right.Speed_Measured) < 10.0;
         end if;
      end Within_Margin;
   begin
      General_Error := False;
      if Within_Margin (Probes (1), Probes (2)) then
         Speed := Probes (1).Speed_Measured;
      elsif Within_Margin (Probes (2), Probes (3)) then
         Speed := Probes (2).Speed_Measured;
      elsif Within_Margin (Probes (3), Probes (1)) then
         Speed := Probes (3).Speed_Measured;
      else
         General_Error := True;
         Speed := 0.0;
      end if;
   end Compute_Speed;

   -----------------
   -- Read_Probes --
   -----------------

   procedure Read_Probes (Probes : out Probe_Array) is
   begin
      Probes (1).Speed_Measured := Float (Probe_1_Speed);
      Probes (1).Error_Flag     := Probe_1_Error /= 0;
      Probes (2).Speed_Measured := Float (Probe_2_Speed);
      Probes (2).Error_Flag     := Probe_2_Error /= 0;
      Probes (3).Speed_Measured := Float (Probe_3_Speed);
      Probes (3).Error_Flag     := Probe_3_Error /= 0;
   end Read_Probes;

   procedure Write_Result (Speed : Float; Error : Boolean) is
   begin
      Probe_1_Speed := Register_32 (Speed);
      if Error then
         Probe_1_Error := Register_32 (10);
      else
         Probe_1_Error := Register_32 (0);
      end if;
   end Write_Result;
end Speed_Computation;
