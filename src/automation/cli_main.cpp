#include <iostream>
#include <string>
#include <vector>
#include "automation/cli_dispatcher.h"
#include "engine/fake_chart_engine.h"

static void printUsage() {
  std::cout << "Usage: asteria_cli <command> [options]\n\n"
            << "Commands:\n"
            << "  compute-natal       --birth-event-id <id> [--house-system <sys>] [--zodiac <mode>]\n"
            << "  compute-synastry    --primary-id <id> --secondary-id <id> [--house-system <sys>] [--zodiac <mode>]\n"
            << "  compute-composite   --primary-id <id> --secondary-id <id> [--house-system <sys>] [--zodiac <mode>]\n"
            << "  compute-transit     --birth-event-id <id> --transit-time <datetime> [--house-system <sys>] [--zodiac <mode>]\n"
            << "  export-svg          --birth-event-id <id> --output <path> [--theme <name>]\n"
            << "  export-png          --birth-event-id <id> --output <path> [--width <px>] [--height <px>] [--dpi <dpi>] [--theme <name>]\n"
            << "  resolve-location    --query <text>\n"
            << "\nOptions:\n"
            << "  --house-system    House system (default: Placidus)\n"
            << "  --zodiac          Zodiac mode (default: Tropical)\n"
            << "  --theme           Theme name (default: Textbook Light)\n";
}

static std::string getArg(const std::vector<std::string>& args, const std::string& flag, const std::string& defaultVal = "") {
  for (size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == flag) return args[i + 1];
  }
  return defaultVal;
}

static std::int64_t getIntArg(const std::vector<std::string>& args, const std::string& flag, std::int64_t defaultVal = 0) {
  auto val = getArg(args, flag);
  if (val.empty()) return defaultVal;
  return std::stoll(val);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printUsage();
    return 1;
  }

  std::vector<std::string> args(argv, argv + argc);
  std::string command = args[1];

  // TODO: Support pluggable engine backends. Currently uses FakeChartEngine.
  asteria::engine::FakeChartEngine engine;
  asteria::automation::CliDispatcher dispatcher(engine);

  asteria::automation::CliDispatcher::CliResult result;

  if (command == "compute-natal") {
    auto id = getIntArg(args, "--birth-event-id");
    auto hs = getArg(args, "--house-system", "Placidus");
    auto zm = getArg(args, "--zodiac", "Tropical");
    result = dispatcher.computeNatal(id, hs, zm);
  } else if (command == "compute-synastry") {
    auto pid = getIntArg(args, "--primary-id");
    auto sid = getIntArg(args, "--secondary-id");
    auto hs = getArg(args, "--house-system", "Placidus");
    auto zm = getArg(args, "--zodiac", "Tropical");
    result = dispatcher.computeSynastry(pid, sid, hs, zm);
  } else if (command == "compute-composite") {
    auto pid = getIntArg(args, "--primary-id");
    auto sid = getIntArg(args, "--secondary-id");
    auto hs = getArg(args, "--house-system", "Placidus");
    auto zm = getArg(args, "--zodiac", "Tropical");
    result = dispatcher.computeComposite(pid, sid, hs, zm);
  } else if (command == "compute-transit") {
    auto id = getIntArg(args, "--birth-event-id");
    auto tt = getArg(args, "--transit-time");
    auto hs = getArg(args, "--house-system", "Placidus");
    auto zm = getArg(args, "--zodiac", "Tropical");
    result = dispatcher.computeTransitToNatal(id, tt, hs, zm);
  } else if (command == "export-svg") {
    auto id = getIntArg(args, "--birth-event-id");
    auto out = getArg(args, "--output");
    auto theme = getArg(args, "--theme", "Textbook Light");
    if (out.empty()) {
      std::cerr << "Error: --output is required for export-svg\n";
      return 1;
    }
    result = dispatcher.exportSvg(id, out, theme);
  } else if (command == "export-png") {
    auto id = getIntArg(args, "--birth-event-id");
    auto out = getArg(args, "--output");
    auto w = static_cast<int>(getIntArg(args, "--width", 1000));
    auto h = static_cast<int>(getIntArg(args, "--height", 1000));
    auto d = static_cast<int>(getIntArg(args, "--dpi", 150));
    auto theme = getArg(args, "--theme", "Textbook Light");
    if (out.empty()) {
      std::cerr << "Error: --output is required for export-png\n";
      return 1;
    }
    result = dispatcher.exportPng(id, out, w, h, d, theme);
  } else if (command == "resolve-location") {
    auto query = getArg(args, "--query");
    if (query.empty()) {
      std::cerr << "Error: --query is required for resolve-location\n";
      return 1;
    }
    result = dispatcher.resolveLocation(query);
  } else {
    std::cerr << "Unknown command: " << command << "\n";
    printUsage();
    return 1;
  }

  if (result.success) {
    std::cout << result.outputJson;
    return 0;
  } else {
    std::cerr << "Error: " << result.errorMessage << "\n";
    return 1;
  }
}
