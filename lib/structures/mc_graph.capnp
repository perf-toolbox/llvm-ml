@0x853174d64d2223d2;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("llvm_ml");

struct MCNode {
  nodeId @0 : UInt16;
  opcode @1 : UInt32;
  isLoad @2 : Bool;
  isStore @3 : Bool;
  isBarrier @4 : Bool;
  isAtomic @5 : Bool;
  isVector @6 : Bool;
  isCompute @7 : Bool;
  isFloat @8 : Bool;
  isVirtualRoot @9 : Bool;
}

struct MCEdge {
  from @0 : UInt16;
  to @1 : UInt16;
  isDataDependency @2 : Bool;
}

struct MCGraph {
  maxOpcode @0 : UInt32;
  hasVirtualRoot @1 : Bool;
  source @2 : Text;

  nodes @3 : List(MCNode);
  edges @4 : List(MCEdge);
}
