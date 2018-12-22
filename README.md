## angrylion-rdp README

forked/imported from https://sourceforge.net/p/angrylions-stuff/code/HEAD/tree/ <br />
This unofficial fork is a pure hobbyist project, use it at your own risk.

### Changes from upstream so far
* Added support for mupen64plus api
* Switched to C language files
* Updated msvc project files to msvc2015
* Slightly restructured repository folders (based on typical folder structure of mupen64plus plugins)
* Cosmetic code changes (currently mainly whitespace and eol fixes)

### Build notes
* For mupen64plus builds use the newly added Makefile in `projects/unix/` (requires mupen64plus api headers and SDL2-devel)
* For Project64 builds use the updated msvc project files in `projects/msvc2015/`
