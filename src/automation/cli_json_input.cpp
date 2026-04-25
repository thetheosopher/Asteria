#include "automation/cli_json_input.h"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>

namespace asteria::automation {

namespace {

template <typename T>
core::Result<T> failureResult(const std::string& code, std::string message) {
  return core::Result<T>::failure({code, std::move(message)});
}

void appendUtf8(std::string& output, std::uint32_t codePoint) {
  if (codePoint <= 0x7F) {
    output.push_back(static_cast<char>(codePoint));
    return;
  }
  if (codePoint <= 0x7FF) {
    output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
    output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    return;
  }
  if (codePoint <= 0xFFFF) {
    output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
    output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    return;
  }
  output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
  output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
  output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
  output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
}

class CliJsonParser {
 public:
  CliJsonParser(std::string text, std::string source)
      : m_text(std::move(text)), m_source(std::move(source)) {}

  core::Result<CliJsonValue> parse() {
    skipWhitespace();
    auto value = parseValue();
    if (!value.ok()) {
      return value;
    }
    skipWhitespace();
    if (!atEnd()) {
      return fail<CliJsonValue>("Unexpected trailing JSON content.");
    }
    return value;
  }

 private:
  template <typename T>
  core::Result<T> fail(const std::string& message) const {
    return failureResult<T>("cli_json_parse_error", positionPrefix() + message);
  }

  bool atEnd() const { return m_position >= m_text.size(); }

  char peek() const { return atEnd() ? '\0' : m_text[m_position]; }

  bool consume(char expected) {
    if (peek() != expected) {
      return false;
    }
    ++m_position;
    return true;
  }

  bool consumeLiteral(const std::string& literal) {
    if (m_text.compare(m_position, literal.size(), literal) != 0) {
      return false;
    }
    m_position += literal.size();
    return true;
  }

  void skipWhitespace() {
    while (!atEnd()) {
      const unsigned char ch = static_cast<unsigned char>(peek());
      if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
        break;
      }
      ++m_position;
    }
  }

  std::string positionPrefix() const {
    std::size_t line = 1;
    std::size_t column = 1;
    for (std::size_t index = 0; index < m_position && index < m_text.size(); ++index) {
      if (m_text[index] == '\n') {
        ++line;
        column = 1;
      } else {
        ++column;
      }
    }

    std::ostringstream stream;
    stream << m_source << ':' << line << ':' << column << ": ";
    return stream.str();
  }

  core::Result<CliJsonValue> parseValue() {
    skipWhitespace();
    if (atEnd()) {
      return fail<CliJsonValue>("Unexpected end of JSON input.");
    }

    const char ch = peek();
    if (ch == '{') {
      return parseObject();
    }
    if (ch == '[') {
      return parseArray();
    }
    if (ch == '"') {
      auto stringValue = parseStringLiteral();
      if (!stringValue.ok()) {
        return failureResult<CliJsonValue>(stringValue.error().code, stringValue.error().message);
      }
      return core::Result<CliJsonValue>::success(CliJsonValue(std::move(stringValue.value())));
    }
    if (ch == 't') {
      return parseLiteral("true", CliJsonValue(true));
    }
    if (ch == 'f') {
      return parseLiteral("false", CliJsonValue(false));
    }
    if (ch == 'n') {
      return parseLiteral("null", CliJsonValue(nullptr));
    }
    if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0) {
      return parseNumber();
    }

    return fail<CliJsonValue>(std::string("Unexpected character '") + ch + "'.");
  }

  core::Result<CliJsonValue> parseLiteral(const std::string& literal, CliJsonValue value) {
    if (!consumeLiteral(literal)) {
      return fail<CliJsonValue>("Invalid JSON literal.");
    }
    return core::Result<CliJsonValue>::success(std::move(value));
  }

  core::Result<CliJsonValue> parseObject() {
    consume('{');
    CliJsonValue::Object object;
    skipWhitespace();
    if (consume('}')) {
      return core::Result<CliJsonValue>::success(CliJsonValue(std::move(object)));
    }

    while (true) {
      auto key = parseStringLiteral();
      if (!key.ok()) {
        return failureResult<CliJsonValue>(key.error().code, key.error().message);
      }

      skipWhitespace();
      if (!consume(':')) {
        return fail<CliJsonValue>("Expected ':' after object key.");
      }

      auto value = parseValue();
      if (!value.ok()) {
        return value;
      }
      object[std::move(key.value())] = std::move(value.value());

      skipWhitespace();
      if (consume('}')) {
        break;
      }
      if (!consume(',')) {
        return fail<CliJsonValue>("Expected ',' or '}' after object member.");
      }
      skipWhitespace();
    }

    return core::Result<CliJsonValue>::success(CliJsonValue(std::move(object)));
  }

