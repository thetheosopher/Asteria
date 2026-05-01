#include <algorithm>
#include <array>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "automation/cli_dispatcher.h"
#include "automation/cli_json_input.h"
#include "core/report_service.h"
#include "engine/astrolog_embedded_engine.h"
#include "util/atlas_service.h"

namespace {

using asteria::automation::CliDispatcher;
using asteria::automation::CliJsonValue;

using JsonPath = std::vector<std::string>;

std::string getArg(const std::vector<std::string>& args,
                   const std::string& flag,
                   const std::string& defaultValue = "");

std::string getArgWithAliases(const std::vector<std::string>& args,
                              const std::vector<std::string>& flags,
                              const std::string& defaultValue = "");

bool hasArg(const std::vector<std::string>& args, const std::string& flag);

std::optional<std::int64_t> parseInt64(const std::string& value);

std::string toLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    if (ch >= 'A' && ch <= 'Z') {
      return static_cast<char>(ch - 'A' + 'a');
    }
    return static_cast<char>(ch);
  });
  return value;
}

std::string trim(std::string value) {
  auto notSpace = [](unsigned char ch) {
    return ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n';
  };
  auto begin = std::find_if(value.begin(), value.end(), notSpace);
  auto end = std::find_if(value.rbegin(), value.rend(), notSpace).base();
  if (begin >= end) return "";
  return std::string(begin, end);
}

