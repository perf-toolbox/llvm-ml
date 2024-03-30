import torch
from torch_geometric.transforms import BaseTransform

class ConnectPriorNodes(BaseTransform):
    def __init__(self, skip_first=True, skip_last=False):
        self.skip_first = skip_first
        self.skip_last = skip_last
    
    def __call__(self, data):
        start = 2 if self.skip_first else 1
        end = len(data.x) - 1 if self.skip_last else len(data.x)

        for i in range(start, end):
            new_nodes = [[i, j] for j in range(start, i - 1)]
            data.edge_index = torch.cat([data.edge_index, new_nodes], dim=1)

        return data
