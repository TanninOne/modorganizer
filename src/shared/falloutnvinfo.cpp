/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "falloutnvinfo.h"
#include "util.h"
#include <tchar.h>
#include <ShlObj.h>
#include <sstream>
#include "windows_error.h"
#include "error_report.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <boost/assign.hpp>

namespace MOShared {


FalloutNVInfo::FalloutNVInfo(const std::wstring &gameDirectory)
  : GameInfo(gameDirectory)
{
  identifyMyGamesDirectory(L"falloutnv");
}

bool FalloutNVInfo::identifyGame(const std::wstring &searchPath)
{
  return FileExists(searchPath, L"FalloutNV.exe") &&
         FileExists(searchPath, L"FalloutNVLauncher.exe");
}

std::wstring FalloutNVInfo::getRegPathStatic()
{
  HKEY key;
  LONG errorcode = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Bethesda Softworks\\FalloutNV",
                                  0, KEY_QUERY_VALUE, &key);

  if (errorcode != ERROR_SUCCESS) {
    return std::wstring();
  }

  WCHAR temp[MAX_PATH];
  DWORD bufferSize = MAX_PATH;

  if (::RegQueryValueExW(key, L"Installed Path", nullptr, nullptr, (LPBYTE)temp, &bufferSize) == ERROR_SUCCESS) {
    return std::wstring(temp);
  } else {
    return std::wstring();
  }
}

std::vector<std::wstring> FalloutNVInfo::getIniFileNames() const
{
  return boost::assign::list_of(L"fallout.ini")(L"falloutprefs.ini");
}

std::wstring FalloutNVInfo::getReferenceDataFile() const
{
  return L"Fallout - Meshes.bsa";
}

bool FalloutNVInfo::rerouteToProfile(const wchar_t *fileName, const wchar_t*) const
{
  static LPCWSTR profileFiles[] = { L"fallout.ini", L"falloutprefs.ini", L"plugins.txt", nullptr };

  for (int i = 0; profileFiles[i] != nullptr; ++i) {
    if (_wcsicmp(fileName, profileFiles[i]) == 0) {
      return true;
    }
  }
  return false;
}

} // namespace MOShared
