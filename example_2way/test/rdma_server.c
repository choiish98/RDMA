// rdma_server.c
#include "rdma_common.h"

void* poll_cq(void *arg) {
    struct ibv_cq *cq;
    struct ibv_wc wc;
    while (1) {
        ibv_get_cq_event(comp_chan, &cq, &arg);
        ibv_req_notify_cq(cq, 0);
        while (ibv_poll_cq(cq, 1, &wc)) {
            if (wc.status == IBV_WC_SUCCESS) {
                if (wc.opcode == IBV_WC_RECV) {
                    printf("Received message: %s\n", recv_region);
                } else if (wc.opcode == IBV_WC_SEND) {
                    printf("Send completed successfully.\n");
                }
            }
        }
        ibv_ack_cq_events(cq, 1);
    }
    return NULL;
}

void* receive_thread(void *arg) {
    struct ibv_sge sge;
    sge.addr = (uintptr_t)recv_region;
    sge.length = BUFFER_SIZE;
    sge.lkey = recv_mr->lkey;

    struct ibv_recv_wr recv_wr = {};
    recv_wr.wr_id = 0;
    recv_wr.sg_list = &sge;
    recv_wr.num_sge = 1;

    struct ibv_recv_wr *bad_recv_wr;
    while (1) {
        if (ibv_post_recv(conn->qp, &recv_wr, &bad_recv_wr)) {
            fprintf(stderr, "Error, ibv_post_recv() failed\n");
            exit(1);
        }
        sleep(1); // Simulating processing time
    }
    return NULL;
}

void* send_thread(void *arg) {
    strcpy(send_region, "Hello from RDMA!");

    struct ibv_sge sge;
    sge.addr = (uintptr_t)send_region;
    sge.length = BUFFER_SIZE;
    sge.lkey = send_mr->lkey;

    struct ibv_send_wr send_wr = {};
    send_wr.wr_id = 0;
    send_wr.sg_list = &sge;
    send_wr.num_sge = 1;
    send_wr.opcode = IBV_WR_SEND;
    send_wr.send_flags = IBV_SEND_SIGNALED;

    struct ibv_send_wr *bad_send_wr;
    while (1) {
        if (ibv_post_send(conn->qp, &send_wr, &bad_send_wr)) {
            fprintf(stderr, "Error, ibv_post_send() failed\n");
            exit(1);
        }
        sleep(1); // Simulating processing time
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <bind IP> <bind PORT> <peer IP> <peer PORT>\n", argv[0]);
        return 1;
    }

    const char *bind_ip = argv[1];
    int bind_port = atoi(argv[2]);
    const char *peer_ip = argv[3];
    int peer_port = atoi(argv[4]);

    struct sockaddr_in bind_addr, peer_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(bind_port);
    if (inet_pton(AF_INET, bind_ip, &bind_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid bind IP address: %s\n", bind_ip);
        return 1;
    }

    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_port);
    if (inet_pton(AF_INET, peer_ip, &peer_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid peer IP address: %s\n", peer_ip);
        return 1;
    }

    ec = rdma_create_event_channel();
    if (!ec) {
        perror("rdma_create_event_channel");
        return 1;
    }

    if (rdma_create_id(ec, &conn, NULL, RDMA_PS_TCP)) {
        perror("rdma_create_id");
        return 1;
    }

    if (rdma_bind_addr(conn, (struct sockaddr *)&bind_addr)) {
        perror("rdma_bind_addr");
        return 1;
    }

    if (rdma_resolve_addr(conn, NULL, (struct sockaddr *)&peer_addr, 2000)) {
        perror("rdma_resolve_addr");
        return 1;
    }

    if (rdma_get_cm_event(ec, &event)) {
        perror("rdma_get_cm_event");
        return 1;
    }

    if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
        fprintf(stderr, "Unexpected event %d\n", event->event);
        return 1;
    }
    rdma_ack_cm_event(event);

    if (rdma_resolve_route(conn, 2000)) {
        perror("rdma_resolve_route");
        return 1;
    }

    if (rdma_get_cm_event(ec, &event)) {
        perror("rdma_get_cm_event");
        return 1;
    }

    if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
        fprintf(stderr, "Unexpected event %d\n", event->event);
        return 1;
    }
    rdma_ack_cm_event(event);

    pd = ibv_alloc_pd(conn->verbs);
    if (!pd) {
        perror("ibv_alloc_pd");
        return 1;
    }

    comp_chan = ibv_create_comp_channel(conn->verbs);
    if (!comp_chan) {
        perror("ibv_create_comp_channel");
        return 1;
    }

    cq = ibv_create_cq(conn->verbs, 10, NULL, comp_chan, 0);
    if (!cq) {
        perror("ibv_create_cq");
        return 1;
    }

    if (ibv_req_notify_cq(cq, 0)) {
        perror("ibv_req_notify_cq");
        return 1;
    }

    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.cap.max_send_wr = 10;
    qp_attr.cap.max_recv_wr = 10;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;
    qp_attr.qp_type = IBV_QPT_RC;

    if (rdma_create_qp(conn, pd, &qp_attr)) {
        perror("rdma_create_qp");
        return 1;
    }

    struct rdma_conn_param conn_param = {};
    if (rdma_connect(conn, &conn_param)) {
        perror("rdma_connect");
        return 1;
    }

    recv_region = malloc(BUFFER_SIZE);
    send_region = malloc(BUFFER_SIZE);
    recv_mr = ibv_reg_mr(pd, recv_region, BUFFER_SIZE, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    send_mr = ibv_reg_mr(pd, send_region, BUFFER_SIZE, IBV_ACCESS_LOCAL_WRITE);

    pthread_t cq_poller_thread, recv_thread, send_thread;
    pthread_create(&cq_poller_thread, NULL, poll_cq, NULL);
    pthread_create(&recv_thread, NULL, receive_thread, NULL);
    pthread_create(&send_thread, NULL, send_thread, NULL);

    pthread_join(cq_poller_thread, NULL);
    pthread_join(recv_thread, NULL);
    pthread_join(send_thread, NULL);

    rdma_disconnect(conn);
    rdma_destroy_qp(conn);
    ibv_dereg_mr(recv_mr);
    ibv_dereg_mr(send_mr);
    free(recv_region);
    free(send_region);
    rdma_destroy_id(conn);
    rdma_destroy_event_channel(ec);

    return 0;
}
