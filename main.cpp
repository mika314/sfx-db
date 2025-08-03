#include "Ui.h"
#include "audio_player.h"
#include "database.h"
#include "sample.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <log/log.hpp>
#include <sdlpp/sdlpp.hpp>
#include <string>
#include <vector>

int main(int /*argc*/, char ** /*argv*/)
{
  try
  {
    // Load window position and size from file
    int initial_window_x = SDL_WINDOWPOS_UNDEFINED;
    int initial_window_y = SDL_WINDOWPOS_UNDEFINED;
    int initial_window_w = 640;
    int initial_window_h = 480;
    std::string initial_filter;
    int initial_selected_sample_idx = -1;

    std::ifstream ifs("window_state.txt");
    if (ifs.is_open())
    {
      ifs >> initial_window_x;
      ifs >> initial_window_y;
      ifs >> initial_window_w;
      ifs >> initial_window_h;
      ifs >> initial_selected_sample_idx;
      // Consume the rest of the line after reading the numbers
      ifs.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      std::getline(ifs, initial_filter);
      ifs.close();
    }

    Database db("sfx.db");
    std::vector<Sample> samples_data;
    db.load_samples(samples_data);

    sdl::Init sdl(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    sdl::Window window("sfx-db",
                       initial_window_x,
                       initial_window_y,
                       initial_window_w,
                       initial_window_h,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    auto gl_context = SDL_GL_CreateContext(window.get());

        Ui ui(window, gl_context, db, samples_data, initial_filter, initial_selected_sample_idx);

    while (ui.isRunning())
    {
      SDL_Event event;
      while (SDL_PollEvent(&event))
      {
        ui.processEvent(event);
      }

      ui.render();
    }

    // Save window position and size to file
    int final_window_x, final_window_y, final_window_w, final_window_h;
    SDL_GetWindowPosition(window.get(), &final_window_x, &final_window_y);
    SDL_GetWindowSize(window.get(), &final_window_w, &final_window_h);

    std::ofstream ofs("window_state.txt");
    if (ofs.is_open())
    {
      ofs << final_window_x << std::endl;
      ofs << final_window_y << std::endl;
      ofs << final_window_w << std::endl;
      ofs << final_window_h << std::endl;
      ofs << ui.getSelectedSampleIdx() << std::endl;
      ofs << ui.getFilter() << std::endl;
      ofs.close();
    }
  }
  catch (const std::exception &e)
  {
    LOG("Error: ", e.what());
    return 1;
  }

  // db is automatically closed by its destructor

  return 0;
}
