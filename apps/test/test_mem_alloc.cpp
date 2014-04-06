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

#include <signal.h>
#include <google/profiler.h>

#include <atomic>
#include <vector>

#include "thread.h"
#include "io_interface.h"
#include "container.h"
#include "concurrency.h"

#include "vertex_index.h"
#include "graph_engine.h"
#include "graph_config.h"

class test_vertex: public compute_directed_vertex
{
	vertex_id_t *adj_list;
public:
	test_vertex() {
		adj_list = NULL;
	}

	test_vertex(vertex_id_t id, const vertex_index *index): compute_directed_vertex(
			id, index) {
		adj_list = NULL;
	}

	void run(graph_engine &graph) {
		vertex_id_t id = get_id();
		request_vertices(&id, 1);
	}

	void run(graph_engine &graph, const page_vertex &vertex);

	virtual void run_on_message(graph_engine &, const vertex_message &msg) {
	}

	friend class part_test_vertex;
};

void test_vertex::run(graph_engine &graph, const page_vertex &vertex)
{
	adj_list = new vertex_id_t[vertex.get_num_edges(BOTH_EDGES)];
	page_byte_array::const_iterator<vertex_id_t> end_it
		= vertex.get_neigh_end(BOTH_EDGES);
	int num_dests = 0;
	for (page_byte_array::const_iterator<vertex_id_t> it
			= vertex.get_neigh_begin(BOTH_EDGES); it != end_it; ++it) {
		vertex_id_t id = *it;
		adj_list[num_dests++] = id;
	}
	delete [] adj_list;
	adj_list = NULL;
}

void int_handler(int sig_num)
{
	if (!graph_conf.get_prof_file().empty())
		ProfilerStop();
	exit(0);
}

void print_usage()
{
	fprintf(stderr,
			"test [options] conf_file graph_file index_file\n");
	fprintf(stderr, "-c confs: add more configurations to the system\n");
	graph_conf.print_help();
	params.print_help();
}

int main(int argc, char *argv[])
{
	int opt;
	std::string confs;
	int num_opts = 0;
	while ((opt = getopt(argc, argv, "c:")) != -1) {
		num_opts++;
		switch (opt) {
			case 'c':
				confs = optarg;
				num_opts++;
				break;
			default:
				print_usage();
		}
	}
	argv += 1 + num_opts;
	argc -= 1 + num_opts;

	if (argc < 3) {
		print_usage();
		exit(-1);
	}

	std::string conf_file = argv[0];
	std::string graph_file = argv[1];
	std::string index_file = argv[2];

	config_map configs(conf_file);
	configs.add_options(confs);
	graph_conf.init(configs);
	graph_conf.print();
	printf("The size of vertex state: %ld\n", sizeof(test_vertex));

	signal(SIGINT, int_handler);
	init_io_system(configs);

	graph_index *index = NUMA_graph_index<test_vertex>::create(
			index_file, graph_conf.get_num_threads(), params.get_num_nodes());
	graph_engine *graph = graph_engine::create(graph_conf.get_num_threads(),
			params.get_num_nodes(), graph_file, index);
	printf("test starts\n");
	printf("prof_file: %s\n", graph_conf.get_prof_file().c_str());
	if (!graph_conf.get_prof_file().empty())
		ProfilerStart(graph_conf.get_prof_file().c_str());

	struct timeval start, end;
	gettimeofday(&start, NULL);
	graph->start_all();
	graph->wait4complete();
	gettimeofday(&end, NULL);

	if (!graph_conf.get_prof_file().empty())
		ProfilerStop();
	if (graph_conf.get_print_io_stat())
		print_io_thread_stat();
	graph_engine::destroy(graph);
	destroy_io_system();
	printf("It takes %f seconds\n", time_diff(start, end));
	while (true) {
	}
}
