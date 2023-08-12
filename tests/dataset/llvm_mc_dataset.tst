# UNSUPPORTED: system-windows
# RUN: mkdir -p %t/samples
# RUN: mkdir -p %t/embeddings
# RUN: %mc-data-gen --mode=samples %t/samples
# RUN: %mc-data-gen --mode=graph %t/embeddings
# RUN: %llvm-mc-dataset -o %t/dataset.cbuf %t/embeddings %t/samples 
# RUN: rm -rf %t
