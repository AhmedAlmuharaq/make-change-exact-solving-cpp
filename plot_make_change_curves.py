"""Create the three complexity curves used in our Make Change report."""

from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


ROOT = Path(__file__).resolve().parent
plt.rcParams.update(
    {
        "font.family": "DejaVu Sans",
        "font.size": 10,
        "axes.titlesize": 13,
        "axes.labelsize": 10,
        "legend.fontsize": 9,
        "figure.dpi": 160,
    }
)


def save_figure(name: str) -> None:
    plt.tight_layout()
    plt.savefig(ROOT / name, bbox_inches="tight", facecolor="white")
    plt.close()


greedy = pd.read_csv(ROOT / "greedy_complexity.csv")
recursive = pd.read_csv(ROOT / "recursive_complexity.csv")

# Curve 1: the greedy scan performs one decision per denomination.
plt.figure(figsize=(7.0, 4.1))
plt.plot(
    greedy["denominations"],
    greedy["ordered_operations"],
    marker="o",
    linewidth=2,
    label="Measured greedy operations",
)
plt.plot(
    greedy["denominations"],
    greedy["denominations"],
    linestyle="--",
    linewidth=1.5,
    label="Linear reference y = n",
)
plt.title("Greedy algorithm linear growth")
plt.xlabel("Number of coin denominations")
plt.ylabel("Number of greedy decisions")
plt.grid(True, alpha=0.25)
plt.legend()
save_figure("curve_greedy_linear.png")

# Curve 2: recursive enumeration grows very quickly as the tree gains levels.
plt.figure(figsize=(7.0, 4.1))
plt.semilogy(
    recursive["coin_types"],
    recursive["all_states"],
    marker="o",
    linewidth=2,
    label="Explored recursive states",
)
plt.semilogy(
    recursive["coin_types"],
    recursive["valid_solutions"],
    marker="s",
    linewidth=1.8,
    label="Valid solutions",
)
plt.title("Growth of complete recursive enumeration")
plt.xlabel("Number of coin types")
plt.ylabel("Count on logarithmic scale")
plt.grid(True, which="both", alpha=0.25)
plt.legend()
save_figure("curve_recursive_growth.png")

# Curve 3: a valid incumbent and an admissible lower bound remove branches
# that cannot improve our best solution.
plt.figure(figsize=(7.0, 4.1))
plt.semilogy(
    recursive["coin_types"],
    recursive["all_states"],
    marker="o",
    linewidth=2,
    label="Without cuts",
)
plt.semilogy(
    recursive["coin_types"],
    recursive["cut_states"],
    marker="s",
    linewidth=2,
    label="With branch-and-bound cuts",
)
plt.title("Effect of cuts on the search space")
plt.xlabel("Number of coin types")
plt.ylabel("Explored states on logarithmic scale")
plt.grid(True, which="both", alpha=0.25)
plt.legend()
save_figure("curve_cut_effect.png")

print("Created the three Make Change curves.")
