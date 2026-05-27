"""Train a value network on Tressette self-play data.

Reads the binary file produced by `tressette_selfplay` and trains a small
transformer that, given (my hand, my captured cards, opponent captured cards),
predicts the final score of the game for "me".

See ../../.claude/plans/create-a-c-program-tranquil-thacker.md for the design.
"""

import argparse
import math
import os
import struct
import sys

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import DataLoader, Dataset

# Binary file constants. Must match the C++ writer in selfplay.cpp.
HEADER_BYTES = 16
RECORD_BYTES = 36
MAGIC = 0x54525353
NUM_CARDS = 40

# Record fields layout (little-endian):
#   uint64 hand_player_0
#   uint64 hand_player_1
#   uint64 captured_player_0
#   uint64 captured_player_1
#   uint8  current_player
#   uint8  final_score_player_0
#   uint8  final_score_player_1
#   uint8  padding
RECORD_DTYPE = np.dtype(
    [
        ("hand_0", "<u8"),
        ("hand_1", "<u8"),
        ("captured_0", "<u8"),
        ("captured_1", "<u8"),
        ("current", "u1"),
        ("score_0", "u1"),
        ("score_1", "u1"),
        ("_pad", "u1"),
    ]
)


def bitmask_to_vector(masks: np.ndarray) -> np.ndarray:
    """Convert an array of uint64 bitmasks to a (N, 40) float32 indicator matrix.

    Bit i of the mask is set iff card id i is in the corresponding set.
    """
    bits = np.unpackbits(masks.view(np.uint8).reshape(-1, 8), axis=1, bitorder="little")
    return bits[:, :NUM_CARDS].astype(np.float32)


class Tressette_Dataset(Dataset):
    """Each underlying snapshot is exposed twice — once per perspective.

    Index 2*k uses player 0 as "me"; index 2*k+1 uses player 1 as "me".
    The model never sees the opponent's hand (that's hidden information).
    """

    def __init__(self, records: np.ndarray):
        self.records = records

    def __len__(self) -> int:
        return len(self.records) * 2

    def __getitem__(self, i: int):
        record_index = i // 2
        perspective = i % 2  # 0 = play as player 0, 1 = play as player 1.
        record = self.records[record_index]

        if perspective == 0:
            my_hand_mask = record["hand_0"]
            my_capt_mask = record["captured_0"]
            opp_capt_mask = record["captured_1"]
            target_score = record["score_0"]
        else:
            my_hand_mask = record["hand_1"]
            my_capt_mask = record["captured_1"]
            opp_capt_mask = record["captured_0"]
            target_score = record["score_1"]

        # Unpack one at a time (each uint64 -> length-40 float vec).
        my_hand = bitmask_to_vector(np.array([my_hand_mask], dtype=np.uint64))[0]
        my_capt = bitmask_to_vector(np.array([my_capt_mask], dtype=np.uint64))[0]
        opp_capt = bitmask_to_vector(np.array([opp_capt_mask], dtype=np.uint64))[0]
        return (
            torch.from_numpy(my_hand),
            torch.from_numpy(my_capt),
            torch.from_numpy(opp_capt),
            torch.tensor(float(target_score), dtype=torch.float32),
        )


def load_dataset(path: str):
    """Read the whole file (it's ~MB-scale) and return (records, num_games)."""
    with open(path, "rb") as f:
        raw = f.read()
    magic, version, num_games, num_snapshots = struct.unpack(
        "<IIII", raw[:HEADER_BYTES]
    )
    if magic != MAGIC:
        raise ValueError(f"bad magic {hex(magic)}, expected {hex(MAGIC)}")
    if version != 1:
        raise ValueError(f"unsupported file version {version}")
    body_bytes = num_snapshots * RECORD_BYTES
    body = raw[HEADER_BYTES : HEADER_BYTES + body_bytes]
    records = np.frombuffer(body, dtype=RECORD_DTYPE).copy()
    return records, int(num_games)


# Each card is one token. Its embedding combines a per-card-id embedding with a
# per-location embedding (where the card currently sits relative to "me").
LOCATION_MY_HAND = 0
LOCATION_MY_CAPTURED = 1
LOCATION_OPP_CAPTURED = 2
LOCATION_UNSEEN = 3
NUM_LOCATIONS = 4


