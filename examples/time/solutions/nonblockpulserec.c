/*
 *  This module demonstrates how a pulse receiver can clean up a burst
 *  of pulses.  When you receive the first one,
 *  you might want to clean up any others before going back to the main loop.
 *
 *  The trick is to turn the blocking MsgReceivePulse() call into a
 *  nonblocking call.  To do this we use a zero length timeout.
 *  If a pulse is waiting then MsgReceivePulse() is not about to
 *  block so it simply returns with the pulse.  If no pulse is waiting
 *  then MsgReceivePulse() is about to block so it immediately times out.
 *  Since it timed out you know that there is no pulse waiting and can
 *  go back to the main loop. 
 *
 *  To make things simple, the pulse deliverer is just another thread in 
 *  this same process.  To test this:
 *
 *    nonblockpulserec -v &
 *
 *  nonblockpulserec will soon start getting bursts of pulses.
 *
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <string.h>

#define OUR_PULSE_CODE  (_PULSE_CODE_MINAVAIL + 7)

void *pulse_sender_thread(void *arg);
void options(int argc, char **argv);

int verbose; /* -v for verbose operation */
int chid; /* the channel for receiving pulses on */
int pulse_rec_coid; /* the connection to send the pulse to */

int main(int argc, char **argv)
{
	rcvid_t rcvid;
	int flags, status;
	struct _pulse pulse;
	int ret;

	printf("nonblockpulserec:  starting...\n");

	options(argc, argv);

	chid = ChannelCreate(_NTO_CHF_PRIVATE);
	if (chid == -1)
	{
		perror("ChannelCreate");
		exit(EXIT_FAILURE);
	}

	/* create the thread that will send us the pulses */
	status = pthread_create(NULL, NULL, pulse_sender_thread, NULL);
	if (status != EOK)
		{
			fprintf(stderr, "pthread_create failed: %s\n", strerror(status));
			exit(EXIT_FAILURE);
		}

	while (1)
	{
		rcvid = MsgReceive(chid, &pulse, sizeof(pulse), NULL);
		if (rcvid == -1)
		{
			perror("MsgReceive");
			exit(EXIT_FAILURE);
		}

		printf("we received a pulse, checking for more");

		flags = _NTO_TIMEOUT_RECEIVE;
		/* loop, receiving (cleaning up) all pulses in receive queue */
		do
		{
			/* MsgReceivePulse() wont block, if there's a pulse it
			 * will return 0, otherwise it will timeout immediately
			 */
			TimerTimeout(CLOCK_MONOTONIC, flags, NULL, NULL, NULL);
			ret = MsgReceivePulse(chid, &pulse, sizeof(pulse), NULL);
			if (ret == 0)
				printf("... pulse");
		} while (ret != -1);
		/* if errno is ETIMEDOUT, then we got all the pulses */
		printf("\n");
	}
}

/*
 *  pulse_sender_thread
 *
 *  This simulates an interrupt that delivers bursts of pulses
 *  every now and then.
 */

void *
pulse_sender_thread(void *arg)
{
	int i, coid;

	coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);
	if (coid == -1)
	{
		perror("ConnectAttach");
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		sleep(5);
		/* send a burst of pulses */
		for (i = 0; i < 7; i++)
			if(MsgSendPulse(coid, OUR_PULSE_CODE, 0, 0) == -1)
				perror("MsgSendPulse");
	}
	return NULL;
}

/*
 *  options
 *
 *  This routine handles the command line options:
 *      -v          verbose operation
 */

void options(int argc, char **argv)
{
	int opt;

	verbose = 0;

	while ((opt = getopt(argc, argv, "v")) != -1)
	{
		switch (opt)
		{
		case 'v':
			verbose = 1;
			break;
		}
	}
}
