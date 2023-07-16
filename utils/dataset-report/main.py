import lib.structures.mc_dataset_pb2
import argparse
import os.path
from tqdm.auto import tqdm
import matplotlib.pyplot as plt
import networkx as nx
import random
from sklearn.manifold import TSNE
import numpy as np
import matplotlib


colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2"]


def blend_colors(hex_colors, weights):
    rgb_colors = [matplotlib.colors.hex2color(color) for color in hex_colors]
    blended_rgb = [sum(rgb[i]*weight for rgb, weight in zip(rgb_colors, weights)) for i in range(3)]
    return matplotlib.colors.rgb2hex(blended_rgb)


def convert_graph(graph_data):
    graph = nx.DiGraph()
    source = graph_data.source.split("\n")
    node_colors = []
    for n in graph_data.nodes:
        color_weights = [0.] * 7
        if n.is_load:
            color_weights[0] = 1.
        if n.is_store:
            color_weights[1] = 1.
        if n.is_barrier:
            color_weights[2] = 1.
        if n.is_atomic:
            color_weights[3] = 1.
        if n.is_vector:
            color_weights[4] = 1.
        if n.is_compute and not n.is_float:
            color_weights[5] = 1.
        if n.is_float:
            color_weights[6] = 1.
        color_weights = np.array(color_weights)
        if np.sum(color_weights) != 0:
            color_weights = color_weights / np.sum(color_weights)
        blended_color = blend_colors(colors, color_weights)
        node_colors.append(blended_color)

        level = 0 if n.is_virtual_root else 1
        graph.add_node(n.node_id, level=level)
        if n.node_id > 0 and graph_data.has_virtual_root:
            graph.nodes[n.node_id]['instruction'] = source[n.node_id -
                                                           1].strip().replace("\t", "    ")
    for e in graph_data.edges:
        graph.add_edge(getattr(e, 'from', 0), e.to)

    return graph, node_colors


def nudge(pos, x_shift, y_shift):
    return {n: (x + x_shift, y + y_shift) for n, (x, y) in pos.items()}


def plot_graph(graph, node_colors, source_str, filename):
    fig, ax = plt.subplots(1, 2, figsize=(14, 7))
    props = dict(boxstyle='round', facecolor='wheat', alpha=0.5)
    ax[0].axis('off')
    ax[0].text(0.5, 0.5, source_str, ha='left', va='center',
               fontsize=16, transform=ax[0].transAxes, bbox=props)

    labels = nx.get_node_attributes(graph, 'instruction')
    pos = nx.multipartite_layout(graph, subset_key="level")
    pos_nudge = nudge(pos, -0.1, 0.1)

    nx.draw(graph, pos, with_labels=False, node_color=node_colors, ax=ax[1])
    nx.draw_networkx_labels(graph, pos_nudge, labels, font_size=16,
                            ax=ax[1])
    ax[1].set_ylim(tuple(j*1.1 for j in ax[1].get_ylim()))
    if os.path.exists(filename):
        os.remove(filename)
    fig.savefig(filename)
    plt.close(fig)


def get_fig_data(piece):
    source_lines = piece.graph.source.split("\n")
    source_lines = [
        x.strip().replace("\t", "    ") for x in source_lines if x.strip().replace("\t", "") != '']
    source_str = '\n'.join(source_lines)
    source_str += "\n"

    measured_cycles = f"Measured cycles: {piece.metrics.measured_cycles / piece.metrics.measured_num_runs}"
    cache_misses = f"Cache misses: {getattr(piece.metrics, 'total_cache_misses', 0)}"
    context_switches = f"Context switches: {getattr(piece.metrics, 'total_context_switches', 0)}"


    return '\n'.join([source_str, measured_cycles, cache_misses, context_switches])


parser = argparse.ArgumentParser(
    prog="mc-dataset-report",
    description="Analyze the contents of MC dataset",
)

parser.add_argument('filename')
parser.add_argument('-o', '--output', default=None, required=True)

args = parser.parse_args()

assert (os.path.exists(args.filename))
assert (os.path.exists(args.output))
assert (os.path.isdir(args.output))

dataset = lib.structures.mc_dataset_pb2.MCDataset()
dataset.ParseFromString(open(args.filename, 'rb').read())

processed_nodes = set()
nodes = []
colored_nodes = []
features = [0] * 7
diameters = []
cycles = []