class Tressette_Value_Net(nn.Module):
    def __init__(
        self,
        d_model: int = 128,
        num_heads: int = 4,
        num_layers: int = 4,
        dropout: float = 0.1,
    ):
        super().__init__()
        self.card_embedding = nn.Embedding(NUM_CARDS, d_model)
        self.location_embedding = nn.Embedding(NUM_LOCATIONS, d_model)
        encoder_layer = nn.Transform2DerEncoderLayer(
            d_model=d_model,
            nhead=num_heads,
            dim_feedforward=4 * d_model,
            dropout=dropout,
            batch_first=True,
            activation="gelu",
        )
        self.encoder = nn.Transform2DerEncoder(encoder_layer, num_layers=num_layers)
        self.head = nn.Sequential(
            nn.Linear(d_model, d_model),
            nn.GELU(),
            nn.Linear(d_model, 1),
        )

    def forward(
        self,
        my_hand: torch.Tensor,
        my_captured: torch.Tensor,
        opp_captured: torch.Tensor,
    ) -> torch.Tensor:
        # my_hand / my_captured / opp_captured: (batch, 40) indicators in {0,1}.
        batch_size = my_hand.shape[0]
        device = my_hand.device

        # Default location is "unseen" (in opponent's hand or still in stock).
        # We can't tell which, and we don't need to.
        location = torch.full(
            (batch_size, NUM_CARDS), LOCATION_UNSEEN, dtype=torch.long, device=device
        )
        location = torch.where(
            my_hand > 0.5, torch.full_like(location, LOCATION_MY_HAND), location
        )
        location = torch.where(
            my_captured > 0.5, torch.full_like(location, LOCATION_MY_CAPTURED), location
        )
        location = torch.where(
            opp_captured > 0.5,
            torch.full_like(location, LOCATION_OPP_CAPTURED),
            location,
        )

        card_ids = (
            torch.arange(NUM_CARDS, device=device).unsqueeze(0).expand(batch_size, -1)
        )
        tokens = self.card_embedding(card_ids) + self.location_embedding(location)
        encoded = self.encoder(tokens)  # (batch, 40, d_model).
        pooled = encoded.mean(dim=1)
        return self.head(pooled).squeeze(-1)


def split_records_by_game(
    records: np.ndarray, num_games: int, val_fraction: float, seed: int
):
    """Split records so that all snapshots from one game land in the same set.

    Games are ordered in the file. Game boundaries are detected when captured
    piles reset to zero after being non-zero — captures only grow during a game
    and reset only at the start of a new one.
    """
    both_captures_zero = (records["captured_0"] == 0) & (records["captured_1"] == 0)

    # New game: both captures are zero (first trick not yet resolved) AND the
    # previous record had non-zero captures (end of prior game). The very first
    # record is already assigned to game 0.
    game_id = np.zeros(len(records), dtype=np.int64)
    current_game = 0
    for i in range(1, len(records)):
        if both_captures_zero[i] and not both_captures_zero[i - 1]:
            current_game += 1
        game_id[i] = current_game

    assert (
        current_game + 1 == num_games
    ), f"reconstructed {current_game + 1} games but header says {num_games}"

    rng = np.random.default_rng(seed)
    permuted = rng.permutation(num_games)
    num_val = max(1, int(round(num_games * val_fraction)))
    val_games = set(permuted[:num_val].tolist())
    train_games = set(permuted[num_val:].tolist())

    train_mask = np.array([g in train_games for g in game_id])
    val_mask = np.array([g in val_games for g in game_id])
    return records[train_mask], records[val_mask]


def count_parameters(model: nn.Module) -> int:
    return sum(p.numel() for p in model.parameters())


