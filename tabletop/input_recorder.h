#pragma once
#include <string>
#include <vector>

#include "tabletop.h"

// Mode of the input pipeline. Live = read raylib every frame.
// Record = read raylib AND accumulate the frame into memory; flush on exit.
// Playback = ignore raylib and feed frames from a previously recorded file.
enum class Input_Mode { Live, Record, Playback };

struct Input_Feed {
  Input_Mode         mode = Input_Mode::Live;
  std::string        path;  // Output path (Record) or input path (Playback).
  std::vector<Input> frames;
  // Index of the next frame to replay (Playback only).
  std::size_t playback_index = 0;
  // True when playback has consumed every frame — main loop should exit.
  bool exhausted = false;

  Input_Feed(Input_Mode mode, const std::string& path);
};

// Populates `rec` for the requested mode. In Playback mode, loads the JSON
// file into `frames` immediately so the loop can stream from memory.

// Returns the input for this frame:
//   Live     → capture_input() pass-through.
//   Record   → capture_input(), pushed onto frames.
//   Playback → next entry from frames; flips `exhausted` after the last one.
Input next_input(Input_Feed& rec);

// Record mode: writes the accumulated frames to `rec.path` via save_to_json.
// No-op in other modes.
void finalize_input_recorder(const Input_Feed& rec);