  core::Result<CliJsonValue> parseArray() {
    consume('[');
    CliJsonValue::Array array;
    skipWhitespace();
    if (consume(']')) {
      return core::Result<CliJsonValue>::success(CliJsonValue(std::move(array)));
    }

    while (true) {
      auto value = parseValue();
      if (!value.ok()) {
        return value;
      }
      array.push_back(std::move(value.value()));

      skipWhitespace();
      if (consume(']')) {
        break;
      }
      if (!consume(',')) {
        return fail<CliJsonValue>("Expected ',' or ']' after array element.");
      }
      skipWhitespace();
    }

    return core::Result<CliJsonValue>::success(CliJsonValue(std::move(array)));
  }

  core::Result<std::uint32_t> parseHexCodeUnit() {
    std::uint32_t value = 0;
    for (int index = 0; index < 4; ++index) {
      if (atEnd()) {
        return fail<std::uint32_t>("Incomplete unicode escape sequence.");
      }
      const char ch = m_text[m_position++];
      value <<= 4;
      if (ch >= '0' && ch <= '9') {
        value |= static_cast<std::uint32_t>(ch - '0');
      } else if (ch >= 'a' && ch <= 'f') {
        value |= static_cast<std::uint32_t>(ch - 'a' + 10);
      } else if (ch >= 'A' && ch <= 'F') {
        value |= static_cast<std::uint32_t>(ch - 'A' + 10);
      } else {
        return fail<std::uint32_t>("Invalid hex digit in unicode escape sequence.");
      }
    }
    return core::Result<std::uint32_t>::success(value);
  }

  core::Result<std::string> parseStringLiteral() {
    if (!consume('"')) {
      return fail<std::string>("Expected a JSON string.");
    }

    std::string value;
    while (!atEnd()) {
      const char ch = m_text[m_position++];
      if (ch == '"') {
        return core::Result<std::string>::success(std::move(value));
      }
      if (static_cast<unsigned char>(ch) < 0x20) {
        return fail<std::string>("Control characters are not allowed inside JSON strings.");
      }
      if (ch != '\\') {
        value.push_back(ch);
        continue;
      }

      if (atEnd()) {
        return fail<std::string>("Unterminated escape sequence.");
      }

      const char escaped = m_text[m_position++];
      switch (escaped) {
        case '"': value.push_back('"'); break;
        case '\\': value.push_back('\\'); break;
        case '/': value.push_back('/'); break;
        case 'b': value.push_back('\b'); break;
        case 'f': value.push_back('\f'); break;
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        case 'u': {
          auto codeUnit = parseHexCodeUnit();
          if (!codeUnit.ok()) {
            return failureResult<std::string>(codeUnit.error().code, codeUnit.error().message);
          }

          std::uint32_t codePoint = codeUnit.value();
          if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
            if (atEnd() || m_position + 1 >= m_text.size() || m_text[m_position] != '\\'
                || m_text[m_position + 1] != 'u') {
              return fail<std::string>("Expected a low surrogate after a high surrogate.");
            }
            m_position += 2;
            auto lowUnit = parseHexCodeUnit();
            if (!lowUnit.ok()) {
              return failureResult<std::string>(lowUnit.error().code, lowUnit.error().message);
            }
            const std::uint32_t lowCode = lowUnit.value();
            if (lowCode < 0xDC00 || lowCode > 0xDFFF) {
              return fail<std::string>("Invalid low surrogate in unicode escape sequence.");
            }
            codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (lowCode - 0xDC00);
          } else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
            return fail<std::string>("Unexpected low surrogate in unicode escape sequence.");
          }

          appendUtf8(value, codePoint);
          break;
        }
        default:
          return fail<std::string>("Invalid JSON escape sequence.");
      }
    }

    return fail<std::string>("Unterminated JSON string.");
  }

  core::Result<CliJsonValue> parseNumber() {
    const std::size_t start = m_position;
    if (peek() == '-') {
      ++m_position;
    }

    if (atEnd()) {
      return fail<CliJsonValue>("Unexpected end of number literal.");
    }

    if (peek() == '0') {
      ++m_position;
    } else {
      if (std::isdigit(static_cast<unsigned char>(peek())) == 0) {
        return fail<CliJsonValue>("Invalid number literal.");
      }
      while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
        ++m_position;
      }
    }

    if (!atEnd() && peek() == '.') {
      ++m_position;
      if (atEnd() || std::isdigit(static_cast<unsigned char>(peek())) == 0) {
        return fail<CliJsonValue>("Invalid fractional number literal.");
      }
      while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
        ++m_position;
      }
    }

    if (!atEnd() && (peek() == 'e' || peek() == 'E')) {
      ++m_position;
      if (!atEnd() && (peek() == '+' || peek() == '-')) {
        ++m_position;
      }
      if (atEnd() || std::isdigit(static_cast<unsigned char>(peek())) == 0) {
        return fail<CliJsonValue>("Invalid numeric exponent.");
      }
      while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
        ++m_position;
      }
    }

    const std::string text = m_text.substr(start, m_position - start);
    try {
      return core::Result<CliJsonValue>::success(CliJsonValue(std::stod(text)));
    } catch (...) {
      return fail<CliJsonValue>("Invalid number literal.");
    }
  }

  std::string m_text;
  std::string m_source;
  std::size_t m_position = 0;
};

