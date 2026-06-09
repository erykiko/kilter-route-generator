#pragma once

#include <string>

bool json_get_string(const std::string& text, const std::string& key, std::string& out);
bool json_get_array(const std::string& text, const std::string& key, std::string& out);
