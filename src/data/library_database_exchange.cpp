#include "library_database_exchange.h"

#include "birth_event_repository.h"
#include "domain/birth_event.h"
#include "domain/person.h"
#include "sqlite_database.h"

#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace asteria::data {

namespace {

template <typename T>
core::Result<T> failResult(const std::string& code, std::string message) {
  return core::Result<T>::failure({code, std::move(message)});
}

class JsonValue {
 public:
  using Object = std::map<std::string, JsonValue>;
  using Array = std::vector<JsonValue>;

  JsonValue() = default;
  JsonValue(std::nullptr_t value) : m_value(value) {}
  JsonValue(bool value) : m_value(value) {}
  JsonValue(double value) : m_value(value) {}
  JsonValue(std::string value) : m_value(std::move(value)) {}
  JsonValue(Object value) : m_value(std::move(value)) {}
  JsonValue(Array value) : m_value(std::move(value)) {}

  bool isNull() const { return std::holds_alternative<std::nullptr_t>(m_value); }
  bool isBool() const { return std::holds_alternative<bool>(m_value); }
  bool isNumber() const { return std::holds_alternative<double>(m_value); }
  bool isString() const { return std::holds_alternative<std::string>(m_value); }
  bool isObject() const { return std::holds_alternative<Object>(m_value); }
  bool isArray() const { return std::holds_alternative<Array>(m_value); }

  const bool* asBool() const { return std::get_if<bool>(&m_value); }
  const double* asNumber() const { return std::get_if<double>(&m_value); }
  const std::string* asString() const { return std::get_if<std::string>(&m_value); }
  const Object* asObject() const { return std::get_if<Object>(&m_value); }
  const Array* asArray() const { return std::get_if<Array>(&m_value); }

 private:
  std::variant<std::nullptr_t, bool, double, std::string, Object, Array> m_value = nullptr;
};

class JsonParser {
 public:
  JsonParser(std::string text, std::string source)
      : m_text(std::move(text)), m_source(std::move(source)) {}

  core::Result<JsonValue> parse() {
    skipWhitespace();
    auto value = parseValue();
    if (!value.ok()) {
      return value;
    }
    skipWhitespace();
    if (!isAtEnd()) {
      return fail<JsonValue>("Unexpected trailing JSON content.");
    }
    return value;
  }

 private:
  template <typename T>
  core::Result<T> fail(std::string message) const {
    return failResult<T>("library_json_parse_error", positionPrefix() + std::move(message));
  }

  std::string positionPrefix() const {
    int line = 1;
    int column = 1;
    for (size_t index = 0; index < m_pos && index < m_text.size(); ++index) {
      if (m_text[index] == '\n') {
        ++line;
        column = 1;
      } else {
        ++column;
      }
    }
    return m_source + ":" + std::to_string(line) + ":" + std::to_string(column) + ": ";
  }

  bool isAtEnd() const { return m_pos >= m_text.size(); }

  char peek() const {
    return isAtEnd() ? '\0' : m_text[m_pos];
  }

  char advance() {
    return isAtEnd() ? '\0' : m_text[m_pos++];
  }

  bool consumeIf(char expected) {
    if (peek() != expected) {
      return false;
    }
    ++m_pos;
    return true;
  }

  void skipWhitespace() {
    while (!isAtEnd() && std::isspace(static_cast<unsigned char>(peek()))) {
      ++m_pos;
    }
  }

  core::Result<JsonValue> parseValue() {
    if (isAtEnd()) {
      return fail<JsonValue>("Unexpected end of JSON input.");
    }

    switch (peek()) {
      case '{':
        return parseObject();
      case '[':
        return parseArray();
      case '"': {
        auto stringValue = parseString();
        if (!stringValue.ok()) {
          return failResult<JsonValue>(stringValue.error().code, stringValue.error().message);
        }
        return core::Result<JsonValue>::success(JsonValue(std::move(stringValue.value())));
      }
      case 't':
        return parseLiteral("true", JsonValue(true));
      case 'f':
        return parseLiteral("false", JsonValue(false));
      case 'n':
        return parseLiteral("null", JsonValue(nullptr));
      default:
        if (peek() == '-' || std::isdigit(static_cast<unsigned char>(peek()))) {
          return parseNumber();
        }
        return fail<JsonValue>(std::string("Unexpected character '") + peek() + "'.");
    }
  }

