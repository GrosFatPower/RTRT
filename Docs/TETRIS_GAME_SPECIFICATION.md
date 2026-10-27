# RTRT Tetris Game Specification Document

**Target Application:** `Games/TetrisGame`  
**Branch:** `Antigravity-tetris`  
**Engine:** RTRTCore (`PathTracer`, `DeferredRenderer`, `SoftwareRasterizer`)  
**Specification Version:** 1.0.0  
**Compliance Standard:** Official Tetris Guideline  

---

## 1. Executive Summary & Goals

The objective is to implement a feature-complete, tournament-grade, guideline-compliant Tetris game within the RTRT repository as a standalone game (`Games/TetrisGame`).

### Core Objectives
1. **100% Tetris Guideline Compliance:** Faithfully implement all standard game mechanics including the Super Rotation System (SRS) with full wall kicks, 7-bag randomizer, hold queue, ghost piece, extended lock delay, T-spin detection (3-corner rule), guideline scoring, back-to-back bonuses, and combo tables.
2. **Showcase RTRT Rendering Backends:** Present the game in an immersive 3D scene with beveled tetrominoes, physical materials (glass, glossy plastic, emissive neon), and dynamic lighting that seamlessly switches between:
   - **DeferredRenderer:** Real-time PBR lighting, point lights parented to active pieces, bloom, reflections, and shadow mapping.
   - **PathTracer:** Photorealistic ray-traced glass blocks, caustics, soft shadows, and ambient occlusion.
   - **SoftwareRasterizer:** Retro CPU-rasterized rendering leveraging SIMD AVX2/NEON optimizations.
3. **Decoupled Architecture:** Cleanly separate headless, deterministic, and unit-testable game logic (`TetrisGameLogic`, `TetrisMatrix`, `TetrisSRS`) from the visual presentation layer (`TetrisSceneManager`, `TetrisGameApp`).
4. **Standard Game Modes:** Provide the classic Guideline modes: Marathon (15 levels / endless), Sprint (40 Lines), and Ultra (2-minute score attack).

---

## 2. Official Tetris Guideline Specifications

### 2.1 The Matrix & Playfield Geometry
- **Playfield Dimensions:** 10 columns wide by 40 rows high.
- **Visible Playfield:** Rows 0 through 19 (20 rows total).
- **Buffer Zone (Hidden Skyline):** Rows 20 through 39 (20 rows total).
- **Coordinate System:**
  - $x \in [0, 9]$ from left to right.
  - $y \in [0, 39]$ from bottom (0) to top (39).
  - Skyline is at row boundary $y = 20$.

### 2.2 Tetromino Types, Colors & Geometries
Each piece is constructed of four square minos. The guideline specifies canonical initial rotations and colors:

| Piece | Color | Hex Code | Bounding Box | Initial Orientation |
|---|---|---|---|---|
| **I** | Cyan | `#00F0F0` | $4 \times 4$ | Horizontal in rows 20-21, centered at columns 3..6 |
| **J** | Blue | `#0000F0` | $3 \times 3$ | Flat side down, corner pointing up-left |
| **L** | Orange | `#F0A000` | $3 \times 3$ | Flat side down, corner pointing up-right |
| **O** | Yellow | `#F0F000` | $4 \times 4$ (or $2 \times 2$) | Centered in columns 4..5, rows 20-21 |
| **S** | Green | `#00F000` | $3 \times 3$ | Horizontal step, top-right / bottom-left |
| **T** | Purple / Magenta | `#A000F0` | $3 \times 3$ | Flat side down, pointing up |
| **Z** | Red | `#F00000` | $3 \times 3$ | Horizontal step, top-left / bottom-right |

### 2.3 Piece Spawning
- **Horizontal Spawn Position:** Centered horizontally.
  - 3-wide pieces (J, L, S, T, Z): columns 3, 4, 5 (x coordinates 3, 4, 5).
  - 4-wide pieces (I, O): columns 3, 4, 5, 6.
- **Vertical Spawn Position:** Rows 20 and 21 (just above the visible row 19 skyline).
- **Spawn Obstruction (Block Out):** If any mino of a newly spawned piece collides with an existing locked mino, the game immediately ends in a **Block Out**.

### 2.4 Random Generator (7-Bag Randomizer)
- Tetrominoes are dealt in sequences ("bags") containing exactly one of each of the 7 tetromino types (`I, J, L, O, S, T, Z`) in uniformly random order.
- When a bag is emptied, a new permutation of 7 tetrominoes is generated and appended to the queue.
- Ensures a piece is never starved for more than 12 piece placements.

