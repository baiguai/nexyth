#include <iostream>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"

#include "config/config.hpp"
#include "core/app.hpp"

int main(int argc, char** argv) {
  using nexyth::App;
  using nexyth::Config;
  std::string configPath = Config::defaultPath();
  if (argc > 1) configPath = argv[1];

  auto screen = ftxui::ScreenInteractive::Fullscreen();
  auto app = ftxui::Make<App>(&screen, configPath);
  screen.Loop(app);
  return 0;
}
