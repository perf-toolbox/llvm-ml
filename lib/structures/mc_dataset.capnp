@0xe010047710ad67ef;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("llvm_ml");

using Metrics = import "mc_metrics.capnp";
using Graph = import "mc_graph.capnp";

struct MCDataPiece {
  metrics @0 : Metrics.MCMetrics;
  graph @1 : Graph.MCGraph;
  id @2 : Text;
  cov @3 : Float32;
}

struct MCDataset {
  data @0 : List(MCDataPiece);
}
