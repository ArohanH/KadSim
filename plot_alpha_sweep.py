#!/usr/bin/env python3
"""Plot p50 and p90 propagation delay vs alpha for the alpha sweep experiments."""

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import os

# Alpha sweep results (KADcast, 800 nodes, dense, beta=3, seeds 49/41)
alpha   = [0.0,   0.125, 0.25,  0.375, 0.5,   0.625, 0.75,  0.875, 1.0]
p50     = [261.1, 269.4, 313.1, 276.9, 308.1, 329.7, 331.4, 395.6, 376.7]
p90     = [357.3, 372.2, 454.1, 393.9, 443.9, 448.5, 439.5, 549.1, 490.4]

classic_p50 = 1016.3
classic_p90 = 1280.4

out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Figures')
os.makedirs(out_dir, exist_ok=True)

# ── Style ──
plt.rcParams.update({
    'font.size': 13,
    'axes.labelsize': 14,
    'axes.titlesize': 15,
    'legend.fontsize': 11,
    'figure.dpi': 200,
})

# ── Plot 1: p50 ──
fig, ax = plt.subplots(figsize=(7, 4.2))
ax.plot(alpha, p50, 'o-', color='#2563EB', linewidth=2.2, markersize=8,
        markeredgecolor='white', markeredgewidth=1.2)
# annotate each point
for i, (a, v) in enumerate(zip(alpha, p50)):
    ax.annotate(f'{v:.1f}', xy=(a, v), xytext=(0, 10),
                textcoords='offset points', fontsize=9, ha='center',
                fontweight='bold' if i == int(np.argmin(p50)) else 'normal',
                color='#1e40af' if i == int(np.argmin(p50)) else '#555')
ax.set_xlabel(r'$\alpha$  (0 = proximity  $\longrightarrow$  1 = random)')
ax.set_ylabel('Median Block Propagation Delay (ms)')
ax.set_title('p50 Propagation Delay vs. $\\alpha$')
ax.set_xticks(alpha)
ax.set_xlim(-0.05, 1.05)
margin = (max(p50) - min(p50)) * 0.25
ax.set_ylim(min(p50) - margin, max(p50) + margin)
ax.grid(True, alpha=0.3)
fig.tight_layout()
fig.savefig(os.path.join(out_dir, 'alpha_sweep_p50.pdf'), bbox_inches='tight')
fig.savefig(os.path.join(out_dir, 'alpha_sweep_p50.png'), bbox_inches='tight')
print(f'Saved {out_dir}/alpha_sweep_p50.pdf')

# ── Plot 2: p90 ──
fig, ax = plt.subplots(figsize=(7, 4.2))
ax.plot(alpha, p90, 's-', color='#EA580C', linewidth=2.2, markersize=8,
        markeredgecolor='white', markeredgewidth=1.2)
# annotate each point
for i, (a, v) in enumerate(zip(alpha, p90)):
    ax.annotate(f'{v:.1f}', xy=(a, v), xytext=(0, 10),
                textcoords='offset points', fontsize=9, ha='center',
                fontweight='bold' if i == int(np.argmin(p90)) else 'normal',
                color='#9a3412' if i == int(np.argmin(p90)) else '#555')
ax.set_xlabel(r'$\alpha$  (0 = proximity  $\longrightarrow$  1 = random)')
ax.set_ylabel('p90 Propagation Delay (ms)')
ax.set_title('p90 Propagation Delay vs. $\\alpha$')
ax.set_xticks(alpha)
ax.set_xlim(-0.05, 1.05)
margin = (max(p90) - min(p90)) * 0.25
ax.set_ylim(min(p90) - margin, max(p90) + margin)
ax.grid(True, alpha=0.3)
fig.tight_layout()
fig.savefig(os.path.join(out_dir, 'alpha_sweep_p90.pdf'), bbox_inches='tight')
fig.savefig(os.path.join(out_dir, 'alpha_sweep_p90.png'), bbox_inches='tight')
print(f'Saved {out_dir}/alpha_sweep_p90.pdf')

plt.close('all')
print('Done.')
