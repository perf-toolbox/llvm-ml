from _llvm_ml_impl import load_pytorch_dataset
from llvm_ml.utils import floor_step
from torch_geometric.data import Data, Dataset
import torch
import random

class BasicBlockDataset(Dataset):
    def __init__(self, dataset_path, step="0.05", prefilter=True, masked=False, banned_ids=[]):
        super().__init__(None, None, None)
        basic_blocks = load_pytorch_dataset(dataset_path, True)

        self.num_opcodes = 21000 + 2
        self.mask_opcode = 21001

        self.masked = masked

        self.data = []
        self.basic_blocks = []
        self.original_opcodes = []
        self.masked_ids = []

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

            y = bb.measured_cycles

            if masked:
                if len(bb.nodes) > 4:
                    masked_id = random.randint(2, len(bb.nodes) - 2)
                    self.original_opcodes.append(bb.nodes[masked_id].clone().detach())
                    bb.nodes[masked_id] = self.mask_opcode
                    self.masked_ids.append(torch.tensor(masked_id))
                else:
                    self.original_opcodes.append(torch.tensor(0, dtype=int))
                    self.masked_ids.append(torch.tensor(0, dtype=int))

            if step is not None:
                y = floor_step(y, step)

            self.data.append(Data(x=bb.nodes, edge_index=torch.transpose(bb.edges, 0, 1).contiguous(), y=torch.tensor(y)))
            self.basic_blocks.append({
                'source': bb.source,
                'id': bb.id,
            })


    def len(self):
        return len(self.basic_blocks)
    

    def get_num_opcodes(self):
        return self.num_opcodes


    def get(self, index):
        x = self.data[index]
        y = self.basic_blocks[index]

        mask_id = torch.tensor(0)
        original = torch.tensor(0)

        if self.masked:
            mask_id = self.masked_ids[index]
            original = self.original_opcodes[index]

        return x, y, mask_id, original
