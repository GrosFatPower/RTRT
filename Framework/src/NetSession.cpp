#include "NetSession.h"

#include <enet/enet.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET RouteSocket;
#define CLOSE_ROUTE_SOCKET( s ) closesocket( s )
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int RouteSocket;
#ifndef INVALID_SOCKET
#define INVALID_SOCKET ( -1 )
#endif
#define CLOSE_ROUTE_SOCKET( s ) close( s )
#endif

namespace RTRT
{

namespace
{
int g_ENetInitRefCount = 0;

void AcquireENet()
{
  if ( g_ENetInitRefCount == 0 )
  {
    enet_initialize();
  }
  ++g_ENetInitRefCount;
}

void ReleaseENet()
{
  if ( g_ENetInitRefCount > 0 )
  {
    --g_ENetInitRefCount;
    if ( g_ENetInitRefCount == 0 )
    {
      enet_deinitialize();
    }
  }
}

std::string SanitizeField( const std::string & iText )
{
  std::string out = iText;
  for ( char & ch : out )
  {
    if ( ch == '|' || ch == '\n' || ch == '\r' )
      ch = ' ';
  }
  return out;
}
} // namespace

// ----------------------------------------------------------------------------
// Impl
// ----------------------------------------------------------------------------
struct NetSession::Impl
{
  ENetHost * _Host            = nullptr;
  ENetPeer * _Peer            = nullptr;
  ENetSocket _BeaconSocket    = ENET_SOCKET_NULL;
  ENetSocket _DiscoverySocket = ENET_SOCKET_NULL;
};

// ----------------------------------------------------------------------------
// CTOR / DTOR
// ----------------------------------------------------------------------------
NetSession::NetSession()
: _Impl( std::make_unique<Impl>() )
{
  AcquireENet();
}

NetSession::~NetSession()
{
  StopLanDiscovery();
  Disconnect();
  ReleaseENet();
}

// ----------------------------------------------------------------------------
// StartHost
// ----------------------------------------------------------------------------
bool NetSession::StartHost( uint16_t iPort,
                            const std::string & iHostName,
                            const std::string & iRoomInfo,
                            uint16_t iDiscoveryPort,
                            bool iEnableUpnp )
{
  Disconnect();

  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = iPort;

  // 1 peer connection slot, 2 channels (0 = Reliable Ordered, 1 = Unreliable Sequenced)
  _Impl->_Host = enet_host_create( &address, 1, 2, 0, 0 );
  if ( !_Impl->_Host )
  {
    std::cerr << "[NET] Error: Failed to create ENet host on port " << iPort << "!" << std::endl;
    return false;
  }

  _Role              = NetRole::Host;
  _State             = NetState::Hosting;
  _BoundPort         = iPort;
  _DiscoveryPort     = iDiscoveryPort;
  _HostName          = SanitizeField( iHostName.empty() ? "Host" : iHostName );
  _RoomInfo          = SanitizeField( iRoomInfo.empty() ? "VS Battle" : iRoomInfo );
  _LastBeaconSentSec = 0.0;

  std::cout << "[NET] Host started: listening on UDP port " << iPort << " (Local IP: " << GetLocalIPv4Address()
            << ", UPnP: " << ( iEnableUpnp ? "enabled" : "disabled" ) << ")" << std::endl;

  // Create UDP broadcast socket for LAN room discovery beacon
  _Impl->_BeaconSocket = enet_socket_create( ENET_SOCKET_TYPE_DATAGRAM );
  if ( _Impl->_BeaconSocket != ENET_SOCKET_NULL )
  {
    enet_socket_set_option( _Impl->_BeaconSocket, ENET_SOCKOPT_NONBLOCK, 1 );
    enet_socket_set_option( _Impl->_BeaconSocket, ENET_SOCKOPT_BROADCAST, 1 );
  }

  // Discover public WAN IP and optionally request UPnP port mapping
  _UpnpEnabled = iEnableUpnp;
  _Upnp.StartMappingAsync( iPort, iPort, "RTRT Game", GetLocalIPv4Address(), _UpnpEnabled );

  return true;
}

// ----------------------------------------------------------------------------
// UpdateHostRoomInfo
// ----------------------------------------------------------------------------
void NetSession::UpdateHostRoomInfo( const std::string & iHostName, const std::string & iRoomInfo )
{
  _HostName = SanitizeField( iHostName.empty() ? "Host" : iHostName );
  _RoomInfo = SanitizeField( iRoomInfo.empty() ? "VS Battle" : iRoomInfo );
}

// ----------------------------------------------------------------------------
// StartClient
// ----------------------------------------------------------------------------
bool NetSession::StartClient( const std::string & iAddress, uint16_t iPort )
{
  Disconnect();

  _Impl->_Host = enet_host_create( nullptr, 1, 2, 0, 0 );
  if ( !_Impl->_Host )
  {
    std::cerr << "[NET] Error: Failed to create client ENet host!" << std::endl;
    return false;
  }

  ENetAddress address;
  address.port = iPort;
  if ( enet_address_set_host_ip( &address, iAddress.c_str() ) != 0 &&
       enet_address_set_host( &address, iAddress.c_str() ) != 0 )
  {
    std::cerr << "[NET] Error: Failed to resolve address: " << iAddress << std::endl;
    enet_host_destroy( _Impl->_Host );
    _Impl->_Host = nullptr;
    return false;
  }

  _Impl->_Peer = enet_host_connect( _Impl->_Host, &address, 2, 0 );
  if ( !_Impl->_Peer )
  {
    std::cerr << "[NET] Error: enet_host_connect failed for " << iAddress << ":" << iPort << "!" << std::endl;
    enet_host_destroy( _Impl->_Host );
    _Impl->_Host = nullptr;
    return false;
  }

  std::cout << "[NET] Client started: connecting to " << iAddress << ":" << iPort << " (timeout: 6.0s)..." << std::endl;

  // Ping frequently for accurate low-latency RTT telemetry
  enet_peer_ping_interval( _Impl->_Peer, 250 );
  enet_peer_timeout( _Impl->_Peer, 16, 3000, 8000 );

  enet_host_flush( _Impl->_Host );

  _Role                = NetRole::Client;
  _State               = NetState::Connecting;
  _BoundPort           = iPort;
  _ConnectStartTimeSec = 0.0;

  return true;
}

// ----------------------------------------------------------------------------
// Disconnect
// ----------------------------------------------------------------------------
void NetSession::Disconnect()
{
  if ( _Role != NetRole::None || _State != NetState::Disconnected )
  {
    std::cout << "[NET] Disconnecting session. (Role=" << ( _Role == NetRole::Host ? "Host" : ( _Role == NetRole::Client ? "Client" : "None" ) )
              << ", State=" << static_cast<int>( _State ) << ")" << std::endl;
  }

  _Upnp.StopMappingAsync();
  _UpnpEnabled = false;

  if ( _Impl->_BeaconSocket != ENET_SOCKET_NULL )
  {
    enet_socket_destroy( _Impl->_BeaconSocket );
    _Impl->_BeaconSocket = ENET_SOCKET_NULL;
  }

  if ( _Impl->_Peer )
  {
    enet_peer_disconnect_now( _Impl->_Peer, 0 );
    if ( _Impl->_Host )
      enet_host_flush( _Impl->_Host );
    _Impl->_Peer = nullptr;
  }

  if ( _Impl->_Host )
  {
    enet_host_destroy( _Impl->_Host );
    _Impl->_Host = nullptr;
  }

  _Role  = NetRole::None;
  _State = NetState::Disconnected;
}

// ----------------------------------------------------------------------------
// DisconnectPeer
// ----------------------------------------------------------------------------
void NetSession::DisconnectPeer()
{
  std::cout << "[NET] Disconnecting current peer while preserving host listening socket." << std::endl;
  if ( _Impl->_Peer )
  {
    enet_peer_disconnect_later( _Impl->_Peer, 0 );
    if ( _Impl->_Host )
      enet_host_flush( _Impl->_Host );
    _Impl->_Peer = nullptr;
  }

  if ( _Role == NetRole::Host )
  {
    _State = NetState::Hosting;
  }
  else
  {
    Disconnect();
  }
}

// ----------------------------------------------------------------------------
// StartLanDiscovery / StopLanDiscovery
// ----------------------------------------------------------------------------
bool NetSession::StartLanDiscovery( uint16_t iDiscoveryPort )
{
  if ( _Impl->_DiscoverySocket != ENET_SOCKET_NULL && _DiscoveryPort == iDiscoveryPort )
    return true;

  StopLanDiscovery();
  _DiscoveryPort = iDiscoveryPort;

  _Impl->_DiscoverySocket = enet_socket_create( ENET_SOCKET_TYPE_DATAGRAM );
  if ( _Impl->_DiscoverySocket == ENET_SOCKET_NULL )
    return false;

  enet_socket_set_option( _Impl->_DiscoverySocket, ENET_SOCKOPT_REUSEADDR, 1 );
  enet_socket_set_option( _Impl->_DiscoverySocket, ENET_SOCKOPT_NONBLOCK, 1 );

  ENetAddress bindAddr;
  bindAddr.host = ENET_HOST_ANY;
  bindAddr.port = _DiscoveryPort;
  if ( enet_socket_bind( _Impl->_DiscoverySocket, &bindAddr ) != 0 )
  {
    enet_socket_destroy( _Impl->_DiscoverySocket );
    _Impl->_DiscoverySocket = ENET_SOCKET_NULL;
    return false;
  }

  return true;
}

void NetSession::StopLanDiscovery()
{
  if ( _Impl->_DiscoverySocket != ENET_SOCKET_NULL )
  {
    enet_socket_destroy( _Impl->_DiscoverySocket );
    _Impl->_DiscoverySocket = ENET_SOCKET_NULL;
  }
  _DiscoveredRooms.clear();
}

bool NetSession::IsLanDiscoveryActive() const
{
  return ( _Impl->_DiscoverySocket != ENET_SOCKET_NULL );
}

// ----------------------------------------------------------------------------
// BroadcastLanBeacon
// ----------------------------------------------------------------------------
void NetSession::BroadcastLanBeacon( double iCurrentTimeSec )
{
  if ( _State != NetState::Hosting || _Impl->_BeaconSocket == ENET_SOCKET_NULL )
    return;

  if ( ( iCurrentTimeSec - _LastBeaconSentSec ) < 0.6 )
    return;
  _LastBeaconSentSec = iCurrentTimeSec;

  char payload[256] = {};
  std::snprintf( payload, sizeof( payload ), "RTRT_LAN_V1|%u|%s|%s",
                 static_cast<unsigned int>( _BoundPort ),
                 _HostName.c_str(),
                 _RoomInfo.c_str() );

  ENetBuffer buf;
  buf.data       = payload;
  buf.dataLength = std::strlen( payload );

  // 1. Broadcast to LAN subnet (255.255.255.255)
  ENetAddress bcastAddr;
  bcastAddr.host = ENET_HOST_BROADCAST;
  bcastAddr.port = _DiscoveryPort;
  enet_socket_send( _Impl->_BeaconSocket, &bcastAddr, &buf, 1 );

  // 2. Also send to loopback (127.0.0.1) so 2 instances on the same PC always discover each other
  ENetAddress loopAddr;
  loopAddr.port = _DiscoveryPort;
  if ( enet_address_set_host_ip( &loopAddr, "127.0.0.1" ) == 0 )
  {
    enet_socket_send( _Impl->_BeaconSocket, &loopAddr, &buf, 1 );
  }
}

// ----------------------------------------------------------------------------
// PollLanDiscovery
// ----------------------------------------------------------------------------
void NetSession::PollLanDiscovery( double iCurrentTimeSec )
{
  if ( _Impl->_DiscoverySocket == ENET_SOCKET_NULL )
    return;

  char recvBuf[512] = {};
  for ( int iter = 0; iter < 16; ++iter )
  {
    ENetAddress senderAddr = {};
    ENetBuffer buf;
    buf.data       = recvBuf;
    buf.dataLength = sizeof( recvBuf ) - 1;

    const int received = enet_socket_receive( _Impl->_DiscoverySocket, &senderAddr, &buf, 1 );
    if ( received <= 0 )
      break;

    recvBuf[received] = '\0';
    std::string msg( recvBuf );

    // Expected format: RTRT_LAN_V1|<port>|<hostName>|<roomInfo>
    const std::string prefix = "RTRT_LAN_V1|";
    if ( msg.rfind( prefix, 0 ) != 0 )
      continue;

    const size_t p1 = prefix.size();
    const size_t p2 = msg.find( '|', p1 );
    if ( p2 == std::string::npos )
      continue;
    const size_t p3 = msg.find( '|', p2 + 1 );
    if ( p3 == std::string::npos )
      continue;

    const int parsedPort = std::atoi( msg.substr( p1, p2 - p1 ).c_str() );
    if ( parsedPort <= 0 || parsedPort > 65535 )
      continue;

    const std::string hostName = msg.substr( p2 + 1, p3 - ( p2 + 1 ) );
    const std::string roomInfo = msg.substr( p3 + 1 );

    char ipStr[64] = {};
    if ( enet_address_get_host_ip( &senderAddr, ipStr, sizeof( ipStr ) ) != 0 )
      std::snprintf( ipStr, sizeof( ipStr ), "127.0.0.1" );

    std::string addrStr( ipStr );
    if ( addrStr == "0.0.0.0" || addrStr.empty() )
      addrStr = "127.0.0.1";

    // Ignore our own host beacon if we happen to be hosting on the same port
    if ( _Role == NetRole::Host && static_cast<uint16_t>( parsedPort ) == _BoundPort &&
         ( addrStr == "127.0.0.1" || addrStr == GetLocalIPv4Address() ) )
    {
      continue;
    }

    bool updated = false;
    for ( LanRoomInfo & room : _DiscoveredRooms )
    {
      if ( room._GamePort == static_cast<uint16_t>( parsedPort ) &&
           ( room._Address == addrStr || room._HostName == hostName ) )
      {
        room._HostName    = hostName;
        room._RoomInfo    = roomInfo;
        // Prefer specific LAN IP over 127.0.0.1 if both arrive, or keep existing valid IP
        if ( room._Address == "127.0.0.1" && addrStr != "127.0.0.1" )
          room._Address = addrStr;
        room._LastSeenSec = iCurrentTimeSec;
        updated           = true;
        break;
      }
    }

    if ( !updated )
    {
      LanRoomInfo info;
      info._HostName    = hostName;
      info._RoomInfo    = roomInfo;
      info._Address     = addrStr;
      info._GamePort    = static_cast<uint16_t>( parsedPort );
      info._LastSeenSec = iCurrentTimeSec;
      _DiscoveredRooms.push_back( info );
    }
  }

  // Prune stale rooms (> 2.8 seconds without beacon)
  _DiscoveredRooms.erase(
    std::remove_if( _DiscoveredRooms.begin(), _DiscoveredRooms.end(),
                    [iCurrentTimeSec]( const LanRoomInfo & iRoom ) {
                      return ( iCurrentTimeSec - iRoom._LastSeenSec ) > 2.8;
                    } ),
    _DiscoveredRooms.end() );
}

// ----------------------------------------------------------------------------
// Poll
// ----------------------------------------------------------------------------
void NetSession::Poll( double iCurrentTimeSec,
                       std::vector<NetPacketEvent> & oReceivedPackets,
                       bool & oPeerConnected,
                       bool & oPeerDisconnected )
{
  oReceivedPackets.clear();
  oPeerConnected    = false;
  oPeerDisconnected = false;

  BroadcastLanBeacon( iCurrentTimeSec );
  PollLanDiscovery( iCurrentTimeSec );

  if ( !_Impl->_Host )
    return;

  if ( _State == NetState::Connecting )
  {
    if ( _ConnectStartTimeSec <= 0.0 )
    {
      _ConnectStartTimeSec = iCurrentTimeSec;
      std::cout << "[NET] Initiating connection handshake..." << std::endl;
    }
    else if ( ( iCurrentTimeSec - _ConnectStartTimeSec ) > 6.0 )
    {
      std::cerr << "[NET] Connection TIMEOUT after 6.0s! No response from target host on UDP port "
                << _BoundPort << ". (Host unreachable or incoming UDP blocked by router / Windows Firewall)." << std::endl;
      Disconnect();
      oPeerDisconnected = true;
      return;
    }
  }

  ENetEvent netEvent;
  while ( _Impl->_Host && enet_host_service( _Impl->_Host, &netEvent, 0 ) > 0 )
  {
    switch ( netEvent.type )
    {
    case ENET_EVENT_TYPE_CONNECT:
    {
      _Impl->_Peer = netEvent.peer;
      enet_peer_ping_interval( _Impl->_Peer, 250 );
      enet_peer_timeout( _Impl->_Peer, 16, 3000, 8000 );
      _State         = NetState::Connected;
      oPeerConnected = true;

      char ipBuf[64] = {};
      enet_address_get_host_ip( &netEvent.peer->address, ipBuf, sizeof( ipBuf ) );
      std::cout << "[NET] ENet CONNECT: connected to " << ipBuf << ":" << netEvent.peer->address.port
                << " (Role=" << ( _Role == NetRole::Host ? "Host" : "Client" ) << ")" << std::endl;
      break;
    }

    case ENET_EVENT_TYPE_RECEIVE:
    {
      if ( netEvent.packet && netEvent.packet->data && netEvent.packet->dataLength > 0 )
      {
        NetPacketEvent pkt;
        pkt._Channel = netEvent.channelID;
        pkt._Data.assign( netEvent.packet->data,
                          netEvent.packet->data + netEvent.packet->dataLength );
        std::cout << "[NET] Packet received on Channel " << static_cast<int>( netEvent.channelID )
                  << " (type " << static_cast<int>( netEvent.packet->data[0] )
                  << ", " << netEvent.packet->dataLength << " bytes)" << std::endl;
        oReceivedPackets.push_back( std::move( pkt ) );
      }
      if ( netEvent.packet )
        enet_packet_destroy( netEvent.packet );
      break;
    }

    case ENET_EVENT_TYPE_DISCONNECT:
    {
      std::cout << "[NET] ENet DISCONNECT from peer." << std::endl;
      _Impl->_Peer      = nullptr;
      oPeerDisconnected = true;
      if ( _Role == NetRole::Host )
      {
        // Host stays open in Hosting state waiting for a new client unless explicitly stopped
        _State = NetState::Hosting;
      }
      else
      {
        Disconnect();
      }
      break;
    }

    default:
      break;
    }
  }

  if ( _Impl->_Host )
  {
    enet_host_flush( _Impl->_Host );
  }
}

// ----------------------------------------------------------------------------
// SendReliable / SendUnreliableSequenced
// ----------------------------------------------------------------------------
bool NetSession::SendReliable( uint8_t iChannel, const void * iData, size_t iSize )
{
  if ( !_Impl->_Host || !_Impl->_Peer || _State != NetState::Connected || !iData || iSize == 0 )
    return false;

  ENetPacket * packet = enet_packet_create( iData, iSize, ENET_PACKET_FLAG_RELIABLE );
  if ( !packet )
    return false;

  if ( enet_peer_send( _Impl->_Peer, iChannel, packet ) < 0 )
  {
    enet_packet_destroy( packet );
    return false;
  }

  std::cout << "[NET] Sent reliable packet on Channel " << static_cast<int>( iChannel )
            << " (type " << static_cast<int>( static_cast<const uint8_t *>( iData )[0] )
            << ", " << iSize << " bytes)" << std::endl;

  enet_host_flush( _Impl->_Host );
  return true;
}

bool NetSession::SendUnreliableSequenced( uint8_t iChannel, const void * iData, size_t iSize )
{
  if ( !_Impl->_Host || !_Impl->_Peer || _State != NetState::Connected || !iData || iSize == 0 )
    return false;

  // Flags = 0 means Unreliable Sequenced in ENet
  ENetPacket * packet = enet_packet_create( iData, iSize, 0 );
  if ( !packet )
    return false;

  if ( enet_peer_send( _Impl->_Peer, iChannel, packet ) < 0 )
  {
    enet_packet_destroy( packet );
    return false;
  }

  return true;
}

// ----------------------------------------------------------------------------
// Telemetry Helpers
// ----------------------------------------------------------------------------
uint32_t NetSession::GetPingMs() const
{
  if ( _Impl->_Peer && _State == NetState::Connected )
    return _Impl->_Peer->roundTripTime;
  return 0;
}

std::string NetSession::GetPeerAddressString() const
{
  if ( !_Impl->_Peer || _State != NetState::Connected )
    return "";

  char ipStr[64] = {};
  if ( enet_address_get_host_ip( &_Impl->_Peer->address, ipStr, sizeof( ipStr ) ) == 0 )
  {
    char fullBuf[80] = {};
    std::snprintf( fullBuf, sizeof( fullBuf ), "%s:%u", ipStr,
                   static_cast<unsigned int>( _Impl->_Peer->address.port ) );
    return std::string( fullBuf );
  }
  return "";
}

std::string NetSession::GetLocalIPv4Address() const
{
  // 1. Primary: Query OS routing table via an unbound UDP connect() to a public IP
  RouteSocket routeSock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
  if ( routeSock != INVALID_SOCKET )
  {
    sockaddr_in remoteAddr = {};
    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_port   = htons( 53 );
    inet_pton( AF_INET, "8.8.8.8", &remoteAddr.sin_addr );

    if ( connect( routeSock, reinterpret_cast<sockaddr *>( &remoteAddr ), sizeof( remoteAddr ) ) == 0 )
    {
      sockaddr_in localAddr = {};
      socklen_t addrLen = sizeof( localAddr );
      if ( getsockname( routeSock, reinterpret_cast<sockaddr *>( &localAddr ), &addrLen ) == 0 )
      {
        char ipStr[64] = {};
        if ( inet_ntop( AF_INET, &localAddr.sin_addr, ipStr, sizeof( ipStr ) ) )
        {
          std::string routedIp( ipStr );
          if ( !routedIp.empty() && routedIp != "0.0.0.0" && routedIp != "127.0.0.1" )
          {
            CLOSE_ROUTE_SOCKET( routeSock );
            return routedIp;
          }
        }
      }
    }
    CLOSE_ROUTE_SOCKET( routeSock );
  }

  // 2. Fallback: Enumerate local hostname addresses
  char hostName[256] = {};
  if ( gethostname( hostName, sizeof( hostName ) ) == 0 )
  {
    addrinfo hints = {};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo * res = nullptr;
    if ( getaddrinfo( hostName, nullptr, &hints, &res ) == 0 && res )
    {
      std::string bestIp = "127.0.0.1";
      for ( addrinfo * p = res; p != nullptr; p = p->ai_next )
      {
        sockaddr_in * ipv4 = reinterpret_cast<sockaddr_in *>( p->ai_addr );
        char ipStr[64] = {};
        if ( inet_ntop( AF_INET, &( ipv4->sin_addr ), ipStr, sizeof( ipStr ) ) )
        {
          std::string candidate( ipStr );
          if ( candidate != "127.0.0.1" && candidate.rfind( "169.254.", 0 ) != 0 )
          {
            bestIp = candidate;
            // Prefer standard private LAN ranges (192.168.x.x or 10.x.x.x), skipping VirtualBox 192.168.56.x
            if ( ( candidate.rfind( "192.168.", 0 ) == 0 && candidate.rfind( "192.168.56.", 0 ) != 0 ) ||
                 candidate.rfind( "10.", 0 ) == 0 )
              break;
          }
        }
      }
      freeaddrinfo( res );
      return bestIp;
    }
  }
  return "127.0.0.1";
}

} // namespace RTRT