  core::Result<JsonValue> parseLiteral(const char* literal, JsonValue value) {
    const size_t literalLength = std::char_traits<char>::length(literal);
    if (m_text.compare(m_pos, literalLength, literal) != 0) {
      return fail<JsonValue>("Invalid JSON literal.");
    }
    m_pos += literalLength;
    return core::Result<JsonValue>::success(std::move(value));
  }

  core::Result<JsonValue> parseObject() {
    consumeIf('{');
    skipWhitespace();

    JsonValue::Object object;
    if (consumeIf('}')) {
      return core::Result<JsonValue>::success(JsonValue(std::move(object)));
    }

    while (true) {
      auto key = parseString();
      if (!key.ok()) {
        return failResult<JsonValue>(key.error().code, key.error().message);
      }
      skipWhitespace();
      if (!consumeIf(':')) {
        return fail<JsonValue>("Expected ':' after object key.");
      }
      skipWhitespace();
      auto value = parseValue();
      if (!value.ok()) {
        return value;
      }
      object.emplace(key.value(), std::move(value.value()));
      skipWhitespace();
      if (consumeIf('}')) {
        break;
      }
      if (!consumeIf(',')) {
        return fail<JsonValue>("Expected ',' or '}' after object member.");
      }
      skipWhitespace();
    }

    return core::Result<JsonValue>::success(JsonValue(std::move(object)));
  }

  core::Result<JsonValue> parseArray() {
    consumeIf('[');
    skipWhitespace();

    JsonValue::Array array;
    if (consumeIf(']')) {
      return core::Result<JsonValue>::success(JsonValue(std::move(array)));
    }

    while (true) {
      auto value = parseValue();
      if (!value.ok()) {
        return value;
      }
      array.push_back(std::move(value.value()));
      skipWhitespace();
      if (consumeIf(']')) {
        break;
      }
      if (!consumeIf(',')) {
        return fail<JsonValue>("Expected ',' or ']' after array element.");
      }
      skipWhitespace();
    }

    return core::Result<JsonValue>::success(JsonValue(std::move(array)));
  }

  static void appendUtf8(std::string& out, unsigned int codePoint) {
    if (codePoint <= 0x7F) {
      out.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
      out.push_back(static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F)));
      out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
      out.push_back(static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F)));
      out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07)));
      out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
  }

  core::Result<std::string> parseString() {
    if (!consumeIf('"')) {
      return fail<std::string>("Expected a JSON string.");
    }

    std::string out;
    while (!isAtEnd()) {
      char ch = advance();
      if (ch == '"') {
        return core::Result<std::string>::success(std::move(out));
      }
      if (static_cast<unsigned char>(ch) < 0x20) {
        return fail<std::string>("Control characters are not allowed inside JSON strings.");
      }
      if (ch != '\\') {
        out.push_back(ch);
        continue;
      }

      if (isAtEnd()) {
        return fail<std::string>("Unterminated JSON string.");
      }

      char escape = advance();
      switch (escape) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
          if (m_pos + 4 > m_text.size()) {
            return fail<std::string>("Invalid Unicode escape sequence.");
          }
          unsigned int codePoint = 0;
          for (int i = 0; i < 4; ++i) {
            char hex = advance();
            codePoint <<= 4;
            if (hex >= '0' && hex <= '9') codePoint |= static_cast<unsigned int>(hex - '0');
            else if (hex >= 'a' && hex <= 'f') codePoint |= static_cast<unsigned int>(hex - 'a' + 10);
            else if (hex >= 'A' && hex <= 'F') codePoint |= static_cast<unsigned int>(hex - 'A' + 10);
            else return fail<std::string>("Invalid Unicode escape sequence.");
          }
          appendUtf8(out, codePoint);
          break;
        }
        default:
          return fail<std::string>("Invalid JSON escape sequence.");
      }
    }

    return fail<std::string>("Unterminated JSON string.");
  }

  core::Result<JsonValue> parseNumber() {
    const size_t start = m_pos;
    if (consumeIf('-') && isAtEnd()) {
      return fail<JsonValue>("Unexpected end of number literal.");
    }

    if (peek() == '0') {
      advance();
    } else if (std::isdigit(static_cast<unsigned char>(peek()))) {
      while (std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
      }
    } else {
      return fail<JsonValue>("Invalid number literal.");
    }

    if (peek() == '.') {
      advance();
      if (!std::isdigit(static_cast<unsigned char>(peek()))) {
        return fail<JsonValue>("Invalid fractional number literal.");
      }
      while (std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
      }
    }

    if (peek() == 'e' || peek() == 'E') {
      advance();
      if (peek() == '+' || peek() == '-') {
        advance();
      }
      if (!std::isdigit(static_cast<unsigned char>(peek()))) {
        return fail<JsonValue>("Invalid numeric exponent.");
      }
      while (std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
      }
    }

    const std::string text = m_text.substr(start, m_pos - start);
    try {
      return core::Result<JsonValue>::success(JsonValue(std::stod(text)));
    } catch (...) {
      return fail<JsonValue>("Invalid number literal.");
    }
  }

  std::string m_text;
  std::string m_source;
  size_t m_pos = 0;
};

