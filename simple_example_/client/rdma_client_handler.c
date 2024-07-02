#include "rdma_common.h"
#include "rdma_client.h"

pthread_t client_init;
pthread_t worker;

struct sockaddr_in s_addr;
int rdma_status;
extern struct ctrl client_session;

static void *process_client_init(void *arg)
{
        rdma_status = RDMA_INIT;
        start_rdma_client(&s_addr);
}

static void *simulator(void *arg)
{
	struct queue *q = get_queue(0);

	printf("%s: start\n", __func__);

        for (int i = 0; i < 100; i++) {
                rdma_send_wr(q, IBV_WR_SEND, &q->ctrl->servermr, NULL);
                rdma_poll_cq(q->cq, 1);
                printf("%d done\n", i);
        }

      rdma_status = RDMA_DISCONNECT;
}

//static inline int get_addr(char *sip)
//{
//        struct addrinfo *info;
//
//        TEST_NZ(getaddrinfo(sip, NULL, NULL, &info));
//        memcpy(&s_addr, info->ai_addr, sizeof(struct sockaddr_in));
//        freeaddrinfo(info);
//        return 0;
//}

void client_handler()
{
	pthread_create(&client_init, NULL, process_client_init, NULL);
	while (rdma_status != RDMA_CONNECT);
	printf("The client is connected successfully\n");

	sleep(2);

	pthread_create(&worker, NULL, simulator, NULL);
	pthread_join(client_init, NULL);

}


