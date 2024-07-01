#include "rdma_common.h"
#include "rdma_server.h"
#include "rdma_client.h"

pthread_t server_init;
pthread_t client_init;
pthread_t receiver;
pthread_t worker_s;
pthread_t worker_c;

struct sockaddr_in s_addr;
struct sockaddr_in c_addr;
int rdma_server_status;
int rdma_client_status;
extern struct ctrl client_session;
extern struct ctrl server_session;
//
atomic_int wr_check[100];

static void *process_server_init(void *arg)
{
	rdma_server_status = RDMA_INIT;
	start_rdma_server(s_addr);
}

static void *process_client_init(void *arg)
{
        rdma_client_status = RDMA_INIT;
//	TEST_NZ(start_rdma_client(&c_addr));
	//@delee
	//TODO!!!
	// To be change &s_addr to something
	// currently this variable for server
	printf("%s: start_rdma_client\n", __func__);
        start_rdma_client(&c_addr);
	printf("%s: compelete start_rdma_client\n", __func__);
//	while (rdma_client_status == RDMA_CONNECT);
}

static void *simulator(void *arg)
{
        int cpu = *(int *)arg;
        struct queue *q = get_queue_client(cpu);

        printf("%s: start on %d\n", __func__, cpu);

//      for (int i = 0; i <= 100; i++) {
        int i = 0 ;
        while(true){
                printf("%s: req %d on %d\n", __func__, i, cpu);
                rdma_send_wr(q, IBV_WR_SEND, &q->ctrl->servermr, NULL);
                rdma_poll_cq(q->cq, 1);
                printf("%s: done %d on %d\n", __func__, i, cpu);
                i++;
                if(i ==100){
                        break;
                        printf("%s: i is 100 in while %d\n", __func__, cpu);
                }
        }
        printf("%s: end of while %d\n", __func__, cpu);

//      rdma_status = RDMA_DISCONNECT;
}

static inline int get_addr(char *sip)
{
        struct addrinfo *info;

        TEST_NZ(getaddrinfo(sip, NULL, NULL, &info));
        memcpy(&c_addr, info->ai_addr, sizeof(struct sockaddr_in));
        freeaddrinfo(info);
        return 0;
}

static void *process_receiver(void *arg)
{
	int cpu = *(int *)arg;
	struct queue *q = get_queue_server(cpu);
//	struct queue *q = get_queue(0);
//
	int cnt = 0;

	printf("%s: start on %d\n", __func__, cpu);

//	for (int i = 0; i < 100; i++) {
	while(true){
//	  	printf("%s: recv %d on %d!\n", __func__, i, cpu);
		rdma_recv_wr(q, &q->ctrl->servermr);
		rdma_poll_cq(q->cq, 1);
//		printf("%s: done %d on %d!\n", __func__, i, cpu);

		atomic_fetch_add(&wr_check[cnt], 1);
		cnt++;
	}
}

//
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


static void usage(void)
{
	printf("[Usage] : ");
	printf("./rdma_main [-port <server port>]\n");
	exit(1);
}

int main(int argc, char* argv[])
{
	int ret, option;

	while ((option = getopt(argc, argv, "i:p:")) != -1) {
		switch (option) {
			case 'i':
				TEST_NZ(get_addr(optarg));
				break;
			case 'p':
				s_addr.sin_port = htons(strtol(optarg, NULL, 0));
				c_addr.sin_port = htons(strtol(optarg, NULL, 0));
				printf("%s: listening on port %s.\n", __func__, optarg);
				break;
			default:
				usage();
				break;
		}
	}

	if (!s_addr.sin_port) {
		usage();
		return ret;
	}
	s_addr.sin_family = AF_INET;

	//@delee
	pthread_create(&server_init, NULL, process_server_init, NULL);
	pthread_create(&client_init, NULL, process_client_init, NULL);
	sleep(120);
	while (rdma_server_status != RDMA_CONNECT);
	printf("The server is connected successfully\n");
	while (rdma_client_status != RDMA_CONNECT);
	TEST_NZ(start_rdma_client(&c_addr));
	printf("The client is connected successfully\n");
        sleep(2);

	//@delee
	// num = 0,1 for server
	// num = 2 for client
        pthread_create(&worker_s, NULL, process_worker, NULL);
	pthread_create(&receiver, NULL, process_receiver, NULL);
	pthread_create(&worker_c, NULL, simulator,NULL);
	printf("All workers and receiver are make successfully\n");

	sleep(1);
	pthread_join(server_init, NULL);
	pthread_join(client_init, NULL);


	return 0;
}
