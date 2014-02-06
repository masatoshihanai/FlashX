#ifndef __GRAPH_ENGINE_H__
#define __GRAPH_ENGINE_H__

/**
 * Copyright 2013 Da Zheng
 *
 * This file is part of SA-GraphLib.
 *
 * SA-GraphLib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SA-GraphLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SA-GraphLib.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "thread.h"
#include "container.h"
#include "concurrency.h"
#include "slab_allocator.h"

#include "vertex.h"
#include "vertex_index.h"
#include "trace_logger.h"
#include "messaging.h"
#include "vertex_interpreter.h"
#include "partitioner.h"
#include "graph_index.h"

/**
 * The size of a message buffer used to pass vertex messages to other threads.
 */
const int GRAPH_MSG_BUF_SIZE = PAGE_SIZE * 4;

class graph_engine;

class compute_vertex: public in_mem_vertex_info
{
public:
	compute_vertex(vertex_id_t id, off_t off, int size): in_mem_vertex_info(
			id, off, size) {
	}

	virtual compute_allocator *create_part_compute_allocator(
			graph_engine *graph, thread *t) {
		return NULL;
	}

	virtual bool has_required_vertices() const {
		return false;
	}

	/**
	 * This translate the required vertex to an I/O request to the file.
	 */
	virtual request_range get_next_request(graph_engine *graph);

	virtual vertex_id_t get_next_required_vertex() {
		assert(0);
		return -1;
	}

	/**
	 * This is a pre-run before users get any information of adjacency list
	 * of vertices.
	 * If it returns true, the graph engine will fetch the adjacency list
	 * of itself. So by default, the graph engine will fetch its adjacency
	 * list.
	 */
	virtual bool run(graph_engine &graph) {
		return true;
	}

	/**
	 * Run user's code when the adjacency list of the vertex is read
	 * from disks.
	 * It returns true if the vertex has completed the iteration.
	 */
	virtual bool run(graph_engine &graph, const page_vertex *vertex) = 0;

	/**
	 * Run user's code when the adjacency lists of its neighbors are read
	 * from disks.
	 * It returns true if the vertex has completed the iteration.
	 */
	virtual bool run_on_neighbors(graph_engine &graph,
			const page_vertex *vertices[], int num) = 0;

	/**
	 * Run user's code when the vertex receives messages from other.
	 */
	virtual void run_on_messages(graph_engine &,
			const vertex_message *msgs[], int num) = 0;
};

class ts_vertex_request;

class ts_compute_vertex: public compute_vertex
{
	vertex_id_t get_next_required_vertex() {
		return -1;
	}
	virtual bool has_required_vertices() const {
		return has_required_ts_vertices();
	}
public:
	ts_compute_vertex(vertex_id_t id, off_t off, int size): compute_vertex(
			id, off, size) {
	}

	virtual compute_allocator *create_part_compute_allocator(
			graph_engine *graph, thread *t);

	virtual bool run_on_neighbors(graph_engine &graph,
			const page_vertex *vertices[], int num);

	virtual request_range get_next_request(graph_engine *graph);

	virtual void get_next_required_ts_vertex(ts_vertex_request &) = 0;
	virtual bool has_required_ts_vertices() const = 0;
	virtual bool run_on_neighbors(graph_engine &graph,
			const TS_page_vertex *vertices[], int num) = 0;
};

class vertex_scheduler
{
public:
	virtual void schedule(std::vector<vertex_id_t> &vertices) = 0;
};

class worker_thread;

class graph_engine
{
	graph_header header;
	graph_index *vertices;
	ext_mem_vertex_interpreter *interpreter;
	vertex_partitioner *partitioner;
	vertex_scheduler *scheduler;

	atomic_integer level;
	volatile bool is_complete;

	// These are used for switching queues.
	pthread_mutex_t lock;
	pthread_barrier_t barrier1;
	pthread_barrier_t barrier2;

	thread *first_thread;
	std::vector<worker_thread *> worker_threads;

	edge_type required_neighbor_type;

	trace_logger *logger;

	int file_id;

	void cleanup() {
		if (logger) {
			logger->close();
			logger = NULL;
		}
	}

	/**
	 * This method returns the message sender of the current thread that
	 * sends messages to the thread with the specified thread id.
	 */
	simple_msg_sender *get_msg_sender(int thread_id) const;
	multicast_msg_sender *get_multicast_sender(int thread_id) const;
	multicast_msg_sender *get_activate_sender(int thread_id) const;
protected:
	graph_engine(int num_threads, int num_nodes, const std::string &graph_file,
			graph_index *index);
public:
	static graph_engine *create(int num_threads, int num_nodes,
			const std::string &graph_file, graph_index *index) {
		return new graph_engine(num_threads, num_nodes, graph_file, index);
	}

	static void destroy(graph_engine *graph) {
		graph->cleanup();
		delete graph;
	}

	~graph_engine();

	compute_vertex &get_vertex(vertex_id_t id) {
		return vertices->get_vertex(id);
	}

	void start(vertex_id_t ids[], int num);
	void start_all();

	void set_required_neighbor_type(edge_type type) {
		required_neighbor_type = type;
	}

