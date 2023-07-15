#include "rdma_client.h"

struct ctrl *cctrl;
struct ibv_comp_channel *io_completion_channel;
struct rdma_event_channel *ec;
struct rdma_cm_id *cm_id;

char client_memory[PAGE_SIZE]; 
struct ibv_mr *client_mr;

static unsigned int queue_ctr = 0;

static int rdma_create_device(struct queue *q)
{
	struct device *dev;

	dev = (struct device *)malloc(sizeof(*dev));
	if (!dev) {
		printf("%s: malloc dev failed\n", __func__);
		return -EINVAL;
	}

	dev->verbs = q->cm_id->verbs;
	dev->pd = ibv_alloc_pd(q->cm_id->verbs);
	if (!dev->pd) {
		printf("%s: ibv_alloc_pd failed\n", __func__);
		return -EINVAL;
	}
	q->ctrl->dev = dev;

	return 0;
}

static int rdma_create_queue(struct queue *q)
{
	struct ibv_qp_init_attr qp_attr = {};
	int ret;

	if (!io_completion_channel) {
		io_completion_channel = ibv_create_comp_channel(q->cm_id->verbs);
		if (!io_completion_channel) {
			printf("%s: ibv_create_comp_channel failed\n", __func__);
			return -EINVAL;
		}
	}

	q->cq = ibv_create_cq(q->cm_id->verbs,
			CQ_CAPACITY,
			NULL,
			io_completion_channel,
			0);
	if (!q->cq) {
		printf("%s: ibv_create_cq failed\n", __func__);
		return -EINVAL;
	}

	ret = ibv_req_notify_cq(q->cq, 0);
	if (ret) {
		printf("%s: ibv_req_notify_cq failed\n", __func__);
		return ret;
	}

    qp_attr.send_cq = q->cq;
    qp_attr.recv_cq = q->cq;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_sge = MAX_SGE;
	qp_attr.cap.max_recv_sge = MAX_SGE;
    qp_attr.cap.max_send_wr = MAX_WR;
    qp_attr.cap.max_recv_wr = MAX_WR;

	ret = rdma_create_qp(q->cm_id, q->ctrl->dev->pd, &qp_attr);
	if (ret) {
		printf("%s: rdma_create_qp failed \n", __func__);
		return ret;
	}

	return ret;
}

static int rdma_create_mr(struct queue *q)
{
	if (!client_mr) {
		client_mr = ibv_reg_mr(
					q->ctrl->dev->pd,
					&client_memory,
					sizeof(client_memory),
					IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
		if (!client_mr) {
			printf("%s: ibv_reg_mr failed\n", __func__);
			return -ENOMEM;
		}
	}

	return 0;
}

static int on_addr_resolved(struct rdma_cm_id *id)
{
	struct queue *q = &cctrl->queues[queue_ctr++];
	int ret; 

	printf("%s\n", __func__);

	id->context = q;
	q->cm_id = id;

	ret = rdma_create_device(q);
	if (ret) {
		printf("%s: rdma_create_device failed\n", __func__);
		return ret;
	}

	ret = rdma_create_queue(q);
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
	int ret;

	printf("%s\n", __func__);

	ret = rdma_create_mr(q);
	if (ret) {
		printf("%s: rdma_create_mr failed\n", __func__);
		return ret;
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

	if (io_completion_channel) {
		ret = ibv_destroy_comp_channel(io_completion_channel);
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

static int alloc_control(void)
{
	int i;

	cctrl = (struct ctrl *)malloc(sizeof(struct ctrl));
	if (!cctrl) {
		printf("%s: malloc ctrl failed\n", __func__);
		return -EINVAL;
	}
	memset(cctrl, 0, sizeof(struct ctrl));

	cctrl->queues = (struct queue *)malloc(sizeof(struct queue) * NUM_CLIENT_QUEUES);
	if (!cctrl->queues) {
		printf("%s: malloc queues failed\n", __func__);
	}
	memset(cctrl->queues, 0, sizeof(struct queue) * NUM_CLIENT_QUEUES);

	for (i = 0; i < NUM_CLIENT_QUEUES; i++) {
		cctrl->queues[i].ctrl = cctrl;
	}

	return 0;
}

int start_rdma_client(struct sockaddr_in *s_addr)
{
	struct rdma_cm_event *event;
	int ret;

	if (!cctrl) {
		ret = alloc_control();
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

	ret = rdma_resolve_addr(cm_id, NULL, (struct sockaddr *) s_addr, 2000);
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

	for (i = 0; i < NUM_CLIENT_QUEUES; i++) {
		ret = on_disconnect(&cctrl->queues[i]);
		if (!ret) {
			printf("%s: on_disconnect failed \n", __func__);
			return ret;
		}
	}

	free(cctrl);
	return ret;
}