def train(args):
    records, num_games = load_dataset(args.data)
    print(f"loaded {len(records)} snapshots from {num_games} games")

    train_records, val_records = split_records_by_game(
        records, num_games, args.val_fraction, seed=args.seed
    )

    if args.min_captured > 0:

        def enough_captured(recs: np.ndarray) -> np.ndarray:
            total = np.array(
                [
                    bin(int(r["captured_0"])).count("1")
                    + bin(int(r["captured_1"])).count("1")
                    for r in recs
                ]
            )
            return recs[total >= args.min_captured]

        train_records = enough_captured(train_records)
        val_records = enough_captured(val_records)

    if args.snapshots_per_game > 0:
        # Subsample snapshots per game to reduce within-game label correlation.
        # Consecutive records from the same game share the same final score, so
        # using all 40 inflates the apparent dataset size without adding new labels.
        rng = np.random.default_rng(args.seed)
        n = args.snapshots_per_game
        # Records are ordered by game; grab chunks of the original 40 and subsample.
        # We don't have game ids, so use the reconstructed game_id array isn't
        # available here — approximate with fixed stride assuming ~40 per game.
        snaps_per_game_actual = len(train_records) // num_games
        if snaps_per_game_actual > n:
            keep = np.concatenate(
                [
                    rng.choice(snaps_per_game_actual, size=n, replace=False)
                    + g * snaps_per_game_actual
                    for g in range(num_games)
                    if (g + 1) * snaps_per_game_actual <= len(train_records)
                ]
            )
            keep.sort()
            train_records = train_records[keep]

    print(f"train snapshots: {len(train_records)}  val snapshots: {len(val_records)}")

    train_dataset = Tressette_Dataset(train_records)
    val_dataset = Tressette_Dataset(val_records)

    train_loader = DataLoader(
        train_dataset,
        batch_size=args.batch_size,
        shuffle=True,
        num_workers=args.num_workers,
        drop_last=True,
    )
    val_loader = DataLoader(
        val_dataset,
        batch_size=args.batch_size,
        shuffle=False,
        num_workers=args.num_workers,
    )

    device = torch.device(
        "cuda"
        if torch.cuda.is_available()
        else "mps" if torch.backends.mps.is_available() else "cpu"
    )
    print(f"device: {device}")

    model = Tressette_Value_Net(
        d_model=args.d_model,
        num_heads=args.num_heads,
        num_layers=args.num_layers,
        dropout=args.dropout,
    ).to(device)

    if args.init_checkpoint and os.path.exists(args.init_checkpoint):
        ckpt = torch.load(args.init_checkpoint, map_location=device)
        model.load_state_dict(ckpt["model_state_dict"])
        print(f"resumed from {args.init_checkpoint}")

    param_count = count_parameters(model)
    print(f"model parameters: {param_count}  (~{param_count * 4 / 1e6:.2f} MB at fp32)")

    optimizer = torch.optim.AdamW(
        model.parameters(), lr=args.lr, weight_decay=args.weight_decay
    )
    total_steps = max(1, args.epochs * len(train_loader))
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=total_steps)

    for epoch in range(args.epochs):
        model.train()
        train_sum_squared_error = 0.0
        train_sum_abs_error = 0.0
        train_count = 0
        for my_hand, my_capt, opp_capt, target in train_loader:
            my_hand = my_hand.to(device)
            my_capt = my_capt.to(device)
            opp_capt = opp_capt.to(device)
            target = target.to(device)

            prediction = model(my_hand, my_capt, opp_capt)
            loss = F.mse_loss(prediction, target)

            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
            scheduler.step()

            batch_n = target.numel()
            train_sum_squared_error += loss.item() * batch_n
            train_sum_abs_error += (prediction - target).abs().sum().item()
            train_count += batch_n

        train_mse = train_sum_squared_error / max(1, train_count)
        train_mae = train_sum_abs_error / max(1, train_count)

        model.eval()
        val_sum_squared_error = 0.0
        val_sum_abs_error = 0.0
        val_count = 0
        with torch.no_grad():
            for my_hand, my_capt, opp_capt, target in val_loader:
                my_hand = my_hand.to(device)
                my_capt = my_capt.to(device)
                opp_capt = opp_capt.to(device)
                target = target.to(device)
                prediction = model(my_hand, my_capt, opp_capt)
                batch_n = target.numel()
                val_sum_squared_error += F.mse_loss(
                    prediction, target, reduction="sum"
                ).item()
                val_sum_abs_error += (prediction - target).abs().sum().item()
                val_count += batch_n
        val_mse = val_sum_squared_error / max(1, val_count)
        val_mae = val_sum_abs_error / max(1, val_count)

        print(
            f"epoch {epoch + 1:3d}/{args.epochs}"
            f"  train_mse={train_mse:.4f}  train_mae={train_mae:.4f}"
            f"  val_mse={val_mse:.4f}  val_mae={val_mae:.4f}"
            f"  lr={scheduler.get_last_lr()[0]:.2e}"
        )

        torch.save(
            {
                "model_state_dict": model.state_dict(),
                "config": {
                    "d_model": args.d_model,
                    "num_heads": args.num_heads,
                    "num_layers": args.num_layers,
                    "dropout": args.dropout,
                },
            },
            args.out,
        )
        print(f"saved model to {args.out}")

        if args.export:
            # Trace on CPU so the exported model is device-agnostic.
            cpu_model = model.to("cpu").eval()
            dummy = torch.zeros(1, NUM_CARDS)
            # check_trace=False: MultiheadAttention takes different code paths at
            # batch_size=1 vs >1, which triggers a false sanity-check failure.
            traced = torch.jit.trace(
                cpu_model, (dummy, dummy, dummy), check_trace=False
            )
            export_path = args.out.replace(".pt", "_traced.pt")
            traced.save(export_path)
            print(f"exported TorchScript model to {export_path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=str, default="selfplay_data.bin")
    parser.add_argument("--out", type=str, default="tressette_value.pt")
    parser.add_argument(
        "--init-checkpoint",
        type=str,
        default="",
        help="warm-start from this checkpoint before training",
    )
    parser.add_argument(
        "--export",
        action="store_true",
        help="also save a TorchScript-traced model loadable from C++ via LibTorch",
    )
    parser.add_argument("--epochs", type=int, default=20)
    parser.add_argument("--batch-size", type=int, default=256)
    parser.add_argument("--lr", type=float, default=3e-4)
    parser.add_argument("--weight-decay", type=float, default=1e-3)
    parser.add_argument("--d-model", type=int, default=128)
    parser.add_argument("--num-heads", type=int, default=4)
    parser.add_argument("--num-layers", type=int, default=4)
    parser.add_argument("--dropout", type=float, default=0.3)
    parser.add_argument(
        "--min-captured",
        type=int,
        default=8,
        help="skip snapshots where total captured cards < this (early-game high-noise states)",
    )
    parser.add_argument(
        "--snapshots-per-game",
        type=int,
        default=0,
        help="max snapshots to keep per game (0 = keep all); reduces within-game label correlation",
    )
    parser.add_argument("--val-fraction", type=float, default=0.1)
    parser.add_argument("--num-workers", type=int, default=0)
    parser.add_argument("--seed", type=int, default=0)
    args = parser.parse_args()

    torch.manual_seed(args.seed)
    np.random.seed(args.seed)

    train(args)


if __name__ == "__main__":
    main()
