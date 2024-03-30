from _llvm_ml_impl import load_pytorch_dataset
from llvm_ml.utils import floor_step
from torch_geometric.data import Data, Dataset
import torch
import random

class BasicBlockDataset(Dataset):
    def __init__(self, dataset_path, opcodes, step="0.05", min_nodes=0, prefilter=True, banned_ids=[]):
        super().__init__(None, None, None)

        self.num_opcodes = len(opcodes) + 4
        self.mask_opcode = len(opcodes)
        # FIXME: figure out how to add a pad opcode
        self.pad_opcode = len(opcodes) + 1
        self.start_opcode = len(opcodes) + 2

        self.opcodes = opcodes

        self.data = []
        self.basic_blocks = []

        basic_blocks = load_pytorch_dataset(dataset_path, self.start_opcode, self.pad_opcode)

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

            if len(bb.nodes) < min_nodes:
                continue

            y = bb.measured_cycles

            if step is not None:
                y = floor_step(y, step)

            self.data.append(Data(x=bb.nodes, edge_index=torch.transpose(bb.edges, 0, 1).contiguous(), edge_attr=bb.features, y=torch.tensor(y)))
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

        return x, y

    def get_mask_opcode(self):
        return self.mask_opcode

    def get_pad_opcode(self):
        return self.pad_opcode

    def to_string(self, opcode: int):
        if opcode == self.mask_opcode:
            return "<MASK>"
        elif opcode == self.pad_opcode:
            return "<PAD>"
        elif opcode == self.start_opcode:
            return "<START>"
        elif opcode >= self.num_opcodes:
            return "<UNK>"
        else:
            return self.opcodes[opcode]
