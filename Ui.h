#pragma once

#include "audio_player.h"
#include "database.h"
#include "sample.h"
#include <imgui/imgui.h>
#include <sdlpp/sdlpp.hpp>
#include <vector>

class Ui
{
public:
  Ui(sdl::Window &window,
     SDL_GLContext gl_context,
     Database &db,
     std::vector<Sample> &samples_data,
     const std::string &initial_filter,
     int initial_selected_sample_idx);
  ~Ui();

  bool processEvent(SDL_Event &event);
  void render();
  bool isRunning() const { return m_running; }
  std::string getFilter() const { return filter; }
  int getSelectedSampleIdx() const { return m_selected_sample_idx; }

private:
  void extract_metadata_and_insert(const char *filepath);
  auto playAndClipboardSample() -> void;
  sdl::Window &m_window;
  SDL_GLContext m_gl_context;
  Database &m_db;
  std::vector<Sample> &m_samples_data;
  AudioPlayer m_audio_player;
  bool m_running;
  int m_selected_sample_idx;
  std::string filter;
  bool m_scroll_to_selected = false;
};
