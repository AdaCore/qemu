
with System;
with Ada.Real_Time;

pragma Warnings (Off);
with uart;
pragma Warnings (On);

procedure Main is
   pragma Priority (System.Priority'First);

   use type Ada.Real_Time.Time_Span;
begin
   --  Non-terminating environment task

   delay until Ada.Real_Time.Time_Last;
end Main;
