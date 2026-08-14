#!/usr/bin/env bash

set -euo pipefail

project_root="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
files=(
  "Source/src/JobSystem.h"
  "Source/src/RasterData.h"
  "Source/src/SoftwareRasterizer.h"
  "Source/src/SoftwareRasterizer.cpp"
)

for file in "${files[@]}"; do
  path="${project_root}/${file}"
  perl -0pi -e 's/unsigned long long/std::uint64_t/g' "${path}"
done

for file in "Source/src/JobSystem.h" "Source/src/RasterData.h" "Source/src/SoftwareRasterizer.h"; do
  path="${project_root}/${file}"
  if ! rg -q '^#include <cstdint>$' "${path}"; then
    perl -0pi -e 's/(#include [^\n]+\n)/$1#include <cstdint>\n/' "${path}"
  fi
done

echo "Replaced unsigned long long with std::uint64_t in ${#files[@]} project files."