### 2.5 Super Rotation System (SRS)
The rotation engine implements the complete SRS state machine with 4 rotation states:
- `0`: Initial spawn state
- `R`: Clockwise rotation (90° from spawn)
- `2`: 180° rotation from spawn
- `L`: Counter-clockwise rotation (270° from spawn)

#### Wall Kick Test Tables
When a rotation is attempted, the piece tests candidate positions in order until one succeeds without collision:
1. **Basic Rotation:** Offset $(0, 0)$.
2. **Kicks 1 through 4:** Sequential translation offsets tested if basic rotation fails.

##### JLSTZ Wall Kick Offsets:
- `0 -> R`: $(0,0), (-1,0), (-1,+1), (0,-2), (-1,-2)$
- `R -> 0`: $(0,0), (+1,0), (+1,-1), (0,+2), (+1,+2)$
- `R -> 2`: $(0,0), (+1,0), (+1,-1), (0,+2), (+1,+2)$
- `2 -> R`: $(0,0), (-1,0), (-1,+1), (0,-2), (-1,-2)$
- `2 -> L`: $(0,0), (+1,0), (+1,+1), (0,-2), (+1,-2)$
- `L -> 2`: $(0,0), (-1,0), (-1,-1), (0,+2), (-1,+2)$
- `L -> 0`: $(0,0), (-1,0), (-1,-1), (0,+2), (-1,+2)$
- `0 -> L`: $(0,0), (+1,0), (+1,+1), (0,-2), (+1,-2)$

##### I-Tetromino Wall Kick Offsets:
- `0 -> R`: $(0,0), (-2,0), (+1,0), (-2,-1), (+1,+2)$
- `R -> 0`: $(0,0), (+2,0), (-1,0), (+2,+1), (-1,-2)$
- `R -> 2`: $(0,0), (-1,0), (+2,0), (-1,+2), (+2,-1)$
- `2 -> R`: $(0,0), (+1,0), (-2,0), (+1,-2), (-2,+1)$
- `2 -> L`: $(0,0), (+2,0), (-1,0), (+2,+1), (-1,-2)$
- `L -> 2`: $(0,0), (-2,0), (+1,0), (-2,-1), (+1,+2)$
- `L -> 0`: $(0,0), (+1,0), (-2,0), (+1,-2), (-2,+1)$
- `0 -> L`: $(0,0), (-1,0), (+2,0), (-1,+2), (+2,-1)$

*Note: The O-piece does not kick or rotate.*

### 2.6 Player Controls & Timing Parameters
- **Delayed Auto Shift (DAS):** Initial hold duration before rapid lateral movement begins. Default: $167\text{ ms}$ (approx. 10 frames @ 60 FPS), user adjustable.
- **Auto Repeat Rate (ARR):** Interval between repeat lateral steps during held input. Default: $33\text{ ms}$ (2 frames @ 60 FPS), user adjustable down to $0\text{ ms}$ (instant DAS).
- **Soft Drop:** Increases downward fall speed by a multiplier (default $20\times$ gravity). Awards $1\text{ point}$ per cell traversed.
- **Hard Drop:** Instantly drops the piece to its ghost position and immediately locks it with no lock delay. Awards $2\text{ points}$ per cell traversed.
- **Ghost Piece:** Real-time projected preview showing the destination of the active tetromino upon hard drop.
- **Hold Queue:**
  - Can store 1 piece.
  - Pressing Hold swaps the active piece with the held piece (or pulls next piece if empty).
  - Piece swap is locked until the current piece locks into the matrix (1 swap per turn rule).
- **Next Queue:** 5-piece preview window showing upcoming tetrominoes.

### 2.7 Lock Delay & Extended Placement Rule
- **Lock Delay Timer:** When an active piece rests on a solid surface (floor or locked mino), a $0.5\text{ second}$ ($500\text{ ms}$) lock timer begins.
- **Move Reset / Step Reset:**
  - Any successful lateral translation or rotation resets the $500\text{ ms}$ lock timer.
  - **Manipulation Limit:** To prevent infinite stalling, the lock timer reset is allowed at most **15 times** on the same floor level.
  - If the piece drops to a lower row ($y_{new} < y_{prev}$), the 15-manipulation counter is reset.

### 2.8 T-Spin Detection System
- **Preconditions:**
  1. Active piece is a **T** tetromino.
  2. The last successful maneuver performed prior to lock was a **rotation** (not a lateral move or gravity step).
