#include "rdma_common.h"
#include "rdma_client.h"

struct sockaddr_in s_addr;
struct sockaddr_in c_addr;
//@delee
//for server
pthread_t server_init;
pthread_t receiver;
pthread_t worker;
atomic_int wr_check[100];

int rdma_status;

//for client
extern struct ctrl client_session;
//pthread_t client_init;
//pthread_t worker[NUM_QUEUES];


//@delee
//for server
static void *process_server_init(void *arg)
{
        rdma_status = RDMA_INIT;
        start_rdma_server(s_addr);
}

//@delee
//for server
static void *process_receiver(void *arg)
{
//      int cpu = *(int *)arg;
//      struct queue *q = get_queue(cpu);
        struct queue *q = get_queue(0);
//
        int cnt = 0;
//      printf("%s: start on %d\n", __func__, cpu);

//      for (int i = 0; i < 100; i++) {
        while(true){
//              printf("%s: recv %d on %d!\n", __func__, i, cpu);
                rdma_recv_wr(q, &q->ctrl->servermr);
                rdma_poll_cq(q->cq, 1);
//              printf("%s: done %d on %d!\n", __func__, i, cpu);

                atomic_fetch_add(&wr_check[cnt], 1);
                cnt++;
        }
}

//@delee
//for server
static void *process_worker(void *arg)
{
        int idx = 0;

        while (true) {
                if (!atomic_load(&wr_check[idx]))
                        continue;

                printf("worker %d on!\n", idx);
                idx++;
        }
}

//static void *process_client_init(void *arg)
//{
//	rdma_status = RDMA_INIT;
//	start_rdma_client(&s_addr);
//	while (rdma_status == RDMA_CONNECT);
//}

//static void *simulator(void *arg)
static void simulator(void)
{
//	int cpu = *(int *)arg;
//	struct queue *q = get_queue(cpu);
	struct queue *q = get_queue(0);

//	printf("%s: start on %d\n", __func__, cpu);

	for (int i = 0; i < 100; i++) {
//		printf("%s: req %d on %d\n", __func__, i, cpu);
		printf("req\n");
		rdma_send_wr(q, IBV_WR_SEND, &q->ctrl->servermr, NULL);
		rdma_poll_cq(q->cq, 1);
//		printf("%s: done %d on %d\n", __func__, i, cpu);
		printf("%d done\n", i);
	}

//	rdma_status = RDMA_DISCONNECT;
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
	exit(1);
}

static void test_server()

int main(int argc, char* argv[])
{
	//@delee
	//ip and port of Host

//	int option;
//
//	while ((option = getopt(argc, argv, "i:p:")) != -1) {
//		switch (option) {
//			case 'i':
//				TEST_NZ(get_addr(optarg));
//				break;
//			case 'p':
//				s_addr.sin_port = htons(strtol(optarg, NULL, 0));
//				break;
//			default:
//				usage();
//				break;
//		}
//	}

	s_port = '50000'
	s_ip = '10.0.0.11'
	n_port = '50001'
	n_ip = '10.0.0.10'

	//This case is a  host side

	//@delee
	//for server
	//s_ip is a own ip
	s_addr.sin_port = htons(strtol(s_port, NULL, 0));
	s_addr.sin_addr.s_addr = htonl(strtol(s_ip, NULL, 0));
	//@delee
	//for client
	c_addr.sin_port = htons(strtol(n_port, NULL, 0));
	c_addr.sin_addr.s_addr = htonl(strtol(n_ip, NULL, 0));


	if (!s_addr.sin_port || !s_addr.sin_addr.s_addr) {
		usage();
		return 0;
	}

//	pthread_create(&client_init, NULL, process_client_init, NULL);
//	while (rdma_status != RDMA_CONNECT);

	TEST_NZ(start_rdma_client(&s_addr));

	// Client is connected with server throught RDMA from now.
	// From now on, You can do what you want to do with RDMA.
	printf("The client is connected successfully\n");
//	for (int i = 0; i < NUM_QUEUES; i++) {
//		pthread_create(&worker[i], NULL, simulator, &i);
//		sleep(1);
//	}
//
// 	pthread_join(client_init, NULL);
	sleep(1);
        simulator();

	return 0;
}
