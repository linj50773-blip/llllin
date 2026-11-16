"""Quick descriptive analysis of Tier 1 Tide CSV logs.

Usage: python3 analyze.py logs/session01.csv
Requires: pip install pandas matplotlib numpy
"""

import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

STATES = {0: "SILENT", 1: "ANCHOR", 2: "STEP", 3: "PLATEAU", 4: "NEUTRAL", 5: "FADE"}


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python3 analyze.py LOG.csv")
    path = Path(sys.argv[1])
    df = pd.read_csv(path, on_bad_lines="skip")
    required = ["t_ms", "state", "rate", "fsr1", "accA"]
    missing = set(required) - set(df.columns)
    if missing:
        raise SystemExit(f"Missing CSV columns: {sorted(missing)}")
    for name in required:
        df[name] = pd.to_numeric(df[name], errors="coerce")
    df = df.dropna(subset=required).sort_values("t_ms").reset_index(drop=True)
    if len(df) < 2:
        raise SystemExit("Need at least two valid data rows")
    df["t_s"] = (df.t_ms - df.t_ms.iloc[0]) / 1000
    df["state_name"] = df.state.astype(int).map(STATES).fillna("UNKNOWN")
    dt = np.diff(df.t_s)
    dt = dt[dt > 0]
    rate_hz = 1 / np.median(dt) if len(dt) else 10
    win = max(round(5 * rate_hz), 2)
    df["fsr1_mean"] = df.fsr1.rolling(win, min_periods=1).mean()
    df["fsr1_std"] = df.fsr1.rolling(win, min_periods=2).std().fillna(0)
    df["acc_std"] = df.accA.rolling(win, min_periods=2).std().fillna(0)
    fig, axes = plt.subplots(4, 1, figsize=(11, 9), sharex=True)
    for ax, col, label in zip(
        axes,
        ["rate", "fsr1", "fsr1_std", "acc_std"],
        ["rate/min", "FSR1", "FSR1 std (5s)", "acc std (5s)"],
    ):
        ax.plot(df.t_s, df[col], linewidth=1)
        ax.set_ylabel(label)
        ax.grid(alpha=0.2)
    axes[1].plot(df.t_s, df.fsr1_mean, linewidth=1.5)
    axes[-1].set_xlabel("time (s)")
    if "event" in df:
        for _, row in df[df.event.fillna("").astype(str) != ""].iterrows():
            for ax in axes:
                ax.axvline(row.t_s, color="gray", alpha=0.2)
    fig.tight_layout()
    fig.savefig(path.with_suffix(".png"), dpi=150)
    plt.close(fig)
    summary = df.groupby("state_name", dropna=False).agg(
        samples=("t_s", "size"),
        fsr1_mean=("fsr1", "mean"),
        fsr1_std=("fsr1_std", "mean"),
        acc_std=("acc_std", "mean"),
    )
    summary.to_csv(path.with_name(path.stem + "_summary.csv"))
    print(summary.round(2))
    print("Saved plot and summary beside", path)


if __name__ == "__main__":
    main()
