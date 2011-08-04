with Speed_Computation; use Speed_Computation;
with Ada.Real_Time;
with Target_Support.Interrupt; use Target_Support.Interrupt;

with IRQ_P;

procedure Computation_Loop is

   use type Ada.Real_Time.Time_Span;
   Offset_T1 : constant Ada.Real_Time.Time_Span
     := Ada.Real_Time.Seconds (1);
   Test_Period : constant Ada.Real_Time.Time_Span
     := Ada.Real_Time.MIlliseconds (1000);

   Speed  : Float;
   Error  : Boolean := False;
   Probes : Probe_Array;
   Next_Time : Ada.Real_Time.Time := Ada.Real_Time.Clock + Offset_T1;

begin

   Enable_External_Interrupt;

   loop
      Next_Time := Next_Time + Test_Period;
      delay until Next_Time;

      Read_Probes (Probes);
      Compute_Speed (Probes, Speed, Error);
      Write_Result (Speed, Error);
      Error := False;
   end loop;
end Computation_Loop;
