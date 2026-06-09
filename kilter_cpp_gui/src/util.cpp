#include "util.hpp"

#include <sys/wait.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

std::string trim(const std::string& s) {
  size_t start = 0;
  while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
    ++start;
  }
  size_t end = s.size();
  while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
    --end;
  }
  return s.substr(start, end - start);
}

std::set<int> parse_python_list_ints(const std::string& list_text) {
  std::set<int> out;
  std::string cleaned;
  cleaned.reserve(list_text.size());
  for (char ch : list_text) {
    if (ch == '[' || ch == ']') {
      continue;
    }
    cleaned.push_back(ch);
  }
  std::stringstream ss(cleaned);
  std::string token;
  while (std::getline(ss, token, ',')) {
    token = trim(token);
    if (token.empty()) {
      continue;
    }
    try {
      out.insert(std::stoi(token));
    } catch (...) {
    }
  }
  return out;
}

std::vector<int> parse_python_list_ints_ordered(const std::string& list_text) {
  std::vector<int> out;
  std::string cleaned;
  cleaned.reserve(list_text.size());
  for (char ch : list_text) {
    if (ch == '[' || ch == ']') {
      continue;
    }
    cleaned.push_back(ch);
  }
  std::stringstream ss(cleaned);
  std::string token;
  while (std::getline(ss, token, ',')) {
    token = trim(token);
    if (token.empty()) {
      continue;
    }
    try {
      out.push_back(std::stoi(token));
    } catch (...) {
    }
  }
  return out;
}

std::string join_ints(const std::vector<int>& values, char sep) {
  std::string out;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) out.push_back(sep);
    out += std::to_string(values[i]);
  }
  return out;
}

std::string join_strings(const std::vector<std::string>& values, char sep) {
  std::string out;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) out.push_back(sep);
    out += values[i];
  }
  return out;
}

std::vector<std::string> list_checkpoints(const std::string& dir_path) {
  std::vector<std::string> out;
  std::error_code ec;
  if (!fs::exists(dir_path, ec) || !fs::is_directory(dir_path, ec)) {
    return out;
  }
  for (const auto& entry : fs::directory_iterator(dir_path, ec)) {
    if (entry.is_regular_file() && entry.path().extension() == ".pt") {
      out.push_back(entry.path().string());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

std::string run_command_capture_stdout(const std::string& cmd, int& exit_code) {
  std::array<char, 4096> buffer{};
  std::string output;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    exit_code = -1;
    return "Failed to start process";
  }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }
  int rc = pclose(pipe);
  exit_code = WEXITSTATUS(rc);
  return output;
}

std::string now_timestamp() {
  std::time_t t = std::time(nullptr);
  std::tm tm_buf{};
  localtime_r(&t, &tm_buf);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
  return std::string(buf);
}
