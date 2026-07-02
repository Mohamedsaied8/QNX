/*
 *  intsimple.c
 *
 *  This module will contain code for handling an interrupt.
 *
 *  To test it, simply run it.  Note that you may have to do something
 *  to cause the interrupts to be generated (e.g. press a key if
 *  handling the keyboard interrupt).
 * 
 *  On an x86_64 box a good choice for the interrupt to use would be:
 *    1 - keyboard 
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/neutrino.h>

int main(int argc, char **argv)
{
	printf("starting...\n");

	//TODO set up an event if giving an event for the kernel to use to wake up
	// this thread.  Use whatever type of event and event handling you want

	//TODO either register as an IST or give the kernel the event

	while (1)
	{
		//TODO block here waiting for the interrupt

		// if using a high frequency interrupt, don't print every interrupt
		printf("We got the event and unblocked\n");
	}
}