	edge_type get_required_neighbor_type() const {
		return required_neighbor_type;
	}

	/**
	 * The algorithm progresses to the next level.
	 * It returns true if no more work can progress.
	 */
	bool progress_next_level();

	/**
	 * Activate vertices that may be processed in the next level.
	 */
	void activate_vertices(vertex_id_t ids[], int num) {
		for (int i = 0; i < num; i++) {
			int part_id = partitioner->map(ids[i]);
			multicast_msg_sender *sender = get_activate_sender(part_id);
			bool ret = false;
			if (sender->has_msg()) {
				ret = sender->add_dest(ids[i]);
			}
			// If we can't add a destination vertex to the multicast msg,
			// or there isn't a msg in the sender.
			if (!ret) {
				vertex_message msg(sizeof(vertex_message), true);
				sender->init(msg);
				ret = sender->add_dest(ids[i]);
				assert(ret);
			}
		}
	}

	void activate_vertex(vertex_id_t vertex) {
		activate_vertices(&vertex, 1);
	}

	/**
	 * Get vertices to be processed in the current level.
	 */
	int get_curr_activated_vertices(vertex_id_t vertices[], int num);
	size_t get_num_curr_activated_vertices() const;

	vertex_id_t get_max_vertex_id() const {
		return vertices->get_max_vertex_id();
	}

	vertex_id_t get_min_vertex_id() const {
		return vertices->get_min_vertex_id();
	}

	void wait4complete();

	int get_num_threads() const {
		return worker_threads.size();
	}

	bool is_directed() const {
		return header.is_directed_graph();
	}

	trace_logger *get_logger() const {
		return logger;
	}

	/**
	 * Get the file id where the graph data is stored.
	 */
	int get_file_id() const {
		return file_id;
	}

	template<class T>
	void multicast_msg(vertex_id_t ids[], int num, const T &msg) {
		for (int i = 0; i < num; i++) {
			int part_id = partitioner->map(ids[i]);
			multicast_msg_sender *sender = get_multicast_sender(part_id);
			bool ret = false;
			if (sender->has_msg()) {
				ret = sender->add_dest(ids[i]);
			}
			// If we can't add a destination vertex to the multicast msg,
			// or there isn't a msg in the sender.
			if (!ret) {
				int retries = 0;
				do {
					retries++;
					sender->init(msg);
					ret = sender->add_dest(ids[i]);
				} while (!ret);
				// We shouldn't try it more than twice.
				assert(retries <= 2);
			}
		}
		// Now we have multicast the message, we need to notify all senders
		// of the end of multicast.
		for (int i = 0; i < this->get_num_threads(); i++) {
			multicast_msg_sender *sender = get_multicast_sender(i);
			// We only send the multicast on the sender that has received
			// the multicast message.
			if (sender->has_msg())
				sender->end_multicast();
		}
	}

	template<class T>
	void send_msg(vertex_id_t dest, T &msg) {
		vertex_id_t id = dest;
		simple_msg_sender *sender = get_msg_sender(partitioner->map(id));
		msg.set_dest(dest);
		sender->send_cached(msg);
	}

	ext_mem_vertex_interpreter *get_vertex_interpreter() const {
		return interpreter;
	}

	compute_allocator *create_part_compute_allocator(thread *t) {
		// We only need to get a vertex that exists.
		int min_id = vertices->get_min_vertex_id();
		return vertices->get_vertex(min_id).create_part_compute_allocator(
				this, t);
	}

	void destroy_part_compute_allocator(compute_allocator *alloc) {
		delete alloc;
	}

	const vertex_partitioner *get_partitioner() const {
		return partitioner;
	}

	worker_thread *get_thread(int idx) const {
		return worker_threads[idx];
	}

	const graph_header &get_graph_header() const {
		return header;
	}

	void set_vertex_scheduler(vertex_scheduler *scheduler);
};

/**
 * This class contains the request of a time-series vertex
 * from the user application.
 */
class ts_vertex_request
{
	vertex_id_t id;
	timestamp_pair range;
	edge_type type;
	bool require_all;
	graph_engine *graph;
public:
	ts_vertex_request(graph_engine *graph) {
		id = 0;
		range = timestamp_pair(INT_MAX, INT_MIN);
		type = edge_type::BOTH_EDGES;
		require_all = false;
		this->graph = graph;
	}

	void set_require_all(bool require_all) {
		this->require_all = require_all;
	}

	void set_vertex(vertex_id_t id);

	void add_timestamp(int timestamp) {
		if (!require_all) {
			if (range.second < timestamp)
				range.second = timestamp + 1;
			if (range.first > timestamp)
				range.first = timestamp;
		}
	}

	void set_edge_type(edge_type type) {
		this->type = type;
	}

	void clear() {
		id = 0;
		range = timestamp_pair(INT_MAX, INT_MIN);
		type = edge_type::BOTH_EDGES;
		require_all = false;
	}

	vertex_id_t get_id() const {
		return id;
	}

	const timestamp_pair &get_range() const {
		return range;
	}

	edge_type get_edge_type() const {
		return type;
	}

	bool is_require_all() const {
		return require_all;
	}
};

#endif
