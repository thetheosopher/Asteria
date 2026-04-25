#pragma once
#include <string>
#include <utility>
#include <variant>

namespace asteria::core {

struct Error {
  std::string code;
  std::string message;
};

template <typename T>
class Result {
 public:
  static Result<T> success(T value) { return Result<T>(std::move(value)); }
  static Result<T> failure(Error error) { return Result<T>(std::move(error)); }

  bool ok() const { return std::holds_alternative<T>(m_value); }
  const T& value() const { return std::get<T>(m_value); }
  T& value() { return std::get<T>(m_value); }
  const Error& error() const { return std::get<Error>(m_value); }

 private:
  explicit Result(T value) : m_value(std::move(value)) {}
  explicit Result(Error error) : m_value(std::move(error)) {}
  std::variant<T, Error> m_value;
};

}  // namespace asteria::core
