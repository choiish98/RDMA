#include "rdma_common.h"
#include "rdma_client.h"

struct sockaddr_in s_addr;
extern struct ctrl client_session; 

static void simulator(void)
{
	struct queue *q = get_queue(0);

	for (int i = 0; i < 100; i++) {
		printf("req\n");
		rdma_send_wr(q, IBV_WR_SEND, &q->ctrl->servermr, NULL);
		rdma_poll_cq(q->cq, 1);
		printf("%d done\n", i);
	}
}

static inline int get_addr(char *sip)
{
	struct addrinfo *info;

	TEST_NZ(getaddrinfo(sip, NULL, NULL, &info));
	memcpy(&s_addr, info->ai_addr, sizeof(struct sockaddr_in));
	freeaddrinfo(info);
	return 0;
}

static void usage(void)
{
	printf("[Usage] : ");
	printf("./rdma_main [-i <server ip>] [-p <server port>]\n");
}

int main(int argc, char* argv[])
{
	int option;

	while ((option = getopt(argc, argv, "i:p:")) != -1) {
		switch (option) {
			case 'i':
				TEST_NZ(get_addr(optarg));
				break;
			case 'p':
				s_addr.sin_port = htons(strtol(optarg, NULL, 0));
				break;
			default:
				usage();
				break;
		}
	}

	if (!s_addr.sin_port || !s_addr.sin_addr.s_addr) {
		usage();
		return 0;
	}

	TEST_NZ(start_rdma_client(&s_addr));
	
	// Client is connected with server throught RDMA from now.
	// From now on, You can do what you want to do with RDMA.
	printf("The client is connected successfully\n");
	sleep(1);
	simulator();

	return 0;
}
