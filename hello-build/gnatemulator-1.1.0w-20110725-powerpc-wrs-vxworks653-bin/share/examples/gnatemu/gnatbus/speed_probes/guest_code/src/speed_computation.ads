with System; use System;
package Speed_Computation is

   Probe_Base_address : constant := 16#8001_0000#;

   type Register_32 is mod 2**32;
   for Register_32'Size use 32;
   pragma Suppress_Initialization (Register_32);

   Probe_1_Speed : Register_32;
   pragma Atomic (Probe_1_Speed);
   for Probe_1_Speed'Address use System'To_Address (Probe_Base_Address);

   Probe_1_Error : Register_32;
   pragma Atomic (Probe_1_Error);
   for Probe_1_Error'Address use System'To_Address (Probe_Base_Address + 4);

   Probe_2_Speed : Register_32;
   pragma Atomic (Probe_2_Speed);
   for Probe_2_Speed'Address use System'To_Address (Probe_Base_Address + 8);

   Probe_2_Error : Register_32;
   pragma Atomic (Probe_2_Error);
   for Probe_2_Error'Address use System'To_Address (Probe_Base_Address + 12);

   Probe_3_Speed : Register_32;
   pragma Atomic (Probe_3_Speed);
   for Probe_3_Speed'Address use System'To_Address (Probe_Base_Address + 16);

   Probe_3_Error : Register_32;
   pragma Atomic (Probe_3_Error);
   for Probe_3_Error'Address use System'To_Address (Probe_Base_Address + 20);

   type Probe_Interface is record
      Speed_Measured : Float   := 0.0;
      Error_Flag     : Boolean := False;
   end record;

   type Probe_Array is array (1 .. 3) of Probe_Interface;

   procedure Compute_Speed
     (Probes          : Probe_Array;
      Speed           : out Float;
      General_Error   : out Boolean);

   procedure Read_Probes (Probes : out Probe_Array);

   procedure Write_Result (Speed : Float; Error : Boolean);


   Clean_Freq : Integer := 100;

end Speed_Computation;
