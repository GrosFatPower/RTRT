#ifndef _NetSession_
#define _NetSession_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace RTRT
{

enum class NetRole : int
{
  None   = 0,
  Host   = 1,
  Client = 2
};

enum class NetState : int
{
  Disconnected = 0,
  Hosting      = 1, // Waiting for peer to connect
  Connecting   = 2, // Outbound connection in progress
  Connected    = 3  // Peer connected and active
};

struct LanRoomInfo
{
  std::string _HostName;
  std::string _RoomInfo;
  std::string _Address;
  uint16_t    _GamePort    = 27015;
  double      _LastSeenSec = 0.0;
};

struct NetPacketEvent
{
  uint8_t              _Channel = 0;
  std::vector<uint8_t> _Data;
};

class NetSession
{
public:
  NetSession();
  virtual ~NetSession();

  bool StartHost( uint16_t iPort,
                  const std::string & iHostName,
                  const std::string & iRoomInfo,
                  uint16_t iDiscoveryPort = 27016 );

  void UpdateHostRoomInfo( const std::string & iHostName, const std::string & iRoomInfo );

  bool StartClient( const std::string & iAddress, uint16_t iPort );

  void Disconnect();

  bool StartLanDiscovery( uint16_t iDiscoveryPort = 27016 );
  void StopLanDiscovery();
  bool IsLanDiscoveryActive() const;
  const std::vector<LanRoomInfo> & GetDiscoveredLanRooms() const { return _DiscoveredRooms; }

  void Poll( double iCurrentTimeSec,
             std::vector<NetPacketEvent> & oReceivedPackets,
             bool & oPeerConnected,
             bool & oPeerDisconnected );

  bool SendReliable( uint8_t iChannel, const void * iData, size_t iSize );
  bool SendUnreliableSequenced( uint8_t iChannel, const void * iData, size_t iSize );

  NetRole GetRole() const { return _Role; }
  NetState GetState() const { return _State; }
  bool IsConnected() const { return _State == NetState::Connected; }
  bool IsHosting() const { return _State == NetState::Hosting; }
  bool IsConnecting() const { return _State == NetState::Connecting; }
  bool IsHost() const { return _Role == NetRole::Host; }

  uint16_t GetBoundPort() const { return _BoundPort; }
  uint32_t GetPingMs() const;
  std::string GetPeerAddressString() const;
  std::string GetLocalIPv4Address() const;

protected:
  void BroadcastLanBeacon( double iCurrentTimeSec );
  void PollLanDiscovery( double iCurrentTimeSec );

protected:
  struct Impl;
  std::unique_ptr<Impl>    _Impl;

  NetRole                  _Role                = NetRole::None;
  NetState                 _State               = NetState::Disconnected;
  uint16_t                 _BoundPort           = 27015;
  uint16_t                 _DiscoveryPort       = 27016;
  std::string              _HostName            = "Player 1";
  std::string              _RoomInfo            = "VS Battle";
  double                   _LastBeaconSentSec   = 0.0;
  double                   _ConnectStartTimeSec = 0.0;
  std::vector<LanRoomInfo> _DiscoveredRooms;
};

} // namespace RTRT

#endif /* _NetSession_ */
