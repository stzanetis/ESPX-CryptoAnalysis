import os
import re
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from datetime import datetime

symbols = [
    "BTC-USDT", "ADA-USDT", "ETH-USDT", "DOGE-USDT",
    "XRP-USDT", "SOL-USDT", "LTC-USDT", "BNB-USDT"
]

MA_DIR = "data/mavg"
TX_DIR = "logs/transactions"
TIMINGS_LOG = "logs/timings.log"
OUT_DIR = "results"
os.makedirs(OUT_DIR, exist_ok=True)

re_ma = re.compile(r'^\[(\d+)\],\s*MovingAvg:\s*([0-9.]+)')
re_tx = re.compile(r'^\[(\d+)\],\s*Price:\s*([0-9.]+),\s*Volune:\s*([0-9.]+)')
re_tim = re.compile(r'^Start:\s*([0-9:.]+),\s*End:\s*([0-9:.]+),\s*Duration:\s*([0-9.]+)\s*ms')

def load_moving_avg():
    rows = []
    for sym in symbols:
        path = f"{MA_DIR}/{sym}.log"
        if not os.path.exists(path):
            continue
        with open(path, 'r') as f:
            for line in f:
                m = re_ma.match(line.strip())
                if m:
                    ts = int(m.group(1))
                    val = float(m.group(2))
                    rows.append((ts, sym, val))
    if not rows:
        return pd.DataFrame()
    df = pd.DataFrame(rows, columns=["timestamp", "symbol", "moving_avg"])
    df['datetime'] = pd.to_datetime(df['timestamp'], unit='s')
    pivot = df.pivot_table(index='datetime', columns='symbol', values='moving_avg', aggfunc='last').sort_index()
    return pivot

def load_transactions_counts():
    per_minute = {}
    for sym in symbols:
        path = f"{TX_DIR}/{sym}.log"
        if not os.path.exists(path):
            continue
        with open(path, 'r') as f:
            for line in f:
                m = re_tx.match(line.strip())
                if not m:
                    continue
                ts = int(m.group(1))
                minute = ts // 60  # minute bucket (epoch minutes)
                per_minute[minute] = per_minute.get(minute, 0) + 1
    if not per_minute:
        return pd.DataFrame(columns=['minute_epoch','trade_count'])
    data = [(k, v) for k, v in per_minute.items()]
    df = pd.DataFrame(data, columns=['minute_epoch','trade_count'])
    df['minute_dt'] = pd.to_datetime(df['minute_epoch']*60, unit='s')
    return df.sort_values('minute_dt')

def correlation_heatmap_from_ma(ma_df):
    if ma_df.empty:
        print("No moving average data to compute correlation.")
        return
    # Drop columns with all NaN
    ma_df = ma_df.dropna(axis=1, how='all')
    # Option: forward fill small gaps (keep minimal to avoid bias)
    filled = ma_df.ffill().bfill()
    corr = filled.corr()
    # Average off-diagonal
    mask_diag = np.eye(len(corr), dtype=bool)
    vals = corr.values[~mask_diag]
    avg_corr = np.nanmean(vals)
    plt.figure(figsize=(12,10))
    ax = sns.heatmap(corr, annot=True, fmt=".3f", cmap="RdBu_r", center=0, vmin=-1, vmax=1,
                     cbar_kws={'label':'Correlation'})
    ax.text(0.02, 0.98, f"Avg Corr: {avg_corr:.3f}", transform=ax.transAxes,
            fontsize=12, bbox=dict(facecolor="white", edgecolor="gray", boxstyle="round,pad=0.3"),
            va='top')
    plt.title("Cryptocurrency Correlation (From Moving Averages)")
    plt.tight_layout()
    plt.savefig(f"{OUT_DIR}/correlation.png")
    plt.close()

