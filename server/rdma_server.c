#include "rdma_server.h"

struct ctrl *server_session;
struct ibv_comp_channel *cc;
struct rdma_event_channel *ec;
struct rdma_cm_id *listener;
int queue_ctr = 0;

char server_memory[PAGE_SIZE];
struct ibv_mr *server_mr;

static int on_connect_request(struct rdma_cm_id *id, struct rdma_conn_param *param)
{
	struct rdma_conn_param cm_params = {};
	struct ibv_device_attr attrs = {};
	struct queue *q = &server_session->queues[queue_ctr++];
	struct device *dev;
	int ret;

	printf("%s\n", __func__);

	id->context = q;
	q->cm_id = id;

	if (!q->ctrl->dev) {
		ret = rdma_create_device(q);
		if (ret) {
			printf("%s: rdma_create_device failed\n", __func__);
			return -EINVAL;
		}
	}

	ret = rdma_create_queue(q, cc);
	if (ret) {
		printf("%s: rdma_create_queue failed\n", __func__);
		return -EINVAL;
	}

	ret = ibv_query_device(q->ctrl->dev->verbs, &attrs);
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
	struct mr_attr mr;
	int ret;

	printf("%s\n", __func__);

	ret = rdma_create_mr(server_session->dev->pd);
	if (ret) {
		printf("%s: rdma_create_mr failed\n", __func__);
		return ret;
	}

	mr.addr = (uint64_t) server_mr->addr;
	mr.length = sizeof(struct mr_attr);
	mr.stag.lkey = server_mr->lkey;
	memcpy(server_memory, &mr, sizeof(struct mr_attr));

	ret = rdma_send_wr(q, IBV_WR_SEND, &mr, NULL);
	if (ret) {
		printf("%s: rdma_send_wr failed\n", __func__);
		return ret;
	}

	ret = rdma_poll_cq(q->cq, 1);
	if (ret) {
		printf("%s: process_cq failed\n", __func__);
		return ret;
	}

	return ret;
}

static int on_disconnect(struct queue *q)
{
	printf("%s\n", __func__);

	rdma_destroy_qp(q->cm_id);
	rdma_destroy_id(q->cm_id);
	rdma_destroy_event_channel(ec);
	ibv_dealloc_pd(server_session->dev->pd);
	free(server_session->dev);
	server_session->dev = NULL;
	return 1;
}

static int on_event(struct rdma_cm_event *event)
{
	struct queue *q = (struct queue *) event->id->context;
	printf("%s\n", __func__);

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

int start_rdma_server(struct sockaddr_in s_addr)
{
	struct rdma_cm_event *event;
	int i, ret;

	ret = rdma_alloc_session(&server_session);
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
