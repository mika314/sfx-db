#pragma once

#include "database.h"
#include "sample.h"
#include <imgui/imgui.h>
#include <sdlpp/sdlpp.hpp>
#include <vector>

class Ui
{
public:
  Ui(sdl::Window &window, SDL_GLContext gl_context, Database &db, std::vector<Sample> &samples_data);
  ~Ui();

  bool processEvent(SDL_Event &event);
  void render();
  void shutdown();
  bool isRunning() const { return m_running; }

private:
  sdl::Window &m_window;
  SDL_GLContext m_gl_context;
  Database &m_db;
  std::vector<Sample> &m_samples_data;
  bool m_running;
  int m_selected_sample_idx;
};
