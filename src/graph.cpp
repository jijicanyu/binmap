//
//   Copyright 2014 QuarksLab
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//

#include "binmap/graph.hpp"
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/floyd_warshall_shortest.hpp>
#include <ciso646>

Graph::Graph() : distance_matrix_(0) {}

Graph::~Graph() {
  if (distance_matrix_) {
    delete[] distance_matrix_;
  }
}

bool Graph::has_node(boost::filesystem::path const &path) const {
  bool res;
#pragma omp critical(graph_)
  res = mapping_.find(path) != mapping_.end();
  return res;
}

Hash const &Graph::hash(graph_type::vertex_descriptor vd) const {
  return boost::get(boost::vertex_hash_t(), graph_, vd);
}

boost::filesystem::path const &
Graph::key(graph_type::vertex_descriptor vd) const {
  return boost::get(boost::vertex_name_t(), graph_, vd);
}

Hash const &Graph::hash(boost::filesystem::path const &key) const {
  if(mapping_.find(key) == mapping_.end())
    throw std::out_of_range(key.string());
  return boost::get(boost::vertex_hash_t(), graph_, mapping_.find(key)->second);
}

void Graph::successors(successors_type &succs,
                       boost::filesystem::path const &key) const {
  for (edge_iterator viter = edge_begin(key); viter != edge_end(key); ++viter)
    succs.insert(this->key(boost::target(*viter, graph_)));
}
void Graph::predecessors(successors_type &preds,
                         boost::filesystem::path const &key) const {
  graph_type::in_edge_iterator in_begin, in_end;
  for (boost::tie(in_begin, in_end) =
           boost::in_edges(mapping_.find(key)->second, graph_);
       in_begin != in_end; ++in_begin)
    preds.insert(this->key(boost::source(*in_begin, graph_)));
}

Graph::const_hashes_t Graph::hashes() const {
  return boost::get(boost::vertex_hash_t(), graph_);
}

Graph::vertex_iterator Graph::begin() const {
  return boost::vertices(graph_).first;
}

Graph::vertex_iterator Graph::end() const {
  return boost::vertices(graph_).second;
}

Graph::graph_type const &Graph::graph() const { return graph_; }

Graph::edge_iterator
Graph::edge_begin(boost::filesystem::path const &input_file) const {
  assert(has_node(input_file));
  return boost::out_edges(mapping_.find(input_file)->second, graph_).first;
}

Graph::edge_iterator
Graph::edge_end(boost::filesystem::path const &input_file) const {
  assert(has_node(input_file));
  return boost::out_edges(mapping_.find(input_file)->second, graph_).second;
}

Graph::edge_iterator
Graph::edge_begin(graph_type::vertex_descriptor input) const {
  return boost::out_edges(input, graph_).first;
}

Graph::edge_iterator
Graph::edge_end(graph_type::vertex_descriptor input) const {
  return boost::out_edges(input, graph_).second;
}

void Graph::add_edge(boost::filesystem::path const &from,
                     boost::filesystem::path const &to) {
  assert(has_node(from));
  assert(has_node(to));
#pragma omp critical(graph_)
  boost::add_edge(mapping_[from], mapping_[to], graph_);
}

void Graph::dot(boost::filesystem::path const &path) const {
  std::ofstream dotfile(path.string().c_str());
#pragma omp critical(graph_)
  boost::write_graphviz(dotfile, graph_, boost::make_label_writer(boost::get(
                                             boost::vertex_name_t(), graph_)));
}

bool Graph::has_path(boost::filesystem::path const &from,
                     boost::filesystem::path const &to) const {
  assert(has_node(from));
  assert(has_node(to));
  compute_distance_matrix();
  graph_type::vertex_descriptor vfrom = mapping_.find(from)->second,
                                vto = mapping_.find(to)->second;
  return distance_matrix_[vfrom][vto] != std::numeric_limits<int>::max();
}

void Graph::compute_distance_matrix() const {
  if (!distance_matrix_) {
    boost::static_property_map<int> weight_map(1);
    distance_matrix_ = new std::vector<int>[boost::num_vertices(graph_)];
    std::fill(distance_matrix_, distance_matrix_ + boost::num_vertices(graph_),
              std::vector<int>(boost::num_vertices(graph_)));
    floyd_warshall_all_pairs_shortest_paths(graph_, distance_matrix_,
                                            boost::weight_map(weight_map));
  }
}

void Graph::add_node(boost::filesystem::path const &input_file,
                     Hash const &input_hash) {
  assert(not has_node(input_file));
#pragma omp critical(graph_)
  {
    graph_type::vertex_descriptor v = boost::add_vertex(graph_);
    mapping_[input_file] = v;

    boost::put(boost::vertex_name_t(), graph_, v, input_file);
    boost::put(boost::vertex_hash_t(), graph_, v, input_hash);
  }
}

size_t Graph::size() const { return boost::num_vertices(graph_); }
