#ifndef _TetrisNetProtocol_
#define _TetrisNetProtocol_

#include <cstdint>

namespace Tetris
{

enum class TetrisNetPacketType : uint8_t
{
  HelloHandshake  = 1, // Reliable Ch 0: Nickname & version exchange on connect / lobby change
  MatchStart      = 2, // Reliable Ch 0: Mode, StartLevel, TargetWins, SharedSeed, ResetSeries
  BoardSnapshot   = 3, // Unreliable Ch 1 (30 Hz): Full 10x20 board, active/hold/next, stats
  GarbageAttack   = 4, // Reliable Ch 0: Lines sent + event ID
  LineClearFx     = 5, // Reliable Ch 0: Cleared rows & cells + TSpin/Perfect flags for remote 3D FX/SFX
  RoundResult     = 6, // Reliable Ch 0: Host-authoritative round end & series score sync
  VersionMismatch = 7  // Reliable Ch 0: Handshake rejection due to incompatible game/protocol version
};

// Protocol magic identifier ('TETR') to quickly reject foreign or corrupted packets
static const uint32_t kTetrisNetMagic        = 0x54455452;

// Bump this integer whenever network packet memory layout or gameplay simulation rules change!
static const uint32_t kTetrisProtocolVersion = 1;

// Human-readable semver string for display in UI, lobbies, and error banners
static const char kTetrisGameVersionStr[]    = "1.0.0";

#pragma pack( push, 1 )

// Legacy handshake packet layout used in initial v1.0.0 builds (commit 755b926 and earlier)
struct TetrisNetLegacyHelloPacket
{
  uint8_t _Type         = static_cast<uint8_t>( TetrisNetPacketType::HelloHandshake );
  char    _Nickname[32] = {};
};

// Handshake packet with protocol versioning and magic header.
// NOTE: _Nickname is deliberately placed at offset 1 (immediately after _Type) so that
// earlier v1.0.0 builds reading only the first 33 bytes parse the nickname cleanly
// without seeing binary magic bytes, enabling bidirectional cross-version play.
struct TetrisNetHelloPacket
{
  uint8_t  _Type            = static_cast<uint8_t>( TetrisNetPacketType::HelloHandshake );
  char     _Nickname[32]    = {};
  uint32_t _Magic           = kTetrisNetMagic;
  uint32_t _ProtocolVersion = kTetrisProtocolVersion;
  char     _GameVersion[16] = {};
};

struct TetrisNetVersionMismatchPacket
{
  uint8_t  _Type                = static_cast<uint8_t>( TetrisNetPacketType::VersionMismatch );
  uint32_t _HostProtocolVersion = kTetrisProtocolVersion;
  char     _HostGameVersion[16] = {};
  char     _ClientGameVersion[16]= {};
  char     _Reason[96]          = {};
};

struct TetrisNetMatchStartPacket
{
  uint8_t  _Type            = static_cast<uint8_t>( TetrisNetPacketType::MatchStart );
  uint8_t  _GameMode        = 0;
  uint8_t  _StartLevel      = 1;
  uint8_t  _TargetRoundWins = 2;
  uint8_t  _ResetSeries     = 1;
  uint32_t _SharedSeed      = 1337;
};

enum TetrisNetSnapshotFlags : uint8_t
{
  kNetSnapFlag_BackToBack       = ( 1 << 0 ),
  kNetSnapFlag_GameOver         = ( 1 << 1 ),
  kNetSnapFlag_Victory          = ( 1 << 2 ),
  kNetSnapFlag_LastLockHardDrop = ( 1 << 3 ),
  kNetSnapFlag_CanHold          = ( 1 << 4 )
};

struct TetrisNetBoardSnapshotPacket
{
  uint8_t  _Type             = static_cast<uint8_t>( TetrisNetPacketType::BoardSnapshot );
  uint32_t _Sequence         = 0;
  uint8_t  _Cells[100]       = {}; // 200 visible cells (10x20, y=0..19) packed as 4-bit nibbles
  int8_t   _ActivePiece      = 0;
  int8_t   _ActiveRotation   = 0;
  int8_t   _ActiveX          = 0;
  int8_t   _ActiveY          = 0;
  int8_t   _GhostY           = 0;
  int8_t   _HoldPiece        = 0;
  uint8_t  _NextQueue[5]     = {};
  int64_t  _Score            = 0;
  int32_t  _LinesCleared     = 0;
  int16_t  _Level            = 1;
  int16_t  _ComboCount       = -1;
  int16_t  _PendingGarbage   = 0;
  int16_t  _TotalAttacksSent = 0;
  float    _ElapsedTime      = 0.0f;
  float    _RemainingTime    = 120.0f;
  uint32_t _LockEventId      = 0;
  uint8_t  _Flags            = 0;
};

struct TetrisNetGarbageAttackPacket
{
  uint8_t  _Type        = static_cast<uint8_t>( TetrisNetPacketType::GarbageAttack );
  uint8_t  _AttackLines = 0;
  uint32_t _EventId     = 0;
};

struct TetrisNetLineClearFxPacket
{
  uint8_t _Type            = static_cast<uint8_t>( TetrisNetPacketType::LineClearFx );
  uint8_t _NumRows         = 0;
  int8_t  _RowY[4]         = { -1, -1, -1, -1 };
  uint8_t _RowCells[4][10] = {};
  uint8_t _TSpinType       = 0;
  uint8_t _PerfectClear    = 0;
  int16_t _ComboCount      = 0;
  char    _BannerText[32]  = {};
};

struct TetrisNetRoundResultPacket
{
  uint8_t _Type       = static_cast<uint8_t>( TetrisNetPacketType::RoundResult );
  int8_t  _WinnerRole = -1; // -1 = Draw, 1 = Host won, 2 = Client won
  uint8_t _HostWins   = 0;
  uint8_t _ClientWins = 0;
  char    _Reason[48] = {};
};

#pragma pack( pop )

} // namespace Tetris

#endif /* _TetrisNetProtocol_ */