struct ImportedPersonRecord {
  domain::Person person;
  std::vector<domain::BirthEvent> birthEvents;
};

class TransactionScope {
 public:
  explicit TransactionScope(SQLiteDatabase& db) : m_db(db) {}

  bool begin() {
    m_active = m_db.execute("BEGIN TRANSACTION;");
    return m_active;
  }

  bool commit() {
    if (!m_active) {
      return false;
    }
    const bool ok = m_db.execute("COMMIT;");
    if (ok) {
      m_active = false;
    }
    return ok;
  }

  ~TransactionScope() {
    if (m_active) {
      m_db.execute("ROLLBACK;");
    }
  }

 private:
  SQLiteDatabase& m_db;
  bool m_active = false;
};

const JsonValue* findMember(const JsonValue::Object& object, std::string_view key) {
  auto it = object.find(std::string(key));
  return it == object.end() ? nullptr : &it->second;
}

std::string trim(std::string_view value) {
  size_t start = 0;
  size_t end = value.size();
  while (start < end && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return std::string(value.substr(start, end - start));
}

std::string normalizeKey(std::string_view value) {
  std::string normalized;
  normalized.reserve(value.size());
  bool previousWasSpace = false;
  for (unsigned char ch : trim(value)) {
    if (std::isspace(ch)) {
      if (!previousWasSpace && !normalized.empty()) {
        normalized.push_back(' ');
      }
      previousWasSpace = true;
      continue;
    }
    normalized.push_back(static_cast<char>(std::tolower(ch)));
    previousWasSpace = false;
  }
  if (!normalized.empty() && normalized.back() == ' ') {
    normalized.pop_back();
  }
  return normalized;
}

std::string personMergeKey(const domain::Person& person) {
  const std::string name = normalizeKey(person.fullName);
  return name.empty() ? normalizeKey(person.displayName) : name;
}

std::string birthEventMergeKey(const domain::BirthEvent& event) {
  return normalizeKey(event.birthDate) + "|" +
         normalizeKey(event.birthTime.value_or("")) + "|" +
         normalizeKey(event.cityInput);
}

void appendJsonString(std::ostream& out, std::string_view value) {
  out << '"';
  for (unsigned char ch : value) {
    switch (ch) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (ch < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<int>(ch) << std::dec << std::setfill(' ');
        } else {
          out << static_cast<char>(ch);
        }
        break;
    }
  }
  out << '"';
}

std::string stringArrayToJson(const std::vector<std::string>& values) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << '[';
  for (size_t index = 0; index < values.size(); ++index) {
    if (index > 0) {
      out << ',';
    }
    appendJsonString(out, values[index]);
  }
  out << ']';
  return out.str();
}