std::vector<std::string> splitCsv(const std::string& value) {
  std::vector<std::string> parts;
  std::string current;
  for (char ch : value) {
    if (ch == ',') {
      const std::string item = trim(current);
      if (!item.empty()) parts.push_back(item);
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  const std::string item = trim(current);
  if (!item.empty()) parts.push_back(item);
  return parts;
}

const CliJsonValue* findFirstJsonPath(const CliJsonValue* root,
                                      const std::vector<JsonPath>& candidatePaths) {
  if (root == nullptr) {
    return nullptr;
  }
  for (const auto& path : candidatePaths) {
    if (const auto* value = asteria::automation::findCliJsonPath(*root, path)) {
      return value;
    }
  }
  return nullptr;
}

std::optional<std::string> getJsonString(const CliJsonValue* root,
                                         const std::vector<JsonPath>& candidatePaths) {
  const auto* value = findFirstJsonPath(root, candidatePaths);
  if (value == nullptr) {
    return std::nullopt;
  }
  return asteria::automation::cliJsonStringValue(*value);
}

std::optional<double> getJsonNumber(const CliJsonValue* root,
                                    const std::vector<JsonPath>& candidatePaths) {
  const auto* value = findFirstJsonPath(root, candidatePaths);
  if (value == nullptr) {
    return std::nullopt;
  }
  return asteria::automation::cliJsonNumberValue(*value);
}

std::optional<bool> getJsonBool(const CliJsonValue* root,
                                const std::vector<JsonPath>& candidatePaths) {
  const auto* value = findFirstJsonPath(root, candidatePaths);
  if (value == nullptr) {
    return std::nullopt;
  }
  if (const bool* boolValue = value->asBool()) {
    return *boolValue;
  }
  if (const auto stringValue = asteria::automation::cliJsonStringValue(*value)) {
    const std::string lowered = toLower(trim(*stringValue));
    if (lowered == "true" || lowered == "1" || lowered == "yes") return true;
    if (lowered == "false" || lowered == "0" || lowered == "no") return false;
  }
  return std::nullopt;
}

bool hasJsonPath(const CliJsonValue* root, const std::vector<JsonPath>& candidatePaths) {
  return findFirstJsonPath(root, candidatePaths) != nullptr;
}

bool getBoolOption(const std::vector<std::string>& args,
                   const std::string& trueFlag,
                   const std::string& falseFlag,
                   const CliJsonValue* inputRoot,
                   const std::vector<JsonPath>& jsonPaths,
                   bool defaultValue) {
  if (hasArg(args, trueFlag)) return true;
  if (hasArg(args, falseFlag)) return false;
  if (auto jsonValue = getJsonBool(inputRoot, jsonPaths)) return *jsonValue;
  return defaultValue;
}

std::string getOptionString(const std::vector<std::string>& args,
                            const std::vector<std::string>& flags,
                            const CliJsonValue* inputRoot,
                            const std::vector<JsonPath>& jsonPaths,
                            const std::string& defaultValue = "") {
  const std::string argValue = getArgWithAliases(args, flags, "");
  if (!argValue.empty()) {
    return argValue;
  }
  if (auto jsonValue = getJsonString(inputRoot, jsonPaths)) {
    return *jsonValue;
  }
  return defaultValue;
}

std::int64_t getIntOption(const std::vector<std::string>& args,
                          const std::string& flag,
                          const CliJsonValue* inputRoot,
                          const std::vector<JsonPath>& jsonPaths,
                          std::int64_t defaultValue = 0) {
  const auto value = getArg(args, flag);
  if (!value.empty()) {
    const auto parsed = parseInt64(value);
    if (!parsed) {
      throw std::runtime_error(std::string("Invalid integer for ") + flag + ": " + value);
    }
    return *parsed;
  }
  if (auto jsonValue = getJsonNumber(inputRoot, jsonPaths)) {
    return static_cast<std::int64_t>(*jsonValue);
  }
  return defaultValue;
}

std::vector<JsonPath> birthJsonPaths(const std::string& prefix, const std::string& suffix) {
  std::vector<std::string> keys;
  if (suffix == "datetime") {
    keys = {"datetime", "dateTime"};
  } else if (suffix == "latitude") {
    keys = {"latitude", "latitudeDegrees"};
  } else if (suffix == "longitude") {
    keys = {"longitude", "longitudeDegrees"};
  } else if (suffix == "timezone") {
    keys = {"timezone", "timezoneHours"};
  } else if (suffix == "dst") {
    keys = {"dst", "dstHours"};
  } else {
    keys = {suffix};
  }

  std::vector<JsonPath> paths;
  paths.reserve(keys.size());
  for (const auto& key : keys) {
    paths.push_back(JsonPath{prefix, key});
  }
  return paths;
}

std::vector<std::string> parseJsonStringList(const CliJsonValue& value,
                                             const std::string& description,
                                             std::string& errorMessage) {
  if (const auto text = asteria::automation::cliJsonStringValue(value)) {
    return splitCsv(*text);
  }

  const auto* array = value.asArray();
  if (array == nullptr) {
    errorMessage = description + " in JSON input must be a CSV string or array of strings.";
    return {};
  }

  std::vector<std::string> items;
  items.reserve(array->size());
  for (const auto& entry : *array) {
    const auto text = asteria::automation::cliJsonStringValue(entry);
    if (!text) {
      errorMessage = description + " in JSON input must contain only strings.";
      return {};
    }
    const std::string item = trim(*text);
    if (!item.empty()) {
      items.push_back(item);
    }
  }
  return items;
}

std::vector<std::pair<std::string, double>> parseJsonOrbOverrides(const CliJsonValue& value,
                                                                  std::string& errorMessage) {
  const auto* object = value.asObject();
  if (object == nullptr) {
    errorMessage = "orbs in JSON input must be an object like {\"Conjunction\": 1.0}.";
    return {};
  }

  std::vector<std::pair<std::string, double>> overrides;
  overrides.reserve(object->size());
  for (const auto& [aspectName, orbValue] : *object) {
    const auto orbDegrees = asteria::automation::cliJsonNumberValue(orbValue);
    if (!orbDegrees || *orbDegrees <= 0.0) {
      errorMessage = "All JSON orb overrides must be positive numeric values.";
      return {};
    }
    overrides.emplace_back(aspectName, *orbDegrees);
  }
  return overrides;
}

void printUsage() {
  std::cout
      << "Usage: asteria_cli <command> [options]\n\n"
      << "Commands:\n"
      << "  compute-natal\n"
      << "      --primary-datetime <datetime> --primary-latitude <deg> --primary-longitude <deg> --primary-timezone <hours>\n"
      << "      [--primary-dst <hours>] [--house-system <sys>] [--zodiac <mode>]\n"
      << "  compute-synastry\n"
      << "      --primary-datetime <datetime> --primary-latitude <deg> --primary-longitude <deg> --primary-timezone <hours>\n"
      << "      --secondary-datetime <datetime> --secondary-latitude <deg> --secondary-longitude <deg> --secondary-timezone <hours>\n"
      << "      [--primary-dst <hours>] [--secondary-dst <hours>] [--house-system <sys>] [--zodiac <mode>]\n"
      << "  compute-composite\n"
      << "      same input flags as compute-synastry\n"
      << "  compute-transit\n"
      << "      --primary-datetime <datetime> --primary-latitude <deg> --primary-longitude <deg> --primary-timezone <hours>\n"
      << "      --transit-datetime <datetime> [--transit-latitude <deg>] [--transit-longitude <deg>] [--transit-timezone <hours>]\n"
      << "      [--primary-dst <hours>] [--transit-dst <hours>] [--house-system <sys>] [--zodiac <mode>]\n"
      << "  generate-transit-timeline\n"
      << "      --natal-datetime <datetime> --natal-latitude <deg> --natal-longitude <deg> --natal-timezone <hours>\n"
      << "      [--natal-dst <hours>] [--subject <name>] [--start <datetime>] [--years <value>]\n"
      << "      [--transit-planets <csv>] [--aspects <csv>] [--orb <Aspect=deg>]... [--output <path>]\n"
      << "  export-svg\n"
      << "  export-png\n"
      << "      --chart-type natal|synastry|composite|transit --output <path>\n"
      << "      use the same input flags as the matching compute command, plus [--theme <name>] and for PNG [--width <px>] [--height <px>] [--dpi <dpi>]\n"
      << "  export-ai-report-pdf\n"
      << "      --chart-type natal|synastry|composite|transit --output <path> [--interpretation-file <md>]\n"
      << "      use the same input flags as the matching compute command, plus [--template client|study-notes|compact-one-page|archive-copy] [--vector-chart] [--archival]\n"
      << "  resolve-location --query <text>\n"
      << "\nOptions:\n"
      << "  --input           Load command options from a JSON object file; inline flags override file values\n"
      << "  --house-system    House system (default: Placidus)\n"
      << "  --zodiac          Zodiac mode (default: Tropical)\n"
      << "  --theme           Theme name (default: Textbook Light)\n";
}

std::string getArg(const std::vector<std::string>& args,
                   const std::string& flag,
                   const std::string& defaultValue) {
  for (std::size_t index = 0; index + 1 < args.size(); ++index) {
    if (args[index] == flag) return args[index + 1];
  }
  return defaultValue;
}

std::vector<std::string> getAllArgs(const std::vector<std::string>& args,
                                    const std::string& flag) {
  std::vector<std::string> values;
  for (std::size_t index = 0; index + 1 < args.size(); ++index) {
    if (args[index] == flag) values.push_back(args[index + 1]);
  }
  return values;
}

bool hasArg(const std::vector<std::string>& args, const std::string& flag) {
  return std::find(args.begin(), args.end(), flag) != args.end();
}

std::optional<double> parseDouble(const std::string& value) {
  try {
    std::size_t processed = 0;
    const double parsed = std::stod(value, &processed);
    if (processed != value.size()) return std::nullopt;
    return parsed;
  } catch (...) {
    return std::nullopt;
  }
}

std::string buildFlag(const std::string& prefix, const std::string& suffix) {
  if (prefix.empty()) return "--" + suffix;
  return "--" + prefix + "-" + suffix;
}

std::string getArgWithAliases(const std::vector<std::string>& args,
                              const std::vector<std::string>& flags,
                              const std::string& defaultValue) {
  for (const auto& flag : flags) {
    const std::string value = getArg(args, flag);
    if (!value.empty()) return value;
  }
  return defaultValue;
}

std::optional<std::int64_t> parseInt64(const std::string& value) {
  try {
    std::size_t processed = 0;
    const std::int64_t parsed = std::stoll(value, &processed);
    if (processed != value.size()) return std::nullopt;
    return parsed;
  } catch (...) {
    return std::nullopt;
  }
}

std::int64_t getIntArg(const std::vector<std::string>& args,
                       const std::string& flag,
                       std::int64_t defaultValue = 0) {
  const auto value = getArg(args, flag);
  if (value.empty()) return defaultValue;
  const auto parsed = parseInt64(value);
  if (!parsed) {
    throw std::runtime_error(std::string("Invalid integer for ") + flag + ": " + value);
  }
  return *parsed;
}

std::string defaultTimelineStartDateTime() {
  std::time_t now = std::time(nullptr);
  std::tm localTime{};
#ifdef _WIN32
  localtime_s(&localTime, &now);
#else
  localTime = *std::localtime(&now);
#endif
  localTime.tm_mon -= 6;
  std::mktime(&localTime);

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M", &localTime);
  return buffer;
}

std::optional<std::string> canonicalTransitObject(const std::string& token) {
  const std::string tokenKey = toLower(trim(token));
  for (const auto& name : asteria::core::TransitReportService::defaultTransitObjects()) {
    if (toLower(name) == tokenKey) {
      return name;
    }
  }
  return std::nullopt;
}

std::optional<asteria::core::TransitReportService::AspectRule> canonicalAspectRule(
    const std::string& token) {
  const std::string tokenKey = toLower(trim(token));
  for (const auto& rule : asteria::core::TransitReportService::defaultAspectRules()) {
    if (toLower(rule.aspectType) == tokenKey) {
      return rule;
    }
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> findExistingPath(
    const std::vector<std::filesystem::path>& candidates) {
  for (const auto& candidate : candidates) {
    if (!candidate.empty() && std::filesystem::exists(candidate)) {
      return candidate;
    }
  }
  return std::nullopt;
}

std::optional<std::filesystem::path> findAstrologDataPath() {
  const std::filesystem::path current = std::filesystem::current_path();
  std::vector<std::filesystem::path> candidates{
      current / "third_party" / "astrolog",
      current / "astrolog_data",
  };
#ifdef ASTERIA_SOURCE_DIR
  const std::filesystem::path sourceDir = ASTERIA_SOURCE_DIR;
  candidates.push_back(sourceDir / "third_party" / "astrolog");
  candidates.push_back(sourceDir / "build" / "default" / "third_party" / "astrolog");
#endif
  return findExistingPath(candidates);
}

std::optional<std::filesystem::path> findAtlasPath() {
  const std::filesystem::path current = std::filesystem::current_path();
  std::vector<std::filesystem::path> candidates{
      current / "assets" / "atlasbig.as",
      current / "atlasbig.as",
  };
#ifdef ASTERIA_SOURCE_DIR
  const std::filesystem::path sourceDir = ASTERIA_SOURCE_DIR;
  candidates.push_back(sourceDir / "assets" / "atlasbig.as");
#endif
  return findExistingPath(candidates);
}

std::optional<std::filesystem::path> findTimezonePath() {
  const std::filesystem::path current = std::filesystem::current_path();
  std::vector<std::filesystem::path> candidates{
      current / "third_party" / "astrolog" / "timezone.as",
      current / "timezone.as",
  };
#ifdef ASTERIA_SOURCE_DIR
  const std::filesystem::path sourceDir = ASTERIA_SOURCE_DIR;
  candidates.push_back(sourceDir / "third_party" / "astrolog" / "timezone.as");
#endif
  return findExistingPath(candidates);
}

bool loadAtlasService(asteria::util::AtlasService& atlas, std::string& errorMessage) {
  const auto atlasPath = findAtlasPath();
  const auto timezonePath = findTimezonePath();
  if (!atlasPath || !timezonePath) {
    errorMessage = "Could not locate atlasbig.as and timezone.as for location resolution.";
    return false;
  }
  if (!atlas.loadAtlas(atlasPath->string())) {
    errorMessage = "Failed to load atlas data from " + atlasPath->string();
    return false;
  }
  if (!atlas.loadTimezones(timezonePath->string())) {
    errorMessage = "Failed to load timezone data from " + timezonePath->string();
    return false;
  }
  return true;
}

std::optional<CliDispatcher::BirthInputOptions> parseBirthInput(
    const std::vector<std::string>& args,
    const CliJsonValue* inputRoot,
    const std::string& prefix,
    std::string& errorMessage,
    bool requireLocation = true,
    const std::optional<CliDispatcher::BirthInputOptions>& defaults = std::nullopt,
    const std::vector<std::string>& dateTimeAliases = {},
    const std::vector<JsonPath>& dateTimeJsonAliases = {}) {
  CliDispatcher::BirthInputOptions options = defaults.value_or(CliDispatcher::BirthInputOptions{});

  std::vector<std::string> dateTimeFlags{buildFlag(prefix, "datetime")};
  dateTimeFlags.insert(dateTimeFlags.end(), dateTimeAliases.begin(), dateTimeAliases.end());
  auto dateTimePaths = birthJsonPaths(prefix, "datetime");
  dateTimePaths.insert(dateTimePaths.end(), dateTimeJsonAliases.begin(), dateTimeJsonAliases.end());
  options.dateTime = getOptionString(args, dateTimeFlags, inputRoot, dateTimePaths, "");
  if (options.dateTime.empty()) {
    errorMessage = dateTimeFlags.front() + " is required.";
    return std::nullopt;
  }

  auto parseNumericFlag = [&](const std::string& suffix,
                              double& target,
                              bool required,
                              std::optional<double> defaultValue) -> bool {
    const std::string flag = buildFlag(prefix, suffix);
    const std::string rawValue = getArg(args, flag);
    if (!rawValue.empty()) {
      const auto parsed = parseDouble(rawValue);
      if (!parsed) {
        errorMessage = flag + " must be numeric.";
        return false;
      }
      target = *parsed;
      return true;
    }

    if (auto jsonValue = getJsonNumber(inputRoot, birthJsonPaths(prefix, suffix))) {
      target = *jsonValue;
      return true;
    }

    if (defaultValue) {
      target = *defaultValue;
      return true;
    }
    if (!required) {
      return true;
    }
    errorMessage = flag + " is required.";
    return false;
  };

  const auto defaultLatitude = defaults ? std::optional<double>(defaults->latitudeDegrees) : std::nullopt;
  const auto defaultLongitude = defaults ? std::optional<double>(defaults->longitudeDegrees) : std::nullopt;
  const auto defaultTimezone = defaults ? std::optional<double>(defaults->timezoneHours) : std::nullopt;
  const auto defaultDst = defaults ? std::optional<double>(defaults->dstHours) : std::optional<double>(0.0);

  if (!parseNumericFlag("latitude", options.latitudeDegrees, requireLocation, defaultLatitude)) {
    return std::nullopt;
  }
  if (!parseNumericFlag("longitude", options.longitudeDegrees, requireLocation, defaultLongitude)) {
    return std::nullopt;
  }
  if (!parseNumericFlag("timezone", options.timezoneHours, requireLocation, defaultTimezone)) {
    return std::nullopt;
  }
  if (!parseNumericFlag("dst", options.dstHours, false, defaultDst)) {
    return std::nullopt;
  }

  return options;
}

std::optional<CliDispatcher::NatalChartOptions> parseNatalChartOptions(
    const std::vector<std::string>& args,
    const CliJsonValue* inputRoot,
    std::string& errorMessage) {
  CliDispatcher::NatalChartOptions options;
  options.houseSystem = getOptionString(
      args,
      {"--house-system"},
      inputRoot,
      {JsonPath{"houseSystem"}, JsonPath{"house-system"}},
      "Placidus");
  options.zodiacMode = getOptionString(
      args,
      {"--zodiac"},
      inputRoot,
      {JsonPath{"zodiacMode"}, JsonPath{"zodiac"}},
      "Tropical");
  auto primary = parseBirthInput(args, inputRoot, "primary", errorMessage);
  if (!primary) return std::nullopt;
  options.primary = *primary;
  return options;
}

std::optional<CliDispatcher::ComparisonChartOptions> parseComparisonChartOptions(
    const std::vector<std::string>& args,
    const CliJsonValue* inputRoot,
    std::string& errorMessage) {
  CliDispatcher::ComparisonChartOptions options;
  options.houseSystem = getOptionString(
      args,
      {"--house-system"},
      inputRoot,
      {JsonPath{"houseSystem"}, JsonPath{"house-system"}},
      "Placidus");
  options.zodiacMode = getOptionString(
      args,
      {"--zodiac"},
      inputRoot,
      {JsonPath{"zodiacMode"}, JsonPath{"zodiac"}},
      "Tropical");
  auto primary = parseBirthInput(args, inputRoot, "primary", errorMessage);
  if (!primary) return std::nullopt;
  auto secondary = parseBirthInput(args, inputRoot, "secondary", errorMessage);
  if (!secondary) return std::nullopt;
  options.primary = *primary;
  options.secondary = *secondary;
  return options;
}

std::optional<CliDispatcher::TransitChartOptions> parseTransitChartOptions(
    const std::vector<std::string>& args,
  const CliJsonValue* inputRoot,
    std::string& errorMessage) {
  CliDispatcher::TransitChartOptions options;
  options.houseSystem = getOptionString(
    args,
    {"--house-system"},
    inputRoot,
    {JsonPath{"houseSystem"}, JsonPath{"house-system"}},
    "Placidus");
  options.zodiacMode = getOptionString(
    args,
    {"--zodiac"},
    inputRoot,
    {JsonPath{"zodiacMode"}, JsonPath{"zodiac"}},
    "Tropical");
  auto primary = parseBirthInput(args, inputRoot, "primary", errorMessage);
  if (!primary) return std::nullopt;
  auto transit = parseBirthInput(args,
                 inputRoot,
                 "transit",
                 errorMessage,
                 false,
                 *primary,
                 {"--transit-time"},
                 {JsonPath{"transit", "time"}, JsonPath{"transitTime"}});
  if (!transit) return std::nullopt;
  options.primary = *primary;
  options.transit = *transit;
  return options;
}

std::optional<CliDispatcher::ExportChartType> parseExportChartType(const std::string& value) {
  const std::string lowered = toLower(trim(value));
  if (lowered.empty() || lowered == "natal") {
    return CliDispatcher::ExportChartType::Natal;
  }
  if (lowered == "synastry") {
    return CliDispatcher::ExportChartType::Synastry;
  }
  if (lowered == "composite") {
    return CliDispatcher::ExportChartType::Composite;
  }
  if (lowered == "transit" || lowered == "transit-to-natal") {
    return CliDispatcher::ExportChartType::TransitToNatal;
  }
  return std::nullopt;
}

std::optional<asteria::core::PdfReportTemplate> parsePdfReportTemplate(const std::string& value) {
  const std::string lowered = toLower(trim(value));
  if (lowered.empty() || lowered == "client" || lowered == "client-report" || lowered == "client_report") {
    return asteria::core::PdfReportTemplate::ClientReport;
  }
  if (lowered == "study" || lowered == "study-notes" || lowered == "study_notes") {
    return asteria::core::PdfReportTemplate::StudyNotes;
  }
  if (lowered == "compact" || lowered == "compact-one-page" || lowered == "compact_one_page") {
    return asteria::core::PdfReportTemplate::CompactOnePage;
  }
  if (lowered == "archive" || lowered == "archive-copy" || lowered == "archive_copy") {
    return asteria::core::PdfReportTemplate::ArchiveCopy;
  }
  return std::nullopt;
}

std::optional<std::string> readTextFile(const std::string& path, std::string& errorMessage) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    errorMessage = "Failed to open text file: " + path;
    return std::nullopt;
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  if (!input.good() && !input.eof()) {
    errorMessage = "Failed to read text file: " + path;
    return std::nullopt;
  }
  return contents.str();
}

std::optional<CliDispatcher::ExportChartOptions> parseExportChartOptions(
    const std::vector<std::string>& args,
    const CliJsonValue* inputRoot,
    bool png,
    std::string& errorMessage) {
  CliDispatcher::ExportChartOptions options;
  options.outputPath = getOptionString(
      args,
      {"--output"},
      inputRoot,
      {JsonPath{"output"}, JsonPath{"outputPath"}},
      "");
  if (options.outputPath.empty()) {
    errorMessage = "--output is required for export.";
    return std::nullopt;
  }
  options.themeName = getOptionString(
      args,
      {"--theme"},
      inputRoot,
      {JsonPath{"theme"}, JsonPath{"themeName"}},
      "Textbook Light");
  options.houseSystem = getOptionString(
      args,
      {"--house-system"},
      inputRoot,
      {JsonPath{"houseSystem"}, JsonPath{"house-system"}},
      "Placidus");
  options.zodiacMode = getOptionString(
      args,
      {"--zodiac"},
      inputRoot,
      {JsonPath{"zodiacMode"}, JsonPath{"zodiac"}},
      "Tropical");
  if (png) {
    options.widthPx = static_cast<int>(
        getIntOption(args, "--width", inputRoot, {JsonPath{"width"}, JsonPath{"widthPx"}}, 1000));
    options.heightPx = static_cast<int>(
        getIntOption(args, "--height", inputRoot, {JsonPath{"height"}, JsonPath{"heightPx"}}, 1000));
    options.dpi = static_cast<int>(
        getIntOption(args, "--dpi", inputRoot, {JsonPath{"dpi"}}, 150));
  }

  const auto chartType = parseExportChartType(getOptionString(
      args,
      {"--chart-type"},
      inputRoot,
      {JsonPath{"chartType"}, JsonPath{"chart-type"}},
      "natal"));
  if (!chartType) {
    errorMessage = "--chart-type must be natal, synastry, composite, or transit.";
    return std::nullopt;
  }
  options.chartType = *chartType;

  switch (options.chartType) {
    case CliDispatcher::ExportChartType::Natal: {
      auto primary = parseBirthInput(args, inputRoot, "primary", errorMessage);
      if (!primary) return std::nullopt;
      options.primary = *primary;
      break;
    }
    case CliDispatcher::ExportChartType::Synastry:
    case CliDispatcher::ExportChartType::Composite: {
      auto primary = parseBirthInput(args, inputRoot, "primary", errorMessage);
      if (!primary) return std::nullopt;
      auto secondary = parseBirthInput(args, inputRoot, "secondary", errorMessage);
      if (!secondary) return std::nullopt;
      options.primary = *primary;
      options.secondary = *secondary;
      break;
    }
    case CliDispatcher::ExportChartType::TransitToNatal: {
      auto primary = parseBirthInput(args, inputRoot, "primary", errorMessage);
      if (!primary) return std::nullopt;
      auto transit = parseBirthInput(args,
                                     inputRoot,
                                     "transit",
                                     errorMessage,
                                     false,
                                     *primary,
                                     {"--transit-time"},
                                     {JsonPath{"transit", "time"}, JsonPath{"transitTime"}});
      if (!transit) return std::nullopt;
      options.primary = *primary;
      options.transit = *transit;
      break;
    }
  }

  return options;
}

std::optional<CliDispatcher::PdfReportOptions> parsePdfReportOptions(
    const std::vector<std::string>& args,
    const CliJsonValue* inputRoot,
    std::string& errorMessage) {
  auto exportOptions = parseExportChartOptions(args, inputRoot, true, errorMessage);
  if (!exportOptions) return std::nullopt;

  CliDispatcher::PdfReportOptions options;
  options.chart = *exportOptions;
  options.title = getOptionString(
      args,
      {"--title"},
      inputRoot,
      {JsonPath{"title"}, JsonPath{"reportTitle"}},
      "");
  options.sourceLabel = getOptionString(
      args,
      {"--source"},
      inputRoot,
      {JsonPath{"source"}, JsonPath{"sourceLabel"}},
      "");
  options.modelLabel = getOptionString(
      args,
      {"--model"},
      inputRoot,
      {JsonPath{"model"}, JsonPath{"modelLabel"}},
      "CLI");

  const std::string interpretationFile = getOptionString(
      args,
      {"--interpretation-file"},
      inputRoot,
      {JsonPath{"interpretationFile"}, JsonPath{"interpretation-file"}},
      "");
  if (!interpretationFile.empty()) {
    auto loaded = readTextFile(interpretationFile, errorMessage);
    if (!loaded) return std::nullopt;
    options.interpretationMarkdown = *loaded;
  }

  const std::string inlineInterpretation = getOptionString(
      args,
      {"--interpretation"},
      inputRoot,
      {JsonPath{"interpretation"}, JsonPath{"interpretationMarkdown"}, JsonPath{"aiInterpretation"}},
      "");
  if (!inlineInterpretation.empty()) {
    options.interpretationMarkdown = inlineInterpretation;
  }

  options.includeBuiltIn = getBoolOption(
      args,
      "--include-built-in",
      "--no-built-in",
      inputRoot,
      {JsonPath{"includeBuiltIn"}, JsonPath{"include-built-in"}},
      true);
  options.archivalMode = getBoolOption(
      args,
      "--archival",
      "--no-archival",
      inputRoot,
      {JsonPath{"archival"}, JsonPath{"archivalMode"}},
      false);
  options.preferVectorChart = getBoolOption(
      args,
      "--vector-chart",
      "--raster-chart",
      inputRoot,
      {JsonPath{"vectorChart"}, JsonPath{"preferVectorChart"}},
      false);

  const std::string templateName = getOptionString(
      args,
      {"--template"},
      inputRoot,
      {JsonPath{"template"}, JsonPath{"reportTemplate"}},
      "client");
  auto reportTemplate = parsePdfReportTemplate(templateName);
  if (!reportTemplate) {
    errorMessage = "--template must be client, study-notes, compact-one-page, or archive-copy.";
    return std::nullopt;
  }
  options.reportTemplate = *reportTemplate;

  if (options.interpretationMarkdown.empty() && !options.includeBuiltIn) {
    errorMessage = "export-ai-report-pdf requires --interpretation, --interpretation-file, or built-in interpretation enabled.";
    return std::nullopt;
  }

  return options;
}

std::optional<CliDispatcher::TransitTimelineOptions> parseTransitTimelineOptions(
    const std::vector<std::string>& args,
    const CliJsonValue* inputRoot,
    std::string& errorMessage) {
  CliDispatcher::TransitTimelineOptions options;
  options.subjectName = getOptionString(
      args,
      {"--subject"},
      inputRoot,
      {JsonPath{"subject"}, JsonPath{"subjectName"}},
      "");
  options.startDateTime = getOptionString(
      args,
      {"--start"},
      inputRoot,
      {JsonPath{"start"}, JsonPath{"startDateTime"}},
      defaultTimelineStartDateTime());
  options.outputPath = getOptionString(
      args,
      {"--output"},
      inputRoot,
      {JsonPath{"output"}, JsonPath{"outputPath"}},
      "");
  options.houseSystem = getOptionString(
      args,
      {"--house-system"},
      inputRoot,
      {JsonPath{"houseSystem"}, JsonPath{"house-system"}},
      "Placidus");
  options.zodiacMode = getOptionString(
      args,
      {"--zodiac"},
      inputRoot,
      {JsonPath{"zodiacMode"}, JsonPath{"zodiac"}},
      "Tropical");

  auto natal = parseBirthInput(args, inputRoot, "natal", errorMessage);
  if (!natal) return std::nullopt;
  options.natal = *natal;

  if (const auto yearsText = getArg(args, "--years"); !yearsText.empty()) {
    const auto years = parseDouble(yearsText);
    if (!years) {
      errorMessage = "--years must be numeric.";
      return std::nullopt;
    }
    options.rangeYears = *years;
  } else if (const auto years = getJsonNumber(
                 inputRoot,
                 {JsonPath{"years"}, JsonPath{"rangeYears"}})) {
    options.rangeYears = *years;
  }

  if (hasArg(args, "--transit-planets")
      || hasJsonPath(inputRoot,
                     {JsonPath{"transitPlanets"}, JsonPath{"transit-planets"}, JsonPath{"transitObjects"}})) {
    options.rules.transitObjects.clear();
    std::vector<std::string> tokens;
    if (hasArg(args, "--transit-planets")) {
      tokens = splitCsv(getArg(args, "--transit-planets"));
    } else {
      const auto* jsonValue = findFirstJsonPath(
          inputRoot,
          {JsonPath{"transitPlanets"}, JsonPath{"transit-planets"}, JsonPath{"transitObjects"}});
      if (jsonValue != nullptr) {
        tokens = parseJsonStringList(*jsonValue, "transitPlanets", errorMessage);
        if (!errorMessage.empty()) {
          return std::nullopt;
        }
      }
    }
    for (const auto& token : tokens) {
      auto canonical = canonicalTransitObject(token);
      if (!canonical) {
        errorMessage = "Unknown transit planet: " + token;
        return std::nullopt;
      }
      options.rules.transitObjects.push_back(*canonical);
    }
  }

  if (hasArg(args, "--aspects")
      || hasJsonPath(inputRoot,
                     {JsonPath{"aspects"}, JsonPath{"aspectRules"}})) {
    options.rules.aspectRules.clear();
    std::vector<std::string> tokens;
    if (hasArg(args, "--aspects")) {
      tokens = splitCsv(getArg(args, "--aspects"));
    } else {
      const auto* jsonValue = findFirstJsonPath(inputRoot, {JsonPath{"aspects"}, JsonPath{"aspectRules"}});
      if (jsonValue != nullptr) {
        tokens = parseJsonStringList(*jsonValue, "aspects", errorMessage);
        if (!errorMessage.empty()) {
          return std::nullopt;
        }
      }
    }
    for (const auto& token : tokens) {
      auto canonical = canonicalAspectRule(token);
      if (!canonical) {
        errorMessage = "Unknown aspect: " + token;
        return std::nullopt;
      }
      options.rules.aspectRules.push_back(*canonical);
    }
  }

  std::unordered_map<std::string, double> orbOverrides;
  if (const auto* jsonOrbs = findFirstJsonPath(inputRoot, {JsonPath{"orbs"}, JsonPath{"orbOverrides"}})) {
    for (const auto& [aspectName, orbDegrees] : parseJsonOrbOverrides(*jsonOrbs, errorMessage)) {
      orbOverrides[toLower(trim(aspectName))] = orbDegrees;
    }
    if (!errorMessage.empty()) {
      return std::nullopt;
    }
  }
  for (const auto& overrideValue : getAllArgs(args, "--orb")) {
    const auto equalsPos = overrideValue.find('=');
    if (equalsPos == std::string::npos) {
      errorMessage = "Orb overrides must use the form Aspect=deg.";
      return std::nullopt;
    }

    const std::string aspectName = trim(overrideValue.substr(0, equalsPos));
    const std::string orbText = trim(overrideValue.substr(equalsPos + 1));
    const auto canonical = canonicalAspectRule(aspectName);
    const auto orb = parseDouble(orbText);
    if (!canonical || !orb || *orb <= 0.0) {
      errorMessage = "Orb overrides require a known aspect and a positive numeric orb.";
      return std::nullopt;
    }
    orbOverrides[toLower(canonical->aspectType)] = *orb;
  }

  for (auto& rule : options.rules.aspectRules) {
    auto it = orbOverrides.find(toLower(rule.aspectType));
    if (it != orbOverrides.end()) {
      rule.orbDegrees = it->second;
      orbOverrides.erase(it);
    }
  }
  if (!orbOverrides.empty()) {
    errorMessage = "Orb overrides must reference enabled aspects.";
    return std::nullopt;
  }

  return options;
}

std::vector<asteria::domain::LocationResolution> atlasSearchResults(
    asteria::util::AtlasService& atlas,
    const std::string& query) {
  std::vector<asteria::domain::LocationResolution> locations;
  for (const auto* entry : atlas.search(query, 20)) {
    asteria::domain::LocationResolution location;
    location.queryText = query;
    location.displayName = entry->name;
    location.region = entry->regionCode;
    location.latitude = entry->latitude;
    location.longitude = entry->longitude;
    location.timezoneName = entry->timezoneName;
    location.resolutionMethod = "atlas";
    location.confidenceScore = 0.9;
    locations.push_back(std::move(location));
  }
  return locations;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printUsage();
    return 1;
  }

  try {
    std::vector<std::string> args(argv, argv + argc);
    const std::string command = args[1];

    if (command == "--help" || command == "help") {
      printUsage();
      return 0;
    }

    std::optional<CliJsonValue> inputRoot;
    if (const std::string inputPath = getArg(args, "--input"); !inputPath.empty()) {
      auto loaded = asteria::automation::loadCliJsonFile(inputPath);
      if (!loaded.ok()) {
        std::cerr << "Error: " << loaded.error().message << "\n";
        return 1;
      }
      inputRoot = std::move(loaded.value());
    }

    if (command == "resolve-location") {
      const std::string query = getOptionString(
          args,
          {"--query"},
          inputRoot ? &*inputRoot : nullptr,
          {JsonPath{"query"}},
          "");
      if (query.empty()) {
        std::cerr << "Error: --query is required for resolve-location\n";
        return 1;
      }

      asteria::util::AtlasService atlas;
      std::string atlasError;
      if (!loadAtlasService(atlas, atlasError)) {
        std::cerr << "Error: " << atlasError << "\n";
        return 1;
      }

      std::cout << CliDispatcher::locationsToJson(atlasSearchResults(atlas, query));
      return 0;
    }

    const auto astrologDataPath = findAstrologDataPath();
    if (!astrologDataPath) {
      std::cerr << "Error: Could not locate Astrolog data files for the embedded engine.\n";
      return 1;
    }

    asteria::engine::AstrologEmbeddedEngine engine(astrologDataPath->string());
    asteria::util::AtlasService atlas;
    std::string atlasError;
    if (!loadAtlasService(atlas, atlasError)) {
      atlasError.clear();
    }

    CliDispatcher dispatcher(engine, atlas.isLoaded() ? &atlas : nullptr);
    CliDispatcher::CliResult result;
    std::string errorMessage;

    if (command == "compute-natal") {
      auto options = parseNatalChartOptions(args, inputRoot ? &*inputRoot : nullptr, errorMessage);
      if (!options) {
        std::cerr << "Error: " << errorMessage << "\n";
        return 1;
      }
      result = dispatcher.computeNatal(*options);
    } else if (command == "compute-synastry") {
      auto options = parseComparisonChartOptions(args, inputRoot ? &*inputRoot : nullptr, errorMessage);
      if (!options) {
        std::cerr << "Error: " << errorMessage << "\n";
        return 1;
      }
      result = dispatcher.computeSynastry(*options);
    } else if (command == "compute-composite") {
      auto options = parseComparisonChartOptions(args, inputRoot ? &*inputRoot : nullptr, errorMessage);
      if (!options) {
        std::cerr << "Error: " << errorMessage << "\n";
        return 1;
      }
      result = dispatcher.computeComposite(*options);
    } else if (command == "compute-transit") {
      auto options = parseTransitChartOptions(args, inputRoot ? &*inputRoot : nullptr, errorMessage);
      if (!options) {
        std::cerr << "Error: " << errorMessage << "\n";
        return 1;
      }
      result = dispatcher.computeTransitToNatal(*options);
    } else if (command == "generate-transit-timeline") {
      auto options = parseTransitTimelineOptions(args, inputRoot ? &*inputRoot : nullptr, errorMessage);
      if (!options) {
        std::cerr << "Error: " << errorMessage << "\n";
        return 1;
      }
      result = dispatcher.generateTransitTimeline(*options);
    } else if (command == "export-svg") {
      auto options = parseExportChartOptions(args, inputRoot ? &*inputRoot : nullptr, false, errorMessage);
      if (!options) {
        std::cerr << "Error: " << errorMessage << "\n";
        return 1;
      }
      result = dispatcher.exportSvg(*options);
    } else if (command == "export-png") {
      auto options = parseExportChartOptions(args, inputRoot ? &*inputRoot : nullptr, true, errorMessage);
      if (!options) {
        std::cerr << "Error: " << errorMessage << "\n";
        return 1;
      }
      result = dispatcher.exportPng(*options);
    } else if (command == "export-ai-report-pdf") {
      auto options = parsePdfReportOptions(args, inputRoot ? &*inputRoot : nullptr, errorMessage);
      if (!options) {
        std::cerr << "Error: " << errorMessage << "\n";
        return 1;
      }
      result = dispatcher.exportPdfReport(*options);
    } else {
      std::cerr << "Unknown command: " << command << "\n";
      printUsage();
      return 1;
    }

    if (result.success) {
      std::cout << result.outputJson;
      return 0;
    }

    std::cerr << "Error: " << result.errorMessage << "\n";
    return 1;
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }
}