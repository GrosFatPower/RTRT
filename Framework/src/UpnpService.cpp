#include "UpnpService.h"

#include "RoomCode.h"

#include <miniupnpc.h>
#include <miniwget.h>
#include <upnpcommands.h>
#include <upnperrors.h>

#include <iostream>

extern "C" int addr_is_reserved( const char * addr_str );

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET StunSocket;
#define CLOSE_STUN_SOCKET(s) closesocket(s)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int StunSocket;
#define INVALID_SOCKET (-1)
#define CLOSE_STUN_SOCKET(s) close(s)
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace RTRT
{

namespace
{

// Performs a 20-byte STUN Binding Request (RFC 5389 / RFC 3489) over UDP to discover external WAN IPv4.
// Typically completes in 10-30 ms without relying on HTTP scraping.
bool QueryStunServer( const char * iHost, uint16_t iPort, unsigned int iTimeoutMs, std::string & oPublicIP )
{
  StunSocket s = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
  if ( s == INVALID_SOCKET )
    return false;

#ifdef _WIN32
  DWORD tv = static_cast<DWORD>( iTimeoutMs );
  setsockopt( s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>( &tv ), sizeof( tv ) );
#else
  struct timeval tv;
  tv.tv_sec  = iTimeoutMs / 1000;
  tv.tv_usec = ( iTimeoutMs % 1000 ) * 1000;
  setsockopt( s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>( &tv ), sizeof( tv ) );
#endif

  struct hostent * he = gethostbyname( iHost );
  if ( !he || !he->h_addr_list || !he->h_addr_list[0] )
  {
    CLOSE_STUN_SOCKET( s );
    return false;
  }

  struct sockaddr_in saddr;
  std::memset( &saddr, 0, sizeof( saddr ) );
  saddr.sin_family = AF_INET;
  saddr.sin_port   = htons( iPort );
  std::memcpy( &saddr.sin_addr, he->h_addr_list[0], sizeof( struct in_addr ) );

  // STUN Binding Request: 20 bytes
  uint8_t req[20];
  std::memset( req, 0, sizeof( req ) );
  req[0] = 0x00; req[1] = 0x01; // Type: Binding Request
  req[2] = 0x00; req[3] = 0x00; // Message Length: 0
  req[4] = 0x21; req[5] = 0x12; req[6] = 0xA4; req[7] = 0x42; // Magic Cookie
  for ( int i = 8; i < 20; ++i )
    req[i] = static_cast<uint8_t>( std::rand() & 0xFF );

  sendto( s, reinterpret_cast<const char *>( req ), sizeof( req ), 0,
          reinterpret_cast<struct sockaddr *>( &saddr ), sizeof( saddr ) );

  uint8_t resp[512];
  const int received = recv( s, reinterpret_cast<char *>( resp ), sizeof( resp ), 0 );
  CLOSE_STUN_SOCKET( s );

  if ( received < 20 )
    return false;

  // Verify Binding Response (0x0101) and Magic Cookie (0x2112A442)
  if ( resp[0] != 0x01 || resp[1] != 0x01 )
    return false;
  if ( resp[4] != 0x21 || resp[5] != 0x12 || resp[6] != 0xA4 || resp[7] != 0x42 )
    return false;

  int idx = 20;
  while ( idx + 4 <= received )
  {
    const uint16_t attrType = static_cast<uint16_t>( ( resp[idx] << 8 ) | resp[idx + 1] );
    const uint16_t attrLen  = static_cast<uint16_t>( ( resp[idx + 2] << 8 ) | resp[idx + 3] );
    idx += 4;
    if ( idx + attrLen > received )
      break;

    if ( attrType == 0x0020 && attrLen >= 8 ) // XOR-MAPPED-ADDRESS
    {
      if ( resp[idx + 1] == 0x01 ) // IPv4
      {
        const uint8_t ipBytes[4] = {
          static_cast<uint8_t>( resp[idx + 4] ^ 0x21 ),
          static_cast<uint8_t>( resp[idx + 5] ^ 0x12 ),
          static_cast<uint8_t>( resp[idx + 6] ^ 0xA4 ),
          static_cast<uint8_t>( resp[idx + 7] ^ 0x42 )
        };
        char ipStr[32] = {};
        std::snprintf( ipStr, sizeof( ipStr ), "%u.%u.%u.%u", ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3] );
        if ( RoomCode::IsPublicInternetIP( ipStr ) )
        {
          oPublicIP = ipStr;
          return true;
        }
      }
    }
    else if ( attrType == 0x0001 && attrLen >= 8 ) // MAPPED-ADDRESS (RFC 3489)
    {
      if ( resp[idx + 1] == 0x01 ) // IPv4
      {
        char ipStr[32] = {};
        std::snprintf( ipStr, sizeof( ipStr ), "%u.%u.%u.%u", resp[idx + 4], resp[idx + 5], resp[idx + 6], resp[idx + 7] );
        if ( RoomCode::IsPublicInternetIP( ipStr ) )
        {
          oPublicIP = ipStr;
          return true;
        }
      }
    }
    idx += ( attrLen + 3 ) & ~3; // Align to 4-byte boundary
  }

  return false;
}

// Queries a lightweight plain HTTP IP reflection endpoint using miniwget.
// NOTE: miniwget's parseURL requires a trailing slash for hostnames without paths!
bool QueryHttpServer( const char * iUrl, std::string & oPublicIP )
{
  int size = 0;
  int statusCode = 0;
  void * data = miniwget( iUrl, &size, 0, &statusCode );
  if ( data && size > 0 && statusCode == 200 )
  {
    std::string ip( reinterpret_cast<char *>( data ), size );
    free( data );
    while ( !ip.empty() && ( ip.back() == '\r' || ip.back() == '\n' || ip.back() == ' ' ) )
      ip.pop_back();
    if ( RoomCode::IsPublicInternetIP( ip ) )
    {
      oPublicIP = ip;
      return true;
    }
  }
  if ( data )
    free( data );
  return false;
}

} // namespace