for d in tqdm(dataset.data):
    for n in d.graph.nodes:
        if n.is_load:
            features[0] += 1
        if n.is_store:
            features[1] += 1
        if n.is_barrier:
            features[2] += 1
        if n.is_atomic:
            features[3] += 1
        if n.is_vector:
            features[4] += 1
        if n.is_compute and not n.is_float:
            features[5] += 1
        if n.is_float:
            features[6] += 1

        if n.opcode not in processed_nodes and not n.is_virtual_root:
            processed_nodes.add(n.opcode)
            binstr = "{0:b}".format(n.opcode)
            bin = []
            for c in reversed(binstr):
                bin.append(int(c))
            bin.extend([0] * (16 - len(binstr)))
            nodes.append(bin)

            color_weights = [0.] * 7
            if n.is_load:
                color_weights[0] = 1.
            if n.is_store:
                color_weights[1] = 1.
            if n.is_barrier:
                color_weights[2] = 1.
            if n.is_atomic:
                color_weights[3] = 1.
            if n.is_vector:
                color_weights[4] = 1.
            if n.is_compute and not n.is_float:
                color_weights[5] = 1.
            if n.is_float:
                color_weights[6] = 1.
            color_weights = np.array(color_weights)
            if np.sum(color_weights) != 0:
                color_weights = color_weights / np.sum(color_weights)
            blended_color = blend_colors(colors, color_weights)
            colored_nodes.append(blended_color)
    
    graph = convert_graph(d.graph)
    cycles.append(d.metrics.measured_cycles / getattr(d.metrics, 'measured_num_runs', 200))
    # diameters.append(nx.diameter(graph))

# with open(os.path.join(args.output, 'graph_stats.md'), 'w') as f:
#     f.write("|Min distance|Max distance|Mean distance|Median distance|\n")
#     f.write("|---|---|---|---|\n")
#     f.write("|{:.2f}|{:.2f}|{:.2f}|{:.2f}|\n".format(
#         min(diameters), max(diameters), np.mean(diameters), np.median(diameters)))

with open(os.path.join(args.output, 'opcodes.txt'), 'w') as f:
    f.write(str(nodes))


dist_fig, ax = plt.subplots()
ax.barh(["is_load", "is_store", "is_barrier", "is_atomic",
        "is_vector", "is_compute", "is_float"], features)
dist_fig.savefig(os.path.join(args.output, 'distribution.png'))
plt.close(dist_fig)

m_fig, ax = plt.subplots()
cycles = np.array(cycles)
cycles_capped = np.where(cycles < 10000)
max_cycles = np.max(cycles[cycles_capped])
ax.hist(cycles, bins=np.arange(0, max_cycles, 1))
m_fig.savefig(os.path.join(args.output, 'cycles_distribution.png'))
plt.close(m_fig)

m_fig, ax = plt.subplots()
filter = cycles < 50
ax.hist(cycles[filter], bins=np.arange(0, 50, 0.5))
m_fig.savefig(os.path.join(args.output, 'zoom_cycles_distribution.png'))
plt.close(m_fig)

for i in range(5):
    piece = random.choice(dataset.data)
    graph, node_colors = convert_graph(piece.graph)

    source_str = get_fig_data(piece)

    filename = os.path.join(args.output, 'graph-%d.png' % (i + 1))

    plot_graph(graph, node_colors, source_str, filename)

    asm_filename = os.path.join(args.output, f"asm-{i + 1}.s")
    f = open(asm_filename, 'w')
    f.write(source_str)
    f.close()


longest_piece = dataset.data[np.argmax(cycles)]
longest_graph, node_colors = convert_graph(longest_piece.graph)
longest_source_str = get_fig_data(longest_piece)
longest_filename = os.path.join(args.output, 'graph-longest.png')
plot_graph(longest_graph, node_colors, longest_source_str, longest_filename)


tsne = TSNE(n_components=3, random_state=0, learning_rate="auto", init="pca")
nodes_tsne = tsne.fit_transform(np.array(nodes))

fig, ax = plt.subplots(1, 1, figsize=(14, 7))
ax.scatter(nodes_tsne[:, 0], nodes_tsne[:, 1], s=10, c=colored_nodes)
fig.savefig(os.path.join(args.output, 'tsne.png'))
plt.close(fig)