std::vector<std::string> parseStringArrayValue(const JsonValue& value) {
  std::vector<std::string> result;
  const auto* array = value.asArray();
  if (array == nullptr) {
    return result;
  }
  result.reserve(array->size());
  for (const JsonValue& entry : *array) {
    if (const auto* text = entry.asString()) {
      result.push_back(*text);
    }
  }
  return result;
}

std::vector<std::string> parseTagJson(std::string_view tagsJson) {
  JsonParser parser(std::string(tagsJson), "people.tags");
  auto parsed = parser.parse();
  if (!parsed.ok()) {
    return {};
  }
  return parseStringArrayValue(parsed.value());
}

std::vector<domain::Person> loadActivePeople(SQLiteDatabase& db) {
  std::vector<domain::Person> people;
  db.query(
      "SELECT person_id, full_name, display_name, gender, tags, notes, "
      "created_at, updated_at, archived_at FROM people WHERE archived_at IS NULL "
      "ORDER BY full_name;",
      [&](sqlite3_stmt* stmt) {
        domain::Person person;
        person.personId = sqlite3_column_int64(stmt, 0);
        person.fullName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        person.displayName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        person.gender = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* tagsText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (tagsText != nullptr) {
          person.tags = parseTagJson(tagsText);
        }
        person.notes = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        person.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        person.updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
          person.archivedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        }
        people.push_back(std::move(person));
      });
  return people;
}

bool insertPerson(SQLiteDatabase& db, domain::Person& person) {
  const std::string tagsJson = stringArrayToJson(person.tags);
  const bool ok = db.executeWithParams(
      "INSERT INTO people (full_name, display_name, gender, tags, notes) VALUES (?, ?, ?, ?, ?);",
      [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, person.fullName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, person.displayName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, person.gender.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, tagsJson.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, person.notes.c_str(), -1, SQLITE_TRANSIENT);
      });
  if (ok) {
    person.personId = db.lastInsertRowId();
  }
  return ok;
}

bool updatePerson(SQLiteDatabase& db, const domain::Person& person) {
  const std::string tagsJson = stringArrayToJson(person.tags);
  return db.executeWithParams(
      "UPDATE people SET full_name=?, display_name=?, gender=?, tags=?, notes=?, "
      "updated_at=datetime('now') WHERE person_id=?;",
      [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, person.fullName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, person.displayName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, person.gender.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, tagsJson.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, person.notes.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 6, person.personId);
      });
}

bool clearLibraryData(SQLiteDatabase& db) {
  static const char* kStatements[] = {
      "DELETE FROM export_artifacts;",
      "DELETE FROM interpretation_notes;",
      "DELETE FROM computed_charts;",
      "DELETE FROM chart_requests;",
      "DELETE FROM birth_events;",
      "DELETE FROM people;",
      "DELETE FROM locations;",
  };

  for (const char* sql : kStatements) {
    if (!db.execute(sql)) {
      return false;
    }
  }
  return true;
}

core::Result<std::string> requiredStringMember(const JsonValue::Object& object, std::string_view key) {
  const JsonValue* value = findMember(object, key);
  if (value == nullptr || value->asString() == nullptr) {
    return failResult<std::string>("library_json_invalid", "Expected string field '" + std::string(key) + "'.");
  }
  return core::Result<std::string>::success(*value->asString());
}

std::optional<std::string> optionalStringMember(const JsonValue::Object& object, std::string_view key) {
  const JsonValue* value = findMember(object, key);
  if (value == nullptr || value->isNull()) {
    return std::nullopt;
  }
  const auto* text = value->asString();
  return text == nullptr ? std::nullopt : std::optional<std::string>(*text);
}

std::optional<double> optionalNumberMember(const JsonValue::Object& object, std::string_view key) {
  const JsonValue* value = findMember(object, key);
  if (value == nullptr || value->isNull()) {
    return std::nullopt;
  }
  const auto* number = value->asNumber();
  return number == nullptr ? std::nullopt : std::optional<double>(*number);
}

