================================================================================
                    RTRT TETRIS - PATH-TRACED ARCADE
================================================================================

Welcome to RTRT Tetris, a GPU path-traced multiplayer implementation of the
official Tetris Guideline standard featuring real-time global illumination,
procedural audio, and low-latency networked multiplayer.

--------------------------------------------------------------------------------
1. QUICK START
--------------------------------------------------------------------------------
1. Extract this archive completely into a folder on your computer.
   DO NOT run TetrisGame.exe directly from inside the ZIP archive.

2. Prerequisites:
   - 64-bit Windows 10 or Windows 11.
   - OpenGL 4.3+ capable GPU (NVIDIA GTX 900+ / AMD RX 400+ / Intel Iris Xe or newer).
   - Microsoft Visual C++ 2015-2022 Redistributable (x64).
     If missing, run 'vc_redist.x64.exe' included in this folder.

3. Launch:
   Double-click 'TetrisGame.exe'.

--------------------------------------------------------------------------------
2. CONTROLS
--------------------------------------------------------------------------------
Gameplay:
  Move Left / Right      : Left Arrow / Right Arrow (or A / D)
  Soft Drop              : Down Arrow (or S)
  Hard Drop (Instant)    : Spacebar
  Rotate Clockwise       : Up Arrow or X
  Rotate Counter-CW      : Z or Left Control
  Hold Piece             : C or Left Shift
  Pause / Game Menu      : Escape

Visuals & Camera:
  F1                     : Arcade Classic Camera (Orthographic Front)
  F2                     : Modern Isometric Camera
  F3                     : Free Orbit Camera (Left Click + Drag, Scroll to Zoom)
  F4                     : Hot-Swap Renderer (GPU Path Tracer <-> Deferred Rasterizer)
  F5                     : Toggle In-Game UI HUD

--------------------------------------------------------------------------------
3. MULTIPLAYER
--------------------------------------------------------------------------------
RTRT Tetris features client-server multiplayer powered by ENet UDP:
  - Port: 7777 (UDP)
  - UPnP: Enabled automatically for routers supporting Universal Plug and Play.
  - To Host: Select 'Multiplayer' -> 'Host Game' -> Choose player limit -> Start.
  - To Join: Select 'Multiplayer' -> 'Join Game' -> Enter Host IP & Port -> Connect.

--------------------------------------------------------------------------------
4. TROUBLESHOOTING
--------------------------------------------------------------------------------
- "Windows protected your PC" (SmartScreen):
  Click "More info" and select "Run anyway". Unsigned indie executables trigger
  this warning by default on Windows.

- "MSVCP140.dll / VCRUNTIME140.dll not found":
  Run the included 'vc_redist.x64.exe' installer to install the Microsoft Visual
  C++ runtime.

- Windows Firewall Prompt:
  Allow access on Private networks when prompted so the game can send and receive
  multiplayer game packets.

- Black screen or OpenGL error on startup:
  Ensure your graphics card drivers are up to date and support OpenGL 4.3 or higher.
================================================================================
