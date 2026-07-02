/*
 * event_server.c
 *
 * This program, along with event_client.c, demonstrate pulses between a
 * server and a client using MsgDeliverEvent().
 *
 * The server attaches a name for the client to find using name_attach().
 * Since it is using name_attach(), it will have to handle some system pulses
 * and possibly messages, as well as the ones we're interested in.
 *
 * It will get a register message from the client.  This message will
 * contain an event to be delivered to the client.  In the notify thread,
 * it will need to deliver this notification every second.
 *
 * When it gets a disconnect notification from the client, it needs to
 * clean up.
 *
 * You need to add the code to store away the client's rcvid and event when
 * you receive the registration message, and to deliver the event when you
 * receive the regular pulse.
 *
 *  To test it, run it as follows:
 *    event_server
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>

#include "event_server.h"
#include <unistd.h>
#include <pthread.h>
#include <string.h>

/* server for client-server pulsing exercise */
union recv_msgs
{
	struct notification_request_msg client_msg;
	struct _pulse pulse;
	uint16_t type;
} recv_buf;

// client tracking information
rcvid_t save_rcvid = 0;
int save_scoid = 0;
struct sigevent save_event;
int notify_count = 0;
pthread_mutex_t save_data_mutex; // protect access to client information between threads using it

// this thread will notify every second any client that needs notification
void * notify_thread(void * ignore);

int main(int argc, char *argv[])
{
	name_attach_t *att;
	rcvid_t rcvid;
	struct _msg_info msg_info;
	int status;

	// register our name so the client can find us
	att = name_attach(NULL, RECV_NAME, 0);
	if (att == NULL)
	{
		perror("name_attach()");
		exit(EXIT_FAILURE);
	}

	// initialize the save data mutex
	status = pthread_mutex_init(&save_data_mutex, NULL );
	if (status != EOK)
	{
		fprintf(stderr, "pthread_mutex_init failed: %s\n", strerror(status));
		exit(EXIT_FAILURE);
	}

	// create the client notification thread
	status = pthread_create(NULL, NULL, notify_thread, NULL );
	if (status != EOK)
	{
		fprintf(stderr, "pthread_create failed: %s\n", strerror(status));
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		// wait for messages and pulses
		rcvid = MsgReceive(att->chid, &recv_buf, sizeof(recv_buf), &msg_info);
		if (rcvid == -1)
		{
			perror("MsgReceive failed");
			exit(EXIT_FAILURE);
		}
		if (rcvid == 0)
		{
			/* we received a pulse
			 */
			switch (recv_buf.pulse.code)
			{
			/* system disconnect pulse */
			case _PULSE_CODE_DISCONNECT:
				/* a client has disconnected.  Verify that it is
				 * our client, and if so, clean up our saved date
				 */
				status = pthread_mutex_lock(&save_data_mutex);
				if (status != EOK)
				{
					fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(status));
					exit(EXIT_FAILURE);
				}
				if (save_scoid == recv_buf.pulse.scoid)
				{
					// our client went away, clean up any data associate
					// with this client
					save_scoid = 0;
					save_rcvid = 0;
				}
				status = pthread_mutex_unlock(&save_data_mutex);
				if (status != EOK)
				{
					fprintf(stderr, "pthread_mutex_unlock failed: %s\n", strerror(status));
					exit(EXIT_FAILURE);
				}

				/* always do the ConnectDetach(), though */
				if (ConnectDetach(recv_buf.pulse.scoid) == -1)
				{
					perror("ConnectDetach");
				}
				printf("disconnect from a client %#x\n", recv_buf.pulse.scoid);
				break;
				/* system unblock pulse */
			case _PULSE_CODE_UNBLOCK:
				// did we forget to Reply to our client?
				printf("got an unblock pulse, did you forget to reply to your client?\n");
				if (MsgError(recv_buf.pulse.value.sival_long, -1 ) == -1)
				{
					perror("MsgError");
				}
				break;
			default:
				printf("unexpected pulse code: %d\n", recv_buf.pulse.code);
				break;
			}
			continue;
		}

		/* not an error, not a pulse, therefore a message */
		switch (recv_buf.type)
		{
		case REQUEST_NOTIFICATIONS:

		    /* TODO: verify the event in the incoming message (recv_buf.client_msg.ev).
		     * If it fails verification, return an error (MsgError) and
		     * continue (wait for more clients).
		     */

			status = pthread_mutex_lock(&save_data_mutex);
			if (status != EOK)
			{
				fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(status));
				exit(EXIT_FAILURE);
			}
            /* TODO: We have a valid event from the client, and
             * will be delivering it later.  This needs the rcvid
             * and event structure to pass to MsgDeliverEvent.
             *
             * Store away the event (recv_buf.client_msg.ev) and rcvid
             * in save_event and save_rcvid.
             */

			// save away the scoid so we can handle disconnect properly
			save_scoid = msg_info.scoid;
			status = pthread_mutex_unlock(&save_data_mutex);
			if (status != EOK)
			{
				fprintf(stderr, "pthread_mutex_unlock failed: %s\n", strerror(status));
				exit(EXIT_FAILURE);
			}

			// TODO: don't forget to reply to the client!

			printf("got register message from client: %#lx\n", rcvid);
			break;
		default:
			/* some other unexpected message */
			printf("unexpected message type: %d\n", recv_buf.type);
			if (MsgError(rcvid, ENOSYS) == -1)
			{
				perror("MsgError");
			}
			break;
		}
	}
	return EXIT_FAILURE;
}

// this thread will notify every second any client that needs notification
void * notify_thread(void * ignore)
{
	int errornum;
	while (1)
	{
		sleep(1);

		errornum = pthread_mutex_lock(&save_data_mutex);
		if (errornum != EOK)
		{
			fprintf(stderr, "pthread_mutex_lock failed: %s\n", strerror(errornum));
			exit(EXIT_FAILURE);
		}
		if (save_rcvid)
		{
			printf("deliver pulse to client %#lx\n", save_rcvid);

			/* TODO: Add the call to MsgDeliverEvent() in order
			 * to send the event to the client.  The rcvid is in
			 * save_rcvid and the event is in save_event.
			 */

		}
		errornum = pthread_mutex_unlock(&save_data_mutex);
		if (errornum != EOK)
		{
			fprintf(stderr, "pthread_mutex_unlock failed: %s\n", strerror(errornum));
			exit(EXIT_FAILURE);
		}
	}
	return NULL;
}