// ----------------------------------------------------------------------------
// ResolvePublicIP
// ----------------------------------------------------------------------------
std::string UpnpService::ResolvePublicIP( unsigned int iTimeoutMs )
{
#ifdef _WIN32
  WSADATA wsaData;
  const bool wsaOk = ( 0 == WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) );
#endif

  std::string publicIp;

  // 1. Primary: Fast UDP STUN Reflection (< 30 ms)
  const struct { const char * host; uint16_t port; } stunServers[] = {
    { "stun.l.google.com", 19302 },
    { "stun1.l.google.com", 19302 },
    { "stun.cloudflare.com", 3478 }
  };

  for ( const auto & s : stunServers )
  {
    if ( QueryStunServer( s.host, s.port, iTimeoutMs, publicIp ) )
    {
      std::cout << "[NET-STUN] Public WAN IP discovered via STUN (" << s.host << "): " << publicIp << std::endl;
      break;
    }
  }

  // 2. Secondary: Plain HTTP with trailing slashes if STUN is blocked
  if ( publicIp.empty() )
  {
    const char * httpEndpoints[] = {
      "http://api.ipify.org/",
      "http://checkip.amazonaws.com/",
      "http://icanhazip.com/"
    };
    for ( const char * url : httpEndpoints )
    {
      if ( QueryHttpServer( url, publicIp ) )
      {
        std::cout << "[NET-HTTP] Public WAN IP discovered via HTTP fallback (" << url << "): " << publicIp << std::endl;
        break;
      }
    }
  }

#ifdef _WIN32
  if ( wsaOk )
    WSACleanup();
#endif

  return publicIp;
}

// ----------------------------------------------------------------------------
// CTOR / DTOR
// ----------------------------------------------------------------------------
UpnpService::UpnpService()
{
}

