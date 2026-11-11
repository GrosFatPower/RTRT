# RTRT Tetris Network Engine & Online Multiplayer Technical Specification

**Target Application:** [`Games/TetrisGame`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame)  
**Core Framework Component:** [`RTRT::NetSession`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/NetSession.h)  
**Protocol Definition:** [`TetrisNetProtocol.h`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisNetProtocol.h)  
**Transport Library:** [`Dependencies/enet`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Dependencies/enet) (Reliable UDP)  
**Document Version:** 1.0.0  

---

## 1. Executive Summary & Design Philosophy

The online multiplayer subsystem for the RTRT Tetris game provides low-latency, tournament-grade head-to-head 2-player competitive gameplay over Local Area Networks (LAN) and direct IP connections (WAN). It is designed to operate seamlessly across all three RTRT engine rendering backends ([`DeferredRenderer`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Engine/src/DeferredRenderer.h), [`PathTracer`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Engine/src/PathTracer.h), and [`SoftwareRasterizer`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Engine/src/SoftwareRasterizer.h)).

### Core Design Principles

1. **Zero-Latency Local Control (Client-Side Authority for Player Input):**
   In fast-paced competitive Tetris (where players execute inputs at sub-frame intervals using DAS and ARR), input delay is unacceptable. Each player runs full, authoritative local simulation on their own board (`_GameLogic[0]`). Inputs are processed immediately on the local frame without waiting for network round-trips.
2. **Asymmetric Dual-Board Viewport:**
   Both players see identical 3D scene compositions: the local player always plays on the **Left Board** ($X = -12.5$), while the remote opponent is mirrored on the **Right Board** ($X = +12.5$). The local player always sees themselves as Player 1 on their screen.
3. **Bandwidth Optimization via Nibble Packing:**
   The visible $10 \times 20$ matrix (200 mino cells) is compressed into 100 bytes using 4-bit nibble encoding, keeping real-time 30 Hz board snapshots compact and lightweight.
4. **Deterministic Shared 7-Bag Generation:**
   Before match kickoff, the Host broadcasts a single 32-bit PRNG seed (`_SharedSeed`). Both players initialize their [`TetrisRandomizer`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisRandomizer.h) with this seed, ensuring that both competitors receive identical piece sequences throughout the match.
5. **Host-Authoritative Round Arbitration:**
   The Host player evaluates round end triggers (topping out, objective completion, time out, score tie-breaks) and transmits definitive round outcomes and series win counts (`TetrisNetRoundResultPacket`) to maintain match integrity.
6. **Competitive Unpausable Model:**
   In online multiplayer, pressing `ESC` opens the in-game menu without halting game gravity or ticking timers. Local player inputs are neutralized while menus are open, preventing pause exploitation.

---

## 2. System Architecture & Layer Hierarchy

The networking stack is organized into distinct, decoupled layers:

```mermaid
flowchart TD
    subgraph UI ["User Interface Layer"]
        HUD["TetrisHud (Main Menu / Online Lobby / In-Game HUD)"]
    end

    subgraph App ["Application & Orchestration Layer"]
        APP["TetrisGameApp (Dual GameLogic, UpdateNetwork, SceneManager, Audio)"]
    end

    subgraph Logic ["Simulation & Serialization Layer"]
        L0["Local Board: TetrisGameLogic[0] (Simulates, Exports Snapshots & Attacks)"]
        L1["Remote Board: TetrisGameLogic[1] (Visual Replay, Applies Snapshots & FX)"]
    end

    subgraph Protocol ["Wire Protocol Layer"]
        PROTO["TetrisNetProtocol.h (POD Packed Packets, Sequence Numbering)"]
    end

    subgraph Framework ["Engine Transport Layer"]
        NET["RTRT::NetSession (Host/Client State, LAN Beacon, Channels)"]
        ENET["ENet Submodule (Reliable UDP, Flow Control, Sockets)"]
    end

    HUD --> APP
    APP --> L0
    APP --> L1
    L0 --> PROTO
    L1 --> PROTO
    PROTO --> NET
    NET --> ENET
```

### Component Breakdown

| Component | File Path | Responsibilities |
|---|---|---|
| **ENet** | [`Dependencies/enet`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Dependencies/enet) | Underlying UDP transport library providing connection-oriented reliable packets, unreliable sequenced packets, and flow control. |
| **`RTRT::NetSession`** | [`Framework/src/NetSession.h`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/NetSession.h)<br>[`Framework/src/NetSession.cpp`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/NetSession.cpp) | Reusable framework class managing ENet host/peer lifecycles, UDP LAN broadcast beacons, dual-channel dispatch, and RTT ping telemetry. |
| **`TetrisNetProtocol`** | [`Games/TetrisGame/src/TetrisNetProtocol.h`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisNetProtocol.h) | 1-byte aligned POD network packet structures, type identifiers, and bitfield flags. |
| **`TetrisGameLogic`** | [`Games/TetrisGame/src/TetrisGameLogic.h`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisGameLogic.h)<br>[`Games/TetrisGame/src/TetrisGameLogic.cpp`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisGameLogic.cpp) | Headless guideline logic; exports compressed board snapshots; unpacks remote snapshots; executes out-of-order sequence guarding. |
| **`TetrisGameApp`** | [`Games/TetrisGame/src/TetrisGameApp.h`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisGameApp.h)<br>[`Games/TetrisGame/src/TetrisGameApp.cpp`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisGameApp.cpp) | Main application; drives network polling loop (`UpdateNetwork`); routes garbage attacks; synchronizes 3D spatial rendering and audio. |
| **`TetrisHud`** | [`Games/TetrisGame/src/TetrisHud.h`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisHud.h)<br>[`Games/TetrisGame/src/TetrisHud.cpp`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisHud.cpp) | Renders the Online Lobby (Host & Join tabs), LAN discovery browser, live latency monitor, series score counters, and unpausable in-game ESC menu. |

