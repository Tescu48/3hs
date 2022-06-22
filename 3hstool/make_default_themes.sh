#!/bin/sh
dir="$(dirname "$0")"
echo $dir

"$dir/3hstool" maketheme "$dir/light.cfg" "$dir/../romfs/light.hstx"
"$dir/3hstool" maketheme "$dir/dark.cfg" "$dir/../romfs/dark.hstx"
