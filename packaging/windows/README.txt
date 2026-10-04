C++Ay - Windows x64 portable package

Extract the entire ZIP to a folder, then double-click C++Ay.exe.
Keep the DLLs, plugins, qml and qt.conf next to the executable.
No Qt or MinGW installation is needed on the destination computer.

Use Add... (L) for the native Windows multiple-file chooser.
Ctrl+A selects all files. Folder... includes subfolders.
Double-click a playlist row to play. X plays, C pauses, V stops,
Z/B select the previous/next track, and G opens the mixer.
Drag the cursor to seek. WAV export always starts at the beginning.
Launch without a file argument to resume the last active song and cursor.
Playback state, playlist and mixer are saved every 10 seconds and on exit.
A crash can lose roughly the last 10 seconds of progress.
The compact mixer groups channel sliders, chip/timing and output controls.

The console program aytool.exe supports inspect, render, trace and psg.
Example from a terminal:
  aytool.exe render "C:\Music\song.pt3" "C:\Music\song.wav"
Exports refuse to overwrite existing files.

The package is built for Windows x86-64 using MinGW GCC 13.1 and Qt 6.11.2.
Supported Qt platform: Windows 11 x64. Tested here under Wine on Linux;
a physical Windows machine has not been tested.

Current native formats: PT3, native PT3.7 TurboSound, PSG, YM3/YM3b.
Complete AY_Emul parity remains unfinished.
Project license: LICENSE. Attribution: THIRD_PARTY.md and AY_EMUL_NOTICE.txt.
Dependency notices: licenses/.
Source and rebuild instructions accompany the project README.md.
