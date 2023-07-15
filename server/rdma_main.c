#include "common.h"
#include "rdma_server.h"

struct sockaddr_in s_addr;

static void usage(void)
{
	printf("[Usage] : ");
	printf("./rdma_main [-port <server port>]\n");
}

int main(int argc, char* argv[])
{
	int ret, option;

	while ((option = getopt(argc, argv, "p:")) != -1) {
		switch (option) {
			case 'p':
				s_addr.sin_port = htons(strtol(optarg, NULL, 0));
				printf("%s: listening on port %s.\n", __func__, optarg);
				break;
			default:
				usage();
		}
	}

	if (!s_addr.sin_port) usage();
	s_addr.sin_family = AF_INET;

	start_rdma_server(s_addr);
	// server is running now.

	return ret;
}
