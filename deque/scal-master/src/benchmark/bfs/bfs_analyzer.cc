// Copyright (c) 2012-2013, the Scal Project Authors.  All rights reserved.
// Please see the AUTHORS file for details.  Use of this source code is governed
// by a BSD license that can be found in the LICENSE file.

#define __STDC_FORMAT_MACROS

#include <gflags/gflags.h>
#include <stdint.h>

#include "benchmark/bfs/graph.h"
#include "datastructures/single_list.h"
#include "util/malloc.h"
#include "util/random.h"
#include "util/threadlocals.h"
#include "util/time.h"

DEFINE_string(prealloc_size, "1g", "tread local space that is initialized");
DEFINE_int64(root, -1, "root for BFS; -1: pseudorandom (time-based seed)");

namespace {

const size_t kMaxDebugLevels = 256;

}  // namespace

int main(int argc, char **argv) {
  std::string usage("bfs-analyzer [options] graph_file");
  google::SetUsageMessage(usage);
  uint32_t cmd_index = google::ParseCommandLineFlags(&
      argc, const_cast<char***>(&argv), true);
  if (cmd_index >= static_cast<uint32_t>(argc)) {
    google::ShowUsageWithFlags(google::GetArgv0());
    exit(EXIT_FAILURE);
  }
  const char *graph_file = argv[cmd_index];

  uint64_t tlsize = scal::human_size_to_pages(FLAGS_prealloc_size.c_str(),
                                              FLAGS_prealloc_size.size());
  scal::tlalloc_init(tlsize, true /* touch pages */);
  scal::ThreadContext::prepare(1);
  scal::ThreadContext::assign_context();

  SingleList<uint64_t> *q = new SingleList<uint64_t>();
  Graph *g = Graph::from_graph_file(graph_file);

  uint64_t debug_levels = 0;
  uint64_t debug_level_cnt[kMaxDebugLevels];
  memset(debug_level_cnt, 0, sizeof(*debug_level_cnt) * kMaxDebugLevels);
 
  uint64_t root_index;
  if (FLAGS_root == -1) {
    root_index = scal::rand_range(0, g->size());
  } else {
    root_index = static_cast<uint64_t>(FLAGS_root);
  }

  Vertex &cur_vertex = g->get(root_index);
  cur_vertex.distance = 0;
  q->enqueue(root_index);
  debug_level_cnt[debug_levels]++;
  uint64_t vertex_index;
  while (q->dequeue(&vertex_index)) {
    cur_vertex = g->get(vertex_index);  
    if (debug_levels == cur_vertex.distance) {
      debug_levels++;
    }
    for (uint64_t i = 0; i < cur_vertex.neighbors.size(); i++) {
      Vertex& neighbor = g->get(cur_vertex.neighbors[i]);
      if (neighbor.distance == Vertex::no_distance) {
        neighbor.distance = cur_vertex.distance + 1;
        q->enqueue(cur_vertex.neighbors[i]);
        debug_level_cnt[debug_levels]++;
      }
    }
  }
  printf("Vertices: %" PRIu64 "\n", g->size());
  printf("Levels: %" PRIu64 "\n", debug_levels);
  printf("Root index: %" PRIu64 "\n", root_index);
  printf("LEVEL   VERTICES\n");
  for (uint64_t i = 0; i < debug_levels; i++) {
    printf("%5" PRIu64 " %10" PRIu64 "\n",
        i, debug_level_cnt[i]);
  }

  return EXIT_SUCCESS;
}
