from llvm_ml.torch import ConnectPriorNodes
from torch_geometric.data import Data
import torch

def test_connect_prior_nodes():
    x = torch.randn(10, 5)
    edge_index = torch.tensor([[0, 1, 2, 3, 4], [1, 2, 3, 4, 5]])
    edge_attr = torch.randn(edge_index.shape[1], 5)
    data = Data(x=x, edge_index=edge_index, edge_attr=edge_attr)

    data = ConnectPriorNodes()(data)

    assert data.x.shape == (12, 5)
    assert data.edge_index.shape == (2, 8)
    assert data.edge_attr.shape == (8, 5)

    # Check that the edges are connected to the correct prior nodes
    assert data.edge_index[0, 0] == 0
    assert data.edge_index[1, 0] == 1
    assert data.edge_index[0, 1] == 1
    assert data.edge_index[1, 1] == 2

    # Check that the edge attributes are correctly updated
    assert torch.allclose(data.edge_attr[0], torch.cat([x[0], x[1]]))
