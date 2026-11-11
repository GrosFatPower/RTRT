#ifndef _RoomCode_
#define _RoomCode_

#include <cstdint>
#include <string>

namespace RTRT
{

// Converts an (IPv4, Port) pair to a short, typo-protected, human friendly code and back.
//
// Alphabet : Crockford Base32 (no I, L, O, U) - case insensitive, O->0 and I/L->1 are tolerated on decode.
// Layout   : [ IPv4 (32 bits) | Port (16 bits, only if != kDefaultPort) | Checksum (4 bits) ]
// Length   : 8 chars ( XXXX-XXXX )     for the default port
//            11 chars ( XXXX-XXXX-XXX ) for any other port
class RoomCode
{
public:
  static const uint16_t kDefaultPort = 27015;

  static std::string Encode( const std::string & iIPv4, uint16_t iPort );

  // Returns false if the text is not a valid room code (wrong length / alphabet / checksum)
  static bool Decode( const std::string & iCode, std::string & oIPv4, uint16_t & oPort );

  // Cheap syntactic test: could this text be a room code (and not an IP or a host name) ?
  static bool LooksLikeRoomCode( const std::string & iText );

  static bool ParseIPv4( const std::string & iIPv4, uint32_t & oValue );
  static std::string FormatIPv4( uint32_t iValue );

  // Identifies RFC 1918 / 5735 / 6598 private, loopback, multicast, or link-local IPs
  static bool IsPrivateOrReservedIP( const std::string & iIPv4 );
  static bool IsPublicInternetIP( const std::string & iIPv4 );
};

} // namespace RTRT

#endif /* _RoomCode_ */