- **3-Corner Rule:**
  - Evaluate the 4 diagonal corner cells around the center mino of the T piece:
    - Front Corners: The 2 corners adjacent to the protrusion of the T.
    - Rear Corners: The 2 corners adjacent to the flat base of the T.
  - A corner is considered "occupied" if it contains a locked mino OR lies outside the matrix boundaries (walls or floor).
  - If $\ge 3$ corners are occupied, the placement is a valid **T-Spin**.
- **T-Spin Mini vs Regular T-Spin:**
  - If both front corners and at least 1 rear corner are occupied $\rightarrow$ **Full T-Spin**.
  - If only 1 front corner and 2 rear corners are occupied $\rightarrow$ **T-Spin Mini** (unless test 5 kick $(1,2)$ was used, which qualifies as full T-Spin).

### 2.9 Scoring & Multipliers
Standard guideline scoring:

| Action | Base Score Formula | Back-to-Back (B2B) Eligible? |
|---|---|---|
| Single Line Clear | $100 \times \text{Level}$ | No |
| Double Line Clear | $300 \times \text{Level}$ | No |
| Triple Line Clear | $500 \times \text{Level}$ | No |
| Tetris (4 Lines) | $800 \times \text{Level}$ | **Yes** ($1200 \times \text{Level}$) |
| T-Spin (No lines) | $100 \times \text{Level}$ | No |
| T-Spin Mini Single | $200 \times \text{Level}$ | **Yes** ($300 \times \text{Level}$) |
| T-Spin Single | $800 \times \text{Level}$ | **Yes** ($1200 \times \text{Level}$) |
| T-Spin Mini Double | $400 \times \text{Level}$ | **Yes** ($600 \times \text{Level}$) |
| T-Spin Double | $1200 \times \text{Level}$ | **Yes** ($1800 \times \text{Level}$) |
| T-Spin Triple | $1600 \times \text{Level}$ | **Yes** ($2400 \times \text{Level}$) |
| Soft Drop | $1 \text{ pt} \times \text{cells}$ | N/A |
| Hard Drop | $2 \text{ pts} \times \text{cells}$ | N/A |

- **Back-to-Back (B2B):** Awarded for consecutive "Difficult" line clears (Tetris or any T-Spin with lines). Grants a $1.5\times$ base score bonus. Regular Single/Double/Triple clears reset the B2B chain.
- **Combo Multiplier:** Awarded for consecutive line-clearing piece placements:
  $$\text{Combo Bonus} = 50 \times \text{Combo Count} \times \text{Level}$$
- **Perfect Clear (All Clear):** If the matrix contains zero minos after a line clear:
  - Single: $+800 \times \text{Level}$
  - Double: $+1200 \times \text{Level}$
  - Triple: $+1800 \times \text{Level}$
  - Tetris: $+2000 \times \text{Level}$ (B2B: $+3200 \times \text{Level}$)

### 2.10 Level Progression & Gravity Formula
- **Leveling:** 10 lines per level in Marathon mode (fixed goal standard).
- **Gravity Table:** Fall time per cell in seconds based on level $L$:
  $$T_{\text{gravity}}(L) = \left(0.8 - ((L - 1) \times 0.007)\right)^{L-1}$$
  - Level 1: $\approx 1.00\text{ s/row}$ ($60\text{ frames}$)
  - Level 5: $\approx 0.32\text{ s/row}$
  - Level 10: $\approx 0.058\text{ s/row}$
  - Level 15: $\approx 0.007\text{ s/row}$ ($\approx 20\text{G}$, instant fall)

### 2.11 Game Over Conditions
1. **Block Out:** Newly generated piece spawns overlapping existing blocks.
2. **Lock Out:** A piece locks into place entirely above the skyline ($y \ge 20$).
3. **Top Out:** Matrix blocks are pushed above row 39 (e.g., from garbage in future multiplayer expansions).

---

## 3. Game Modes

### 3.1 Marathon Mode
- **Goal:** Clear 150 lines across 15 levels (or continue indefinitely in Endless mode).
- **Level Progression:** Starts at Level 1, advances every 10 lines cleared up to Level 15 (with optional level selection at start: 1, 5, 10, 15).
- **Endless Option:** Toggleable setting in UI allowing players to continue playing past Level 15 at maximum 20G speed.

### 3.2 Sprint Mode (40 Lines)
- **Goal:** Clear exactly 40 lines in the shortest possible time.
- **Timer:** High-precision millisecond stopwatch ($mm:ss.mmm$) starting upon first piece input.
- **Level/Gravity:** Fixed gravity (configurable, default Level 1 or 20G practice).
- **Personal Best:** Tracks best completion time saved locally.

