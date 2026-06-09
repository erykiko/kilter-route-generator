bool GeneratorProcess::start(...) {
  int in_pipe[2] = {-1, -1};
  int out_pipe[2] = {-1, -1};
  if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
    last_error_ = "Failed to create pipes for generator process.";
    return false;
  }

  pid_t pid = fork();
  if (pid == 0) {
    dup2(in_pipe[0], STDIN_FILENO);
    dup2(out_pipe[1], STDOUT_FILENO);
    execl(python_path.c_str(), python_path.c_str(), script_path.c_str(),
          "--token-map", token_map_path.c_str(), "--id-map", id_map_path.c_str(),
          static_cast<char*>(nullptr));
    _exit(127);
  }
  // handshake: oczekiwanie na {"status":"ready"}
}

bool GeneratorProcess::request(const std::string& json_line, std::string& response_out) {
  fputs(json_line.c_str(), writer_);
  fputc('\n', writer_);
  fflush(writer_);
  return read_line(response_out);
}
