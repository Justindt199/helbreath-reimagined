Helbreath client: DirectX -> SFML 3 port
=========================================

The client no longer uses DirectDraw, DirectInput or DirectSound. All of it now goes
through SFML 3 (window, rendering, mouse, audio, image and font loading).

What changed
------------
  Old (DirectX folder)                   New (Engine folder)
  DXC_ddraw   (DirectDraw 7)          -> Renderer   (sf::RenderWindow + sf::Texture, sf::Font text)
  IDirectDrawSurface7                 -> Surface    (plain RGB565 memory surface, BMP loading via sf::Image)
  DXC_dinput  (DirectInput)           -> Mouse      (SFML mouse events)
  YWSound / SoundBuffer (DirectSound) -> AudioDevice / SoundBuffer (sf::SoundBuffer + sf::Sound)
  Mydib       (GDI BMP painter)       -> removed (Surface::CreateFromBitmapFile)
  DXC_dsound, DDraw.h, DInput.h, DSound.h, d3dtypes.h, DDRAW/DINPUT/DSOUND/DXGUID.LIB -> removed
  Sprite, SpriteID, SoundID, TileSpr, Effect -> moved to Engine (Sprite ported to CSurface)

The game still renders in software into a 640x480 RGB565 back buffer, so all sprite
effects, transparency tables and colours look exactly as before. Each frame that
buffer is uploaded to an SFML texture and drawn scaled to the window.

Game code changes: m_DDraw -> m_Renderer, m_DInput -> m_Mouse, m_DSound -> m_Audio,
"surface lost / RestoreSprites" handling removed (memory surfaces are never lost),
GDI text calls replaced by the renderer's SFML text functions.

Setting up SFML (one time)
--------------------------
1. Download SFML 3.0.2, "Visual C++ 17 (2022) - 32-bit", from
   https://www.sfml-dev.org/download/sfml/3.0.2/
2. Extract it so this folder exists:
       D:\Helbreath Unrivaled\Helbreath Unrivaled\SFML-3.0.2\include
   (that is, next to the "lib" folder the project already uses), OR put it anywhere
   and set an environment variable SFML_DIR to that folder, then restart Visual Studio.
3. Open "All Build.sln" and build Client (Debug or Release, Win32).

The project now uses C++17 and the DLL runtime (/MD, /MDd), which SFML requires.
A post-build step copies the SFML DLLs next to Client.exe. When you ship the client,
include these four files with it:
    Release: sfml-graphics-3.dll, sfml-window-3.dll, sfml-audio-3.dll, sfml-system-3.dll
    Debug:   the same names with -d (sfml-graphics-d-3.dll, ...)
Players also need the Microsoft Visual C++ 2015-2022 Redistributable (x86), which most
PCs already have.

Removing the old DirectX files
------------------------------
Run remove_old_directx.bat in this folder. It deletes Client3\DirectX (including the
stale duplicate copies of the client inside it), Client3\Map\DirectX and
Client3\Res\DirectX. None of those are part of the build any more. If you use SVN,
"svn delete" those folders instead.
Client.sln / Client.vcproj are the old Visual Studio 2005 project and still list the
DirectX files. "All Build.sln" uses Client.vcxproj, which is the one that was updated.

Behaviour notes
---------------
* Fullscreen is now borderless fullscreen at the desktop resolution (no display mode
  switch), with the 640x480 image scaled up and letterboxed. Alt+Enter toggles
  fullscreen/windowed at any time, and the choice is saved for next launch (registry
  value "Fullscreen" next to the game's other settings).
* Windowed mode opens at 2x (1280x960) when it fits on screen, otherwise 1x.
  CRenderer::SetWindowScale(n) forces a scale.
* Text uses Trebuchet MS (as before). To ship your own font, put it at
  fonts\default.ttf in the game folder. In Engine\Renderer.cpp, kTextAntialias = false
  restores the old hard-edged GDI look and kFontCharSize sets the size.
* Screenshots are still saved as SAVE\*.bmp.
* Keyboard input, the chat/IME edit box and networking are Win32/Winsock, not DirectX,
  and keep working unchanged: SFML attaches to the game's existing window and passes
  every message on to the game's WndProc.
