#include "rdma_common.h"
#include "rdma_client.h"

pthread_t client_init;
pthread_t worker[NUM_QUEUES];

struct sockaddr_in s_addr;
int rdma_status;
extern struct ctrl client_session;

static void *process_client_init(void *arg)
{
    rdma_status = RDMA_INIT;
    start_rdma_client(&s_addr);
    return NULL;
}

static void *simulator(void *arg)
{
    int cpu = *(int *)arg;
    struct queue *q = get_queue(cpu);

    if (q == NULL) {
        printf("simulator: Failed to get queue for CPU %d\n", cpu);
        return NULL;
    }

    printf("%s: start on %d\n", __func__, cpu);

    int i = 0;
    while (true) {
        printf("%s: req %d on %d\n", __func__, i, cpu);
        rdma_send_wr(q, IBV_WR_SEND, &q->ctrl->servermr, NULL);
        rdma_poll_cq(q->cq, 1);
        printf("%s: done %d on %d\n", __func__, i, cpu);
        i++;
        if (i == 100) {
            break;
        }
    }
    printf("%s: end of while %d\n", __func__, cpu);
    return NULL;
}

static inline int get_addr(char *sip)
{
    struct addrinfo *info;

    if (getaddrinfo(sip, NULL, NULL, &info) != 0) {
        printf("Failed to get address info for %s\n", sip);
        return -1;
    }
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
                if (get_addr(optarg) != 0) {
                    usage();
                    return 1;
                }
                break;
            case 'p':
                s_addr.sin_port = htons(strtol(optarg, NULL, 0));
                break;
            default:
                usage();
                return 1;
        }
    }

    if (!s_addr.sin_port || !s_addr.sin_addr.s_addr) {
        usage();
        return 1;
    }

    pthread_create(&client_init, NULL, process_client_init, NULL);
    while (rdma_status != RDMA_CONNECT);

    int i = 0;
    while (i < NUM_QUEUES) {
        int *cpu = malloc(sizeof(int));
        if (cpu == NULL) {
            printf("Failed to allocate memory for CPU %d\n", i);
            return 1;
        }
        *cpu = i;
        pthread_create(&worker[i], NULL, simulator, cpu);
        i++;
    }

    i = 0;
    while (i < NUM_QUEUES) {
        pthread_join(worker[i], NULL);
        i++;
    }

    return 0;
}
