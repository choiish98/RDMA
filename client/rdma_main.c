#include "rdma_common.h"
#include "rdma_client.h"

struct sockaddr_in s_addr;
extern struct ctrl client_session; 

static inline int get_addr(char *sip)
{
	struct addrinfo *info;
	int ret;

	ret = getaddrinfo(sip, NULL, NULL, &info);
	if (ret) {
		printf("%s: getaddrinfo failed\n", __func__);
		return ret;
	}

	memcpy(&s_addr, info->ai_addr, sizeof(struct sockaddr_in));
	freeaddrinfo(info);
	return ret;
}

static void usage(void)
{
	printf("[Usage] : ");
	printf("./rdma_main [-i <server ip>] [-p <server port>]\n");
}

int main(int argc, char* argv[])
{
	int ret, option;

	while ((option = getopt(argc, argv, "i:p:")) != -1) {
		switch (option) {
			case 'i':
				ret = get_addr(optarg);
				if (ret) {
						printf("%s: get_addr failed\n", __func__);
						return ret;
				}
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
		return ret;
	}

	ret = start_rdma_client(&s_addr);
	if (ret) {
		printf("%s: start_rdma_client failed\n", __func__);
		return ret;
	}
	
	// Client is connected with server throught RDMA from now.
	// From now on, You can do what you want to do with RDMA.
	printf("The client is connected successfully\n");
	sleep(5);

	ret = disconnect_rdma_client();
	if (!ret) {
		printf("%s: disconnect_rdma_client failed\n", __func__);
		return ret;
	}

	return ret;
}
