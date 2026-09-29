#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 CONFIG-PROBE" >&2
  exit 2
fi
probe=$1
tmp=${TMPDIR:-/tmp}/rawosd-config.$$
mkdir -p "$tmp"
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

expect_fail()
{
  name=$1
  shift
  file=$tmp/$name.conf
  cat >"$file"
  if "$probe" "$file" >/dev/null 2>&1; then
    echo "config test failed: $name unexpectedly accepted" >&2
    exit 1
  fi
}

expect_fail unknown-root <<'EOC'
unknown = true;
EOC

expect_fail invalid-position <<'EOC'
position = "middle";
EOC

expect_fail duration-low <<'EOC'
duration_ms = 99;
EOC

expect_fail duration-high <<'EOC'
duration_ms = 5001;
EOC

expect_fail unknown-color <<'EOC'
colors = { normal = { shadow = "#000000"; }; };
EOC

expect_fail bad-color-type <<'EOC'
colors = { normal = { foreground = 42; }; };
EOC

expect_fail long-layout-symbol <<'EOC'
layouts = { tile = "123456789012345678901234567890123"; };
EOC

expect_fail bad-geometry-type <<'EOC'
geometry = "yes";
EOC

cat >"$tmp/valid.conf" <<'EOC'
font = "monospace:size=10";
position = "top";
geometry = true;
duration_ms = 1200;
padding = 8;
offset = 12;
min_width = 160;
colors = {
  normal = { foreground = "#ffffff"; background = "#111111"; accent = "#3366ff"; };
  floating = { foreground = "#ffffff"; background = "#222222"; accent = "#ffcc00"; };
};
layouts = { tile = "T"; grid = "G"; };
EOC
"$probe" "$tmp/valid.conf" >/dev/null

printf '%s\n' 'config tests: ok'
