#include "rdma_common.h"
#include "rdma_client.h"
#include "rdma_client_handler.h"
#include "rdma_server.h"
#include "rdma_server_handler.h"
//pthread_t client_init;
//pthread_t worker[NUM_QUEUES];

extern struct sockaddr_in s_addr;
extern struct sockaddr_in c_addr;
//int rdma_status;
//extern struct ctrl client_session;

//static void *process_client_init(void *arg)
//{
//	rdma_status = RDMA_INIT;
//	start_rdma_client(&s_addr);
//	while (rdma_status == RDMA_CONNECT);
//}

//static void *simulator(void *arg)
//static void simulator(void)
//{
////	int cpu = *(int *)arg;
////	struct queue *q = get_queue(cpu);
//	struct queue *q = get_queue(0);
//
////	printf("%s: start on %d\n", __func__, cpu);
//
//	for (int i = 0; i < 100; i++) {
////		printf("%s: req %d on %d\n", __func__, i, cpu);
//		printf("req\n");
//		rdma_send_wr(q, IBV_WR_SEND, &q->ctrl->servermr, NULL);
//		rdma_poll_cq(q->cq, 1);
////		printf("%s: done %d on %d\n", __func__, i, cpu);
//		printf("%d done\n", i);
//	}
//
////	rdma_status = RDMA_DISCONNECT;
//}

static inline int get_addr(char *sip)
{
	struct addrinfo *info;

	TEST_NZ(getaddrinfo(sip, NULL, NULL, &info));
	memcpy(&c_addr, info->ai_addr, sizeof(struct sockaddr_in));
	freeaddrinfo(info);
	return 0;
}

//void print_sockaddr_in(const struct sockaddr_in *addr) {
//	char ip[INET_ADDRSTRLEN];
//
//	if (inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN) == NULL) {
//		perror("inet_ntop");
//		return;
//	}
//
//	int port = ntohs(addr->sin_port);
//
//	printf("IP: %s\n", ip);
//	printf("Port: %d\n", port);
//}

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
//				c_addr.sin_port = htons(strtol(optarg, NULL, 0));
				c_addr.sin_port = s_addr.sin_port;
				break;
			default:
				usage();
				break;
		}
	}

//	if (!c_addr.sin_port || !c_addr.sin_addr.s_addr) {
//		usage();
//		return 0;
//	}

	s_addr.sin_family = AF_INET;

////	pthread_create(&client_init, NULL, process_client_init, NULL);
////	while (rdma_status != RDMA_CONNECT);
//	
//	TEST_NZ(start_rdma_client(&s_addr));
//
//	// Client is connected with server throught RDMA from now.
//	// From now on, You can do what you want to do with RDMA.
//	printf("The client is connected successfully\n");
////	for (int i = 0; i < NUM_QUEUES; i++) {
////		pthread_create(&worker[i], NULL, simulator, &i);
////		sleep(1);
////	}
////
//// 	pthread_join(client_init, NULL);
//	sleep(1);
//        simulator();

	printf("s_addr\n");
	print_sockaddr_in(&s_addr);
	printf("c_addr\n");
	print_sockaddr_in(&c_addr);
	server_handler();
	sleep(2);
//	client_handler();

	return 0;
}
