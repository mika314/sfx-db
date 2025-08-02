#pragma once

#include "database.h"
#include "sample.h"
#include "audio_player.h"
#include <imgui/imgui.h>
#include <sdlpp/sdlpp.hpp>
#include <vector>

class Ui
{
public:
  Ui(sdl::Window &window, SDL_GLContext gl_context, Database &db, std::vector<Sample> &samples_data, AudioPlayerManager& audio_player_manager);
  ~Ui();

  bool processEvent(SDL_Event &event);
  void render();
  bool isRunning() const { return m_running; }

private:
  void extract_metadata_and_insert(const char *filepath);
  sdl::Window &m_window;
  SDL_GLContext m_gl_context;
  Database &m_db;
  std::vector<Sample> &m_samples_data;
  AudioPlayerManager& m_audio_player_manager;
  bool m_running;
  int m_selected_sample_idx;
};
