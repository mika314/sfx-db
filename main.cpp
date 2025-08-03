#include "Ui.h"
#include "audio_player.h"
#include "database.h"
#include "sample.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <log/log.hpp>
#include <msgpack/msgpack-ser.hpp>
#include <sdlpp/sdlpp.hpp>
#include <ser/macro.hpp>
#include <string>
#include <vector>

struct Cfg
{
  int window_x = SDL_WINDOWPOS_UNDEFINED;
  int window_y = SDL_WINDOWPOS_UNDEFINED;
  int window_w = 640;
  int window_h = 480;
  std::string filter;
  int selected_sample_idx = -1;
  SER_PROPS(window_x, window_y, window_w, window_h, filter, selected_sample_idx);
};

int main(int /*argc*/, char ** /*argv*/)
{
  try
  {

    char *pref_path_c = SDL_GetPrefPath("sfx-db", "sfx-db");
    std::string pref_path = "";
    if (pref_path_c)
    {
      pref_path = pref_path_c;
      SDL_free(pref_path_c);
    }
    std::string config_file_path = pref_path + "sfx-db.cfg";

    Cfg cfg;
    {
      std::ifstream ifs(config_file_path);
      if (ifs.is_open())
        msgpackDeser(ifs, cfg);
    }

    Database db("sfx.db");
    std::vector<Sample> samples_data;
    db.load_samples(samples_data);

    sdl::Init sdl(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    sdl::Window window("sfx-db",
                       cfg.window_x,
                       cfg.window_y,
                       cfg.window_w,
                       cfg.window_h,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    auto gl_context = SDL_GL_CreateContext(window.get());

    Ui ui(window, gl_context, db, samples_data, cfg.filter, cfg.selected_sample_idx);

    while (ui.isRunning())
    {
      SDL_Event event;
      while (SDL_PollEvent(&event))
      {
        ui.processEvent(event);
      }

      ui.render();
    }

    std::ofstream ofs(config_file_path);
    if (ofs.is_open())
    {
      SDL_GetWindowPosition(window.get(), &cfg.window_x, &cfg.window_y);
      SDL_GetWindowSize(window.get(), &cfg.window_w, &cfg.window_h);
      cfg.selected_sample_idx = ui.getSelectedSampleIdx();
      cfg.filter = ui.getFilter();
      msgpackSer(ofs, cfg);
    }
  }
  catch (const std::exception &e)
  {
    LOG("Error: ", e.what());
    return 1;
  }

  return 0;
}