std::optional<double> parseNumberLike(const std::string& text) {
  try {
    std::size_t processed = 0;
    const double value = std::stod(text, &processed);
    if (processed != text.size()) {
      return std::nullopt;
    }
    return value;
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace

CliJsonValue::CliJsonValue(std::nullptr_t value) : m_value(value) {}

CliJsonValue::CliJsonValue(bool value) : m_value(value) {}

CliJsonValue::CliJsonValue(double value) : m_value(value) {}

CliJsonValue::CliJsonValue(std::string value) : m_value(std::move(value)) {}

CliJsonValue::CliJsonValue(Object value) : m_value(std::move(value)) {}

CliJsonValue::CliJsonValue(Array value) : m_value(std::move(value)) {}

bool CliJsonValue::isNull() const { return std::holds_alternative<std::nullptr_t>(m_value); }

bool CliJsonValue::isBool() const { return std::holds_alternative<bool>(m_value); }

bool CliJsonValue::isNumber() const { return std::holds_alternative<double>(m_value); }

bool CliJsonValue::isString() const { return std::holds_alternative<std::string>(m_value); }

bool CliJsonValue::isObject() const { return std::holds_alternative<Object>(m_value); }

bool CliJsonValue::isArray() const { return std::holds_alternative<Array>(m_value); }

const bool* CliJsonValue::asBool() const { return std::get_if<bool>(&m_value); }

const double* CliJsonValue::asNumber() const { return std::get_if<double>(&m_value); }

const std::string* CliJsonValue::asString() const { return std::get_if<std::string>(&m_value); }

const CliJsonValue::Object* CliJsonValue::asObject() const { return std::get_if<Object>(&m_value); }

const CliJsonValue::Array* CliJsonValue::asArray() const { return std::get_if<Array>(&m_value); }

core::Result<CliJsonValue> loadCliJsonFile(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return failureResult<CliJsonValue>(
        "cli_json_io_error",
        "Could not open JSON input file: " + path.string());
  }

  const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  CliJsonParser parser(text, path.string());
  auto parsed = parser.parse();
  if (!parsed.ok()) {
    return parsed;
  }
  if (!parsed.value().isObject()) {
    return failureResult<CliJsonValue>(
        "cli_json_parse_error",
        "JSON input root must be an object: " + path.string());
  }
  return parsed;
}

const CliJsonValue* findCliJsonMember(const CliJsonValue& value, const std::string& key) {
  const auto* object = value.asObject();
  if (object == nullptr) {
    return nullptr;
  }
  auto it = object->find(key);
  if (it == object->end()) {
    return nullptr;
  }
  return &it->second;
}

const CliJsonValue* findCliJsonPath(const CliJsonValue& value, const std::vector<std::string>& path) {
  const CliJsonValue* current = &value;
  for (const auto& key : path) {
    current = findCliJsonMember(*current, key);
    if (current == nullptr) {
      return nullptr;
    }
  }
  return current;
}

std::optional<std::string> cliJsonStringValue(const CliJsonValue& value) {
  if (const auto* text = value.asString()) {
    return *text;
  }
  return std::nullopt;
}

std::optional<double> cliJsonNumberValue(const CliJsonValue& value) {
  if (const auto* number = value.asNumber()) {
    return *number;
  }
  if (const auto* text = value.asString()) {
    return parseNumberLike(*text);
  }
  return std::nullopt;
}

}  // namespace asteria::automation