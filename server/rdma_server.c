#include "rdma_server.h"

static unsigned int queue_ctr = 0;

struct ctrl *gctrl;
struct ibv_comp_channel *io_completion_channel;
struct rdma_event_channel *ec;
struct rdma_cm_id *listener;

char server_memory[PAGE_SIZE];
struct ibv_mr *server_mr;

static struct device *rdma_create_device(struct queue *q)
{
	struct device *dev = NULL;

	if (!q->ctrl->dev) {
		dev = (struct device *) malloc(sizeof(*dev));
		if (!dev) {
			printf("%s: malloc dev failed\n", __func__);
			return NULL;
		}

		dev->verbs = q->cm_id->verbs;
		dev->pd = ibv_alloc_pd(dev->verbs);
		if (!dev->pd) {
			printf("%s: ibv_allod_pd failed\n", __func__);
			return NULL;
		}

		q->ctrl->dev = dev;
	}

	return q->ctrl->dev;
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

	qp_attr.send_cq = q->cq;
	qp_attr.recv_cq = q->cq;
	qp_attr.qp_type = IBV_QPT_RC;
	qp_attr.cap.max_send_wr = 1024;
	qp_attr.cap.max_recv_wr = 1024;
	qp_attr.cap.max_send_sge = 1;
	qp_attr.cap.max_recv_sge = 1;

	ret = rdma_create_qp(q->cm_id, q->ctrl->dev->pd, &qp_attr);
	if (ret) {
		printf("%s: rdma_create_qp failed\n", __func__);
		return ret;
	}
	q->qp = q->cm_id->qp;

	return ret;
}

static int rdma_create_mr(struct queue *q)
{
	if (!server_mr) {
		server_mr = ibv_reg_mr(
					q->ctrl->dev->pd,
					&server_memory,
					sizeof(server_memory),
					IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
		if (!server_mr) {
			printf("%s: ibv_reg_mr failed\n", __func__);
			return -ENOMEM;
		}
	}

	return 0;
}

static int on_connect_request(struct rdma_cm_id *id, struct rdma_conn_param *param)
{
	struct rdma_conn_param cm_params = {};
	struct ibv_device_attr attrs = {};
	struct queue *q = &gctrl->queues[queue_ctr++];
	struct device *dev;
	int ret;

	printf("%s\n", __FUNCTION__);

	id->context = q;
	q->cm_id = id;

	dev = rdma_create_device(q);
	if (!dev) {
		printf("%s: rdma_create_device failed\n", __func__);
		return -EINVAL;
	}

	ret = rdma_create_queue(q);
	if (ret) {
		printf("%s: rdma_create_queue failed\n", __func__);
		return -EINVAL;
	}

	ret = ibv_query_device(dev->verbs, &attrs);
	if (ret) {
		printf("%s: ibv_query_device failed\n", __func__);
		return -EINVAL;
	}

	cm_params.initiator_depth = param->initiator_depth;
	cm_params.responder_resources = param->responder_resources;
	cm_params.rnr_retry_count = param->rnr_retry_count;
	cm_params.flow_control = param->flow_control;

	ret = rdma_accept(q->cm_id, &cm_params);
	if (ret) {
		printf("%s: rdma_accept failed\n", __func__);
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

	return ret;
}

static int on_disconnect(struct queue *q)
{
	printf("%s\n", __FUNCTION__);

	rdma_destroy_qp(q->cm_id);
	rdma_destroy_id(q->cm_id);
	rdma_destroy_event_channel(ec);
	ibv_dealloc_pd(gctrl->dev->pd);
	free(gctrl->dev);
	gctrl->dev = NULL;
	return 1;
}

int on_event(struct rdma_cm_event *event)
{
	struct queue *q = (struct queue *) event->id->context;
	printf("%s\n", __FUNCTION__);

	switch (event->event) {
		case RDMA_CM_EVENT_CONNECT_REQUEST:
			return on_connect_request(event->id, &event->param.conn);
		case RDMA_CM_EVENT_ESTABLISHED:
			return on_connection(q);
		case RDMA_CM_EVENT_DISCONNECTED:
			printf("recv disconnect\n");
			return on_disconnect(q);
		default:
			printf("unknown event: %s\n", rdma_event_str(event->event));
			return 1;
	}
}

static int alloc_control(void)
{
	gctrl = (struct ctrl *) malloc(sizeof(struct ctrl));
	if (!gctrl) {
		printf("%s: malloc gctrl failed\n", __func__);
		return -EINVAL;
	}
	memset(gctrl, 0, sizeof(struct ctrl));

	gctrl->queues = (struct queue *) malloc(sizeof(struct queue) * NUM_QUEUES);
	if (!gctrl->queues) {
		printf("%s: malloc queues failed\n", __func__);
		return -EINVAL;
	}
	memset(gctrl->queues, 0, sizeof(struct queue) * NUM_QUEUES);

	gctrl->queues[0].ctrl = gctrl;
	return 0;
}

int start_rdma_server(struct sockaddr_in s_addr)
{
	struct rdma_cm_event *event;
	int i, ret;

	ret = alloc_control();
	if (ret) {
		printf("%s: malloc queues failed\n", __func__);
		return -ret;
	}

	ec = rdma_create_event_channel();
	if (!ec) {
		printf("%s: rdma_create_event_channel failed\n", __func__);
		return -EINVAL;
	}

	ret = rdma_create_id(ec, &listener, NULL, RDMA_PS_TCP);
	if (ret) {
		printf("%s: rdma_create_id failed\n", __func__);
		return ret;
	}

	ret = rdma_bind_addr(listener, (struct sockaddr *) &s_addr);
	if (ret) {
		printf("%s: rdma_bind_addr failed\n", __func__);
		return ret;
	}

	ret = rdma_listen(listener, 1);
	if (ret) {
		printf("%s: rdma_listen failed\n", __func__);
		return ret;
	}

	while (!rdma_get_cm_event(ec, &event)) {
		struct rdma_cm_event event_copy;

		memcpy(&event_copy, event, sizeof(*event));
		rdma_ack_cm_event(event);

		if (on_event(&event_copy))
			break;
	}

	return 0;
}