bool optionalBoolMember(const JsonValue::Object& object, std::string_view key, bool defaultValue) {
  const JsonValue* value = findMember(object, key);
  if (value == nullptr || value->isNull() || value->asBool() == nullptr) {
    return defaultValue;
  }
  return *value->asBool();
}

core::Result<domain::BirthEvent> parseBirthEventRecord(const JsonValue& value) {
  const auto* object = value.asObject();
  if (object == nullptr) {
    return failResult<domain::BirthEvent>("library_json_invalid", "Each birthEvents entry must be an object.");
  }

  domain::BirthEvent event;
  auto birthDate = requiredStringMember(*object, "birthDate");
  if (!birthDate.ok()) {
    return failResult<domain::BirthEvent>(birthDate.error().code, birthDate.error().message);
  }
  event.birthDate = birthDate.value();
  event.birthTime = optionalStringMember(*object, "birthTime");

  const std::string accuracy = optionalStringMember(*object, "timeAccuracy").value_or("unknown");
  if (accuracy == "exact") event.timeAccuracy = domain::TimeAccuracy::Exact;
  else if (accuracy == "approximate") event.timeAccuracy = domain::TimeAccuracy::Approximate;
  else event.timeAccuracy = domain::TimeAccuracy::Unknown;

  event.noonDefaultApplied = optionalBoolMember(*object, "noonDefaultApplied", false);
  event.housesEnabled = optionalBoolMember(*object, "housesEnabled", true);
  event.sourceDescription = optionalStringMember(*object, "sourceDescription").value_or("");
  event.confidenceScore = optionalNumberMember(*object, "confidenceScore").value_or(0.0);
  event.cityInput = optionalStringMember(*object, "cityInput").value_or("");
  event.timezoneName = optionalStringMember(*object, "timezoneName");
  event.dstMode = optionalStringMember(*object, "dstMode").value_or("");
  event.latitudeDeg = optionalNumberMember(*object, "latitudeDeg");
  event.longitudeDeg = optionalNumberMember(*object, "longitudeDeg");
  event.timezoneOffsetHours = optionalNumberMember(*object, "timezoneOffsetHours");
  event.dstOffsetHours = optionalNumberMember(*object, "dstOffsetHours");
  event.locationId.reset();
  return core::Result<domain::BirthEvent>::success(std::move(event));
}

core::Result<ImportedPersonRecord> parsePersonRecord(const JsonValue& value) {
  const auto* object = value.asObject();
  if (object == nullptr) {
    return failResult<ImportedPersonRecord>("library_json_invalid", "Each people entry must be an object.");
  }

  ImportedPersonRecord record;
  auto fullName = requiredStringMember(*object, "fullName");
  if (!fullName.ok()) {
    return failResult<ImportedPersonRecord>(fullName.error().code, fullName.error().message);
  }

  record.person.fullName = fullName.value();
  record.person.displayName = optionalStringMember(*object, "displayName").value_or(record.person.fullName);
  if (trim(record.person.displayName).empty()) {
    record.person.displayName = record.person.fullName;
  }
  record.person.gender = optionalStringMember(*object, "gender").value_or("");
  record.person.notes = optionalStringMember(*object, "notes").value_or("");

  if (const JsonValue* tagsValue = findMember(*object, "tags")) {
    if (!tagsValue->isArray()) {
      return failResult<ImportedPersonRecord>("library_json_invalid", "Field 'tags' must be an array.");
    }
    for (const JsonValue& entry : *tagsValue->asArray()) {
      if (entry.asString() == nullptr) {
        return failResult<ImportedPersonRecord>("library_json_invalid", "Each tags entry must be a string.");
      }
      record.person.tags.push_back(*entry.asString());
    }
  }

  const JsonValue* eventsValue = findMember(*object, "birthEvents");
  if (eventsValue == nullptr || !eventsValue->isArray()) {
    return failResult<ImportedPersonRecord>("library_json_invalid", "Field 'birthEvents' must be an array.");
  }

  for (const JsonValue& entry : *eventsValue->asArray()) {
    auto event = parseBirthEventRecord(entry);
    if (!event.ok()) {
      return failResult<ImportedPersonRecord>(event.error().code, event.error().message);
    }
    record.birthEvents.push_back(std::move(event.value()));
  }

  return core::Result<ImportedPersonRecord>::success(std::move(record));
}