---

## 3. Transport Subsystem (`RTRT::NetSession`)

[`RTRT::NetSession`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/NetSession.h) encapsulates `ENet` inside an opaque pointer implementation (`NetSession::Impl`), shielding the rest of the engine from low-level networking headers (`winsock2.h`, `ws2tcpip.h`, or BSD socket headers).

### 3.1 Global Initialization & Reference Counting
ENet requires global initialization and teardown via `enet_initialize()` and `enet_deinitialize()`. To prevent redundant initializations across multiple session instances or unit tests, [`NetSession.cpp`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/NetSession.cpp) maintains an atomic reference counter:

```cpp
static int g_ENetInitRefCount = 0;

void AcquireENet()
{
  if ( g_ENetInitRefCount == 0 )
    enet_initialize();
  ++g_ENetInitRefCount;
}

void ReleaseENet()
{
  if ( g_ENetInitRefCount > 0 )
  {
    --g_ENetInitRefCount;
    if ( g_ENetInitRefCount == 0 )
      enet_deinitialize();
  }
}
```

### 3.2 Dual-Channel Topology
Each ENet peer connection allocates two channels:

| Channel | Delivery Mode | ENet Flag | Purpose |
|---|---|---|---|
| **Channel 0** | **Reliable Ordered** | `ENET_PACKET_FLAG_RELIABLE` | Crucial state transitions: Nickname Handshake (`HelloHandshake`), Seed & Match Config (`MatchStart`), Garbage Attacks (`GarbageAttack`), Line-Clear Visual Events (`LineClearFx`), and Round Results (`RoundResult`). Packets are acknowledged and guaranteed to arrive in order. |
| **Channel 1** | **Unreliable Sequenced** | `0` (Unreliable Sequenced) | High-frequency 30 Hz continuous board state replication (`BoardSnapshot`). If a packet drops or arrives late, it is discarded immediately rather than blocking newer snapshots. |

### 3.3 Connection Lifecycles & State Machine
The session transitions through four distinct states defined by `NetState`:

```mermaid
stateDiagram-v2
    [*] --> Disconnected
    Disconnected --> Hosting: StartHost(port)
    Disconnected --> Connecting: StartClient(ip, port)
    Hosting --> Connected: Peer Connected (ENET_EVENT_TYPE_CONNECT)
    Connecting --> Connected: Peer Connected (ENET_EVENT_TYPE_CONNECT)
    Connecting --> Disconnected: Timeout (6.0s elapsed)
    Connected --> Hosting: Peer Disconnects (Host keeps room open)
    Connected --> Disconnected: Peer Disconnects (Client resets)
    Hosting --> Disconnected: Disconnect() / StopHost
    Connected --> Disconnected: Disconnect()
```

- **Host Persistence:** When an active client disconnects from a host, the host automatically reverts to `NetState::Hosting`, resuming its LAN discovery beacon so another client can connect without restarting the game.
- **Client Timeout:** During `NetState::Connecting`, if no connection is established within `6.0 seconds`, the client automatically aborts the connection attempt and transitions to `NetState::Disconnected`.
- **Low-Latency Telemetry:** The session configures ping intervals and aggressive timeouts upon connection:
  ```cpp
  enet_peer_ping_interval( peer, 250 );       // Ping every 250 ms
  enet_peer_timeout( peer, 16, 3000, 8000 );  // Min 3s, Max 8s timeout
  ```
  Current Round-Trip Time (RTT) is queried via `enet_peer->roundTripTime` and exposed by `NetSession::GetPingMs()`.

---

## 4. LAN Discovery Subsystem

To enable frictionless local multiplayer without manual IP configuration, [`RTRT::NetSession`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/NetSession.h) features an autonomous UDP beacon and discovery mechanism.

### 4.1 Port Allocation
- **Default Game Port:** `27015` (configurable via UI input).
- **Default Discovery Port:** `27016` (used by UDP broadcast sockets).

### 4.2 Discovery Beacon Wire Format
Hosts broadcast an ASCII datagram beacon at a frequency of **~1.67 Hz** (every 600 ms). Pipes (`|`) serve as delimiters:

$$\text{Beacon Payload: } \texttt{"RTRT\_LAN\_V1|<GamePort>|<HostName>|<RoomInfo>"}$$

*Example Payload:*
```text
RTRT_LAN_V1|27015|Alice|Versus (First to 2) | Lvl 1
```

All textual fields are sanitized to remove existing pipe characters or newline characters before transmission.

### 4.3 Dual-Broadcast Architecture (LAN + Loopback)
Standard UDP broadcast (`255.255.255.255`) is blocked by operating system networking stacks from looping back to separate processes running on the same physical machine. To ensure that two game instances running on the same PC can discover and connect to each other effortlessly, `NetSession::BroadcastLanBeacon` executes a **dual-send**:

