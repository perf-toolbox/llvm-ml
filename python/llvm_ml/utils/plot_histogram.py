import numpy as np
import math
import io

def plot_histogram(x, y, percentile=1, num_grids=20, title="Prediction distribution", xlabel="True", ylabel="Predicted"):
    import matplotlib.pyplot as plt

    if percentile < 1:
        mask = np.where(x < np.percentile(x, percentile))
        x = x[mask]
        y = y[mask]

    plt.figure()
    plt.plot(240, 240)
    plt.title(title)

    max_val = max(np.max(x), np.max(y))
    max_val = math.ceil(max_val)
    grid_step = max_val / num_grids
    plt.hist2d(x, y, bins=(np.arange(0, max_val, grid_step), np.arange(0, max_val, grid_step)))
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    buf = io.BytesIO()
    plt.savefig(buf, format='png')
    buf.seek(0)
    plt.close()
    return buf