UpnpService::~UpnpService()
{
  StopMappingAsync();
  JoinWorker();
}

// ----------------------------------------------------------------------------
// StartMappingAsync / StopMappingAsync
// ----------------------------------------------------------------------------
void UpnpService::StartMappingAsync( uint16_t iInternalPort,
                                     uint16_t iExternalPort,
                                     const std::string & iDescription,
                                     const std::string & iLocalInterface,
                                     bool iPerformPortMapping )
{
  StopMappingAsync();
  JoinWorker();

  UpnpStatusInfo status;
  status._State         = UpnpState::Discovering;
  status._InternalPort  = iInternalPort;
  status._ExternalPort  = iExternalPort;
  status._StatusMessage = iPerformPortMapping ? "Discovering router & public IP..." : "Resolving public IP...";

  {
    std::lock_guard<std::mutex> lock( _Mutex );
    _Status        = status;
    _StopRequested = false;
  }

  _Running = true;
  _Worker  = std::thread( &UpnpService::WorkerMain, this, iInternalPort, iExternalPort, iDescription, iLocalInterface, iPerformPortMapping );
}

void UpnpService::StopMappingAsync()
{
  {
    std::lock_guard<std::mutex> lock( _Mutex );
    _StopRequested = true;
  }
  _StopCondition.notify_all();
}

void UpnpService::JoinWorker()
{
  if ( _Worker.joinable() )
    _Worker.join();
}

// ----------------------------------------------------------------------------
// Status
// ----------------------------------------------------------------------------
UpnpStatusInfo UpnpService::GetStatus() const
{
  std::lock_guard<std::mutex> lock( _Mutex );
  return _Status;
}

bool UpnpService::IsBusy() const
{
  return _Running.load();
}

void UpnpService::SetStatus( const UpnpStatusInfo & iStatus )
{
  std::lock_guard<std::mutex> lock( _Mutex );
  _Status = iStatus;
}

// ----------------------------------------------------------------------------
// WorkerMain
// ----------------------------------------------------------------------------
void UpnpService::WorkerMain( uint16_t iInternalPort, uint16_t iExternalPort, std::string iDescription, std::string iLocalInterface, bool iPerformPortMapping )
{
#ifdef _WIN32
  WSADATA wsaData;
  const bool wsaOk = ( 0 == WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) );
#endif

  RunMapping( iInternalPort, iExternalPort, iDescription, iLocalInterface, iPerformPortMapping );

#ifdef _WIN32
  if ( wsaOk )
    WSACleanup();
#endif
}

