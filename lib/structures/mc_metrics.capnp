@0xe73292a9e88b7cca;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("llvm_ml");

struct MCSample {
  failed @0 : Bool;
  cycles @1 : UInt64;
  instructions @2 : UInt64;
  microOps @3 : UInt64;
  cacheMisses @4 : UInt16;
  contextSwitches @5 : UInt16;
  numRepeat @6 : UInt16;
}

struct MCMetrics {
  measuredCycles @0 : UInt64;
  measuredMicroOps @1 : UInt64;
  numRepeat @2 : UInt16;

  source @3 : Text;

  noiseSamples @4 : List(MCSample);
  workloadSamples @5 : List(MCSample);
}
