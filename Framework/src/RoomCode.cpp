#include "RoomCode.h"

#include <cstdio>
#include <cstdlib>

namespace RTRT
{

namespace
{
const char * kAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

const int kShortChars = 8;  // 36 bits -> 40 bits capacity
const int kLongChars  = 11; // 52 bits -> 55 bits capacity

int AlphabetIndex( char iChar )
{
  if ( iChar >= 'a' && iChar <= 'z' )
    iChar = static_cast<char>( iChar - 'a' + 'A' );

  if ( iChar == 'O' )
    iChar = '0';
  else if ( iChar == 'I' || iChar == 'L' )
    iChar = '1';

  for ( int i = 0; i < 32; ++i )
  {
    if ( kAlphabet[i] == iChar )
      return i;
  }
  return -1;
}

uint32_t ComputeChecksum( uint32_t iIP, uint32_t iPort, bool iHasPort )
{
  uint32_t sum = 0x5u;
  for ( int i = 0; i < 4; ++i )
    sum = ( sum * 7u + ( ( iIP >> ( 8 * i ) ) & 0xFFu ) ) & 0xFu;
  if ( iHasPort )
  {
    sum = ( sum * 7u + ( iPort & 0xFFu ) ) & 0xFu;
    sum = ( sum * 7u + ( ( iPort >> 8 ) & 0xFFu ) ) & 0xFu;
  }
  return sum;
}

// Keeps only alphabet characters, drops separators, returns false on any other character
bool Normalize( const std::string & iText, std::string & oChars )
{
  oChars.clear();
  for ( char ch : iText )
  {
    if ( ch == '-' || ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' )
      continue;
    if ( AlphabetIndex( ch ) < 0 )
      return false;
    oChars.push_back( kAlphabet[AlphabetIndex( ch )] );
  }
  return true;
}
} // namespace

// ----------------------------------------------------------------------------
// ParseIPv4 / FormatIPv4
// ----------------------------------------------------------------------------
bool RoomCode::ParseIPv4( const std::string & iIPv4, uint32_t & oValue )
{
  unsigned int a = 0, b = 0, c = 0, d = 0;
  char tail = 0;
  if ( std::sscanf( iIPv4.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail ) != 4 )
    return false;
  if ( a > 255 || b > 255 || c > 255 || d > 255 )
    return false;

  oValue = ( a << 24 ) | ( b << 16 ) | ( c << 8 ) | d;
  return true;
}

std::string RoomCode::FormatIPv4( uint32_t iValue )
{
  char buf[32] = {};
  std::snprintf( buf, sizeof( buf ), "%u.%u.%u.%u",
                 ( iValue >> 24 ) & 0xFFu, ( iValue >> 16 ) & 0xFFu, ( iValue >> 8 ) & 0xFFu, iValue & 0xFFu );
  return std::string( buf );
}

// ----------------------------------------------------------------------------
// Encode
// ----------------------------------------------------------------------------
std::string RoomCode::Encode( const std::string & iIPv4, uint16_t iPort )
{
  uint32_t ip = 0;
  if ( !ParseIPv4( iIPv4, ip ) )
    return "";

  const bool hasPort = ( iPort != kDefaultPort );
  const uint64_t checksum = ComputeChecksum( ip, iPort, hasPort );

  uint64_t value = 0;
  int numChars   = 0;
  if ( hasPort )
  {
    value    = ( static_cast<uint64_t>( ip ) << 20 ) | ( static_cast<uint64_t>( iPort ) << 4 ) | checksum;
    numChars = kLongChars;
  }
  else
  {
    value    = ( static_cast<uint64_t>( ip ) << 4 ) | checksum;
    numChars = kShortChars;
  }

  std::string code( numChars, '0' );
  for ( int i = numChars - 1; i >= 0; --i )
  {
    code[i] = kAlphabet[value & 0x1Fu];
    value >>= 5;
  }

  // Cosmetic grouping: XXXX-XXXX or XXXX-XXXX-XXX
  std::string grouped;
  for ( int i = 0; i < numChars; ++i )
  {
    if ( i > 0 && ( i % 4 ) == 0 )
      grouped.push_back( '-' );
    grouped.push_back( code[i] );
  }
  return grouped;
}

// ----------------------------------------------------------------------------
// Decode
// ----------------------------------------------------------------------------
bool RoomCode::Decode( const std::string & iCode, std::string & oIPv4, uint16_t & oPort )
{
  std::string chars;
  if ( !Normalize( iCode, chars ) )
    return false;

  const int numChars = static_cast<int>( chars.size() );
  if ( numChars != kShortChars && numChars != kLongChars )
    return false;

  uint64_t value = 0;
  for ( char ch : chars )
    value = ( value << 5 ) | static_cast<uint64_t>( AlphabetIndex( ch ) );

  const bool hasPort = ( numChars == kLongChars );
  const int usedBits = hasPort ? 52 : 36;
  if ( ( value >> usedBits ) != 0 ) // padding bits must be zero
    return false;

  const uint32_t checksum = static_cast<uint32_t>( value & 0xFu );
  uint32_t ip   = 0;
  uint32_t port = kDefaultPort;
  if ( hasPort )
  {
    port = static_cast<uint32_t>( ( value >> 4 ) & 0xFFFFu );
    ip   = static_cast<uint32_t>( value >> 20 );
  }
  else
  {
    ip = static_cast<uint32_t>( value >> 4 );
  }

  if ( checksum != ComputeChecksum( ip, port, hasPort ) )
    return false;
  if ( hasPort && ( port == kDefaultPort || port == 0 ) )
    return false;

  oIPv4 = FormatIPv4( ip );
  oPort = static_cast<uint16_t>( port );
  return true;
}

// ----------------------------------------------------------------------------
// LooksLikeRoomCode
// ----------------------------------------------------------------------------
bool RoomCode::LooksLikeRoomCode( const std::string & iText )
{
  std::string chars;
  if ( !Normalize( iText, chars ) )
    return false;
  return ( chars.size() == kShortChars || chars.size() == kLongChars );
}

} // namespace RTRT
