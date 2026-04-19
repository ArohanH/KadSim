import numpy as np
import matplotlib.pyplot as plt
import os

os.makedirs("figures", exist_ok=True)

densities = ["Sparse", "Moderate", "Dense"]
x = np.arange(len(densities))

# Better spacing
width = 0.18

# Classic baseline
classic_p50 = np.array([1171, 983, 1129])
classic_messages = np.array([529, 2550, 6680])
classic_traffic = np.array([1125, 4914, 11975])
classic_stale = np.array([61.9, 50.9, 72.6])

# KADcast
kad_p50 = {1: [3205, 1228, 736], 3: [1801, 715, 439], 5: [1480, 613, 336]}
kad_messages = {1: [102, 137, 168], 3: [716, 1230, 1420], 5: [1180, 1890, 1790]}
kad_traffic = {1: [191, 279, 343], 3: [1392, 2555, 3163], 5: [2191, 4127, 3514]}
kad_stale = {1: [80.8, 65.8, 51.6], 3: [66.1, 61.8, 41.6], 5: [66.1, 50.0, 38.7]}


def add_labels(bars):
    for bar in bars:
        height = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2,
                 height,
                 f"{height:.0f}",
                 ha='center', va='bottom', fontsize=8)


def plot_bar(metric_name, classic, kad_dict, ylabel, filename):
    plt.figure(figsize=(7, 4.5))

    # Shift positions for grouping
    offsets = [-1.5*width, -0.5*width, 0.5*width, 1.5*width]

    bars1 = plt.bar(x + offsets[0], classic, width, label="Classic")
    bars2 = plt.bar(x + offsets[1], kad_dict[1], width, label="KAD β=1")
    bars3 = plt.bar(x + offsets[2], kad_dict[3], width, label="KAD β=3")
    bars4 = plt.bar(x + offsets[3], kad_dict[5], width, label="KAD β=5")

    # Labels on bars
    add_labels(bars1)
    add_labels(bars2)
    add_labels(bars3)
    add_labels(bars4)

    plt.xticks(x, densities, fontsize=11)
    plt.ylabel(ylabel, fontsize=12)
    plt.title(metric_name, fontsize=13)

    plt.legend(frameon=False, fontsize=10)
    plt.grid(axis='y', linestyle='--', alpha=0.6)

    plt.tight_layout()
    plt.savefig(f"figures/{filename}", dpi=300)
    plt.close()


# Generate plots
plot_bar("p50 Latency Comparison", classic_p50, kad_p50, "Latency (ms)", "p50_latency.png")
plot_bar("Message Overhead Comparison", classic_messages, kad_messages, "Messages (K)", "messages.png")
plot_bar("Traffic Comparison", classic_traffic, kad_traffic, "Traffic (MB)", "traffic.png")
plot_bar("Stale Rate Comparison", classic_stale, kad_stale, "Stale Rate (%)", "stale.png")

print("Better plots saved in ./figures/")