### 3.3 Ultra Mode (2 Minutes)
- **Goal:** Achieve the highest possible score within 120 seconds ($2\text{ minutes}$).
- **Timer:** Strict countdown from $02:00.000$ to $00:00.000$.
- **Level/Gravity:** Fixed or progressing score-attack conditions.
- **Personal Best:** Tracks highest score achieved.

---

## 4. Visual Presentation & RTRT Engine Integration

### 4.1 3D Scene Composition
The Tetris board is modeled as a complete 3D scene within the RTRT scene graph:
- **Playfield Housing:** A sleek, 3D beveled chassis/enclosure surrounding the $10 \times 20$ visible matrix with glass backplate and subtle neon edge accents.
- **Block Geometry:** High-detail beveled cube meshes (`Mesh` / `Instance`) for every mino, avoiding hard un-beveled edges to produce clean specular highlights in both rasterized and ray-traced modes.
- **Ghost Piece:** Semitransparent glass / wireframe material marking the drop position with subtle emission.
- **Active Piece:** Glowing, vibrant PBR materials with customizable roughness, metallic, and emissive properties.
- **Dynamic Point Lights:**
  - Real-time point light parented to the active piece, casting colored light onto adjacent locked minos and the board frame.
  - Impact flash lights triggered on Hard Drop and Tetris line clears.

### 4.2 Multi-Backend Rendering Support
The game supports seamless real-time switching between all three RTRTCore renderer backends:
1. **DeferredRenderer (Real-Time PBR):**
   - High performance, smooth 60+ FPS gameplay.
   - Dynamic point light illumination from active tetrominoes.
   - Screen Space Ambient Occlusion (SSAO), Screen Space Reflections (SSR), and Bloom.
   - Fast shadow mapping on the board backplate and side walls.
2. **PathTracer (GPU Ray Tracing):**
   - Photorealistic rendering showcasing glass/dielectric minos with transmission, refraction, and internal caustics.
   - Accurate soft shadows, multi-bounce diffuse global illumination, and emissive block lighting.
   - Depth of field (DoF) focusing on the active piece with soft background blur.
3. **SoftwareRasterizer (CPU SIMD Fallback):**
   - CPU-only software rasterizer utilizing AVX2/NEON vector instructions.
   - Nostalgic retro-styled software rendering aesthetic demonstrating the portability of RTRTCore.

### 4.3 Camera System
- **Frontal 3D View (Default):** Centered on the matrix with slight perspective depth, perfectly balancing competitive readability with 3D immersion.
- **Angled / Isometric View:** $30^\circ$ pitch and yaw showcase camera, accentuating 3D depth and specular reflections.
- **Screen Shake:** Subtle physics-based impulse applied on Hard Drop and violent impact on 4-line Tetris clears.
- **Orbit Camera (Free Mode):** Allows mouse orbit and zoom to inspect ray-traced materials.

### 4.4 HUD & UI Overlay (Dear ImGui)
- **Gameplay HUD:**
  - Left Panel: Hold Queue box, Level counter, Lines cleared counter.
  - Center: 3D matrix viewport with floating score popups (e.g., "T-SPIN DOUBLE", "TETRIS", "BACK-TO-BACK").
  - Right Panel: Next Queue (5 previews), Current Score, High Score, Mode Stopwatch / Countdown.
  - Bottom / Action Ticker: Real-time combo streak display and B2B indicator.
- **Engine Control & Settings Window:**
  - Backend selector (`DeferredRenderer`, `PathTracer`, `SoftwareRasterizer`).
  - Handling settings: DAS, ARR, Soft Drop speed, Lock Delay sliders.
  - Visual settings: Camera preset, bloom intensity, emissive brightness, block material preset (Plastic, Glass, Neon, Metallic).

---

## 5. Technical Architecture & Class Design

### 5.1 Project Layout
```
Games/TetrisGame/
├── CMakeLists.txt
└── src/
    ├── main.cpp                     # Application entry point
    ├── TetrisGameApp.h / .cpp       # GLFW loop, input dispatch, renderer coordination, ImGui UI
    ├── TetrisGameLogic.h / .cpp     # Headless guideline game loop, state machine, timers, scoring
    ├── TetrisMatrix.h / .cpp        # 10x40 grid, collision detection, line clear logic
    ├── TetrisPiece.h / .cpp         # Tetromino definitions, bounding boxes, SRS kick tables
    ├── TetrisRandomizer.h / .cpp    # 7-bag random generator
    ├── TetrisSceneManager.h / .cpp  # Translates Tetris state to RTRT Scene (Meshes, Instances, Lights)
    ├── TetrisTypes.h                # Enums, structs (PieceType, RotationState, GameMode, TimingConfig)
    └── TetrisHud.h / .cpp           # ImGui HUD and settings rendering
```

