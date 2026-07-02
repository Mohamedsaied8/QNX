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

#define INT_PULSE_CODE	(_PULSE_CODE_MINAVAIL+0)
#define INTERUPT_PRIORITY (15)

int main(int argc, char **argv)
{
	int id, chid, self_coid;
	int ret_val;
	int count = 0;
	struct _pulse msg;
	struct sigevent int_event; // the event to wake up the thread

	printf("starting...\n");

	// create a channel for receiving the pulse message
	chid = ChannelCreate(_NTO_CHF_PRIVATE);
	if (chid == -1) {
		perror("ChannelCreate");
		exit(EXIT_FAILURE);
	}

	// create a connection to our channel so that the kernel will know where to
	// send the pulse
	self_coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);
	if (self_coid == -1) {
		perror("ConnectAttach");
		exit(EXIT_FAILURE);
	}

	// set up an event for the kernel to use to wake up
	// this thread.  Use whatever type of event and event handling you want.
	// In this case it's a pulse.
	SIGEV_PULSE_INIT(&int_event, self_coid, INTERUPT_PRIORITY, INT_PULSE_CODE, 0);

	// give the kernel the event
	id = InterruptAttachEvent(1, &int_event, 0);
	if (id == -1) {
		perror("InterruptAttachEvent()");
		exit(EXIT_FAILURE);
	}

    while (1) {
    	// block here waiting for the event
    	ret_val = MsgReceivePulse(chid, &msg, sizeof(msg), NULL);
		if (ret_val == -1) {
			perror("MsgReceive");
			exit(EXIT_FAILURE);

		}
		if (msg.code == INT_PULSE_CODE) {
			// we got the pulse from the interrupt, do any work and unmask it
			if (InterruptUnmask(0, id) == -1) {
				perror("InterruptUnmask");
			}
			// if using a high frequency interrupt, don't print every interrupt
			printf("We got the event and unblocked, count = %d\n", count);
			count++;

		} else {
			fprintf(stderr, "Got unexpected pulse with code: %d\n", msg.code);
		}
    }
}
