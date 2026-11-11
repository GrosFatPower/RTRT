#ifndef _UpnpService_
#define _UpnpService_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace RTRT
{

enum class UpnpState : int
{
  Idle         = 0,
  Discovering  = 1, // Looking for the router (SSDP)
  Mapping      = 2, // Router found, requesting the port mapping
  Success      = 3, // Port is open and public IP is known
  NotAvailable = 4, // No UPnP router found / router behind another NAT
  Failed       = 5  // Router refused the mapping
};

struct UpnpStatusInfo
{
  UpnpState   _State         = UpnpState::Idle;
  std::string _ExternalIP    = "";
  std::string _LocalIP       = "";
  uint16_t    _ExternalPort  = 0;
  uint16_t    _InternalPort  = 0;
  std::string _StatusMessage = "";
  std::string _RoomCode      = "";
};

// Opens a UDP port on the home router through UPnP IGD (miniupnpc) in a background thread.
// The calling thread is never blocked by network discovery.
class UpnpService
{
public:
  UpnpService();
  ~UpnpService();

  // (Re)starts the worker: discover router -> map port -> wait -> unmap on stop
  void StartMappingAsync( uint16_t iInternalPort,
                          uint16_t iExternalPort,
                          const std::string & iDescription = "RTRT Game",
                          const std::string & iLocalInterface = "",
                          bool iPerformPortMapping = true );

  // Requests the removal of the mapping. Returns immediately; the worker is joined on the next start or on destruction.
  void StopMappingAsync();

  UpnpStatusInfo GetStatus() const;
  bool IsBusy() const;

  // Resolves the public, globally routable WAN IPv4 address via STUN (and HTTP fallback).
  static std::string ResolvePublicIP( unsigned int iTimeoutMs = 1500 );

private:
  void WorkerMain( uint16_t iInternalPort, uint16_t iExternalPort, std::string iDescription, std::string iLocalInterface, bool iPerformPortMapping );
  void RunMapping( uint16_t iInternalPort, uint16_t iExternalPort, const std::string & iDescription, const std::string & iLocalInterface, bool iPerformPortMapping );
  void SetStatus( const UpnpStatusInfo & iStatus );
  void JoinWorker();

private:
  mutable std::mutex      _Mutex;
  std::condition_variable _StopCondition;
  std::thread             _Worker;
  bool                    _StopRequested = false;
  std::atomic<bool>       _Running{ false };
  UpnpStatusInfo          _Status;
};

} // namespace RTRT

#endif /* _UpnpService_ */
