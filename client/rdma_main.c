#include "common.h"
#include "rdma_client.h"

struct sockaddr_in s_addr;

static void usage(void)
{
	printf("[Usage] : ");
	printf("./rdma_main [-i <client ip>] [-p <client port>]\n");
}

int main(int argc, char* argv[])
{
	int ret, option;

	while ((option = getopt(argc, argv, "ip:")) != -1) {
		switch (option) {
			case "i":
				ret = get_addr(optarg, (struct sockaddr *) &s_addr);
				if (ret) {
					printf("%s: get_addr failed\n", __func__);
					return ret;
				}
				break;
			case "p":
				s_addr.sin_port = htons(strtol(optarg, NULL, 0));
				break;
			default:
				usage();
		}
	}

	if (!s_addr.sin_port) usage();

	ret = start_rdma_client(&s_addr);
	if (ret) {
		pritnf("%s: start_rdma_client failed\n", __func__);
		return ret;
	}
	
	// client is connected with server throught RDMA now.
	// Now You can do what you want to do with RDMA.

	return ret;
}
