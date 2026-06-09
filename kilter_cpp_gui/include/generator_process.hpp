#pragma once

#include <string>

// Long-lived Python inference worker (fork/exec + pipe IPC).
class GeneratorProcess {
 public:
  GeneratorProcess() = default;
  ~GeneratorProcess();

  GeneratorProcess(const GeneratorProcess&) = delete;
  GeneratorProcess& operator=(const GeneratorProcess&) = delete;

  bool running() const { return pid_ > 0; }
  const std::string& last_error() const { return last_error_; }

  bool start(const std::string& python_path,
             const std::string& script_path,
             const std::string& token_map_path,
             const std::string& id_map_path,
             const std::string& working_dir);

  bool request(const std::string& json_line, std::string& response_out);
  void stop();

 private:
  bool read_line(std::string& out);

  pid_t pid_ = -1;
  int to_child_ = -1;
  int from_child_ = -1;
  FILE* writer_ = nullptr;
  FILE* reader_ = nullptr;
  std::string last_error_;
};
