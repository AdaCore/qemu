/****************************************************************************
 *                                                                          *
 *                       P A C K E T _ H A N D L E R S                      *
 *                                                                          *
 *                                  S p e c                                 *
 *                                                                          *
 *                        Copyright (C) 2011, AdaCore                       *
 *                                                                          *
 * This program is free software;  you can redistribute it and/or modify it *
 * under terms of  the GNU General Public License as  published by the Free *
 * Softwareg Foundation;  either version 3,  or (at your option)  any later *
 * version. This progran is distributed in the hope that it will be useful, *
 * but  WITHOUT  ANY  WARRANTY;   without  even  the  implied  warranty  of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                          *
 * As a special exception under Section 7 of GPL version 3, you are granted *
 * additional permissions  described in the GCC  Runtime Library Exception, *
 * version 3.1, as published by the Free Software Foundation.               *
 *                                                                          *
 * You should have received a copy  of the GNU General Public License and a *
 * copy of the  GCC Runtime Library Exception along  with this program; see *
 * the  files  COPYING3  and  COPYING.RUNTIME  respectively.  If  not,  see *
 * <http://www.gnu.org/licenses/>.                                          *
 *                                                                          *
 ****************************************************************************/

#ifndef _PACKET_HANDLERS_H_
#define _PACKET_HANDLERS_H_

#include "gnat-bus-interface.h"
#include "bus_interface.h"

GnatBusPacket *receive_packet(Device *dev);

GnatBusPacket_Response *send_and_wait_resp(Device                *dev,
                                           GnatBusPacket_Request *request);

int send_packet(Device *dev, GnatBusPacket *packet);

void process_packet(Device *dev, GnatBusPacket *packet);

#endif /* ! _PACKET_HANDLERS_H_ */