def plot_moving_averages(ma_df):
    if ma_df.empty:
        print("No moving average data for plot.")
        return
    fig, axes = plt.subplots(2,4, figsize=(20,10), sharex=True)
    axes = axes.flatten()
    for i, sym in enumerate(symbols):
        ax = axes[i]
        if sym in ma_df.columns:
            ax.plot(ma_df.index, ma_df[sym], label='15m MA', linewidth=1.2)
            ax.set_title(sym)
            ax.legend(fontsize=8)
        else:
            ax.text(0.5,0.5,"No Data", ha='center', va='center')
            ax.set_title(sym)
    plt.tight_layout()
    plt.savefig(f"{OUT_DIR}/moving_averages.png")
    plt.close()

def load_timings():
    if not os.path.exists(TIMINGS_LOG):
        return pd.DataFrame()
    rows = []
    with open(TIMINGS_LOG,'r') as f:
        for line in f:
            m = re_tim.match(line.strip())
            if m:
                start_s, end_s, dur_ms = m.groups()
                # Times have no date; use a dummy date (today)
                today = datetime.utcnow().date().isoformat()
                try:
                    start_dt = datetime.strptime(today + " " + start_s, "%Y-%m-%d %H:%M:%S.%f")
                    end_dt = datetime.strptime(today + " " + end_s, "%Y-%m-%d %H:%M:%S.%f")
                except ValueError:
                    # Fallback without milliseconds
                    start_dt = datetime.strptime(today + " " + start_s.split('.')[0], "%Y-%m-%d %H:%M:%S")
                    end_dt = datetime.strptime(today + " " + end_s.split('.')[0], "%Y-%m-%d %H:%M:%S")
                rows.append((start_dt, end_dt, float(dur_ms)))
    if not rows:
        return pd.DataFrame()
    df = pd.DataFrame(rows, columns=['start','end','duration_ms'])
    df = df.sort_values('start')
    return df

def plot_timings(timing_df, trades_df):
    if timing_df.empty:
        print("No timing data.")
        return
    # Line plot of durations
    plt.figure(figsize=(10,5))
    plt.plot(timing_df['start'], timing_df['duration_ms'], marker='o', linestyle='-', linewidth=1)
    plt.title("Processing Durations")
    plt.ylabel("Duration (ms)")
    plt.xlabel("Start Time")
    plt.tight_layout()
    plt.savefig(f"{OUT_DIR}/timings_durations.png")
    plt.close()

    # Merge with trade counts per minute (align start minute)
    if trades_df.empty:
        return
    timing_df['minute_epoch'] = (timing_df['start'].astype('int64') // 10**9) // 60
    merged = timing_df.merge(trades_df[['minute_epoch','trade_count']], on='minute_epoch', how='left')
    plt.figure(figsize=(10,5))
    ax1 = plt.gca()
    ax1.plot(merged['start'], merged['duration_ms'], color='tab:blue', label='Duration (ms)')
    ax1.set_ylabel('Duration (ms)', color='tab:blue')
    ax2 = ax1.twinx()
    ax2.bar(merged['start'], merged['trade_count'].fillna(0), alpha=0.3, color='tab:orange', label='Trades/min')
    ax2.set_ylabel('Trades per minute', color='tab:orange')
    plt.title("Processing Duration vs Trade Volume")
    # Combined legend
    lines, labels = [], []
    for ax in (ax1, ax2):
        l, lab = ax.get_legend_handles_labels()
        lines.extend(l); labels.extend(lab)
    plt.legend(lines, labels, loc='upper right')
    plt.tight_layout()
    plt.savefig(f"{OUT_DIR}/timings_vs_trades.png")
    plt.close()

def main():
    print("Evaluating project data...")
    ma_df = load_moving_avg()
    correlation_heatmap_from_ma(ma_df)
    plot_moving_averages(ma_df)
    trades_df = load_transactions_counts()
    timing_df = load_timings()
    plot_timings(timing_df, trades_df)
    print(f"Generated assets in {OUT_DIR}")

if __name__ == "__main__":
    main()