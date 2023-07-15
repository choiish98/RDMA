#ifndef _RDMA_CLIENT_H
#define _RDMA_CLIENT_H

#include "common.h"

int start_rdma_client(struct sockaddr_in *s_addr);
int client_disconnect_and_clean(int idx);
#endif
