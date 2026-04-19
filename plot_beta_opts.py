#!/usr/bin/env python3
"""Plot grouped bar charts comparing 4 adaptive/dynamic beta configs + Classic."""

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import os

out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Figures')
os.makedirs(out_dir, exist_ok=True)

# ── Data (800 nodes, dense, β=3, α=0.5, seeds 49/41) ──
configs = ['Classic Flooding', 'Baseline Kadcast', 'Kadcast+Dynamic']
short   = ['Classic Flooding', 'Baseline Kadcast', 'Kadcast+Dynamic']

# Propagation delay
p50  = [1016.3, 308.1, 248.5]
p90  = [1280.4, 443.9, 309.9]

# Messages (in millions)
msgs_raw = [8608654, 1613963, 3449802]
msgs = [m / 1e6 for m in msgs_raw]

# Stale rate (%)
stale = [63.64, 42.19, 32.20]

# Contention (% queued)
contention = [92.9, 31.4, 64.8]

# Traffic (MB)
traffic_raw = [18693472, 3341228, 8197599]
traffic = [t / 1024 for t in traffic_raw]  # MB

# ── Style ──
plt.rcParams.update({
    'font.size': 12,
    'axes.labelsize': 13,
    'axes.titlesize': 14,
    'legend.fontsize': 10,
    'figure.dpi': 200,
})

colors = ['#DC2626', '#6B7280', '#16A34A']

def save(fig, name):
    fig.savefig(os.path.join(out_dir, name + '.pdf'), bbox_inches='tight')
    fig.savefig(os.path.join(out_dir, name + '.png'), bbox_inches='tight')
    print(f'  Saved {name}')

# ── Plot 1: Propagation Delay (p50 + p90 grouped) ──
fig, ax = plt.subplots(figsize=(8, 4.5))
x = np.arange(len(configs))
w = 0.35
b1 = ax.bar(x - w/2, p50, w, label='p50', color=colors, edgecolor='white', linewidth=0.8, alpha=0.85)
b2 = ax.bar(x + w/2, p90, w, label='p90', color=colors, edgecolor='white', linewidth=0.8, alpha=0.5)
for bar, val in zip(b1, p50):
    ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 15,
            f'{val:.0f}', ha='center', va='bottom', fontsize=9, fontweight='bold')
for bar, val in zip(b2, p90):
    ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 15,
            f'{val:.0f}', ha='center', va='bottom', fontsize=9, color='#555')
ax.set_xticks(x)
ax.set_xticklabels(short)
ax.set_ylabel('Propagation Delay (ms)')
ax.set_title('Block Propagation Delay: Classic vs KADcast Variants')
ax.legend()
ax.grid(axis='y', alpha=0.3)
fig.tight_layout()
save(fig, 'beta_opts_latency')

# ── Plot 2: Messages (millions) ──
fig, ax = plt.subplots(figsize=(7, 4))
bars = ax.bar(x, msgs, 0.55, color=colors, edgecolor='white', linewidth=0.8)
for bar, val in zip(bars, msgs):
    ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.1,
            f'{val:.1f}M', ha='center', va='bottom', fontsize=10, fontweight='bold')
ax.set_xticks(x)
ax.set_xticklabels(short)
ax.set_ylabel('Total Messages (millions)')
ax.set_title('Message Overhead')
ax.grid(axis='y', alpha=0.3)
fig.tight_layout()
save(fig, 'beta_opts_messages')

# ── Plot 3: Stale Rate ──
fig, ax = plt.subplots(figsize=(7, 4))
bars = ax.bar(x, stale, 0.55, color=colors, edgecolor='white', linewidth=0.8)
for bar, val in zip(bars, stale):
    ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.8,
            f'{val:.1f}%', ha='center', va='bottom', fontsize=10, fontweight='bold')
ax.set_xticks(x)
ax.set_xticklabels(short)
ax.set_ylabel('Stale Rate (%)')
ax.set_title('Stale Block Rate')
ax.grid(axis='y', alpha=0.3)
fig.tight_layout()
save(fig, 'beta_opts_stale')

# ── Plot 4: Latency vs Messages trade-off (scatter) ──
fig, ax = plt.subplots(figsize=(7, 4.5))
for i, (label, m, lat) in enumerate(zip(configs, msgs, p50)):
    ax.scatter(m, lat, s=120, c=colors[i], zorder=5, edgecolors='white', linewidth=1)
    offset_x = 0.15
    offset_y = -18
    ax.annotate(label, (m, lat), xytext=(offset_x, offset_y),
                textcoords='offset points', fontsize=10, fontweight='bold',
                color=colors[i])
ax.set_xlabel('Total Messages (millions)')
ax.set_ylabel('p50 Propagation Delay (ms)')
ax.set_title('Latency vs. Message Overhead Trade-off')
ax.grid(True, alpha=0.3)
fig.tight_layout()
save(fig, 'beta_opts_tradeoff')

plt.close('all')
print('Done.')
