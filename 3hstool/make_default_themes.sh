#!/bin/sh
dir="$(dirname "$0")"
echo $dir

"$dir/3hstool" maketheme "$dir/light.cfg" "$dir/../data/light.hstx"
"$dir/3hstool" maketheme "$dir/dark.cfg" "$dir/../data/dark.hstx"
