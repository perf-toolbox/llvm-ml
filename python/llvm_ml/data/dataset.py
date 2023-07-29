from python.llvm_ml.utils import load_data

def load_pyg_dataset(dataset_path, use_binary_opcode=True, prefilter=True):
    from torch_geometric.data import Data, Dataset
    import torch


    class BasicBlockDataset(Dataset):
        def __init__(self, dataset_path, use_binary_opcode=True, prefilter=True):
            super().__init__(None, None, None)
            basic_blocks = load_data(dataset_path, undirected=False, use_binary_opcode=use_binary_opcode)

            self.data = []
            self.basic_blocks = []

            for bb in basic_blocks:
                if prefilter:
                    if bb.cycles > 500 or bb.cycles <= 0:
                        continue
                    if bb.context_switches != 0:
                        continue
                    if bb.cache_misses != 0:
                        continue

                self.data.append(Data(x=torch.from_numpy(bb.nodes), edge_index=torch.from_numpy(bb.edges).contiguous(), y=torch.tensor(bb.cycles)))
                self.basic_blocks.append({
                    'source': bb.source,
                })


        def len(self):
            return len(self.basic_blocks)


        def get(self, index):
            x = self.data[index]
            y = self.basic_blocks[index]

            return x, y

    return BasicBlockDataset(dataset_path, use_binary_opcode, prefilter)
