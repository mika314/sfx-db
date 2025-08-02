#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "imgui-impl-opengl3.h"
#include "imgui-impl-sdl.h"
#include "tinyfiledialogs.h"
#include <imgui/imgui.h>
#include <log/log.hpp>
#include <sdlpp/sdlpp.hpp>

#include "audio_player.h"
#include "database.h"
#include "sample.h"

#include "audio_decoder.h"

int main(int /*argc*/, char ** /*argv*/)
{
  try
  {
    Database db("sfx.db");
    std::vector<Sample> samples_data;
    db.load_samples(samples_data);

    sdl::Init sdl(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    SDL_AudioSpec wanted_spec, audio_spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = 44100;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = 2;
    wanted_spec.samples = 4096;
    auto audio_device = sdl::Audio{NULL, 0, &wanted_spec, &audio_spec, 0, [](Uint8 *stream, int len) {
                                     return audio_callback(stream, len);
                                   }};
    audio_device.pause(0); // Start audio playback
    sdl::Window window(
      "sfx-db", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_OPENGL);

    auto gl_context = SDL_GL_CreateContext(window.get());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window.get(), gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    bool running = true;
    static int selected_sample_idx = -1;
    while (running)
    {
      SDL_Event event;
      while (SDL_PollEvent(&event))
      {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT)
        {
          running = false;
        }
      }

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplSDL2_NewFrame();
      ImGui::NewFrame();

      if (ImGui::BeginMainMenuBar())
      {
        if (ImGui::BeginMenu("File"))
        {
          if (ImGui::MenuItem("Add Sample"))
          {
            char const *lTheOpenFileName =
              tinyfd_openFileDialog("Select a sound file", "", 0, NULL, NULL, 0);
            if (lTheOpenFileName)
            {
              LOG("Selected file: ", lTheOpenFileName);

              Sample new_sample;
              new_sample.filepath = lTheOpenFileName;
              std::filesystem::path p(new_sample.filepath);
              new_sample.filename = p.filename().string();

              try
              {
                new_sample.size = std::filesystem::file_size(p);
              }
              catch (const std::filesystem::filesystem_error &e)
              {
                LOG("Error getting file size: ", e.what());
                new_sample.size = 0;
              }

              AVFormatContext *pFormatCtx = NULL;
              double duration = 0.0;
              int sample_rate = 0;
              int channels = 0;
              int bit_depth = 0;
              int audio_stream_idx = -1;

              if (avformat_open_input(&pFormatCtx, lTheOpenFileName, NULL, NULL) != 0)
              {
                LOG("Couldn't open input stream.");
              }
              else
              {
                if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
                {
                  LOG("Couldn't find stream information.");
                }
                else
                {
                  duration = (double)pFormatCtx->duration / AV_TIME_BASE;
                  LOG("Duration: ", duration);
                  for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
                  {
                    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                    {
                      audio_stream_idx = i;
                      sample_rate = pFormatCtx->streams[i]->codecpar->sample_rate;
                      channels = pFormatCtx->streams[i]->codecpar->channels;
                      bit_depth = av_get_bits_per_sample(pFormatCtx->streams[i]->codecpar->codec_id);
                      LOG("Sample Rate: ", sample_rate);
                      LOG("Channels: ", channels);
                      LOG("Bit Depth: ", bit_depth);
                      break;
                    }
                  }
                }
                avformat_close_input(&pFormatCtx);
              }

              if (audio_stream_idx != -1)
              { // Only insert if audio stream found
                new_sample.duration = duration;
                new_sample.sample_rate = sample_rate;
                new_sample.bit_depth = bit_depth;
                new_sample.channels = channels;
                new_sample.tags = ""; // Empty tags for now
                db.insert_sample(new_sample);
                db.load_samples(samples_data); // Refresh data after insertion
              }
              else
              {
                LOG("No audio stream found in selected file. Not inserting into database.");
              }
            }
          }
          if (ImGui::MenuItem("Scan Directory"))
          {
            char const *lTheSelectedDirectory =
              tinyfd_selectFolderDialog("Select a directory to scan", "");
            if (lTheSelectedDirectory)
            {
              LOG("Selected directory: ", lTheSelectedDirectory);
              db.scan_directory(lTheSelectedDirectory);
            }
          }
          if (ImGui::MenuItem("Exit"))
          {
            running = false;
          }
          ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
      }

      ImGui::Begin("Sound Samples");

      if (ImGui::BeginTable("samples", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
      {
        ImGui::TableSetupColumn("Filepath");
        ImGui::TableSetupColumn("Filename");
        ImGui::TableSetupColumn("Size");
        ImGui::TableSetupColumn("Duration");
        ImGui::TableSetupColumn("Sample Rate");
        ImGui::TableSetupColumn("Bit Depth");
        ImGui::TableSetupColumn("Channels");
        ImGui::TableSetupColumn("Tags");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < samples_data.size(); ++i)
        {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          if (ImGui::Selectable(samples_data[i].filepath.c_str(),
                                selected_sample_idx == (int)i,
                                ImGuiSelectableFlags_SpanAllColumns))
          {
            selected_sample_idx = i;
          }
          ImGui::TableSetColumnIndex(1);
          ImGui::Text("%s", samples_data[i].filename.c_str());
          ImGui::TableSetColumnIndex(2);
          ImGui::Text("%lld KB", samples_data[i].size / 1024);
          ImGui::TableSetColumnIndex(3);
          ImGui::Text("%.2f s", samples_data[i].duration);
          ImGui::TableSetColumnIndex(4);
          ImGui::Text("%d Hz", samples_data[i].sample_rate);
          ImGui::TableSetColumnIndex(5);
          ImGui::Text("%d bit", samples_data[i].bit_depth);
          ImGui::TableSetColumnIndex(6);
          ImGui::Text("%d channels", samples_data[i].channels);
          ImGui::TableSetColumnIndex(7);
          ImGui::Text("%s", samples_data[i].tags.c_str());
        }
        ImGui::EndTable();
      }

      if (selected_sample_idx != -1)
      {
        ImGui::Separator();
        ImGui::Text("Selected Sample: %s", samples_data[selected_sample_idx].filename.c_str());
        if (ImGui::Button("Play"))
        {
          play_audio_sample(samples_data[selected_sample_idx]);
        }
      }

      ImGui::End();

      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      window.glSwap();
    }
  }
  catch (const std::exception &e)
  {
    LOG("Error: ", e.what());
    return 1;
  }

  // db is automatically closed by its destructor

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  return 0;
}
