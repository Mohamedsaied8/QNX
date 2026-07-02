/*
 *  intsimple.c
 *
 *  This module will contain code for handling an interrupt.
 *
 *  To test it, simply run it.  Note that you may have to do something
 *  to cause the interrupts to be generated (e.g. press a key if
 *  handling the keyboard interrupt).
 *
 *  On an x86_64 box a good choices for the interrupt to use would be:
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
	int id;
	int count = 0;

	printf("starting...\n");

	// register as an IST
	id = InterruptAttachThread(1, 0);
	if (id == -1) {
		perror("InterruptAttachThread()");
		exit(EXIT_FAILURE);
	}

    while (1) {
    	// block here waiting for the event
		if (InterruptWait(_NTO_INTR_WAIT_FLAGS_FAST, NULL) == -1) {
			perror("InterruptWait");
			exit(EXIT_FAILURE);
		}
		// we got the interrupt, do any work and unmask it
		if (InterruptUnmask(0, id) == -1) {
			perror("InterruptUnmask");
		}
    	// if using a high frequency interrupt, don't print every interrupt
		printf("We got the event and unblocked, count = %d\n", count);
		count++;
    }
}
