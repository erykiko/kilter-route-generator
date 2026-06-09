#include "generator_process.hpp"

#include <unistd.h>
#include <sys/wait.h>

#include <cstdio>
#include <cstdlib>

GeneratorProcess::~GeneratorProcess() { stop(); }

bool GeneratorProcess::start(const std::string& python_path,
                             const std::string& script_path,
                             const std::string& token_map_path,
                             const std::string& id_map_path,
                             const std::string& working_dir) {
  if (running()) {
    return true;
  }
  last_error_.clear();

  int in_pipe[2] = {-1, -1};
  int out_pipe[2] = {-1, -1};
  if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
    last_error_ = "Failed to create pipes for generator process.";
    return false;
  }

  pid_t pid = fork();
  if (pid < 0) {
    last_error_ = "fork() failed for generator process.";
    ::close(in_pipe[0]);
    ::close(in_pipe[1]);
    ::close(out_pipe[0]);
    ::close(out_pipe[1]);
    return false;
  }

  if (pid == 0) {
    dup2(in_pipe[0], STDIN_FILENO);
    dup2(out_pipe[1], STDOUT_FILENO);
    ::close(in_pipe[0]);
    ::close(in_pipe[1]);
    ::close(out_pipe[0]);
    ::close(out_pipe[1]);
    if (!working_dir.empty()) {
      if (chdir(working_dir.c_str()) != 0) {
        _exit(126);
      }
    }
    execl(python_path.c_str(), python_path.c_str(), script_path.c_str(),
          "--token-map", token_map_path.c_str(), "--id-map", id_map_path.c_str(),
          static_cast<char*>(nullptr));
    _exit(127);
  }

  ::close(in_pipe[0]);
  ::close(out_pipe[1]);
  pid_ = pid;
  to_child_ = in_pipe[1];
  from_child_ = out_pipe[0];
  writer_ = fdopen(to_child_, "w");
  reader_ = fdopen(from_child_, "r");
  if (writer_ == nullptr || reader_ == nullptr) {
    last_error_ = "Failed to open generator pipe streams.";
    stop();
    return false;
  }

  std::string handshake;
  if (!read_line(handshake) || handshake.find("\"ready\"") == std::string::npos) {
    last_error_ = handshake.empty()
                        ? "Generator worker did not start (check Python env)."
                        : ("Generator handshake failed: " + handshake);
    stop();
    return false;
  }
  return true;
}

bool GeneratorProcess::request(const std::string& json_line, std::string& response_out) {
  if (!running() || writer_ == nullptr || reader_ == nullptr) {
    last_error_ = "Generator process is not running.";
    return false;
  }
  if (fputs(json_line.c_str(), writer_) == EOF || fputc('\n', writer_) == EOF ||
      fflush(writer_) != 0) {
    last_error_ = "Failed to send request to generator process.";
    stop();
    return false;
  }
  if (!read_line(response_out)) {
    last_error_ = "Generator process closed the connection.";
    stop();
    return false;
  }
  return true;
}

void GeneratorProcess::stop() {
  if (writer_ != nullptr) {
    fputs("{\"cmd\":\"shutdown\"}\n", writer_);
    fflush(writer_);
    fclose(writer_);
    writer_ = nullptr;
    to_child_ = -1;
  }
  if (reader_ != nullptr) {
    fclose(reader_);
    reader_ = nullptr;
    from_child_ = -1;
  }
  if (pid_ > 0) {
    int status = 0;
    waitpid(pid_, &status, 0);
    pid_ = -1;
  }
}

bool GeneratorProcess::read_line(std::string& out) {
  out.clear();
  if (reader_ == nullptr) {
    return false;
  }
  int ch;
  while ((ch = fgetc(reader_)) != EOF) {
    if (ch == '\n') {
      return true;
    }
    out.push_back(static_cast<char>(ch));
  }
  return !out.empty();
}
