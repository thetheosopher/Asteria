#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace asteria::domain {

struct Person {
  std::int64_t personId = 0;
  std::string fullName;
  std::string displayName;
  std::string gender;
  std::vector<std::string> tags;
  std::string notes;
  std::string createdAt;
  std::string updatedAt;
  std::string archivedAt;
};

}  // namespace asteria::domain
