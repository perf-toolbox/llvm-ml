import lib.structures.mc_dataset_pb2
from python.llvm_ml.data import BasicBlock


def load_data(path, show_progress=False, undirected=False, use_binary_opcode=True):
    dataset = lib.structures.mc_dataset_pb2.MCDataset()
    dataset.ParseFromString(open(path, 'rb').read())

    result = []

    if show_progress:
        from tqdm.auto import tqdm
        for d in tqdm(dataset.data):
            result.append(BasicBlock(d, undirected=undirected, use_binary_opcode=use_binary_opcode))
    else:
        for d in dataset.data:
            result.append(BasicBlock(d, undirected=undirected, use_binary_opcode=use_binary_opcode))

    return result
