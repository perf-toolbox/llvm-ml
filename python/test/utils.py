from llvm_ml.utils import floor_step

def test_floor_step():
    assert (floor_step(0.0, "0.05") - 0.0) < 0.001
    assert (floor_step(1.04, "0.05") - 1.0) < 0.001
    assert (floor_step(1.05, "0.05") - 1.05) < 0.001
    assert (floor_step(1.23, "0.05") - 1.20) < 0.001
    assert (floor_step(1.35, "0.05") - 1.35) < 0.001