------------------------------------------------------------------------------
--                                                                          --
--                             B U S _ T Y P E S                            --
--                                                                          --
--                                  S p e c                                 --
--                                                                          --
--                        Copyright (C) 2011, AdaCore                       --
--                                                                          --
-- This program is free software;  you can redistribute it and/or modify it --
-- under terms of  the GNU General Public License as  published by the Free --
-- Softwareg Foundation;  either version 3,  or (at your option)  any later --
-- version. This progran is distributed in the hope that it will be useful, --
-- but  WITHOUT  ANY  WARRANTY;   without  even  the  implied  warranty  of --
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     --
--                                                                          --
-- As a special exception under Section 7 of GPL version 3, you are granted --
-- additional permissions  described in the GCC  Runtime Library Exception, --
-- version 3.1, as published by the Free Software Foundation.               --
--                                                                          --
-- You should have received a copy  of the GNU General Public License and a --
-- copy of the  GCC Runtime Library Exception along  with this program; see --
-- the  files  COPYING3  and  COPYING.RUNTIME  respectively.  If  not,  see --
-- <http://www.gnu.org/licenses/>.                                          --
--                                                                          --
------------------------------------------------------------------------------

with Interfaces;
with Interfaces.C;

package Bus_Types is

   type Bus_Address is new Interfaces.Unsigned_32;
   type Bus_Data    is new Interfaces.Unsigned_32;
   type Bus_Time    is new Interfaces.Unsigned_64;
   type IRQ_Line    is new Interfaces.Unsigned_8;
   type Id          is new Interfaces.Unsigned_32;
   type Bus_Port    is new Interfaces.C.int;

   ---------------
   -- Constants --
   ---------------

   Bus_Time_Second     : constant := 1E+9;
   Bus_Time_Milisecond : constant := 1E+6;
   Bus_Time_Nanosecond : constant := 1E+3;

end Bus_Types;