### 5.2 Decoupled Class Architecture

```
                                +-------------------+
                                |    GLFW / Window  |
                                +---------+---------+
                                          |
                                          v
                                +-------------------+
                                |   TetrisGameApp   |
                                +----+---------+----+
                                     |         |
                     +---------------+         +---------------+
                     |                                         |
                     v                                         v
           +-------------------+                     +-------------------+
           |  TetrisGameLogic  |                     |TetrisSceneManager |
           +---------+---------+                     +---------+---------+
                     |                                         |
         +-----------+-----------+                             v
         |           |           |                     +-------------------+
         v           v           v                     |  RTRT::Renderer   |
  +------------+ +--------+ +--------------+           | (Deferred / PT /  |
  |TetrisMatrix| |  SRS   | |7-Bag Random  |           |     Software)     |
  +------------+ +--------+ +--------------+           +-------------------+
```

### 5.3 Core Classes

#### `TetrisMatrix`
- Manages the $10 \times 40$ cell array (`uint8_t` mino color / empty).
- `bool IsCellOccupied( int iX, int iY ) const`
- `bool CanPieceFit( const TetrisPiece & iPiece, int iOriginX, int iOriginY ) const`
- `int LockPiece( const TetrisPiece & iPiece, int iOriginX, int iOriginY )`
- `int ClearFullLines( std::vector<int> & oClearedLines )`

#### `TetrisPiece` & `TetrisSRS`
- Encapsulates tetromino geometry, colors, and 4 rotation states.
- Implements SRS kick offset tables for JLSTZ and I pieces.
- `bool TryRotate( TetrisMatrix & iMatrix, int & ioX, int & ioY, RotationState iTargetState )`

#### `TetrisGameLogic`
- Fully deterministic and tick-driven: `void Update( double iDeltaTime )`.
- Handles DAS, ARR, Gravity tick, Lock Delay countdown, and 15-manipulation move reset counter.
- Evaluates 3-corner T-Spin rule, B2B streaks, combo sequences, and computes scores.
- Dispatches gameplay events (`OnPiecePlaced`, `OnLineClear`, `OnTSpin`, `OnGameOver`).

#### `TetrisSceneManager`
- Owns and updates the 3D `RTRT::Scene`.
- Generates/manages beveled block instances for:
  - Static frame and chassis.
  - Placed/locked matrix blocks.
  - Active falling piece.
  - Ghost piece (translucent / wireframe).
  - Next and Hold preview blocks.
- Updates active dynamic point lights matching current piece color.
- Triggers visual particle bursts or illumination spikes upon line clears.

---

## 6. Verification & Testing Strategy

1. **Automated Unit Tests for Game Logic:**
   - Headless test suite verifying:
     - 7-Bag generation distribution (every piece appears exactly once per 7-piece window).
     - Full SRS kick test suite (standard floor kicks, wall kicks, and T-spin kicks).
     - Accurate 3-corner T-Spin detection (Full vs Mini vs No-TSpin).
     - Extended placement lock delay (verifying 15-move manipulation cap).
     - Line clear and scoring calculations including B2B and combos.
2. **Interactive Regression Testing:**
   - Visual verification across all 3 renderers: `DeferredRenderer`, `PathTracer`, and `SoftwareRasterizer`.
   - Resolution and window resizing responsiveness.
   - Smooth 60 FPS input handling with zero perceived input lag (responsive DAS/ARR).

---

## 7. Approval & Next Steps

Upon review and approval of this specification:
1. Create implementation plan breakdown.
2. Configure CMake target `Games/TetrisGame` and link with `RTRTCore` and `RTRTFramework`.
3. Implement core headless logic (`TetrisMatrix`, `TetrisPiece`, `TetrisSRS`, `TetrisRandomizer`).
4. Implement `TetrisGameLogic` and mode loops (Marathon, Sprint, Ultra).
5. Build `TetrisSceneManager` and 3D visual presentation for RTRTCore renderers.
6. Assemble `TetrisGameApp` with ImGui HUD, controls, and renderer switching.
