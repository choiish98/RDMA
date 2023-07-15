#ifndef _RDMA_CLIENT_H
#define _RDMA_CLIENT_H

#include "common.h"

#define NUM_CLIENT_QUEUES 1

int start_rdma_client(struct sockaddr_in *s_addr);
int disconnect_rdma_client(void);
#endif