core::Result<std::vector<ImportedPersonRecord>> loadLibraryFile(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return failResult<std::vector<ImportedPersonRecord>>(
        "library_json_open_failed",
        "Unable to open JSON file: " + path.string());
  }

  const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  JsonParser parser(text, path.string());
  auto parsed = parser.parse();
  if (!parsed.ok()) {
    return failResult<std::vector<ImportedPersonRecord>>(parsed.error().code, parsed.error().message);
  }
  const auto* root = parsed.value().asObject();
  if (root == nullptr) {
    return failResult<std::vector<ImportedPersonRecord>>(
        "library_json_invalid",
        "Library JSON root must be an object.");
  }

  auto format = requiredStringMember(*root, "format");
  if (!format.ok()) {
    return failResult<std::vector<ImportedPersonRecord>>(format.error().code, format.error().message);
  }
  if (format.value() != "asteria-library") {
    return failResult<std::vector<ImportedPersonRecord>>(
        "library_json_invalid",
        "Unsupported library JSON format: " + format.value());
  }

  const JsonValue* versionValue = findMember(*root, "version");
  if (versionValue == nullptr || versionValue->asNumber() == nullptr || *versionValue->asNumber() != 1.0) {
    return failResult<std::vector<ImportedPersonRecord>>(
        "library_json_invalid",
        "Unsupported library JSON version.");
  }

  const JsonValue* peopleValue = findMember(*root, "people");
  if (peopleValue == nullptr || !peopleValue->isArray()) {
    return failResult<std::vector<ImportedPersonRecord>>(
        "library_json_invalid",
        "Field 'people' must be an array.");
  }

  std::vector<ImportedPersonRecord> records;
  records.reserve(peopleValue->asArray()->size());
  for (const JsonValue& personValue : *peopleValue->asArray()) {
    auto person = parsePersonRecord(personValue);
    if (!person.ok()) {
      return failResult<std::vector<ImportedPersonRecord>>(person.error().code, person.error().message);
    }
    records.push_back(std::move(person.value()));
  }
  return core::Result<std::vector<ImportedPersonRecord>>::success(std::move(records));
}

void appendIndent(std::ostream& out, int depth) {
  for (int index = 0; index < depth; ++index) {
    out << "  ";
  }
}

void appendOptionalString(std::ostream& out, const std::optional<std::string>& value) {
  if (value) {
    appendJsonString(out, *value);
  } else {
    out << "null";
  }
}

void appendOptionalNumber(std::ostream& out, const std::optional<double>& value) {
  if (value) {
    out << *value;
  } else {
    out << "null";
  }
}

void writeBirthEventJson(std::ostream& out, const domain::BirthEvent& event, int depth) {
  appendIndent(out, depth);
  out << "{\n";
  appendIndent(out, depth + 1);
  out << "\"birthDate\": ";
  appendJsonString(out, event.birthDate);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"birthTime\": ";
  appendOptionalString(out, event.birthTime);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"timeAccuracy\": ";
  switch (event.timeAccuracy) {
    case domain::TimeAccuracy::Exact:
      appendJsonString(out, "exact");
      break;
    case domain::TimeAccuracy::Approximate:
      appendJsonString(out, "approximate");
      break;
    default:
      appendJsonString(out, "unknown");
      break;
  }
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"noonDefaultApplied\": " << (event.noonDefaultApplied ? "true" : "false") << ",\n";
  appendIndent(out, depth + 1);
  out << "\"housesEnabled\": " << (event.housesEnabled ? "true" : "false") << ",\n";
  appendIndent(out, depth + 1);
  out << "\"sourceDescription\": ";
  appendJsonString(out, event.sourceDescription);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"confidenceScore\": " << event.confidenceScore << ",\n";
  appendIndent(out, depth + 1);
  out << "\"cityInput\": ";
  appendJsonString(out, event.cityInput);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"timezoneName\": ";
  appendOptionalString(out, event.timezoneName);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"dstMode\": ";
  appendJsonString(out, event.dstMode);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"latitudeDeg\": ";
  appendOptionalNumber(out, event.latitudeDeg);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"longitudeDeg\": ";
  appendOptionalNumber(out, event.longitudeDeg);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"timezoneOffsetHours\": ";
  appendOptionalNumber(out, event.timezoneOffsetHours);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"dstOffsetHours\": ";
  appendOptionalNumber(out, event.dstOffsetHours);
  out << '\n';
  appendIndent(out, depth);
  out << '}';
}

