from llvm_ml.torch.nn import MCBERT
import argparse
import os
from torch_geometric.loader import DataLoader
import torch
from lightning.pytorch.loggers import TensorBoardLogger
import lightning.pytorch as pl
from lightning.pytorch.callbacks import ModelSummary, LearningRateMonitor
from lightning.pytorch.callbacks.early_stopping import EarlyStopping
from llvm_ml.torch import BasicBlockDataset

import warnings
warnings.filterwarnings('ignore', category=UserWarning, message='TypedStorage is deprecated')

parser = argparse.ArgumentParser(
        prog='llvm-ml NN Trainer',
        description='Train neural networks for llvm-ml'
        )

parser.add_argument('network', choices=["mcbert"], required=True)
parser.add_argument('config', type=str)
parser.add_argument('output', required=True, type=str)
parser.add_argument('epochs', type=int, default=100)
parser.add_argument('device', choices=["gpu", "cpu"], default="cpu")
parser.add_argument('dataset', type=str, required=True)
parser.add_argument('early_stopping', action='store_true')

args = parser.parse_args()

config = {}

if os.path.exists(args.config):
    with open(args.config) as json_data:
        config = json.load(json_data)

model = None
dataset = None

if args.network == 'mcbert':
    banned_ids = []
    dataset = BasicBlockDataset(args.dataset, masked=True, banned_ids=banned_ids, prefilter=True)
    model = MCBERT(config)

num_training = int(0.7 * len(dataset))
num_val = len(dataset) - num_training

train_dataset, val_dataset = torch.utils.data.random_split(dataset, [num_training, num_val])
train_loader = DataLoader(train_dataset, batch_size=batch_size, shuffle=True, num_workers=6, drop_last=True)
val_loader = DataLoader(val_dataset, batch_size=batch_size, shuffle=False, num_workers=6, drop_last=True)

logger = TensorBoardLogger("runs", name="bert")
logger.log_graph(model)
callbacks = [
    ModelSummary(max_depth=-1),
    LearningRateMonitor(),
]

if args.early_stopping:
    callbacks.append(EarlyStopping(monitor="val_loss", mode="min"))

trainer = pl.Trainer(max_epochs=args.epochs,
                     logger=logger,
                     precision='16-mixed',
                     callbacks=callbacks,
                     )
trainer.fit(model, train_loader, val_loader)
torch.save({'state_dict': model.state_dict()}, args.output)

