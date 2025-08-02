#include "Ui.h"
#include "audio_player.h"
#include "imgui-impl-opengl3-loader.h"
#include "imgui-impl-opengl3.h"
#include "imgui-impl-sdl.h"
#include "tinyfiledialogs.h"
#include <filesystem>
#include <fstream>
#include <log/log.hpp>

#include "miniaudio.h"

Ui::Ui(sdl::Window &window,
       SDL_GLContext gl_context,
       Database &db,
       std::vector<Sample> &samples_data,
       AudioPlayerManager &audio_player_manager)
  : m_window(window),
    m_gl_context(gl_context),
    m_db(db),
    m_samples_data(samples_data),
    m_audio_player_manager(audio_player_manager),
    m_running(true),
    m_selected_sample_idx(-1)
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable; // Disable multi-viewports
  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForOpenGL(m_window.get(), m_gl_context);
  ImGui_ImplOpenGL3_Init("#version 130");
}

Ui::~Ui()
{
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
}

bool Ui::processEvent(SDL_Event &event)
{
  ImGui_ImplSDL2_ProcessEvent(&event);
  if (event.type == SDL_QUIT)
  {
    m_running = false;
    return true;
  }
  return false;
}

void Ui::render()
{
  // Clear the screen
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  // Create a full-window ImGui window to contain all content
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::Begin("MainUI", nullptr, window_flags);

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
          extract_metadata_and_insert(lTheOpenFileName);
        }
      }
      if (ImGui::MenuItem("Scan Directory"))
      {
        char const *lTheSelectedDirectory = tinyfd_selectFolderDialog("Select a directory to scan", "");
        if (lTheSelectedDirectory)
        {
          LOG("Selected directory: ", lTheSelectedDirectory);
          m_db.scan_directory(lTheSelectedDirectory);
        }
      }
      if (ImGui::MenuItem("Exit"))
      {
        m_running = false;
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  // Your table or main content
  ImGui::Text("Sound Samples");
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

    for (size_t i = 0; i < m_samples_data.size(); ++i)
    {
      ImGui::PushID(i); // Push unique ID for each row
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      if (ImGui::Selectable(m_samples_data[i].filepath.c_str(),
                            m_selected_sample_idx == (int)i,
                            ImGuiSelectableFlags_SpanAllColumns))
      {
        m_selected_sample_idx = i;
        m_audio_player_manager.play_audio_sample(m_samples_data[m_selected_sample_idx]);
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", m_samples_data[i].filename.c_str());
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%lld KB", m_samples_data[i].size / 1024);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%.2f s", m_samples_data[i].duration);
      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%d Hz", m_samples_data[i].sample_rate);
      ImGui::TableSetColumnIndex(5);
      ImGui::Text("%d bit", m_samples_data[i].bit_depth);
      ImGui::TableSetColumnIndex(6);
      ImGui::Text("%d channels", m_samples_data[i].channels);
      ImGui::TableSetColumnIndex(7);
      ImGui::Text("%s", m_samples_data[i].tags.c_str());
      ImGui::PopID(); // Pop the ID
    }
    ImGui::EndTable();
  }

  if (m_selected_sample_idx != -1)
  {
    ImGui::Separator();
    ImGui::Text("Selected Sample: %s", m_samples_data[m_selected_sample_idx].filename.c_str());
    if (ImGui::Button("Play"))
    {
      m_audio_player_manager.play_audio_sample(m_samples_data[m_selected_sample_idx]);
    }
  }

  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  m_window.glSwap();
}

void Ui::extract_metadata_and_insert(const char *filepath)
{
  Sample new_sample;
  new_sample.filepath = filepath;
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

  ma_decoder decoder;
  ma_result result = ma_decoder_init_file(filepath, NULL, &decoder);
  if (result != MA_SUCCESS)
  {
    LOG("Failed to open audio file: ", filepath);
    return;
  }

  ma_uint64 frame_count;
  if (ma_decoder_get_length_in_pcm_frames(&decoder, &frame_count) != MA_SUCCESS)
  {
    LOG("Failed to get duration for: ", filepath);
    ma_decoder_uninit(&decoder);
    return;
  }

  new_sample.duration = (double)frame_count / decoder.outputSampleRate;
  new_sample.sample_rate = decoder.outputSampleRate;
  new_sample.channels = decoder.outputChannels;
  new_sample.bit_depth = ma_get_bytes_per_sample(decoder.outputFormat) * 8;
  new_sample.tags = ""; // Empty tags for now

  m_db.insert_sample(new_sample);
  m_db.load_samples(m_samples_data); // Refresh data after insertion

  ma_decoder_uninit(&decoder);
}
