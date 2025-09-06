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
CORR_DIR = "data/corr"
TIMINGS_LOG = "logs/timings.log"
CPU_IDLE_LOG = "logs/cpu_idle.log"
OUT_DIR = "results"
os.makedirs(OUT_DIR, exist_ok=True)

re_ma = re.compile(r'^\[(\d+)\],\s*MovingAvg:\s*([0-9.]+)')
re_tim = re.compile(r'^Start:\s*([0-9:.]+),\s*End:\s*([0-9:.]+),\s*Duration:\s*([0-9.]+)\s*ms')
re_cpu = re.compile(r'^\[(\d+)\],\s*([0-9.]+)')

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

def load_corr():
    corr_sum = pd.DataFrame(0.0, index=symbols, columns=symbols, dtype=float)
    count = pd.DataFrame(0, index=symbols, columns=symbols, dtype=int)

    for sym_file in symbols:
        path = os.path.join(CORR_DIR, f"{sym_file}.log")
        if not os.path.isfile(path):
            continue

        with open(path, "r") as f:
            for raw in f:
                line = raw.strip()
                if not line:
                    continue
                parts = line.split(",")
                if len(parts) < 11:
                    continue
                try:
                    vals = [float(x) for x in parts[-8:]]
                except ValueError:
                    continue

                for i, target_sym in enumerate(symbols):
                    corr_sum.loc[sym_file, target_sym] += vals[i]
                    count.loc[sym_file, target_sym] += 1

    corr = pd.DataFrame(index=symbols, columns=symbols, dtype=float)
    for a in symbols:
        for b in symbols:
            if count.loc[a, b] > 0:
                corr.loc[a, b] = corr_sum.loc[a, b] / count.loc[a, b]
            else:
                corr.loc[a, b] = np.nan

    n = len(symbols)
    for i in range(n):
        for j in range(i + 1, n):
            a, b = symbols[i], symbols[j]
            ab, ba = corr.loc[a, b], corr.loc[b, a]
            if pd.notna(ab) and pd.notna(ba):
                v = 0.5 * (ab + ba)
            elif pd.notna(ab):
                v = ab
            else:
                v = ba
            corr.loc[a, b] = v
            corr.loc[b, a] = v

    for s in symbols:
        if pd.isna(corr.loc[s, s]):
            corr.loc[s, s] = 1.0
    corr = corr.fillna(0.0)

    return corr.astype(float)

def plot_heatmap_corr(corr_df):
    if corr_df is None or corr_df.empty:
        print("No correlation data for heatmap.")
        return

    corr = corr_df.astype(float)

    # labels: numbers off-diagonal, '-' on diagonal
    labels = corr.applymap(lambda x: f"{x:.2f}")
    for i in range(len(symbols)):
        labels.iat[i, i] = "-"

    # mask the diagonal to render those cells white
    diag_mask = np.eye(len(corr), dtype=bool)

    plt.figure(figsize=(12, 10))
    ax = sns.heatmap(
        corr,
        mask=diag_mask,               # make diagonal white
        annot=labels.values,          # string labels
        fmt="",                       # use provided strings
        cmap="coolwarm",
        vmin=-1, vmax=1,
        linewidths=0.5,
        cbar_kws={"shrink": 0.8},
        square=True,
        annot_kws={"color": "white"}  # all text white
    )
    ax.set_title("Cryptocurrency Correlation Matrix")
    plt.tight_layout()
    plt.savefig(os.path.join(OUT_DIR, "correlation_heatmap.png"))
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

def plot_timings(timing_df):
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

def load_cpu_idle():
    if not os.path.exists(CPU_IDLE_LOG):
        return pd.DataFrame()
    
    data = []
    with open(CPU_IDLE_LOG, 'r') as f:
        for line in f:
            m = re_cpu.match(line.strip())
            if m:
                ts = int(m.group(1))
                idle_pct = float(m.group(2))
                data.append((ts, idle_pct))
    
    if not data:
        return pd.DataFrame()
    
    df = pd.DataFrame(data, columns=['timestamp', 'cpu_idle_pct'])
    df['datetime'] = pd.to_datetime(df['timestamp'], unit='s')
    return df

def plot_cpu_idle(cpu_df):
    if cpu_df.empty:
        print("No CPU idle data available.")
        return
    
    # Create a histogram of CPU idle distribution
    plt.figure(figsize=(10, 5))
    avg_idle = cpu_df['cpu_idle_pct'].mean()
    sns.histplot(cpu_df['cpu_idle_pct'], bins=20, kde=True)
    plt.title("Distribution of CPU Idle Percentage")
    plt.xlabel("CPU Idle (%)")
    plt.ylabel("Frequency")
    plt.axvline(x=avg_idle, color='r', linestyle='--', label=f'Avg: {avg_idle:.2f}%')
    plt.legend()
    plt.tight_layout()
    plt.savefig(f"{OUT_DIR}/cpu_idle_histogram.png")
    plt.close()

def main():
    print("Evaluating project data...")

    ma_df = load_moving_avg()
    plot_moving_averages(ma_df)

    corr_df = load_corr()
    plot_heatmap_corr(corr_df)

    timing_df = load_timings()
    plot_timings(timing_df)

    cpu_df = load_cpu_idle()
    plot_cpu_idle(cpu_df)

    print(f"Generated assets in {OUT_DIR}")

if __name__ == "__main__":
    main()