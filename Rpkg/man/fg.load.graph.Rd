% Generated by roxygen2 (4.1.1): do not edit by hand
% Please edit documentation in R/flashgraph.R
\name{fg.load.graph}
\alias{fg.get.graph}
\alias{fg.load.graph}
\alias{fg.load.igraph}
\title{Load a graph to FlashR.}
\usage{
fg.load.graph(graph, index.file = NULL, graph.name = graph,
  directed = TRUE, nthreads = 1)

fg.load.igraph(graph, graph.name = paste("igraph-v", vcount(graph), "-e",
  ecount(graph), sep = ""), nthreads = 1)

fg.get.graph(graph.name)
}
\arguments{
\item{graph}{The input graph file or the input iGraph object.}

\item{index.file}{The input index file for the graph. A user only needs
to provide an index file if the input graph uses
the FlashGraph format.}

\item{graph.name}{The graph name a user provides when a graph is
loaded to FlashR.}

\item{directed}{Indicate whether the input graph is directed. This is
only used if the input graph use the edge list format.}

\item{nthreads}{The number of threads used to construct a graph to
the FlashGraph format.}
}
\value{
a FlashGraph object.
}
\description{
Load a graph to FlashR from difference sources.
}
\details{
`fg.load.graph' loads a graph from the following sources: an edge list
file in the text format and an adjacency list file in the FlashGraph format.

`fg.load.igraph' loads a graph from an iGraph object.

`fg.get.graph' gets a FlashGraph object that references a graph
that has been loaded to FlashR.

Once a graph is loaded to FlashR, FlashR will maintain it.
A user can use fg.list.graphs() to list all graphs that have been loaded
FlashR and use fg.get.graph() to get a reference to a graph. A user
should provide a name for the graph so later on he or she will be able
to identify the graph more easily. By default, the graph name is
the input graph file name.

A graph in the FlashGraph format is stored in two files: a FlashGraph
adjacency list file and a FlashGraph index file. When a user provides
an index file, the input graph file is considered as an adjacency list
file, otherwise, an edge list file.

When loading a graph from an edge list file, FlashR will construct
it into the FlashGraph format. A user needs to indicate whether the edge
list represents a directed graph. A user can also use multiple threads
to accelerate constructing a graph.

When loading a graph from iGraph, FlashR
will construct it into the FlashGraph format. A user can use multiple
threads to accelerate graph construction.
}
\examples{
fg <- fg.load.graph("edge_list.txt")
fg <- fg.load.graph("graph.adj", "graph.index")
ig <- read.graph("edge_list.txt")
fg <- fg.load.igraph(ig)
}
\author{
Da Zheng <dzheng5@jhu.edu>
}

