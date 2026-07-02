////////////////////////////////////////////////////////////////////////////////
// server.c
//
// A QNX msg passing server.  It should receive a string from a client,
// calculate a checksum on it, and reply back the client with the checksum.
//
// The server prints out its pid and chid so that the client can be made aware
// of them.
//
// Using the comments below, put code in to complete the program.  Look up 
// function arguments in the course book or the QNX documentation.
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <process.h>

#include "msg_def.h"  //layout of msgs should always defined by a struct, here's its definition

int
calculate_checksum(char *text);

typedef union
{
	uint16_t type;
	cksum_msg_t cksum_msg;
	struct _pulse pulse;
} recv_buf_t;

int main(void)
{
	int chid;
	int pid;
	rcvid_t rcvid;
	//	cksum_msg_t msg;
	recv_buf_t rbuf;
	int status;
	int checksum;

	//PUT CODE HERE to create a channel, store channel id in the chid variable
	chid = ChannelCreate(0);
	if (chid == -1)
	{ //was there an error creating the channel?
		perror("ChannelCreate()"); //look up the errno code and print
		exit(EXIT_FAILURE);
	}

	pid = getpid(); //get our own pid
	printf("Server's pid: %d, chid: %d\n", pid, chid); //print our pid/chid so
	//client can be told where to connect

	while (1)
	{
		//PUT CODE HERE to receive msg from client, store the receive id in rcvid
		rcvid = MsgReceive(chid, &rbuf, sizeof(rbuf), NULL );
		if (rcvid == -1)
		{ //was there an error receiving msg?
			perror("MsgReceive"); //look up errno code and print
			exit(EXIT_FAILURE); //give up
		}
		else if (rcvid == 0)
		{
			printf("we got a pulse with a code of %d, and value of %#lx\n", rbuf.pulse.code,
					rbuf.pulse.value.sival_long);
		}
		else // we got a message
		{
			if (rbuf.type == CKSUM_MSG_TYPE)
			{
				printf("Got a checksum message\n");
				checksum = calculate_checksum(rbuf.cksum_msg.string_to_cksum);
				
				//PUT CODE HERE TO reply to client with checksum, store the return status in status
				status = MsgReply(rcvid, EOK, &checksum, sizeof(checksum));
				if (status == -1)
				{
					perror("MsgReply");
				}
			}
			else
			{
				// unknown message type, unblock client with an error
				if (MsgError(rcvid, ENOSYS) == -1)
					perror("MsgError");
			}
		}
	}
	return 0;
}

int calculate_checksum(char *text)
{
	char *c;
	int cksum = 0;

	for (c = text; *c != '\0'; c++)
		cksum += *c;
	return cksum;
}

