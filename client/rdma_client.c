#include "rdma_client.h"

struct ctrl *client_session;
struct ibv_comp_channel *cc;
struct rdma_event_channel *ec;
struct rdma_cm_id *cm_id;
int queue_ctr = 0;

char client_memory[PAGE_SIZE]; 
struct ibv_mr *client_mr;

static int on_addr_resolved(struct rdma_cm_id *id)
{
	struct queue *q = &client_session->queues[queue_ctr++];
	int ret; 

	printf("%s\n", __func__);

	id->context = q;
	q->cm_id = id;

	if (!q->ctrl->dev) {
		ret = rdma_create_device(q);
		if (ret) {
			printf("%s: rdma_create_device failed\n", __func__);
			return ret;
		}
	}

	ret = rdma_create_queue(q, cc);
	if (ret) {
		printf("%s: rdma_create_queue failed\n", __func__);
		return ret;
	}

	ret = rdma_resolve_route(q->cm_id, CONNECTION_TIMEOUT_MS);
	if (ret) {
		printf("%s: rdma_resolve_route failed\n", __func__);
		return ret;
	}

	return ret;
}

static int on_route_resolved(struct queue *q)
{
	struct rdma_conn_param conn_param;
	int ret;

	printf("%s\n", __func__);

	bzero(&conn_param, sizeof(conn_param));
	conn_param.initiator_depth = 3;
	conn_param.responder_resources = 3;
	conn_param.retry_count = 3;

	ret = rdma_connect(q->cm_id, &conn_param);
	if (ret) {
		printf("%s: rdma_connect failed\n", __func__);
		return ret;
	}

	return ret;
}

static int on_connection(struct queue *q)
{
	struct mr_attr mr;
	int ret;

	printf("%s\n", __func__);

	if (!client_mr) {
		ret = rdma_create_mr(client_session->dev->pd);
		if (ret) {
			printf("%s: rdma_create_client_mr failed\n", __func__);
			return ret;
		}

		mr.addr = (uint64_t) client_mr->addr;
		mr.length = sizeof(struct mr_attr);
		mr.stag.lkey = client_mr->lkey;
		memcpy(client_memory, &mr, sizeof(struct mr_attr));
	
		ret = rdma_recv_wr(&client_session->queues[0], &mr);
		if (ret) {
			printf("%s: rdma_recv_mr failed\n", __func__);
			return ret;
		}

		ret = rdma_poll_cq(client_session->queues[0].cq, 1);
		if (ret) {
			printf("%s: rdma_poll_cq failed\n", __func__);
			return ret;
		}
	}

	return 1;
}

static int on_disconnect(struct queue *q) 
{
	int ret; 

	printf("%s\n", __func__);

	ret = rdma_disconnect(q->cm_id);
    if (ret) {
        printf("%s: rdma_disconnect \n", __func__);
		return ret;
    }

    rdma_destroy_qp(q->cm_id);
    ret = rdma_destroy_id(q->cm_id);
    if (ret) {
        printf("%s: rdma_destroy_id failed\n", __func__);
		return ret;
    }

    ret = ibv_destroy_cq(q->cq);
    if (ret) {
        printf("%s: ibv_destroy_cq failed\n", __func__);
		return ret;
    }

	if (cc) {
		ret = ibv_destroy_comp_channel(cc);
	    if (ret) {
		    printf("%s: ibv_destroy_comp_channel failed\n", __func__);
			return ret;
	    }
	}

	if (q->ctrl->dev->pd) {
		ret = ibv_dealloc_pd(q->ctrl->dev->pd);
	    if (!ret) {
		    printf("%s: ibv_dealloc_pd failed\n", __func__);
			return ret;
	    }
	}

	if (ec) {
		rdma_destroy_event_channel(ec);
	}

	return 1;
}

static int on_event(struct rdma_cm_event *event)
{
	struct queue *q = (struct queue *) event->id->context;
	printf("%s\n", __func__);

	switch (event->event) {
		case RDMA_CM_EVENT_ADDR_RESOLVED:
			return on_addr_resolved(event->id);
		case RDMA_CM_EVENT_ROUTE_RESOLVED:
			return on_route_resolved(q);
		case RDMA_CM_EVENT_ESTABLISHED:
			return on_connection(q);
		case RDMA_CM_EVENT_REJECTED:
			printf("%s: RDMA_CM_EVENT_REJECTD\n", __func__);
			return 1;
		case RDMA_CM_EVENT_DISCONNECTED:
			return on_disconnect(q);
		default:
			printf("%s: %s\n", __func__, rdma_event_str(event->event));
			return 1;
	}
}

int start_rdma_client(struct sockaddr_in *s_addr)
{
	struct rdma_cm_event *event;
	int ret;

	if (!client_session) {
		ret = rdma_alloc_session(&client_session);
		if (ret) {
			printf("%s: alloc_control failed\n", __func__);
			return -EINVAL;
		}
	}

	ec = rdma_create_event_channel();
	if (!ec) {
		printf("%s: rdma_create_event_channel failed\n", __func__);
		return -EINVAL;
	}

	ret = rdma_create_id(ec, &cm_id, NULL, RDMA_PS_TCP);
	if (ret) {
		printf("%s: rdma_create_id failed \n", __func__);
		return -EINVAL;
	}

	ret = rdma_resolve_addr(cm_id, NULL, (struct sockaddr *) s_addr, CONNECTION_TIMEOUT_MS);
	if (ret) {
		printf("%s: rdma_resolve_addr failed\n", __func__);
		return -EINVAL;
	}

	while (!rdma_get_cm_event(ec, &event)) {
		struct rdma_cm_event event_copy;

		memcpy(&event_copy, event, sizeof(*event));
		rdma_ack_cm_event(event);

		if (on_event(&event_copy)) {
			break;
		}
	}

	return ret;
}

int disconnect_rdma_client(void)
{
	int i, ret;

	printf("%s\n", __func__);

	for (i = 0; i < NUM_QUEUES; i++) {
		ret = on_disconnect(&client_session->queues[i]);
		if (!ret) {
			printf("%s: on_disconnect failed \n", __func__);
			return ret;
		}
	}

	free(client_session);
	return ret;
}