void writePersonJson(std::ostream& out,
                     const domain::Person& person,
                     const std::vector<domain::BirthEvent>& birthEvents,
                     int depth) {
  appendIndent(out, depth);
  out << "{\n";
  appendIndent(out, depth + 1);
  out << "\"fullName\": ";
  appendJsonString(out, person.fullName);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"displayName\": ";
  appendJsonString(out, person.displayName);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"gender\": ";
  appendJsonString(out, person.gender);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"tags\": ";
  out << stringArrayToJson(person.tags);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"notes\": ";
  appendJsonString(out, person.notes);
  out << ",\n";
  appendIndent(out, depth + 1);
  out << "\"birthEvents\": [\n";
  for (size_t index = 0; index < birthEvents.size(); ++index) {
    writeBirthEventJson(out, birthEvents[index], depth + 2);
    if (index + 1 < birthEvents.size()) {
      out << ',';
    }
    out << '\n';
  }
  appendIndent(out, depth + 1);
  out << "]\n";
  appendIndent(out, depth);
  out << '}';
}

std::optional<size_t> findMatchingPersonIndex(const std::vector<domain::Person>& people,
                                              const domain::Person& candidate) {
  const std::string key = personMergeKey(candidate);
  if (key.empty()) {
    return std::nullopt;
  }
  for (size_t index = 0; index < people.size(); ++index) {
    if (personMergeKey(people[index]) == key) {
      return index;
    }
  }
  return std::nullopt;
}

std::optional<size_t> findMatchingBirthEventIndex(const std::vector<domain::BirthEvent>& events,
                                                  const domain::BirthEvent& candidate) {
  const std::string key = birthEventMergeKey(candidate);
  for (size_t index = 0; index < events.size(); ++index) {
    if (birthEventMergeKey(events[index]) == key) {
      return index;
    }
  }
  return std::nullopt;
}

core::Result<LibraryExchangeStats> applyImportedRecords(SQLiteDatabase& db,
                                                        const std::vector<ImportedPersonRecord>& records,
                                                        bool replaceExisting) {
  TransactionScope transaction(db);
  if (!transaction.begin()) {
    return failResult<LibraryExchangeStats>("library_db_transaction_failed", db.lastError());
  }

  if (replaceExisting && !clearLibraryData(db)) {
    return failResult<LibraryExchangeStats>("library_db_clear_failed", db.lastError());
  }

  BirthEventRepository birthRepo(db);
  std::vector<domain::Person> existingPeople = loadActivePeople(db);
  LibraryExchangeStats stats;

  for (const ImportedPersonRecord& record : records) {
    stats.peopleProcessed += 1;
    stats.birthEventsProcessed += static_cast<int>(record.birthEvents.size());

    domain::Person person = record.person;
    if (trim(person.displayName).empty()) {
      person.displayName = person.fullName;
    }

    std::optional<size_t> matchingPersonIndex;
    if (!replaceExisting) {
      matchingPersonIndex = findMatchingPersonIndex(existingPeople, person);
    }

    if (matchingPersonIndex) {
      person.personId = existingPeople[*matchingPersonIndex].personId;
      if (!updatePerson(db, person)) {
        return failResult<LibraryExchangeStats>("library_db_update_failed", db.lastError());
      }
      existingPeople[*matchingPersonIndex] = person;
      stats.peopleUpdated += 1;
    } else {
      if (!insertPerson(db, person)) {
        return failResult<LibraryExchangeStats>("library_db_insert_failed", db.lastError());
      }
      existingPeople.push_back(person);
      stats.peopleInserted += 1;
    }

    std::vector<domain::BirthEvent> existingEvents = replaceExisting
        ? std::vector<domain::BirthEvent>{}
        : birthRepo.findByPersonId(person.personId);
    const bool pairwiseUpdate = !replaceExisting && existingEvents.size() == 1 && record.birthEvents.size() == 1;

    for (size_t index = 0; index < record.birthEvents.size(); ++index) {
      domain::BirthEvent event = record.birthEvents[index];
      event.personId = person.personId;
      event.locationId.reset();

      std::optional<size_t> matchingEventIndex;
      if (pairwiseUpdate) {
        matchingEventIndex = 0;
      } else if (!replaceExisting) {
        matchingEventIndex = findMatchingBirthEventIndex(existingEvents, event);
      }

      if (matchingEventIndex) {
        event.birthEventId = existingEvents[*matchingEventIndex].birthEventId;
        if (!birthRepo.update(event)) {
          return failResult<LibraryExchangeStats>("library_db_update_failed", db.lastError());
        }
        existingEvents[*matchingEventIndex] = event;
        stats.birthEventsUpdated += 1;
      } else {
        if (!birthRepo.insert(event)) {
          return failResult<LibraryExchangeStats>("library_db_insert_failed", db.lastError());
        }
        existingEvents.push_back(event);
        stats.birthEventsInserted += 1;
      }
    }
  }

  if (!transaction.commit()) {
    return failResult<LibraryExchangeStats>("library_db_commit_failed", db.lastError());
  }
  return core::Result<LibraryExchangeStats>::success(stats);
}

}  // namespace

