#include <iostream>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"

#include "config/config.hpp"
#include "core/app.hpp"

// Custom Developer Documentation //////////////////////////////////////////////

/*[/Introduction]
    Nexyth is not really a multiplexer, however it is the front end that you usually see with them.
    You can create panes and tabs etc.

    HOWEVER!
    Nexyth is REALLY fast - because it uses MODES like Vim does!
    So there are a LOT of actions you can take that require a single key press!
*/

/*[/Scripts]
    Nexyth ships with some helpful scripts:

    * app.sh
        This script JUST run Nexyth. It doesn't do a build etc.

    * build-windows.sh
        Builds the Windows executable version of Nexyth.

    * build.sh
        Builds Nexyth - if you call the script followed by the parameter 'r' like:
        ./build.sh r
        It will do a release build.
        (note: when you do a build, this help system is re-generated)

    * leakcheck.sh
        Run this - and do work in Nexyth for a bit - when you exit it will report any potential memory leaks.

    * run.sh
        This will do a build and then run Nexyth from that build.

    * setup.sh
        This will ensure that the dependencies are installed on your machine (Linux obviously).

    * upgrade.sh
        This script will upgrade the dependencies that Nexyth uses.
*/

////////////////////////////////////////////////////////////////////////////////




//!_structure=/Project Structure=../
//!_coderoot=/Source Code

/*!
    Main Method

    !_method

    ----
*/
//>>
int main(int argc, char** argv)
{
    using nexyth::App;
    using nexyth::Config;
    std::string configPath = Config::defaultPath();

    if (argc > 1) configPath = argv[1];

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto app = ftxui::Make<App>(&screen, configPath);
    screen.Loop(app);

    return 0;
}
//<<
//!