1. **Subnet Broadcast:** Sends to `255.255.255.255:27016` with socket option `ENET_SOCKOPT_BROADCAST` enabled for other PCs on the physical LAN.
2. **Loopback Unicast:** Sends directly to `127.0.0.1:27016` for local processes on the same host.

```cpp
// 1. Broadcast to LAN subnet
ENetAddress bcastAddr;
bcastAddr.host = ENET_HOST_BROADCAST;
bcastAddr.port = _DiscoveryPort;
enet_socket_send( _Impl->_BeaconSocket, &bcastAddr, &buf, 1 );

// 2. Unicast to loopback for same-PC testing
ENetAddress loopAddr;
loopAddr.port = _DiscoveryPort;
if ( enet_address_set_host_ip( &loopAddr, "127.0.0.1" ) == 0 )
  enet_socket_send( _Impl->_BeaconSocket, &loopAddr, &buf, 1 );
```

### 4.4 Room Table State Machine & Pruning
- The receiver socket binds to `ENET_HOST_ANY` on port `27016` with `ENET_SOCKOPT_REUSEADDR` and `ENET_SOCKOPT_NONBLOCK`.
- When packets arrive, self-broadcasts (beacons originating from the host's own port and local IP) are filtered out.
- Existing entries in `_DiscoveredRooms` have their `_LastSeenSec` timestamp updated. If a room advertises both `127.0.0.1` and a real LAN IP (e.g. `192.168.1.50`), the LAN IP is prioritized.
- **Stale Room Pruning:** Any discovered room whose last seen beacon exceeds **2.8 seconds** is automatically pruned from the table.

---

## 5. Wire Protocol & Packet Specification (`TetrisNetProtocol.h`)

All network packets are defined as plain-old-data (POD) C++ structs packed to 1-byte alignment via `#pragma pack(push, 1)`. Every packet begins with a 1-byte `uint8_t _Type` identifier corresponding to `TetrisNetPacketType`.

```cpp
enum class TetrisNetPacketType : uint8_t
{
  HelloHandshake = 1, // Reliable Ch 0: Nickname exchange on connect / lobby edit
  MatchStart     = 2, // Reliable Ch 0: Mode, StartLevel, TargetWins, SharedSeed, ResetSeries
  BoardSnapshot  = 3, // Unreliable Ch 1 (30 Hz): Full 10x20 board, active/hold/next, stats
  GarbageAttack  = 4, // Reliable Ch 0: Lines sent + event ID
  LineClearFx    = 5, // Reliable Ch 0: Cleared rows & cells + TSpin/Perfect flags for FX
  RoundResult    = 6  // Reliable Ch 0: Host-authoritative round end & series score sync
};
```

### 5.1 `TetrisNetHelloPacket` (Type 1)
Exchanged immediately upon establishing a connection or when either player edits their nickname in the lobby.

| Offset (Bytes) | Field | Type | Description |
|---|---|---|---|
| `0x00` | `_Type` | `uint8_t` | Constant value `1` (`HelloHandshake`). |
| `0x01` | `_Nickname[32]` | `char[32]` | Null-terminated UTF-8 player handle (up to 31 chars). |
| **Total Size** | **33 bytes** | | |

---

### 5.2 `TetrisNetMatchStartPacket` (Type 2)
Transmitted by the Host to start a match or advance to the next round.

| Offset (Bytes) | Field | Type | Description |
|---|---|---|---|
| `0x00` | `_Type` | `uint8_t` | Constant value `2` (`MatchStart`). |
| `0x01` | `_GameMode` | `uint8_t` | Game mode enum (`0=Marathon`, `1=Sprint`, `2=Ultra`, `3=Versus`). |
| `0x02` | `_StartLevel` | `uint8_t` | Initial starting gravity level ($1 \dots 15$). |
| `0x03` | `_TargetRoundWins`| `uint8_t` | Number of round wins required to win the series ($1 \dots 5$). |
| `0x04` | `_ResetSeries` | `uint8_t` | Boolean (`1` = reset win scores to 0; `0` = continue existing series). |
| `0x05` | `_SharedSeed` | `uint32_t` | 32-bit random seed for deterministic 7-bag randomizer synchronization. |
| **Total Size** | **9 bytes** | | |

---

### 5.3 `TetrisNetBoardSnapshotPacket` (Type 3)
Transmitted continuously at 30 Hz (or triggered immediately upon piece lock or death). Contains the complete operational state of the sender's board.

| Offset (Bytes) | Field | Type | Description |
|---|---|---|---|
| `0x00` | `_Type` | `uint8_t` | Constant value `3` (`BoardSnapshot`). |
| `0x01` | `_Sequence` | `uint32_t` | Monotonically increasing snapshot sequence ID. |
| `0x05` | `_Cells[100]` | `uint8_t[100]` | **200 visible cells** ($10 \times 20$, $Y = 0 \dots 19$) packed as 4-bit nibbles. |
| `0x69` | `_ActivePiece` | `int8_t` | Current falling tetromino type (`0=None`, $1 \dots 7$). |
| `0x6A` | `_ActiveRotation`| `int8_t` | Current rotation state ($0=0^\circ, 1=90^\circ, 2=180^\circ, 3=270^\circ$). |
| `0x6B` | `_ActiveX` | `int8_t` | Current piece pivot matrix coordinate $X$. |
| `0x6C` | `_ActiveY` | `int8_t` | Current piece pivot matrix coordinate $Y$. |
| `0x6D` | `_GhostY` | `int8_t` | Computed projection drop position $Y$ of ghost piece. |
| `0x6E` | `_HoldPiece` | `int8_t` | Currently held tetromino piece type. |
| `0x6F` | `_NextQueue[5]` | `uint8_t[5]` | Types of the next 5 upcoming tetrominoes. |
| `0x74` | `_Score` | `int64_t` | Current accumulated game score. |
| `0x7C` | `_LinesCleared`| `int32_t` | Total lines cleared in the round. |
| `0x80` | `_Level` | `int16_t` | Current dynamic gravity level. |
| `0x82` | `_ComboCount` | `int16_t` | Consecutive line clear combo counter ($-1$ when inactive). |
| `0x84` | `_PendingGarbage`| `int16_t`| Incoming garbage line queue awaiting spawn. |
| `0x86` | `_TotalAttacksSent`|`int16_t`| Total garbage lines sent to opponent so far. |
| `0x88` | `_ElapsedTime` | `float` | Elapsed match time in seconds. |
| `0x8C` | `_RemainingTime`| `float` | Countdown timer in seconds (for Ultra mode). |
| `0x90` | `_LockEventId` | `uint32_t` | Incremented every time a piece locks to trigger drop SFX. |
| `0x94` | `_Flags` | `uint8_t` | Bitmask of gameplay flags (see table below). |
| **Total Size** | **149 bytes** | | |

#### Snapshot Flags Bitmask (`TetrisNetSnapshotFlags`)
- `0x01` (`kNetSnapFlag_BackToBack`): Back-to-Back bonus chain is active.
- `0x02` (`kNetSnapFlag_GameOver`): Player board has topped out / died.
- `0x03` (`kNetSnapFlag_Victory`): Player has achieved victory condition.
- `0x04` (`kNetSnapFlag_LastLockHardDrop`): Previous locked piece locked via Hard Drop (triggers impact audio/vibration).
- `0x05` (`kNetSnapFlag_CanHold`): Hold action is currently allowed for this piece.

---

### 5.4 4-Bit Nibble Cell Compression

The visible Tetris playfield consists of 10 columns by 20 rows ($200 \text{ cells}$). Each cell can hold either empty space or one of 7 tetromino mino colors (requiring only 3 bits of information). 

Instead of sending 200 raw bytes, cells are packed pairwise into 4-bit nibbles:
- **Low Nibble (`bits 0..3`):** Even index cell $i = 2k$.
- **High Nibble (`bits 4..7`):** Odd index cell $i = 2k + 1$.

$$\text{Packed Byte } k = \Big(\text{cell}[2k] \ \& \ \text{0x0F}\Big) \ \Big| \ \Big(\big(\text{cell}[2k + 1] \ \& \ \text{0x0F}\big) \ll 4\Big)$$

```mermaid
flowchart LR
    C0["Cell 0 (0..15)"] -->|Bits 0..3| B0["Byte 0 (100 Bytes Total)"]
    C1["Cell 1 (0..15)"] -->|Bits 4..7| B0
    C2["Cell 2 (0..15)"] -->|Bits 0..3| B1["Byte 1"]
    C3["Cell 3 (0..15)"] -->|Bits 4..7| B1
    C198["Cell 198 (0..15)"] -->|Bits 0..3| B99["Byte 99"]
    C199["Cell 199 (0..15)"] -->|Bits 4..7| B99
```

This compression cuts playfield payload by **50%** (100 bytes instead of 200 bytes). At 30 Hz replication, the continuous upstream bandwidth is only:

$$\text{Bandwidth} = 149 \text{ bytes} \times 30 \text{ packets/s} \times 8 \approx 35.7 \text{ kbps}$$

---

### 5.5 `TetrisNetGarbageAttackPacket` (Type 4)
Sent on Channel 0 whenever a player clears lines in Versus mode that generate garbage attack lines.

| Offset (Bytes) | Field | Type | Description |
|---|---|---|---|
| `0x00` | `_Type` | `uint8_t` | Constant value `4` (`GarbageAttack`). |
| `0x01` | `_AttackLines` | `uint8_t` | Number of garbage lines sent to opponent ($1 \dots 20$). |
| `0x02` | `_EventId` | `uint32_t` | Monotonically increasing attack event counter. |
| **Total Size** | **6 bytes** | | |

---

### 5.6 `TetrisNetLineClearFxPacket` (Type 5)
Sent on Channel 0 to trigger synchronized visual and audio feedback on the opponent's remote board (such as line flash animations, floating action callouts, and combo audio).

| Offset (Bytes) | Field | Type | Description |
|---|---|---|---|
| `0x00` | `_Type` | `uint8_t` | Constant value `5` (`LineClearFx`). |
| `0x01` | `_NumRows` | `uint8_t` | Count of rows cleared simultaneously ($1 \dots 4$). |
| `0x02` | `_RowY[4]` | `int8_t[4]` | Y-indices ($0 \dots 19$) of cleared rows. |
| `0x06` | `_RowCells[4][10]`|`uint8_t[40]`| Color IDs of every mino cell on the cleared rows before deletion. |
| `0x2E` | `_TSpinType` | `uint8_t` | T-Spin categorization (`0=None`, `1=Mini`, `2=Regular`). |
| `0x2F` | `_PerfectClear`| `uint8_t` | Boolean (`1` if the clear resulted in an empty board). |
| `0x30` | `_ComboCount` | `int16_t` | Combo streak count at time of clear. |
| `0x32` | `_BannerText[32]`| `char[32]` | Text string to display (e.g. `"T-SPIN DOUBLE"`, `"TETRIS B2B"`). |
| **Total Size** | **82 bytes** | | |

---

### 5.7 `TetrisNetRoundResultPacket` (Type 6)
Transmitted exclusively by the Host on Channel 0 to arbitrate the winner of a round and synchronize match series progress.

| Offset (Bytes) | Field | Type | Description |
|---|---|---|---|
| `0x00` | `_Type` | `uint8_t` | Constant value `6` (`RoundResult`). |
| `0x01` | `_WinnerRole` | `int8_t` | Outcome: `-1` = Draw, `1` = Host won round, `2` = Client won round. |
| `0x02` | `_HostWins` | `uint8_t` | Cumulative round wins achieved by Host. |
| `0x03` | `_ClientWins` | `uint8_t` | Cumulative round wins achieved by Client. |
| `0x04` | `_Reason[48]` | `char[48]` | Textual explanation (e.g. `"Opponent Topped Out"`, `"40 Lines Completed"`). |
| **Total Size** | **52 bytes** | | |

---

## 6. Gameplay Synchronization & Authority Model

### 6.1 Deterministic 7-Bag PRNG Synchronization
Tetris pieces are dealt using the canonical 7-bag randomizer ([`TetrisRandomizer`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisRandomizer.h)). When an online match begins:
1. The Host generates a non-zero 32-bit random seed using `std::random_device`.
2. The Host sends this seed in [`TetrisNetMatchStartPacket`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisNetProtocol.h).
3. Both Host and Client call `_GameLogic[0].StartNewGame(mode, startLevel, seed)`.
4. Because the randomizer is completely deterministic, both players draw the exact same sequence of pieces in the exact same order.

```mermaid
sequenceDiagram
    autonumber
    participant Host as Host (Player 1)
    participant Client as Client (Player 2)

    Note over Host: User clicks "START MATCH"
    Host->>Client: TetrisNetMatchStartPacket (Seed=0x9E3779B9, Mode=Versus, Wins=2)
    Note over Host,Client: Initialize TetrisRandomizer with Seed
    Host->>Host: StartMatch() -> Board[0]=Left, Board[1]=Right
    Client->>Client: StartMatch() -> Board[0]=Left, Board[1]=Right
```

### 6.2 Board Snapshot Replication & Sequence Guarding
Local simulation updates at the monitor refresh rate (60–240+ FPS). Every $33.3\text{ ms}$ (30 Hz), the local state is serialized via `_GameLogic[0].ExportNetSnapshot` and dispatched across the network.

When receiving a snapshot on the remote endpoint:
```cpp
void TetrisGameLogic::ApplyNetSnapshot( const TetrisNetBoardSnapshotPacket & iPacket )
{
  // Drop out-of-order packets arriving late over UDP
  if ( _LastAppliedNetSequence != 0 && iPacket._Sequence < _LastAppliedNetSequence )
    return;

  _LastAppliedNetSequence = iPacket._Sequence;
  // Unpack nibbles into visible matrix rows 0..19
  // Update active piece coordinates, ghost Y, score, level, lines, hold piece
}
```
If an out-of-order UDP packet arrives, it is discarded immediately, preventing visual stutter or backward jumps.

### 6.3 Garbage Attack Pipeline & Offset Mechanics
In Versus mode, line clears generate garbage sent to the opponent according to the official Guideline attack table:

$$\text{Attack Lines} = f(\text{Lines}, \text{TSpin}, \text{B2B}, \text{Combo}, \text{PerfectClear})$$

When the local board clears lines:
1. `_GameLogic[0].OffsetPendingGarbage(attackLines)`: If the local board already had incoming pending garbage, the attack lines **cancel/offset** local pending garbage first.
2. Any leftover attack lines are packed into a [`TetrisNetGarbageAttackPacket`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisNetProtocol.h) and sent reliably on Channel 0.
3. Upon receipt on the other machine, the opponent's `_GameLogic[0].AddPendingGarbage(lines)` registers the attack.
4. Pending garbage is displayed on the player's HUD red garbage warning meter. When the player locks a piece without clearing any lines, the pending garbage rises from the bottom of their matrix with a randomized column hole.

```mermaid
flowchart LR
    Clear["Player Clears 4 Lines (Tetris)"] --> Calc["ComputeGarbageAttack: 4 Lines"]
    Calc --> Cancel{"Has Pending Garbage?"}
    Cancel -->|Yes| Sub["Cancel Local Pending Garbage"]
    Cancel -->|No| Leftover["Attack Remainder"]
    Sub --> Leftover
    Leftover --> Send["SendReliable: TetrisNetGarbageAttackPacket"]
    Send --> Opponent["Opponent AddPendingGarbage()"]
    Opponent --> Lock{"Opponent Locks Piece Without Clear"}
    Lock --> Spawn["Garbage Rows Rise from Bottom"]
```

### 6.4 3D Spatial Audio & Line Clear Effects
Audio and visual feedback are fully synchronized between both boards:
- **Audio Listener:** Centered at position $(0, 20, 30)$ looking toward $(0, 20, 0)$.
- **Local Audio:** SFX from the local board (at $X = -12.5$) are panned left or spatialized with a left-side bias.
- **Remote Audio:** When a [`TetrisNetLineClearFxPacket`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisNetProtocol.h) arrives, `_GameLogic[1].ApplyNetLineClearFx()` plays the corresponding clear, combo, or T-Spin audio spatialized at the right board's 3D position ($X = +12.5$).
- **Visual FX:** Line flash animations, camera shake impulses, and floating 3D text banners (e.g. `"TETRIS"`, `"PERFECT CLEAR"`) render on the right board identically to how the opponent saw them.

### 6.5 Host-Authoritative Round Arbitration
To prevent split-brain outcomes (e.g., both players simultaneously claiming victory in Sprint mode or claiming the other died first), the Host acts as the sole referee:

1. In `TetrisGameApp::UpdateGame()`, the Host continuously checks whether either player has topped out (`IsGameOver()`) or achieved their victory condition (`IsVictory()`).
2. When a condition is triggered, the Host resolves the winner:
   - If one player tops out, the other wins.
   - If both top out or both complete the objective on the same tick, the player with the higher score wins.
   - If scores are identical, the round is declared a Draw.
3. The Host increments the appropriate win counter (`_RoundWins[0]` or `_RoundWins[1]`).
4. The Host sends a [`TetrisNetRoundResultPacket`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisNetProtocol.h) to the Client.
5. Both players display the match result overlay. If the target round wins have not been reached, the Host has the option to click **"NEXT ROUND"** to launch the next round.

---

## 7. User Interface & Multiplayer Flow

The user interface integrates online multiplayer directly into the primary menu navigation hierarchy.

### 7.1 Unified Menu Navigation
In the main menu, local and online multiplayer are consolidated under a single entry:

```text
[ MAIN MENU ]
  - SINGLE PLAYER (Marathon, Sprint, Ultra)
  - MULTIPLAYER (2P)  <--- Unified Entry
  - SETTINGS
  - QUIT
```

Selecting **`MULTIPLAYER (2P)`** advances to `TetrisMenuPage::MultiplayerTypeSelect`:
- **`LOCAL (SPLIT-SCREEN)`:** Opens the local presets menu (keyboard/gamepad routing, Marathon/Sprint/Ultra/Versus).
- **`ONLINE (LAN / INTERNET)`:** Opens `TetrisMenuPage::OnlineLobby`.

```mermaid
flowchart TD
    Main["Main Menu: MULTIPLAYER (2P)"] --> Select["TetrisMenuPage::MultiplayerTypeSelect"]
    Select -->|Local| Local["TetrisMenuPage::MultiplayerModeSelect (Local 2P)"]
    Select -->|Online| Lobby["TetrisMenuPage::OnlineLobby"]
    Lobby --> HostTab["HOST TAB: Port, Match Mode, Target Wins, Start Level"]
    Lobby --> JoinTab["JOIN TAB: LAN Room Browser & Direct IP Connect"]
```

### 7.2 Online Lobby Pages & Features
The Online Lobby offers two tabs:

#### 1. Host Tab
- **Nickname & Room Name:** Customizable player handle and room description.
- **Match Configuration:** Match Mode (`Versus`, `Marathon`, `Sprint`, `Ultra`), Target Round Wins (First to 1 through 5), and Starting Gravity Level ($1 \dots 15$).
- **Network Controls:** Port selector (default `27015`), **"HOST ROOM"** button, and dynamic status indicators (`Waiting for peer...` / `Peer connected!`).
- **Kickoff:** When a client joins, the Host's UI unlocks the **"START MATCH"** button.

#### 2. Join Tab
- **Local LAN Room Browser:** Automatically populates with servers discovered via the UDP beacon subsystem. Displays Host Nickname, IP address, Port, Match rules, and ping latency. Clicking a room copies its parameters into the join fields.
- **Direct Connect:** Allows manual input of an IPv4 address / hostname and port for WAN or VPN connections.
- **Connect / Cancel:** Starts the client connection attempt with animated connection status and timeout handling.

### 7.3 In-Game Telemetry & Remote HUD
During active gameplay, the HUD presents:
- **Ping Telemetry:** Real-time round-trip latency (`PING: 12 ms`) displayed above the board.
- **Series Score Counter:** `ROUND 2 - FIRST TO 3 WINS (1 - 0)`.
- **Opponent Preview Queue & Matrix:** 3D rendered hold piece, matrix skyline, ghost projection, and next 5 pieces for the opponent.
- **Garbage Danger Bar:** Vertical red meter visualizing incoming lines about to rise.
- **Opponent Disconnect Modal:** If a peer leaves mid-match, an alert modal informs the user and allows returning to the main menu without crashing or hanging.

---

## 8. Verification & Automated Testing

The networking subsystem is covered by automated unit tests integrated into the [`TetrisUnitTests`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisUnitTests.h) test suite.

### 8.1 Unit Tests Overview

| Test Name | File | Description |
|---|---|---|
| `TestNetSnapshotSerialization` | [`TetrisUnitTests.cpp`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisUnitTests.cpp#L524) | Verifies 4-bit nibble cell compression, board deserialization, telemetry reconstruction, and out-of-order sequence packet rejection. |
| `TestOnlineLoopbackSession` | [`TetrisUnitTests.cpp`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisUnitTests.cpp#L595) | Instantiates two live `NetSession` instances on ports `27098`/`27099`, completes an ENet loopback handshake, sends a reliable `GarbageAttack` on Channel 0, and sends an unreliable `BoardSnapshot` on Channel 1. |

### 8.2 Execution Instructions
To run the automated headless unit tests:

```powershell
# Build release binaries
cmake --build build --config Release --target TetrisGame

# Run the complete unit test suite
.\Build\Release\TetrisGame.exe --unit-tests
```

> [!NOTE]
> On sandboxed agent environments where network socket creation (`bind`) is restricted, `TestOnlineLoopbackSession` detects the bind restriction and safely skips the live loopback check without failing the test suite. When executed in standard environments or with sandbox bypass, all 11/11 tests execute and pass completely.

---

## 9. Code Reference & Key Symbols

For fast navigation in IDEs and codebase exploration:

- **Transport Core:**
  - Class [`RTRT::NetSession`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/NetSession.h#L42)
  - Host Startup: [`NetSession::StartHost`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/NetSession.cpp#L90)
  - Client Startup: [`NetSession::StartClient`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/NetSession.cpp#L137)
  - LAN Discovery Beacon: [`NetSession::BroadcastLanBeacon`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/NetSession.cpp#L252)
  - Polling & Packet Ingestion: [`NetSession::Poll`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/NetSession.cpp#L385)
- **Protocol & Packing:**
  - Packet Definitions: [`TetrisNetProtocol.h`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisNetProtocol.h#L19)
  - Snapshot Export: [`TetrisGameLogic::ExportNetSnapshot`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisGameLogic.cpp#L874)
  - Snapshot Import: [`TetrisGameLogic::ApplyNetSnapshot`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisGameLogic.cpp#L924)
- **App Orchestration:**
  - Network Update Loop: [`TetrisGameApp::UpdateNetwork`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisGameApp.cpp#L602)
  - Online Match Kickoff: [`TetrisGameApp::HostLaunchOnlineMatch`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisGameApp.cpp#L523)
  - Host Arbitration Logic: [`TetrisGameApp::UpdateGame`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisGameApp.cpp#L1736)
- **UI & Lobby:**
  - Online Lobby Rendering: [`TetrisHud::DrawOnlineLobbyPage`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisHud.cpp#L704)
  - Multiplayer Type Selector: [`TetrisHud::DrawMultiplayerTypeSelectPage`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisHud.cpp#L376)

---

## 10. Internet Hosting: UPnP Port Forwarding, STUN Discovery & Room Codes

Hosting over the Internet needs the host's router to forward the game port (UDP `27015`) and the players to have an easy way to exchange addresses without typing raw IP addresses. [`RTRT::UpnpService`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/UpnpService.h) and [`RTRT::RoomCode`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/RoomCode.h) achieve this automatically without requiring external relay servers.

### 10.1 UpnpService & STUN Discovery Lifecycle
When `NetSession::StartHost( ..., iEnableUpnp )` is called, [`UpnpService::StartMappingAsync`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/UpnpService.cpp) launches an asynchronous background thread:

1. **Instant WAN IP Resolution via STUN (RFC 5389):**
   Immediately sends a 20-byte UDP STUN binding request to `stun.l.google.com:19302` (and `stun.cloudflare.com:3478`). In ~20 ms, the real public WAN IP is discovered and the true Internet Room Code is generated before IGD discovery even completes.
2. **HTTP Trailing-Slash Fallback:**
   If STUN is filtered by an enterprise firewall, plain HTTP reflection endpoints (`http://api.ipify.org/`, `http://checkip.amazonaws.com/`, `http://icanhazip.com/`) are queried via `miniwget`.
3. **Router Discovery & Port Mapping (UPnP IGD):**
   Discovers the home gateway device via SSDP (`upnpDiscover`, targeting the active local network interface).
4. **Double NAT Resilience:**
   If the local router's WAN IP is in a private subnet (e.g. `192.168.x.x` behind an upstream modem), the port is mapped on the local router while the room code continues to use the verified public IP.
5. **Port Forwarding:**
   Calls `UPNP_AddPortMapping` for UDP `27015` with a 7200 s lease, falling back to a permanent lease (0) if timed leases are refused.
6. **Graceful Teardown:**
   When hosting stops or the application exits, `UPNP_DeletePortMapping` cleanly unmaps the port.

### 10.2 Room Code System ([`RoomCode`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/RoomCode.h))
`(IPv4, Port)` is encoded using Crockford Base32 (no `I L O U`, case-insensitive, hyphen/space tolerant) with a 4-bit checksum.

| Port | Payload | Format | Example |
|---|---|---|---|
| `27015` (default) | IPv4 (32) + checksum (4) = 36 bits | `XXXX-XXXX` (8 chars) | `0Q5W-6AK1` |
| custom port | IPv4 (32) + port (16) + checksum (4) = 52 bits | `XXXX-XXXX-XXX` (11 chars) | `0Q5W-6AK1-9B2` |

- **IP Classification:** [`RoomCode::IsPublicInternetIP()`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Framework/src/RoomCode.h) and `RoomCode::IsPrivateOrReservedIP()` distinguish private LAN/CGNAT addresses (RFC 1918, RFC 5735, RFC 6598) from globally routable public IPs.
- **HUD Join Card:** Features a dedicated `JOIN VIA ROOM CODE` card with an input field, a one-click **`[ PASTE ]`** button, live green checkmark validation, and immediate connect button.

### 10.3 Test Suite Coverage
All 14 automated headless tests pass in [`TetrisUnitTests.cpp`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Games/TetrisGame/src/TetrisUnitTests.cpp):
1. 7-Bag Randomizer uniformity & starvation.
2. SRS Wall & Floor Kick verification.
3. 3-Corner T-Spin detection geometry.
4. Extended placement (15 manipulation limit) lock delay.
5. Guideline Line Clear & Perfect Clear check.
6. AudioEngine synthesis & sound bank lifecycle.
7. Matrix upward shift & garbage line insertion.
8. Guideline VS Garbage attack table, cancellation & shared-seed parity.
9. Local Multiplayer device assignment & split keyboard isolation.
10. Online 4-bit board snapshot serialization & sequence guard.
11. ENet Loopback Host/Client handshake & dual-channel packet delivery.
12. Room code encoding, decoding, checksum validation & IP classification.
13. UPnP service async start/stop lifecycle & STUN WAN IP resolution.
14. Protocol version handshake & version mismatch rejection arbitration.

---

## 11. Online Versioning, Protocol Compatibility & Handshake Rejection

When players iterate on the game, incompatible versions connecting over the network must not cause memory misalignment, silent corruption, or desyncs. The game enforces a layered versioning architecture:

### 11.1 Version Layers & Developer Rules

| Change Type | What Changed? | Breaks Online Play? | What to Increment |
|---|---|---|---|
| **Cosmetic / Local** | Shaders, lighting, camera FOV, audio SFX, HUD styling, keybindings | **No** (Safe) | Increment `kTetrisGameVersionStr` patch if desired. Leave `kTetrisProtocolVersion` untouched. Players on different cosmetic builds can still play together. |
| **Simulation / Rules** | SRS kick tables, 7-Bag RNG, garbage attack formulas, lock delays | **Yes** (Desync) | Increment `kTetrisProtocolVersion` (e.g. from `1` to `2`) and bump `kTetrisGameVersionStr`. |
| **Wire Protocol** | Packet struct layout, new packet fields, new packet enum types | **Yes** (Crash / Memory Corrupt) | Increment `kTetrisProtocolVersion` and bump `kTetrisGameVersionStr`. |

### 11.2 Wire Protocol Handshake Specification

The network protocol is identified by:
- **Magic Constant:** `kTetrisNetMagic = 0x54455452` (`'TETR'`). Foreign packets or port probes are immediately discarded.
- **Protocol Version:** `kTetrisProtocolVersion = 1` (`uint32_t`).
- **Semantic Version:** `kTetrisGameVersionStr = "1.0.0"` (`char[16]`).

#### Hello Handshake (`TetrisNetHelloPacket`, Type `1`)
Transmitted reliably on Channel 0 immediately after ENet connection establishment:
```cpp
struct TetrisNetHelloPacket
{
  uint8_t  _Type            = 1;
  char     _Nickname[32]    = "PLAYER";   // Deliberately at offset 1 for backward compatibility
  uint32_t _Magic           = 0x54455452; // 'TETR'
  uint32_t _ProtocolVersion = 1;
  char     _GameVersion[16] = "1.0.0";
};
```
> [!NOTE]
> **Backward Compatibility Grace Mode:** Early v1.0.0 clients (commit `755b926`) transmit a 33-byte `TetrisNetLegacyHelloPacket` (`_Type` + `_Nickname[32]`). Because simulation and packet layouts are identical, the engine gracefully accepts legacy handshakes as `"1.0.0 (legacy)"` without breaking existing clients.

#### Incompatibility Rejection (`TetrisNetVersionMismatchPacket`, Type `7`)
If the Host inspects the incoming `HelloHandshake` and detects that `_ProtocolVersion != kTetrisProtocolVersion`, the Host sends:
```cpp
struct TetrisNetVersionMismatchPacket
{
  uint8_t  _Type                = 7;
  uint32_t _HostProtocolVersion = 1;
  char     _HostGameVersion[16] = "1.0.0";
  char     _ClientGameVersion[16]= "0.9.0";
  char     _Reason[96]          = "Version mismatch: Host is 1.0.0 (Proto 1), Peer is 0.9.0 (Proto 0)";
};
```
The Host then disconnects only that peer (`NetSession::DisconnectPeer()`), preserving the host server socket and UPnP port mapping so the room remains open for compatible opponents.

### 11.3 HUD & User Feedback
- **Lobby & Title Screen:** Shows current build version: `v1.0.0 (NetProto 1)` in the online lobby header and main menu footer.
- **Peer Version Tag:** Upon successful connection, shows peer's game version: `Connected to Alice [v1.0.0]`.
- **Rejection Banner:** If a client or host encounters a version mismatch, a red alert banner is rendered in the Online Lobby with an explicit explanation and a `[DISMISS]` button.