LibraryDatabaseExchange::LibraryDatabaseExchange(SQLiteDatabase& db) : m_db(db) {}

core::Result<LibraryExchangeStats> LibraryDatabaseExchange::exportToFile(
    const std::filesystem::path& path) const {
  std::error_code ec;
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      return failResult<LibraryExchangeStats>("library_json_write_failed", ec.message());
    }
  }

  const std::vector<domain::Person> people = loadActivePeople(m_db);
  BirthEventRepository birthRepo(m_db);
  LibraryExchangeStats stats;
  stats.peopleProcessed = static_cast<int>(people.size());

  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(15);
  out << "{\n";
  appendIndent(out, 1);
  out << "\"format\": \"asteria-library\",\n";
  appendIndent(out, 1);
  out << "\"version\": 1,\n";
  appendIndent(out, 1);
  out << "\"people\": [\n";

  for (size_t index = 0; index < people.size(); ++index) {
    const std::vector<domain::BirthEvent> events = birthRepo.findByPersonId(people[index].personId);
    stats.birthEventsProcessed += static_cast<int>(events.size());
    writePersonJson(out, people[index], events, 2);
    if (index + 1 < people.size()) {
      out << ',';
    }
    out << '\n';
  }

  appendIndent(out, 1);
  out << "]\n";
  out << "}\n";

  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return failResult<LibraryExchangeStats>(
        "library_json_write_failed",
        "Unable to write JSON file: " + path.string());
  }
  stream << out.str();
  if (!stream.good()) {
    return failResult<LibraryExchangeStats>(
        "library_json_write_failed",
        "Failed while writing JSON file: " + path.string());
  }

  return core::Result<LibraryExchangeStats>::success(stats);
}

core::Result<LibraryExchangeStats> LibraryDatabaseExchange::importFromFile(
    const std::filesystem::path& path) {
  auto records = loadLibraryFile(path);
  if (!records.ok()) {
    return failResult<LibraryExchangeStats>(records.error().code, records.error().message);
  }
  return applyImportedRecords(m_db, records.value(), true);
}

core::Result<LibraryExchangeStats> LibraryDatabaseExchange::mergeFromFile(
    const std::filesystem::path& path) {
  auto records = loadLibraryFile(path);
  if (!records.ok()) {
    return failResult<LibraryExchangeStats>(records.error().code, records.error().message);
  }
  return applyImportedRecords(m_db, records.value(), false);
}

}  // namespace asteria::data