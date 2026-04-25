#pragma once

#include "core/result.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace asteria::automation {

class CliJsonValue {
 public:
  using Object = std::map<std::string, CliJsonValue>;
  using Array = std::vector<CliJsonValue>;

  CliJsonValue() = default;
  explicit CliJsonValue(std::nullptr_t value);
  explicit CliJsonValue(bool value);
  explicit CliJsonValue(double value);
  explicit CliJsonValue(std::string value);
  explicit CliJsonValue(Object value);
  explicit CliJsonValue(Array value);

  bool isNull() const;
  bool isBool() const;
  bool isNumber() const;
  bool isString() const;
  bool isObject() const;
  bool isArray() const;

  const bool* asBool() const;
  const double* asNumber() const;
  const std::string* asString() const;
  const Object* asObject() const;
  const Array* asArray() const;

 private:
  std::variant<std::nullptr_t, bool, double, std::string, Object, Array> m_value = nullptr;
};

core::Result<CliJsonValue> loadCliJsonFile(const std::filesystem::path& path);

const CliJsonValue* findCliJsonMember(const CliJsonValue& value, const std::string& key);

const CliJsonValue* findCliJsonPath(const CliJsonValue& value,
                                    const std::vector<std::string>& path);

std::optional<std::string> cliJsonStringValue(const CliJsonValue& value);

std::optional<double> cliJsonNumberValue(const CliJsonValue& value);

}  // namespace asteria::automation