import lightning.pytorch as pl
import torch.nn as nn
import torch_geometric.nn as gnn
from torch_geometric.utils import to_dense_batch, to_dense_adj, scatter
import torch.nn.functional as F
import torchmetrics
from torch.optim import AdamW
from torch.nn import Module
from torch.optim import lr_scheduler
import torch
from transformers import get_linear_schedule_with_warmup


def to_dense_adj_batch(edge_index, batch, num_nodes=0):
    batch_size = int(batch.max()) + 1
    adj = torch.zeros((batch_size, num_nodes, num_nodes), dtype=torch.int64, device=edge_index.device)

    one = batch.new_ones(batch.size(0))
    num_nodes = scatter(one, batch, dim=0, dim_size=batch_size, reduce='sum')

    print(num_nodes)

    return adj


class MCNNConfig:
    def __init__(self,
                 num_opcodes,
                 batch_size=64,
                 embedding_size=128,
                 forward_expansion=4,
                 reg_forward_expansion=4,
                 num_heads_encoder=4,
                 num_heads_decoder=4,
                 num_encoders=4,
                 num_decoders=4,
                 dropout=0.1,
                 learning_rate=1e-3,
                 weight_decay=0.01,
                 warmup_steps=None,
                 total_steps=None,
                 class_weight=None,
                 ):
        self.num_opcodes = num_opcodes
        self.warmup_steps = warmup_steps
        self.total_steps = total_steps
        self.batch_size = batch_size
        self.embedding_size = embedding_size
        self.forward_expansion = forward_expansion
        self.reg_forward_expansion = reg_forward_expansion
        self.num_heads_encoder = num_heads_encoder
        self.num_encoders = num_encoders
        self.dropout = dropout
        self.learning_rate = learning_rate
        self.weight_decay = weight_decay
        self.class_weight = class_weight


class MCEmbedding(Module):
    def __init__(self, num_opcodes, emb_size):
        super().__init__()

        self.embedding = nn.Embedding(num_opcodes, emb_size)
        self.pos_encoding = gnn.PositionalEncoding(emb_size)
        self.norm = gnn.LayerNorm(emb_size)

    def forward(self, input_tensor):
        pos_tensor = self.pos_encoding(input_tensor)

        output = self.embedding(input_tensor) + pos_tensor

        return self.norm(output), pos_tensor


class MCGraphAttention(nn.Module):
    def __init__(self, in_size, heads=4):
        super().__init__()

        self.num_heads = heads

        self.key = nn.Linear(in_size, in_size, bias=False)
        self.query = nn.Linear(in_size, in_size, bias=False)
        self.value = nn.Linear(in_size, in_size, bias=False)

        self.proj = nn.Linear(in_size, in_size, bias=False)

    def forward(self, nodes, edge_index, mask=None):
        B, T, C = nodes.shape

        mask = mask.view(B, T, 1)
        mask = torch.broadcast_to(mask, (B, T, C))

        nodes = nodes.masked_fill(mask == False, float(0))

        k = self.key(nodes).view(B, T, self.num_heads, C // self.num_heads).transpose(1, 2)
        q = self.query(nodes).view(B, T, self.num_heads, C // self.num_heads).transpose(1, 2)

        weight = torch.matmul(q, k.transpose(-2, -1)) * self.num_heads ** -0.5

        scale = 3 * edge_index + torch.ones(edge_index.shape, device=edge_index.device)

        scale = torch.cat([torch.cat([scale[x] for _ in range(self.num_heads)]) for x in range(scale.shape[0])]).view(B, self.num_heads, T, T)

        weight = weight * scale

        weight = F.softmax(weight, dim=-1)

        v = self.value(nodes).view(B, T, self.num_heads, C // self.num_heads).transpose(1, 2)

        res = torch.matmul(weight, v)

        res = res.transpose(1, 2).contiguous().view(B, T, C)

        return self.proj(res)


class MCGraphEncoder(Module):
    def __init__(self, emb_size, fwd_exp, num_heads=4, dropout=0.1):
        super().__init__()

        self.norm1 = gnn.LayerNorm(emb_size)
        self.attention = MCGraphAttention(emb_size, heads=8)
        self.norm2 = gnn.LayerNorm(emb_size)

        self.feed_forward = nn.Sequential(
            nn.Linear(emb_size, fwd_exp * emb_size),
            nn.GELU(),
            nn.Linear(fwd_exp * emb_size, emb_size),
            nn.Dropout(dropout)
        )

    def forward(self, nodes, edge_index, mask):
        dense_context = self.attention(nodes, edge_index, mask)
        dense_context = self.norm1(nodes + dense_context)

        res = self.feed_forward(dense_context)

        return self.norm2(res + dense_context)


class MCBERT(pl.LightningModule):
    def __init__(self, config: MCNNConfig):
        super().__init__()

        self.config = config

        self.embedding = MCEmbedding(self.config.num_opcodes, self.config.embedding_size)

        self.encoders = nn.ModuleList(
           [MCGraphEncoder(self.config.embedding_size, self.config.forward_expansion, self.config.num_heads_encoder, self.config.dropout) for _ in range(self.config.num_encoders)])

        self.token_prediction = nn.Linear(self.config.embedding_size, self.config.num_opcodes)

    def forward(self, nodes, edge_index, batch):
        embedded, pos_enc = self.embedding(nodes)

        encoded, mask = to_dense_batch(embedded, batch)

        # FIXME this is supposed to generate a dense adjacency matrix for my inputs, not sure if it does
        dense_edges = to_dense_adj(edge_index, batch)
        dense_edges = dense_edges.view(encoded.shape[0], encoded.shape[1], encoded.shape[1])

        for encoder in self.encoders:
            encoded = encoder(encoded, dense_edges, mask)

        token_predictions = self.token_prediction(encoded)

        return token_predictions

    def _step(self, batch, stage: str):

        bb, raw, mask_id, original_token = batch

        masked_token = self.forward(bb.x, bb.edge_index, bb.batch)

        dense_x, _ = to_dense_batch(bb.x, bb.batch)

        log_prefix = "train" if stage == 'train' else "val"
        target_token = dense_x.clone()

        for i in range(self.config.batch_size):
            if mask_id[i] != 0:
                target_token[i, mask_id[i]] = original_token[i]

        loss = F.cross_entropy(masked_token.view(-1, self.config.num_opcodes), target_token.view(-1).long(), weight=self.config.class_weight, ignore_index=0)

        self.log(f"{log_prefix}_loss", loss, on_epoch=True, batch_size=self.config.batch_size)

        return loss, bb, raw

    def training_step(self, batch, batch_idx):
        loss, _, _ = self._step(batch, 'train')
        return loss

    def validation_step(self, batch, batch_idx):
        loss, _, _ = self._step(batch, 'val')
        return loss

    def configure_optimizers(self):
        optimizer = AdamW(self.parameters(), lr=self.config.learning_rate, weight_decay=self.config.weight_decay)
        scheduler = get_linear_schedule_with_warmup(
            optimizer,
            num_warmup_steps=self.config.warmup_steps,
            num_training_steps=self.config.total_steps
        )

        return {
            'optimizer': optimizer,
            'lr_scheduler': {
                'scheduler': scheduler,
                'interval': 'step',
                'frequency': 1,
            }
        }

