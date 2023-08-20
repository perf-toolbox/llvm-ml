from llvm_ml.utils import load_dataset, floor_step

def load_pyg_dataset(dataset_path, step="0.05", use_binary_opcode=True, prefilter=True, banned_ids=[]):
    from torch_geometric.data import Data, Dataset
    import torch
    import numpy as np


    class BasicBlockDataset(Dataset):
        def __init__(self, dataset_path, use_binary_opcode=True, prefilter=True, banned_ids=[]):
            super().__init__(None, None, None)
            basic_blocks = load_dataset(dataset_path, True, False)

            self.data = []
            self.basic_blocks = []

            for bb in basic_blocks:
                if prefilter:
                    if bb.measured_cycles > 35 or bb.measured_cycles <= 0:
                        continue
                    if "rep" in bb.source:
                        continue
                    if "cpuid" in bb.source:
                        continue
                    if "prefetch" in bb.source:
                        continue

                if bb.id in banned_ids:
                    continue

                if use_binary_opcode:
                    nodes = np.zeros((len(bb.nodes), 32))
                else:
                    nodes = np.zeros(len(bb.nodes), dtype=np.int_)

                for idx, n in enumerate(bb.nodes):
                    if use_binary_opcode:
                        for j in range(32):
                            nodes[idx, j] = n.binary_opcode[j]
                    else:
                        nodes[idx] = n.opcode

                edges = np.zeros((len(bb.edges), 2), dtype=np.int_)

                for idx, e in enumerate(bb.edges):
                    edges[idx, 0] = e.from_node
                    edges[idx, 1] = e.to_node

                y = bb.measured_cycles

                if step is not None:
                    y = floor_step(y, step)

                self.data.append(Data(x=torch.from_numpy(nodes), edge_index=torch.from_numpy(np.transpose(edges)).contiguous(), y=torch.tensor(y)))
                self.basic_blocks.append({
                    'source': bb.source,
                    'id': bb.id,
                })


        def len(self):
            return len(self.basic_blocks)


        def get(self, index):
            x = self.data[index]
            y = self.basic_blocks[index]

            return x, y

    return BasicBlockDataset(dataset_path, use_binary_opcode, prefilter, banned_ids)
