import os
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns

symbols = [
        "BTC-USDT", "ADA-USDT", "ETH-USDT", "DOGE-USDT",
        "XRP-USDT", "SOL-USDT", "LTC-USDT", "BNB-USDT"
    ]

def correlation_heatmap():
    corr_matrix = pd.DataFrame(0.0, index=symbols, columns=symbols, dtype=float)
    counts = pd.DataFrame(0, index=symbols, columns=symbols, dtype=int)

    for i, symbol in enumerate(symbols):
        log_path = f"data/corr/{symbol}.log"
        if not os.path.exists(log_path):
            continue
            
        try:
            df = pd.read_csv(log_path, header=None)
            for _, row in df.iterrows():
                if len(row) >= 11:  # timestamp, max_symbol, max_corr + 8 correlations
                    correlations = row[3:11].values  # Get 8 correlation values
                    for j in range(len(symbols)):
                        if j < len(correlations):
                            corr = float(correlations[j])
                            if not np.isnan(corr):
                                corr_matrix.loc[symbol, symbols[j]] += corr
                                counts.loc[symbol, symbols[j]] += 1
        except Exception as e:
            print(f"Error processing {symbol}: {e}")

    # Calculate averages
    for symbol in symbols:
        for other_symbol in symbols:
            if counts.loc[symbol, other_symbol] > 0:
                corr_matrix.loc[symbol, other_symbol] /= counts.loc[symbol, other_symbol]
            else:
                corr_matrix.loc[symbol, other_symbol] = 0.0

    # Calculate average correlation (excluding diagonal)
    mask_diag = np.eye(len(corr_matrix), dtype=bool)
    off_diag_values = corr_matrix.values[~mask_diag]
    avg_corr = np.nanmean(off_diag_values[off_diag_values != 0])
    
    # Create heatmap
    plt.figure(figsize=(12, 10))
    ax = sns.heatmap(
        corr_matrix.astype(float), 
        annot=True, 
        fmt=".3f", 
        cmap="RdBu_r", 
        center=0, 
        vmin=-1, 
        vmax=1,
        cbar_kws={'label': 'Correlation Strength'}
    )
    
    # Add average correlation text
    ax.text(
        0.02, 0.98,
        f"Avg Correlation: {avg_corr:.3f}",
        transform=ax.transAxes,
        fontsize=12,
        bbox=dict(facecolor="white", edgecolor="gray", boxstyle="round,pad=0.3"),
        verticalalignment='top'
    )

    plt.title("Cryptocurrency Correlation Heatmap - FinalProject")
    plt.tight_layout()
    
    # Create output directory if it doesn't exist
    os.makedirs("test/assets", exist_ok=True)
    plt.savefig("test/assets/correlation.png")
    plt.close()

def analyze_volume_patterns():
    axes = plt.subplots(2, 4, figsize=(20, 10))
    axes = axes.flatten()
    
    for i, symbol in enumerate(symbols):
        ma_path = f"data/mavg/{symbol}.log"
        if os.path.exists(ma_path):
            try:
                # Only 2 columns: timestamp, moving_average
                df = pd.read_csv(ma_path, header=None, names=['timestamp', 'moving_avg'])
                df['datetime'] = pd.to_datetime(df['timestamp'], unit='s')
                
                axes[i].plot(df['datetime'], df['moving_avg'], label='15-min Moving Average', alpha=0.7)
                axes[i].set_title(symbol)
                axes[i].set_ylabel('Price (Moving Average)')
                axes[i].tick_params(axis='x', rotation=45)
                axes[i].legend()
            except Exception as e:
                axes[i].text(0.5, 0.5, f'Error: {e}', ha='center', va='center', transform=axes[i].transAxes)
                axes[i].set_title(f"{symbol} - No Data")
        else:
            axes[i].text(0.5, 0.5, 'No moving average data', ha='center', va='center', transform=axes[i].transAxes)
            axes[i].set_title(f"{symbol} - No Data")
    
    plt.tight_layout()
    plt.savefig("test/assets/moving_averages.png")
    plt.close()

if __name__ == "__main__":
    print("Generating analysis for FinalProject...")
    correlation_heatmap()
    analyze_volume_patterns()
    print("Generated plots in test/assets/ directory")