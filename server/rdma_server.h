#ifndef _RDMA_SERVER_H
#define _RDMA_SERVER_H

#include "common.h"

#define NUM_QUEUES 1

int start_rdma_server(struct sockaddr_in s_addr);
#endif
