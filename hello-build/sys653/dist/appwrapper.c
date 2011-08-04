#include <apex/apexError.h>
#include <stdio.h>

#define STR(X) #X

static const APEX_BYTE msg[] = "Application exited";

extern void ENTRY_POINT (void);

void usrAppInit (void)
{
  RETURN_CODE_TYPE code;

  printf ("Starting " STR(APP) "\n");
  printf ("###>> ECHO ON\n");

  ENTRY_POINT ();

  RAISE_APPLICATION_ERROR (APPLICATION_ERROR,
			   (APEX_BYTE *)msg,
			   sizeof(msg) - 1,
			   &code);
}

