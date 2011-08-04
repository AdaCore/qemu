/* 
 * Copyright (c) 2007 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

#include <stdio.h>
#include <apex/apexError.h>

void hello (void)
    {
#define MSG "explicit raise"
      static APEX_BYTE msg[] = MSG;
      RETURN_CODE_TYPE code;
      int i;

    printf ("Hello, world!\n");
    for (i = 0; i < 2; i++)
      RAISE_APPLICATION_ERROR (APPLICATION_ERROR,
  			     msg,
			     sizeof(msg) - 1,
			     &code);
    printf ("Result: code=%u\n", code);
    }

