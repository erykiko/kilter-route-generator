if (!state->generator.running()) {
  state->generator.start(state->python_path,
                         state->generate_server_script_path,
                         state->token_map_path,
                         state->id_map_path,
                         state->repo_root);
}

std::stringstream req;
req << "{\"cmd\":\"generate\","
    << "\"checkpoint\":\"" << state->checkpoint_path << "\","
    << "\"layout_id\":" << layout_id << ","
    << "\"grade_id\":" << grade_id << ","
    << "\"greedy\":" << (greedy ? "true" : "false") << "}";

std::string response;
state->generator.request(req.str(), response);
// parsowanie JSON: tokens / hole_ids -> rysowanie trasy
