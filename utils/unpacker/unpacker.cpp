#include "llvm-ml/structures/structures.hpp"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"

#include <capnp/message.h>
#include <cstdio>
#include <filesystem>
#include <indicators/indicators.hpp>

using namespace llvm;
namespace fs = std::filesystem;

static cl::opt<std::string> GraphDirectory(cl::Positional,
                                           cl::desc("<graph dir>"));

static cl::opt<std::string> SourceDirectory(cl::Positional,
                                            cl::desc("<source dir>"));

static cl::opt<std::string> MetricsDirectory(cl::Positional,
                                             cl::desc("<metrics dir>"));

static cl::opt<std::string> InFile("i", cl::desc("in file"), cl::Required);

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  cl::ParseCommandLineOptions(argc, argv, "unpack Machine Code dataset\n");

  fs::path path{InFile.c_str()};

  fs::path graphDir{GraphDirectory.c_str()};
  fs::path srcDir{SourceDirectory.c_str()};
  fs::path metricsDir{MetricsDirectory.c_str()};

  indicators::show_console_cursor(false);
  using namespace indicators;

  llvm_ml::readFromFile<llvm_ml::MCDataset>(
      path, [&](llvm_ml::MCDataset::Reader &reader) {
        BlockProgressBar bar{
            option::BarWidth{80}, option::ForegroundColor{Color::green},
            option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
            option::MaxProgress{reader.getData().size()}};
        for (const auto &datapiece : reader.getData()) {
          std::string id{datapiece.getId().cStr()};
          fs::path graphPath = graphDir / fs::path{id + ".cbuf"};
          fs::path srcPath = srcDir / fs::path{id + ".s"};
          fs::path metricsPath = metricsDir / fs::path{id + ".cbuf"};

          std::FILE *srcFile = std::fopen(srcPath.c_str(), "w");
          std::fwrite(datapiece.getGraph().getSource().cStr(), sizeof(char),
                      datapiece.getGraph().getSource().size(), srcFile);
          std::fclose(srcFile);

          capnp::MallocMessageBuilder graphMsg;
          graphMsg.setRoot<llvm_ml::MCGraph::Reader>(datapiece.getGraph());
          llvm_ml::writeToFile(graphPath, graphMsg);

          capnp::MallocMessageBuilder metricsMsg;
          metricsMsg.setRoot<llvm_ml::MCMetrics::Reader>(
              datapiece.getMetrics());
          llvm_ml::writeToFile(metricsPath, metricsMsg);

          bar.tick();
        }
      });
  indicators::show_console_cursor(true);
}