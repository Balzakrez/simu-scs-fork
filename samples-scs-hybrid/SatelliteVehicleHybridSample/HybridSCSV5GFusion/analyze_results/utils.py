import matplotlib.pyplot as plt

# ============================================================================
# Publication style — Wong (2011) colorblind-safe palette
# ============================================================================
BLUE       = "#0072B2"
ORANGE     = "#E69F00"
GREEN      = "#009E73"
VERMILLION = "#D55E00"
SKY_BLUE   = "#56B4E9"
RED        = "#F44336"
PINK       = "#CC79A7"
BLACK      = "#000000"

# Figure sizes — A4 single-column thesis (width ~6 in = ~15.2 cm)
FIG_SINGLE = (8, 4.5)    # single-panel plots
FIG_DOUBLE = (10, 3.5)   # two-subplot side-by-side
FIG_SQUARE = (5.5, 5.5)  # pie chart

def apply_style():
    plt.rcParams.update({
        "font.family":        "serif",
        "font.size":          11,
        "axes.titlesize":     12,
        "axes.labelsize":     11,
        "legend.fontsize":    9,
        "xtick.labelsize":    9,
        "ytick.labelsize":    9,
        "axes.spines.top":    False,
        "axes.spines.right":  False,
        "axes.grid":          True,
        "grid.linestyle":     "--",
        "grid.alpha":         0.35,
        "figure.dpi":         150,
    })