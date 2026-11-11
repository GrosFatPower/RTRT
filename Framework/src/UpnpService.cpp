#include "UpnpService.h"

#include "RoomCode.h"

#include <miniupnpc.h>
#include <miniwget.h>
#include <upnpcommands.h>
#include <upnperrors.h>

extern "C" int addr_is_reserved( const char * addr_str );

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace RTRT
{

namespace
{
std::string FetchPublicIPFallback()
{
  int size = 0;
  int statusCode = 0;
  void * data = miniwget( "http://api.ipify.org", &size, 0, &statusCode );
  if ( data && size > 0 && statusCode == 200 )
  {
    std::string ip( reinterpret_cast<char *>( data ), size );
    free( data );
    while ( !ip.empty() && ( ip.back() == '\r' || ip.back() == '\n' || ip.back() == ' ' ) )
      ip.pop_back();
    uint32_t parsed = 0;
    if ( RoomCode::ParseIPv4( ip, parsed ) )
      return ip;
  }
  if ( data )
    free( data );
  return "";
}
} // namespace

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
                                     const std::string & iLocalInterface )
{
  StopMappingAsync();
  JoinWorker();

  UpnpStatusInfo status;
  status._State         = UpnpState::Discovering;
  status._InternalPort  = iInternalPort;
  status._ExternalPort  = iExternalPort;
  status._StatusMessage = "Contacting router...";

  {
    std::lock_guard<std::mutex> lock( _Mutex );
    _Status        = status;
    _StopRequested = false;
  }

  _Running = true;
  _Worker  = std::thread( &UpnpService::WorkerMain, this, iInternalPort, iExternalPort, iDescription, iLocalInterface );
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
void UpnpService::WorkerMain( uint16_t iInternalPort, uint16_t iExternalPort, std::string iDescription, std::string iLocalInterface )
{
#ifdef _WIN32
  // miniupnpc does not initialize Winsock itself (reference counted, safe next to ENet)
  WSADATA wsaData;
  const bool wsaOk = ( 0 == WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) );
#endif

  RunMapping( iInternalPort, iExternalPort, iDescription, iLocalInterface );

#ifdef _WIN32
  if ( wsaOk )
    WSACleanup();
#endif
}

// ----------------------------------------------------------------------------
// RunMapping
// ----------------------------------------------------------------------------
void UpnpService::RunMapping( uint16_t iInternalPort, uint16_t iExternalPort, const std::string & iDescription, const std::string & iLocalInterface )
{
  UpnpStatusInfo status = GetStatus();

  auto stopRequested = [this]() {
    std::lock_guard<std::mutex> lock( _Mutex );
    return _StopRequested;
  };

  // 1. Discover the Internet Gateway Device (target the active LAN IP first if multiple adapters exist)
  int discoverError = 0;
  UPNPDev * devList = nullptr;

  if ( !iLocalInterface.empty() && iLocalInterface != "127.0.0.1" )
  {
    devList = upnpDiscover( 2000, iLocalInterface.c_str(), nullptr, UPNP_LOCAL_PORT_ANY, 0, 2, &discoverError );
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
    igdStatus = UPNP_GetValidIGD( devList, &urls, &data, lanAddr, sizeof( lanAddr ), wanAddr, sizeof( wanAddr ) );

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
    _Running = false;
    return;
  }

  status._State   = UpnpState::Mapping;
  status._LocalIP = lanAddr;
  status._StatusMessage = "Opening port on router...";
  SetStatus( status );

  // 2. Public IP
  char externalIp[64] = {};
  const int ipResult = UPNP_GetExternalIPAddress( urls.controlURL, data.first.servicetype, externalIp );
  const bool hasExternalIp = ( UPNPCOMMAND_SUCCESS == ipResult ) && ( externalIp[0] != '\0' );

  if ( igdStatus == UPNP_DISCONNECTED_IGD )
  {
    FreeUPNPUrls( &urls );
    status._State         = UpnpState::NotAvailable;
    status._StatusMessage = "Router reports no Internet connection";
    SetStatus( status );
    _Running = false;
    return;
  }

  const bool isDoubleNat = ( igdStatus == UPNP_PRIVATEIP_IGD ) ||
                           ( hasExternalIp && ( 0 != addr_is_reserved( externalIp ) ) );

  // 3. Port mapping (UDP). Some routers refuse a timed lease, so fall back to a permanent one.
  char extPortStr[16] = {};
  char intPortStr[16] = {};
  std::snprintf( extPortStr, sizeof( extPortStr ), "%u", static_cast<unsigned int>( iExternalPort ) );
  std::snprintf( intPortStr, sizeof( intPortStr ), "%u", static_cast<unsigned int>( iInternalPort ) );

  int mapResult = UPNP_AddPortMapping( urls.controlURL, data.first.servicetype, extPortStr, intPortStr,
                                       lanAddr, iDescription.c_str(), "UDP", nullptr, "7200" );
  if ( UPNPCOMMAND_SUCCESS != mapResult )
  {
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
    _Running = false;
    return;
  }

  std::string routableIp;
  if ( hasExternalIp && ( 0 == addr_is_reserved( externalIp ) ) )
  {
    routableIp = externalIp;
  }
  else
  {
    // If the local router reported a private WAN IP (Double NAT), resolve the public Internet IP
    routableIp = FetchPublicIPFallback();
  }

  status._State         = UpnpState::Success;
  status._ExternalIP    = !routableIp.empty() ? routableIp : ( hasExternalIp ? externalIp : "" );
  status._StatusMessage = isDoubleNat ? "Port opened via UPnP (Note: Double NAT detected with upstream router)"
                                      : "Port opened via UPnP";
  if ( !status._ExternalIP.empty() )
    status._RoomCode = RoomCode::Encode( status._ExternalIP, iExternalPort );
  SetStatus( status );

  // 4. Keep the mapping alive until the stop request, then remove it
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
