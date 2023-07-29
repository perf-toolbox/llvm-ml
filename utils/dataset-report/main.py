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
import llvm_ml
import llvm_ml.utils


colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2"]


def blend_colors(hex_colors, weights):
    rgb_colors = [matplotlib.colors.hex2color(color) for color in hex_colors]
    blended_rgb = [sum(rgb[i]*weight for rgb, weight in zip(rgb_colors, weights)) for i in range(3)]
    return matplotlib.colors.rgb2hex(blended_rgb)


def convert_graph(basic_block):
    graph = nx.DiGraph()
    source = basic_block.source.split("\n")
    node_colors = []
    node_id = 0
    for n in basic_block.node_properties:
        color_weights = [0.] * 7
        if n['is_load']:
            color_weights[0] = 1.
        if n['is_store']:
            color_weights[1] = 1.
        if n['is_barrier']:
            color_weights[2] = 1.
        if n['is_atomic']:
            color_weights[3] = 1.
        if n['is_vector']:
            color_weights[4] = 1.
        if n['is_compute'] and not n['is_float']:
            color_weights[5] = 1.
        if n['is_float']:
            color_weights[6] = 1.
        color_weights = np.array(color_weights)
        if np.sum(color_weights) != 0:
            color_weights = color_weights / np.sum(color_weights)
        blended_color = blend_colors(colors, color_weights)
        node_colors.append(blended_color)

        level = 0 if n['is_virtual'] else 1
        graph.add_node(node_id, level=level)
        if node_id > 0 and basic_block.has_virtual_root:
            graph.nodes[node_id]['instruction'] = source[node_id - 1].strip().replace("\t", "    ")
        elif not basic_block.has_virtual_root:
            graph.nodes[node_id]['instruction'] = source[node_id].strip().replace("\t", "    ")
        node_id += 1
    for e in np.transpose(basic_block.edges):
        graph.add_edge(e[0], e[1])

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


def get_fig_data(basic_block):
    source_lines = basic_block.source.split("\n")
    source_lines = [
        x.strip().replace("\t", "    ") for x in source_lines if x.strip().replace("\t", "") != '']
    source_str = '\n'.join(source_lines)
    source_str += "\n"

    measured_cycles = f"Measured cycles: {basic_block.cycles}"
    cache_misses = f"Cache misses: {basic_block.cache_misses}"
    context_switches = f"Context switches: {basic_block.context_switches}"

    return '\n'.join([source_str, measured_cycles, cache_misses, context_switches])


def plot_distribution(path, markdown, cycles):
    m_fig, ax = plt.subplots()
    cycles_capped = np.where(cycles < 10000)
    max_cycles = np.max(cycles[cycles_capped])
    ax.hist(cycles, bins=np.arange(0, max_cycles, 1))
    m_fig.savefig(path)
    plt.close(m_fig)
    markdown.write(f"![Cycles Distribution]({os.path.basename(path)})\n\n")


def print_sample(sample_id, basic_block, base_path, markdown):
    asm_path = os.path.join(base_path, f"asm_{sample_id}.s")
    asm_file = open(asm_path, 'w')
    asm_file.write(basic_block.source)
    asm_file.close()

    graph, node_colors = convert_graph(basic_block)

    info_str = get_fig_data(basic_block)

    graph_filename = f"graph-{sample_id}.png"
    graph_path = os.path.join(base_path, graph_filename)

    plot_graph(graph, node_colors, info_str, graph_path)
    markdown.write(f"![Sample graph]({graph_filename})\n\n")


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

basic_blocks = llvm_ml.utils.load_data(args.filename, show_progress=True)
basic_blocks = sorted(basic_blocks, key = lambda bb: bb.cycles)

markdown = open(os.path.join(args.output, "report.md"), 'w')
markdown.write(f"# Report for {args.filename}\n\n")
markdown.write(f"## Basic info\n\n")
markdown.write(f"Total samples: {len(basic_blocks)}\n")
markdown.write(f"Non-zero context switches: {sum(1 if bb.context_switches != 0 else 0 for bb in basic_blocks)}\n")
markdown.write(f"Non-zero cache misses: {sum(1 if bb.cache_misses != 0 else 0 for bb in basic_blocks)}\n")

markdown.write("\n## Cycles distribution\n\n")
cycles = np.array([bb.cycles for bb in basic_blocks])
plot_distribution(os.path.join(args.output, 'cycles_distribution.png'), markdown, cycles)
markdown.write("Zoomed up to 50 cycles\n\n")
cycles_zoom = np.where(cycles <= 50)
plot_distribution(os.path.join(args.output, 'cycles_distribution_zoom.png'), markdown, cycles[cycles_zoom])

markdown.write(f"## 5 random samples\n\n")
for i in range(5):
    bb = random.choice(basic_blocks)
    markdown.write(f"### Sample {i + 1}\n\n")
    markdown.write(f"Cycles: {bb.cycles}\n")
    markdown.write(f"Cache misses: {bb.cache_misses}\n")
    markdown.write(f"Context switches: {bb.context_switches}\n")
    print_sample(i + 1, bb, args.output, markdown)

markdown.write(f"## 10 longest samples\n\n")
num_longest = 1
for bb in basic_blocks[-10:]:
    markdown.write(f"### Sample {num_longest}\n\n")
    markdown.write(f"Cycles: {bb.cycles}\n")
    markdown.write(f"Cache misses: {bb.cache_misses}\n")
    markdown.write(f"Context switches: {bb.context_switches}\n")
    print_sample(f"longest_{num_longest}", bb, args.output, markdown)
    num_longest += 1

markdown.close()

