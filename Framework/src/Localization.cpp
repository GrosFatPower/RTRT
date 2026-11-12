#include "Localization.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace RTRT
{

// ----------------------------------------------------------------------------
// LocalizationManager
// ----------------------------------------------------------------------------
LocalizationManager & LocalizationManager::Instance()
{
  static LocalizationManager s_Instance;
  return s_Instance;
}

LocalizationManager::LocalizationManager()
{
  _AvailableLanguages =
  {
    { Language::English, "en", "English" },
    { Language::French,  "fr", "Français" }
  };

  Initialize();
}

void LocalizationManager::Initialize()
{
  if ( _Initialized )
  {
    return;
  }

  const size_t totalCount = static_cast<size_t>(LocId::Count);

  for ( size_t i = 0; i < static_cast<size_t>(Language::Count); ++i )
  {
    _StringTables[i].assign(totalCount, nullptr);
  }

  // 1. Populate English strings & build key-to-id map
  size_t countEN = 0;
  const LocStringEntry * entriesEN = GetEnglishEntries(countEN);
  for ( size_t i = 0; i < countEN; ++i )
  {
    const LocStringEntry & entry = entriesEN[i];
    size_t idIdx = static_cast<size_t>(entry._Id);
    if ( idIdx < totalCount )
    {
      _StringTables[static_cast<size_t>(Language::English)][idIdx] = entry._Text;
    }
    if ( entry._Key && entry._Key[0] != '\0' )
    {
      _KeyToId[entry._Key] = entry._Id;
    }
  }

  // 2. Populate French strings
  size_t countFR = 0;
  const LocStringEntry * entriesFR = GetFrenchEntries(countFR);
  for ( size_t i = 0; i < countFR; ++i )
  {
    const LocStringEntry & entry = entriesFR[i];
    size_t idIdx = static_cast<size_t>(entry._Id);
    if ( idIdx < totalCount )
    {
      _StringTables[static_cast<size_t>(Language::French)][idIdx] = entry._Text;
    }
  }

  _Initialized = true;
}

void LocalizationManager::SetLanguage( Language iLang )
{
  if ( iLang >= Language::English && iLang < Language::Count )
  {
    _CurrentLanguage = iLang;
  }
}

Language LocalizationManager::GetLanguage() const
{
  return _CurrentLanguage;
}

const LanguageInfo & LocalizationManager::GetLanguageInfo( Language iLang ) const
{
  size_t idx = static_cast<size_t>(iLang);
  if ( idx < _AvailableLanguages.size() )
  {
    return _AvailableLanguages[idx];
  }
  return _AvailableLanguages[0];
}

const std::vector<LanguageInfo> & LocalizationManager::GetAvailableLanguages() const
{
  return _AvailableLanguages;
}

const char * LocalizationManager::Get( LocId iId ) const
{
  size_t idIdx = static_cast<size_t>(iId);
  if ( idIdx >= static_cast<size_t>(LocId::Count) )
  {
    return "";
  }

  size_t langIdx = static_cast<size_t>(_CurrentLanguage);
  if ( langIdx < static_cast<size_t>(Language::Count) )
  {
    const char * str = _StringTables[langIdx][idIdx];
    if ( str && str[0] != '\0' )
    {
      return str;
    }
  }

  // Fallback to English
  const char * fb = _StringTables[static_cast<size_t>(Language::English)][idIdx];
  return (fb && fb[0] != '\0') ? fb : "";
}

const char * LocalizationManager::Get( const std::string & iKey, const char * iDefaultVal ) const
{
  auto itId = _KeyToId.find(iKey);
  if ( itId != _KeyToId.end() )
  {
    return Get(itId->second);
  }

  auto itCustom = _CustomStrings.find(iKey);
  if ( itCustom != _CustomStrings.end() )
  {
    return itCustom->second.c_str();
  }

  return (iDefaultVal != nullptr) ? iDefaultVal : iKey.c_str();
}

bool LocalizationManager::LoadLocaleFile( const std::string & iFilePath )
{
  std::ifstream file(iFilePath);
  if ( !file.is_open() )
  {
    return false;
  }

  try
  {
    nlohmann::json root = nlohmann::json::parse(file);
    if ( !root.is_object() )
    {
      return false;
    }

    for ( auto & item : root.items() )
    {
      if ( item.value().is_string() )
      {
        std::string key = item.key();
        std::string val = item.value().get<std::string>();

        _CustomStrings[key] = val;

        auto itId = _KeyToId.find(key);
        if ( itId != _KeyToId.end() )
        {
          size_t idIdx = static_cast<size_t>(itId->second);
          if ( idIdx < static_cast<size_t>(LocId::Count) )
          {
            // Store permanent string in _CustomStrings and reference pointer
            _StringTables[static_cast<size_t>(_CurrentLanguage)][idIdx] = _CustomStrings[key].c_str();
          }
        }
      }
    }
    return true;
  }
  catch ( const std::exception & e )
  {
    std::cerr << "[Localization] Failed to parse JSON locale: " << e.what() << std::endl;
    return false;
  }
}

bool LocalizationManager::IsComplete( Language iLang ) const
{
  size_t langIdx = static_cast<size_t>(iLang);
  if ( langIdx >= static_cast<size_t>(Language::Count) )
  {
    return false;
  }

  const size_t totalCount = static_cast<size_t>(LocId::Count);
  for ( size_t i = 0; i < totalCount; ++i )
  {
    const char * s = _StringTables[langIdx][i];
    if ( !s || s[0] == '\0' )
    {
      return false;
    }
  }

  return true;
}

} // namespace RTRT
