//===--- structures.cpp - Data structures used by llvm-mc-* tools ---------===//
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "llvm-ml/structures/structures.hpp"

#include "capnp/message.h"
#include "capnp/serialize-packed.h"
#include "kj/io.h"

#include <fcntl.h>
#include <filesystem>

namespace fs = std::filesystem;

void llvm_ml::writeToFile(fs::path path, capnp::MessageBuilder &message) {
  int fd = open(path.c_str(), O_CREAT | O_RDWR,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0) {
    perror("Failed to create output file");
    abort();
  }
  kj::AutoCloseFd _(fd);
  capnp::writePackedMessageToFd(fd, message);
}

template <typename T>
int llvm_ml::readFromFile(fs::path path,
                          std::function<void(typename T::Reader &)> cb) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    perror("Failed to create output file");
    return 1;
  }
  kj::AutoCloseFd autoFd(fd);

  capnp::ReaderOptions opts{.traversalLimitInWords = 128 * 1024 * 1024,
                            .nestingLimit = 128};

  capnp::PackedFdMessageReader message(std::move(autoFd), opts);

  typename T::Reader reader = message.getRoot<T>();

  cb(reader);

  return 0;
}

template int llvm_ml::readFromFile<llvm_ml::MCDataset>(
    std::filesystem::path, std::function<void(llvm_ml::MCDataset::Reader &)>);
template int llvm_ml::readFromFile<llvm_ml::MCGraph>(
    std::filesystem::path path,
    std::function<void(llvm_ml::MCGraph::Reader &)>);
template int llvm_ml::readFromFile<llvm_ml::MCMetrics>(
    std::filesystem::path path,
    std::function<void(llvm_ml::MCMetrics::Reader &)>);
