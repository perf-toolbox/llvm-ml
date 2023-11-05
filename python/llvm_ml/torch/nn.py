import lightning.pytorch as pl
import torch.nn as nn
import torch_geometric.nn as gnn
from torch_geometric.utils import to_dense_batch, to_dense_adj, scatter
import torch.nn.functional as F
import torchmetrics
from torch.optim import Adam
from torch.nn import Module
from torch.optim import lr_scheduler
import torch


def to_dense_adj_batch(edge_index, batch, num_nodes=0):
    batch_size = int(batch.max()) + 1
    adj = torch.zeros((batch_size, num_nodes, num_nodes), dtype=torch.int64, device=edge_index.device)

    one = batch.new_ones(batch.size(0))
    num_nodes = scatter(one, batch, dim=0, dim_size=batch_size, reduce='sum')

    print(num_nodes)

    return adj


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
    def __init__(self, emb_size, out_size, num_heads=4, dropout=0.1):
        super().__init__()

        self.norm1 = gnn.LayerNorm(emb_size)
        self.attention = MCGraphAttention(emb_size, heads=8)
        self.norm2 = gnn.LayerNorm(emb_size)

        self.feed_forward = nn.Sequential(
            nn.Linear(emb_size, 4 * emb_size),
            nn.GELU(),
            nn.Linear(4 * emb_size, emb_size),
            nn.Dropout(dropout)
        )

    def forward(self, nodes, edge_index, mask):
        nodes = self.norm1(nodes)
        dense_context = self.attention(nodes, edge_index, mask)

        res = self.feed_forward(dense_context)

        return self.norm2(res)


class MCBERT(pl.LightningModule):
    def __init__(self, config):
        super().__init__()

        if 'hidden_size' not in config:
            config['hidden_size'] = 64
        if 'dropout' not in config:
            config['dropout'] = 0.1
        if 'learning_rate' not in config:
            config['learning_rate'] = 1e-5
        if 'num_heads' not in config:
            config['num_heads'] = 4
        if 'num_encoders' not in config:
            config['num_encoders'] = 8

        self.config = config

        num_opcodes = config['num_opcodes']
        emb_size = config['embedding_size']
        num_heads_encoder = config['num_heads_encoder']
        num_encoders = config['num_encoders']
        hidden_size = config['hidden_size']
        dropout = config['dropout']

        self.embedding = MCEmbedding(num_opcodes, emb_size)

        self.encoders = nn.ModuleList(
           [MCGraphEncoder(emb_size, hidden_size, num_heads_encoder, dropout) for _ in range(num_encoders)])

        self.token_prediction = nn.Linear(emb_size, num_opcodes)

        self.proj = nn.Sequential(
            nn.Linear(emb_size, 4 * emb_size),
            nn.Linear(4 * emb_size, emb_size)
        )

    def forward(self, nodes, edge_index, batch):
        embedded, pos_enc = self.embedding(nodes)

        encoded, mask = to_dense_batch(embedded, batch)

        # TODO this does not do what I expected
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

        for i in range(self.config['batch_size']):
            if mask_id[i] != 0:
                target_token[i, mask_id[i]] = original_token[i]

        loss = F.cross_entropy(masked_token.view(-1, self.config['num_opcodes']), target_token.view(-1).long())

        self.log(f"{log_prefix}_loss", loss, on_epoch=True, batch_size=self.config['batch_size'])

        return loss, bb, raw

    def training_step(self, batch, batch_idx):
        loss, _, _ = self._step(batch, 'train')
        return loss

    def validation_step(self, batch, batch_idx):
        loss, _, _ = self._step(batch, 'val')
        return loss

    def configure_optimizers(self):
        optimizer = Adam(self.parameters(), lr=self.config['learning_rate'], weight_decay=1e-3)
        scheduler = lr_scheduler.ReduceLROnPlateau(optimizer, patience=3, factor=0.1)
        return {
            'optimizer': optimizer,
            'lr_scheduler': {
                'scheduler': scheduler,
                'monitor': 'val_loss',
            }
        }

