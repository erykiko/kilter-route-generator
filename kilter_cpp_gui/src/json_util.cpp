#include "json_util.hpp"

bool json_get_string(const std::string& text, const std::string& key, std::string& out) {
  const std::string pat = "\"" + key + "\"";
  size_t k = text.find(pat);
  if (k == std::string::npos) return false;
  size_t colon = text.find(':', k + pat.size());
  if (colon == std::string::npos) return false;
  size_t q1 = text.find('"', colon + 1);
  if (q1 == std::string::npos) return false;
  size_t q2 = text.find('"', q1 + 1);
  if (q2 == std::string::npos) return false;
  out = text.substr(q1 + 1, q2 - q1 - 1);
  return true;
}

bool json_get_array(const std::string& text, const std::string& key, std::string& out) {
  const std::string pat = "\"" + key + "\"";
  size_t k = text.find(pat);
  if (k == std::string::npos) return false;
  size_t lb = text.find('[', k + pat.size());
  if (lb == std::string::npos) return false;
  size_t rb = text.find(']', lb);
  if (rb == std::string::npos) return false;
  out = text.substr(lb, rb - lb + 1);
  return true;
}
