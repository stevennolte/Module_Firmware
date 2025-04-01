#ifndef OTAPullMethods
#define OTAPullMethods
#include <Arduino.h>


void callback(int offset, int totallength)
{
	Serial.printf("Updating %d of %d (%02d%%)...\n", offset, totallength, 100 * offset / totallength);
#if defined(LED_BUILTIN) // flicker LED on update
	static int status = LOW;
	status = status == LOW && offset < totallength ? HIGH : LOW;
	digitalWrite(LED_BUILTIN, status);
#endif
}


#endif