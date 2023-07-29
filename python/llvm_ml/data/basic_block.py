import numpy as np

class BasicBlock:
    def __init__(self, basic_block, opcode_len=16, undirected=False, use_binary_opcode=True, dtype=np.float32):
        if use_binary_opcode:
            self.nodes = np.zeros((len(basic_block.graph.nodes), opcode_len), dtype=dtype)
        else:
            self.nodes = np.zeros(len(basic_block.graph.nodes), dtype=np.int_)
        self.node_properties = []
        self.has_virtual_root = basic_block.graph.has_virtual_root

        for i in range(len(basic_block.graph.nodes)):
            node = basic_block.graph.nodes[i]
            if use_binary_opcode and not getattr(node, 'is_virtual', False):
                binstr = "{0:b}".format(node.opcode)
                binstr = list(reversed(binstr))
                for j in range(len(binstr)):
                    self.nodes[i, j] = binstr[j]
            elif not use_binary_opcode:
                self.nodes[i] = node.opcode
            self.node_properties.append({
                'is_virtual': getattr(node, 'is_virtual_root', False),
                'is_load': getattr(node, 'is_load', False),
                'is_store': getattr(node, 'is_store', False),
                'is_barrier': getattr(node, 'is_barrier', False),
                'is_atomic': getattr(node, 'is_atomic', False),
                'is_vector': getattr(node, 'is_vector', False),
                'is_compute': getattr(node, 'is_compute', False),
                'is_float': getattr(node, 'is_float', False)
            })
        
        edge_coef = 2 if undirected else 1
        self.edges = np.zeros((2, len(basic_block.graph.edges) * edge_coef), dtype=np.int_)
        for i in range(len(basic_block.graph.edges)):
            edge = basic_block.graph.edges[i]
            edge_from = getattr(edge, 'from', 0)
            edge_to = getattr(edge, 'to', 0)
            self.edges[0, i * edge_coef] = edge_from
            self.edges[1, i * edge_coef] = edge_to
            if undirected:
                self.edges[0, i * edge_coef + 1] = edge_to 
                self.edges[1, i * edge_coef + 1] = edge_from

        self.source = getattr(basic_block.graph, 'source', '')

        self.cycles = basic_block.metrics.measured_cycles / basic_block.metrics.measured_num_runs
        self.cache_misses = basic_block.metrics.workload_cache_misses - basic_block.metrics.noise_cache_misses
        self.context_switches = basic_block.metrics.workload_context_switches - basic_block.metrics.noise_context_switches
