// SPDX-License-Identifier: BSD-3-Clause

#include "ring_buffer.h"

int ring_buffer_init(so_ring_buffer_t *ring, size_t cap)
{
	ring->data = malloc(cap);

	ring->read_pos = 0;
	ring->write_pos = 0;

	ring->len = 0;
	ring->cap = cap;
	ring->stopped = false;
	ring->next_seq = 0;

	pthread_mutex_init(&ring->mutex, NULL);
	pthread_cond_init(&ring->not_empty, NULL);
	pthread_cond_init(&ring->not_full, NULL);

	return 0;
}

ssize_t ring_buffer_enqueue(so_ring_buffer_t *ring, void *data, size_t size)
{
	// Lock writing on the buffer
	pthread_mutex_lock(&ring->mutex);

	// Wait until is space in buffer
	while (ring->len == ring->cap && !ring->stopped)
		pthread_cond_wait(&ring->not_full, &ring->mutex);

	if (ring->len == ring->cap && ring->stopped) {
		pthread_mutex_unlock(&ring->mutex);
		return -1;
	}

	// Copy data in ring buffer
	memcpy(&(ring->data)[ring->write_pos], data, size);
	ring->len += size;
	ring->write_pos = (ring->write_pos + size) % ring->cap;

	pthread_cond_signal(&ring->not_empty);
	pthread_mutex_unlock(&ring->mutex);

	return size;
}

ssize_t ring_buffer_dequeue(so_ring_buffer_t *ring, void *data, size_t size, unsigned long *seq)
{
	// Lock writing on the buffer
	pthread_mutex_lock(&ring->mutex);

	// Wait until is something in buffer
	while (ring->len == 0 && !ring->stopped)
		pthread_cond_wait(&ring->not_empty, &ring->mutex);

	if (ring->len == 0 && ring->stopped) {
		pthread_mutex_unlock(&ring->mutex);
		return -1;
	}

	// Copy data in ring buffer
	memcpy(data, &(ring->data)[ring->read_pos], size);
	ring->len -= size;
	ring->read_pos = (ring->read_pos + size) % ring->cap;

	*seq = ring->next_seq++;

	pthread_cond_signal(&ring->not_full);
	pthread_mutex_unlock(&ring->mutex);

	return size;
}

void ring_buffer_destroy(so_ring_buffer_t *ring)
{
	free(ring->data);

	pthread_mutex_destroy(&ring->mutex);
	pthread_cond_destroy(&ring->not_empty);
	pthread_cond_destroy(&ring->not_full);
}

void ring_buffer_stop(so_ring_buffer_t *ring)
{
	pthread_mutex_lock(&ring->mutex);

	ring->stopped = 1;
	pthread_cond_broadcast(&ring->not_empty);
	pthread_mutex_unlock(&ring->mutex);
}
