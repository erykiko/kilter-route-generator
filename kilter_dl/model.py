from __future__ import annotations

import torch
from torch import nn


class KilterRouteGenerator(nn.Module):
    """Conditional decoder: (layout, grade) -> route token sequence."""

    def __init__(
        self,
        vocab_size: int,
        layout_vocab_size: int,
        grade_vocab_size: int,
        embed_dim: int = 128,
        hidden_dim: int = 256,
        num_layers: int = 2,
        dropout: float = 0.2,
        condition_every_step: bool = False,
    ) -> None:
        super().__init__()
        self.condition_every_step = condition_every_step
        self.token_embedding = nn.Embedding(vocab_size, embed_dim)
        self.layout_embedding = nn.Embedding(layout_vocab_size, embed_dim)
        self.grade_embedding = nn.Embedding(grade_vocab_size, embed_dim)
        self.context_proj = nn.Sequential(
            nn.Linear(embed_dim * 2, hidden_dim),
            nn.Tanh(),
        )
        if condition_every_step:
            self.input_fusion = nn.Sequential(
                nn.Linear(embed_dim * 3, embed_dim),
                nn.Tanh(),
            )
        else:
            self.input_fusion = None
        self.decoder = nn.GRU(
            input_size=embed_dim,
            hidden_size=hidden_dim,
            num_layers=num_layers,
            dropout=dropout if num_layers > 1 else 0.0,
            batch_first=True,
        )
        self.dropout = nn.Dropout(dropout)
        self.head = nn.Linear(hidden_dim, vocab_size)

    def context_embedding(self, source: torch.Tensor) -> torch.Tensor:
        # source shape: [batch, 2] -> [layout_id, grade_id]
        layout_vec = self.layout_embedding(source[:, 0])
        grade_vec = self.grade_embedding(source[:, 1])
        return torch.cat([layout_vec, grade_vec], dim=-1)

    def initial_hidden(self, source: torch.Tensor) -> torch.Tensor:
        context = self.context_embedding(source)
        hidden0 = self.context_proj(context).unsqueeze(0)
        return hidden0.repeat(self.decoder.num_layers, 1, 1)

    def decoder_input(self, source: torch.Tensor, token_ids: torch.Tensor) -> torch.Tensor:
        token_vec = self.token_embedding(token_ids)
        if self.input_fusion is None:
            return token_vec

        context = self.context_embedding(source).unsqueeze(1).expand(-1, token_ids.shape[1], -1)
        return self.input_fusion(torch.cat([token_vec, context], dim=-1))

    @staticmethod
    def apply_repetition_penalty(logits: torch.Tensor, seen: torch.Tensor, penalty: float) -> None:
        if seen.numel() == 0:
            return
        seen_logits = logits[seen]
        logits[seen] = torch.where(seen_logits > 0, seen_logits / penalty, seen_logits * penalty)

    def forward(self, source: torch.Tensor, tgt_input: torch.Tensor) -> torch.Tensor:
        hidden = self.initial_hidden(source)
        token_vec = self.decoder_input(source, tgt_input)
        out, _ = self.decoder(token_vec, hidden)
        out = self.dropout(out)
        return self.head(out)

    @torch.no_grad()
    def generate(
        self,
        source: torch.Tensor,
        sos_id: int,
        eos_id: int,
        max_len: int = 128,
        do_sample: bool = True,
        temperature: float = 1.0,
        top_k: int = 40,
        top_p: float = 0.95,
        repetition_penalty: float = 1.1,
    ) -> torch.Tensor:
        self.eval()
        batch = source.shape[0]
        hidden = self.initial_hidden(source)
        prev = torch.full((batch, 1), sos_id, dtype=torch.long, device=source.device)
        generated = []
        finished = torch.zeros(batch, dtype=torch.bool, device=source.device)

        for _ in range(max_len):
            token_vec = self.decoder_input(source, prev)
            out, hidden = self.decoder(token_vec, hidden)
            logits = self.head(out[:, -1, :])

            if do_sample:
                temperature = max(1e-5, temperature)
                logits = logits / temperature

                if repetition_penalty > 1.0 and generated:
                    for b in range(batch):
                        history = torch.stack([step[b] for step in generated], dim=0)
                        seen = torch.unique(history)
                        self.apply_repetition_penalty(logits[b], seen, repetition_penalty)

                if top_k > 0 and top_k < logits.shape[-1]:
                    topk_vals, _ = torch.topk(logits, top_k, dim=-1)
                    threshold = topk_vals[:, -1].unsqueeze(1)
                    logits = torch.where(logits < threshold, torch.full_like(logits, float("-inf")), logits)

                if 0.0 < top_p < 1.0:
                    sorted_logits, sorted_indices = torch.sort(logits, descending=True, dim=-1)
                    sorted_probs = torch.softmax(sorted_logits, dim=-1)
                    cumulative_probs = torch.cumsum(sorted_probs, dim=-1)
                    sorted_remove = cumulative_probs > top_p
                    sorted_remove[:, 0] = False
                    remove_mask = torch.zeros_like(sorted_remove, dtype=torch.bool)
                    remove_mask.scatter_(1, sorted_indices, sorted_remove)
                    logits = logits.masked_fill(remove_mask, float("-inf"))

                probs = torch.softmax(logits, dim=-1)
                next_token = torch.multinomial(probs, num_samples=1).squeeze(1)
            else:
                next_token = torch.argmax(logits, dim=-1)

            generated.append(next_token)
            finished = finished | (next_token == eos_id)
            prev = next_token.unsqueeze(1)
            if finished.all():
                break

        return torch.stack(generated, dim=1) if generated else torch.empty((batch, 0), dtype=torch.long, device=source.device)