// ----------------------------------------------------------------------------
// RunMapping
// ----------------------------------------------------------------------------
void UpnpService::RunMapping( uint16_t iInternalPort, uint16_t iExternalPort, const std::string & iDescription, const std::string & iLocalInterface, bool iPerformPortMapping )
{
  UpnpStatusInfo status = GetStatus();

  std::cout << "[UPnP] Worker starting for UDP internal port " << iInternalPort << " -> external port " << iExternalPort
            << " (PerformMapping: " << ( iPerformPortMapping ? "yes" : "no" ) << ")" << std::endl;

  auto stopRequested = [this]() {
    std::lock_guard<std::mutex> lock( _Mutex );
    return _StopRequested;
  };

  // 1. Immediately resolve true public WAN IP via STUN / HTTP reflection
  std::string routableIp = ResolvePublicIP( 1500 );
  if ( !routableIp.empty() )
  {
    status._ExternalIP = routableIp;
    status._RoomCode   = RoomCode::Encode( routableIp, iExternalPort );
    SetStatus( status );
    std::cout << "[UPnP] Public WAN IP verified: " << routableIp << " -> Room Code: " << status._RoomCode << std::endl;
  }
  else
  {
    std::cerr << "[UPnP] Warning: Failed to discover public WAN IP via STUN or HTTP reflection!" << std::endl;
  }

  if ( !iPerformPortMapping || stopRequested() )
  {
    if ( !iPerformPortMapping )
    {
      status._State         = UpnpState::Idle;
      status._StatusMessage = "UPnP disabled: using direct connection with public IP";
      SetStatus( status );
    }
    _Running = false;
    return;
  }

  // 2. Discover the Internet Gateway Device (target the active LAN IP first if multiple adapters exist)
  int discoverError = 0;
  UPNPDev * devList = nullptr;

  const std::string iface = ( !iLocalInterface.empty() && iLocalInterface != "127.0.0.1" ) ? iLocalInterface : "";
  std::cout << "[UPnP] Searching for IGD router via SSDP (interface: " << ( iface.empty() ? "auto" : iface ) << ")..." << std::endl;

  if ( !iface.empty() )
  {
    devList = upnpDiscover( 2000, iface.c_str(), nullptr, UPNP_LOCAL_PORT_ANY, 0, 2, &discoverError );
  }

  if ( !devList && !stopRequested() )
  {
    devList = upnpDiscover( 1500, nullptr, nullptr, UPNP_LOCAL_PORT_ANY, 0, 2, &discoverError );
  }

  UPNPUrls urls;
  IGDdatas data;
  std::memset( &urls, 0, sizeof( urls ) );
  std::memset( &data, 0, sizeof( data ) );
  char lanAddr[64]  = {};
  char wanAddr[64]  = {};
  int  igdStatus    = UPNP_NO_IGD;

  if ( devList )
  {
    igdStatus = UPNP_GetValidIGD( devList, &urls, &data, lanAddr, sizeof( lanAddr ), wanAddr, sizeof( wanAddr ) );
    std::cout << "[UPnP] SSDP device discovery finished. UPNP_GetValidIGD status code: " << igdStatus
              << " (1=Valid IGD, 2=Double NAT / Private WAN, 3=Connected, 0=No IGD)" << std::endl;
    std::cout << "[UPnP] Router local LAN IP: " << ( lanAddr[0] ? lanAddr : "none" )
              << ", Router WAN IP: " << ( wanAddr[0] ? wanAddr : "none" ) << std::endl;
    if ( urls.controlURL )
      std::cout << "[UPnP] Router Control URL: " << urls.controlURL << std::endl;
  }
  else
  {
    std::cerr << "[UPnP] SSDP discovery found no UPnP devices on local network (discoverError=" << discoverError << ")." << std::endl;
  }

  if ( devList )
    freeUPNPDevlist( devList );

  if ( stopRequested() )
  {
    if ( igdStatus != UPNP_NO_IGD )
      FreeUPNPUrls( &urls );
    status = UpnpStatusInfo();
    SetStatus( status );
    _Running = false;
    return;
  }

  if ( igdStatus == UPNP_NO_IGD || igdStatus == UPNP_UNKNOWN_DEVICE )
  {
    if ( igdStatus != UPNP_NO_IGD )
      FreeUPNPUrls( &urls );
    status._State         = UpnpState::NotAvailable;
    status._StatusMessage = "No UPnP router found (UPnP disabled on router?)";
    SetStatus( status );
    std::cerr << "[UPnP] " << status._StatusMessage << std::endl;
    _Running = false;
    return;
  }

  status._State         = UpnpState::Mapping;
  status._LocalIP       = lanAddr;
  status._StatusMessage = "Opening port on router...";
  SetStatus( status );

  // 3. Router's external IP check
  char externalIp[64] = {};
  const int ipResult = UPNP_GetExternalIPAddress( urls.controlURL, data.first.servicetype, externalIp );
  const bool hasExternalIp = ( UPNPCOMMAND_SUCCESS == ipResult ) && ( externalIp[0] != '\0' );

  std::cout << "[UPnP] UPNP_GetExternalIPAddress result: " << ( hasExternalIp ? externalIp : "failed/empty" )
            << " (ipResult=" << ipResult << ")" << std::endl;

  if ( igdStatus == UPNP_DISCONNECTED_IGD )
  {
    FreeUPNPUrls( &urls );
    status._State         = UpnpState::NotAvailable;
    status._StatusMessage = "Router reports no Internet connection";
    SetStatus( status );
    std::cerr << "[UPnP] " << status._StatusMessage << std::endl;
    _Running = false;
    return;
  }

  const bool isDoubleNat = ( igdStatus == UPNP_PRIVATEIP_IGD ) ||
                           ( hasExternalIp && ( 0 != addr_is_reserved( externalIp ) ) );
  if ( isDoubleNat )
  {
    std::cout << "[UPnP] Notice: Double NAT / CGNAT detected. Router external IP ("
              << ( externalIp[0] ? externalIp : "none" ) << ") is private/reserved, while public WAN IP is "
              << routableIp << "." << std::endl;
  }

  // 4. Port mapping (UDP). Some routers refuse a timed lease, so fall back to permanent (0).
  char extPortStr[16] = {};
  char intPortStr[16] = {};
  std::snprintf( extPortStr, sizeof( extPortStr ), "%u", static_cast<unsigned int>( iExternalPort ) );
  std::snprintf( intPortStr, sizeof( intPortStr ), "%u", static_cast<unsigned int>( iInternalPort ) );

  std::cout << "[UPnP] Requesting port mapping: UDP external " << extPortStr << " -> " << lanAddr << ":" << intPortStr << "..." << std::endl;

  int mapResult = UPNP_AddPortMapping( urls.controlURL, data.first.servicetype, extPortStr, intPortStr,
                                       lanAddr, iDescription.c_str(), "UDP", nullptr, "7200" );
  if ( UPNPCOMMAND_SUCCESS != mapResult )
  {
    std::cout << "[UPnP] Lease 7200s returned " << mapResult << " (" << strupnperror( mapResult ) << "), retrying with permanent lease 0s..." << std::endl;
    mapResult = UPNP_AddPortMapping( urls.controlURL, data.first.servicetype, extPortStr, intPortStr,
                                     lanAddr, iDescription.c_str(), "UDP", nullptr, "0" );
  }

  if ( UPNPCOMMAND_SUCCESS != mapResult )
  {
    FreeUPNPUrls( &urls );
    char msg[128] = {};
    std::snprintf( msg, sizeof( msg ), "Router refused the port mapping (%s)", strupnperror( mapResult ) );
    status._State         = UpnpState::Failed;
    status._StatusMessage = msg;
    SetStatus( status );
    std::cerr << "[UPnP] ERROR: " << msg << " (code: " << mapResult << ")" << std::endl;
    _Running = false;
    return;
  }

  std::cout << "[UPnP] SUCCESS: UDP port " << extPortStr << " mapped successfully on router!" << std::endl;

  // Prefer STUN's verified public IP; fall back to router's external IP only if it's genuinely public
  if ( routableIp.empty() && hasExternalIp && ( 0 == addr_is_reserved( externalIp ) ) )
  {
    routableIp = externalIp;
  }

  status._State         = UpnpState::Success;
  status._ExternalIP    = !routableIp.empty() ? routableIp : ( hasExternalIp ? externalIp : "" );
  status._StatusMessage = isDoubleNat ? "Port opened via UPnP (Note: Double NAT detected with upstream router)"
                                      : "Port opened via UPnP";
  if ( !status._ExternalIP.empty() )
    status._RoomCode = RoomCode::Encode( status._ExternalIP, iExternalPort );
  SetStatus( status );

  // 5. Keep mapping alive until stop request, then unmap
  {
    std::unique_lock<std::mutex> lock( _Mutex );
    _StopCondition.wait( lock, [this]() { return _StopRequested; } );
  }

  UPNP_DeletePortMapping( urls.controlURL, data.first.servicetype, extPortStr, "UDP", nullptr );
  FreeUPNPUrls( &urls );

  SetStatus( UpnpStatusInfo() );
  _Running = false;
}

} // namespace RTRT
