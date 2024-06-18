#include "rdma_common.h"
#include "rdma_server.h"

pthread_t server_init;
//pthread_t receiver[NUM_QUEUES];
pthread_t receiver;
pthread_t worker;

struct sockaddr_in s_addr;
int rdma_status;

//
atomic_int wr_check[100];

static void *process_server_init(void *arg)
{
	rdma_status = RDMA_INIT;
	start_rdma_server(s_addr);
}

static void *process_receiver(void *arg)
{
//	int cpu = *(int *)arg;
//	struct queue *q = get_queue(cpu);
	struct queue *q = get_queue(0);
//
	int cnt = 0;
//	printf("%s: start on %d\n", __func__, cpu);

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

	while ((option = getopt(argc, argv, "p:")) != -1) {
		switch (option) {
			case 'p':
				s_addr.sin_port = htons(strtol(optarg, NULL, 0));
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

	pthread_create(&server_init, NULL, process_server_init, NULL);
	while (rdma_status != RDMA_CONNECT);

//	printf("The server is connected successfully\n");

//	for (int i = 0; i < NUM_QUEUES; i++) {
//		pthread_create(&receiver[i], NULL, process_receiver, &i);
//		sleep(1);
//	}
//
        sleep(2);
        pthread_create(&worker, NULL, process_worker, NULL);
        pthread_create(&receiver, NULL, process_receiver, NULL);

	pthread_join(server_init, NULL);
	return ret;
}
