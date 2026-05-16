**Configuration:**
1. Right click RAS Game and click properties
2. Go to General and select C++ Language Standard as ISO C++17 Standard (/std:c++17)
3. Go to C/C++, General, Additional Include Directories, All Configurations, put $(SolutionDir)\External\include;%(AdditionalIncludeDirectories)
4. Go to Linker, General, Additional Include Directories, All Configurations, put $(SolutionDir)\External\lib;%(AdditionalLibraryDirectories)
5. Go to Linker, Input, Additional Dependencies, Release, put sfml-window.lib;sfml-system.lib;sfml-graphics.lib;sfml-audio.lib;sfml-network.lib;%(AdditionalDependencies)
6. Go to Linker, Input, Additional Dependencies, Debug, put sfml-window-d.lib;sfml-system-d.lib;sfml-graphics-d.lib;sfml-audio-d.lib;sfml-network-d.lib;%(AdditionalDependencies)
7. Click Apply
8. Start from Version 6, the configuration to activate the game is release, not debug
9. You are done!
