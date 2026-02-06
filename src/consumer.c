// SPDX-License-Identifier: BSD-3-Clause

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include "consumer.h"
#include "ring_buffer.h"
#include "packet.h"
#include "utils.h"

void *consumer_thread(so_consumer_ctx_t *ctx)
{
	char buffer[PKT_SZ];
	char out_buf[256];
	ssize_t ret;
	int len;
	bool done = false;
	unsigned long my_seq = 0;

	while (!done) {
		ret = ring_buffer_dequeue(ctx->producer_rb, buffer, PKT_SZ, &my_seq);
		if (ret < 0) {
			done = true;
		} else {
			struct so_packet_t *pkt = (struct so_packet_t *)buffer;

			int action = process_packet(pkt);
			unsigned long hash = packet_hash(pkt);
			unsigned long timestamp = pkt->hdr.timestamp;

			len = snprintf(out_buf, 256, "%s %016lx %lu\n", RES_TO_STR(action), hash, timestamp);

			pthread_mutex_lock(&ctx->log_mutex);
			while (my_seq != ctx->next_seq_to_write)
				pthread_cond_wait(&ctx->log_cond, &ctx->log_mutex);

			ret = write(ctx->fd, out_buf, len);
			DIE(ret < 0, "Error write");

			ctx->next_seq_to_write++;
			pthread_cond_broadcast(&ctx->log_cond);
			pthread_mutex_unlock(&ctx->log_mutex);
		}
	}

	pthread_exit(NULL);
	(void) len;
}

int create_consumers(pthread_t *tids,
					 int num_consumers,
					 struct so_ring_buffer_t *rb,
					 const char *out_filename)
{
	so_consumer_ctx_t *ctx = calloc(1, sizeof(so_consumer_ctx_t));

	DIE(ctx == NULL, "ctx calloc failed");

	ctx->producer_rb = rb;
	ctx->out_filename = out_filename;
	ctx->fd = open(out_filename, O_WRONLY | O_CREAT |  O_TRUNC, 0644);
	DIE(ctx->fd < 0, "Error out_file");

	pthread_mutex_init(&ctx->log_mutex, NULL);
	pthread_cond_init(&ctx->log_cond, NULL);

	ctx->next_seq_to_write = 0;

	for (int i = 0; i < num_consumers; i++) {
		int rc = pthread_create(&tids[i], NULL, (void *)consumer_thread, ctx);

		DIE(rc != 0, "Error pthread_create");
	}
	return num_consumers;
}
