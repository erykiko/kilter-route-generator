#pragma once

#include <set>
#include <string>
#include <vector>

std::string trim(const std::string& s);
std::set<int> parse_python_list_ints(const std::string& list_text);
std::vector<int> parse_python_list_ints_ordered(const std::string& list_text);
std::string join_ints(const std::vector<int>& values, char sep);
std::string join_strings(const std::vector<std::string>& values, char sep);
std::vector<std::string> list_checkpoints(const std::string& dir_path);
std::string run_command_capture_stdout(const std::string& cmd, int& exit_code);
std::string now_timestamp();
