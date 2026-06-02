#include "input_recorder.h"

#include <cstdio>
#include <fstream>

#include "../struct/serialize.h"

Input_Feed::Input_Feed(Input_Mode mode, const std::string& path) {
  this->mode = mode;
  this->path = path;
  this->frames.clear();
  this->playback_index = 0;
  this->exhausted      = false;

  if (mode == Input_Mode::Playback) {
    this->frames = load_struct<std::vector<Input>>(path);
    fprintf(
      stderr,
      "[input_recorder] playback loaded %zu frames from %s\n",
      this->frames.size(),
      path.c_str()
    );
  } else if (mode == Input_Mode::Record) {
    fprintf(stderr, "[input_recorder] recording to %s\n", path.c_str());
  }
}

Input next_input(Input_Feed& rec) {
  switch (rec.mode) {
    case Input_Mode::Live: {
      return capture_input();
    }
    case Input_Mode::Record: {
      Input in = capture_input();
      rec.frames.push_back(in);
      save_struct(rec.frames, rec.path);
      return in;
    }
    case Input_Mode::Playback: {
      if (rec.playback_index >= rec.frames.size()) {
        rec.exhausted = true;
        // Return a zeroed Input so any single-frame leftover work is benign.
        return Input{};
      }
      return rec.frames[rec.playback_index++];
    }
  }
  return Input{};
}

void finalize_input_recorder(const Input_Feed& rec) {
  if (rec.mode != Input_Mode::Record) return;
  save_struct(rec.frames, rec.path);
  fprintf(
    stderr,
    "[input_recorder] wrote %zu frames to %s\n",
    rec.frames.size(),
    rec.path.c_str()
  );
}
