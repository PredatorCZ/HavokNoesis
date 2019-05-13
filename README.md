# HavokNoesis
HavokNoesis is Havok importer/exporter for Noesis.\
Builded with VS2015.

## Building
You must have set ***Working Directory*** under ***Debugging*** in ***Project Properties*** to location, where Noesis is installed (where noesis.exe is). \
If not set, then builded .dll file will be copied into $(ProjectDir), rather than noesis plugins directory.

## Installation
### [Latest Release](https://github.com/PredatorCZ/HavokNoesis/releases/)
Move corresponding .dll file into ***%noesis installation directory%/plugins***. \
Additionally plugin will require **Visual C++ Redistributable for Visual Studio 2015** to be installed in order to work.

## License
This plugin is available under GPL v3 license. (See LICENSE.md)

This plugin uses following libraries:

* HavokLib, Copyright (c) 2016-2019 Lukas Cone
