////////////////////////////////////////////////////////////////////////////////
// iov_server.c
//
// demonstrates using input/output vector (IOV) messaging
//
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>

#include "iov_server.h"

typedef union
{
	uint16_t msg_type;
	struct _pulse pulse;
	cksum_header_t cksum_hdr;
} msg_buf_t;

int calculate_checksum(char *text);

int main(void)
{
	rcvid_t rcvid;
	name_attach_t* attach;
	msg_buf_t msg;
	int status;
	int checksum;
	char* data;
	struct _msg_info minfo;

	attach = name_attach(NULL, CKSUM_SERVER_NAME, 0);
	if (attach == NULL)
	{ //was there an error creating the channel?
		perror("name_attach"); //look up the errno code and print
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		printf("Waiting for a message...\n");
		rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), &minfo);
		if (rcvid == -1)
		{ //was there an error receiving msg?
			perror("MsgReceive"); //look up errno code and print
			exit(EXIT_FAILURE); //give up
		}
		else if (rcvid > 0)
		{ //msg
			// did we get enough bytes for a type?
			if (minfo.msglen < sizeof(msg.msg_type))
			{
				MsgError(rcvid, EBADMSG);
				continue;
			}
			switch (msg.msg_type)
			{
			case CKSUM_IOV_MSG_TYPE:
				// did we get a full header?
				if (minfo.msglen < sizeof(msg.cksum_hdr))
				{
					MsgError(rcvid, EBADMSG);
					continue;
				}
				printf("Received a checksum request msg, header says the data is %d bytes\n",
						msg.cksum_hdr.data_size);
				// Did the client send all the data (now check srcmsglen)?
				if(minfo.srcmsglen < sizeof(msg.cksum_hdr) + msg.cksum_hdr.data_size)
				{
					MsgError(rcvid, EBADMSG);
					continue;
				}
				data = malloc(msg.cksum_hdr.data_size);
				if (data == NULL)
				{
					if (MsgError(rcvid, ENOMEM ) == -1)
					{
						perror("MsgError");
						continue;
					}
				}
				else
				{
					status = MsgRead(rcvid, data, msg.cksum_hdr.data_size, sizeof(cksum_header_t));
					if (status == -1)
					{
						const int save_errno = errno;
						perror("MsgRead");
						MsgError(rcvid, save_errno);
						free(data);
						continue;
					}
					checksum = calculate_checksum(data);
					free(data);
					status = MsgReply(rcvid, EOK, &checksum, sizeof(checksum));
					if (status == -1)
					{
						if (errno == ESRVRFAULT) {
							perror("MsgReply fatal");
							exit(EXIT_FAILURE);
						}
						perror("MsgReply");
					}
				}
				break;
			default:
				if (MsgError(rcvid, ENOSYS) == -1)
				{
					perror("MsgError");
				}
				break;
			}
		}
		else
		{ //pulse
			switch (msg.pulse.code)
			{
			case _PULSE_CODE_DISCONNECT:
				printf("Received disconnect pulse\n");
				if (ConnectDetach(msg.pulse.scoid) == -1)
				{
					perror("ConnectDetach");
				}
				break;
			case _PULSE_CODE_UNBLOCK:
				printf("Received unblock pulse\n");
				if (MsgError(msg.pulse.value.sival_long, -1) == -1)
				{
					perror("MsgError");
				}
				break;

			default:
				printf("unknown pulse received, code = %d\n", msg.pulse.code);
				break;
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

