#!/bin/sh
set -eu

case $0 in
  */*) test_dir=${0%/*} ;;
  *) test_dir=. ;;
esac
. "$test_dir/support/xvfb.sh"

if [ "$#" -ne 4 ]; then
  echo "usage: $0 RAWOSD FAKE-RAWM WINDOW-PROBE CONFIG" >&2
  exit 2
fi
rawosd=$1
fake=$2
probe=$3
config=$4
tmp=${TMPDIR:-/tmp}/rawosd-renderer.$$
mkdir -p "$tmp"
rawosd_pid=
fake_pid=
cleanup()
{
  if [ -n "$fake_pid" ]; then
    kill "$fake_pid" 2>/dev/null || true
    wait "$fake_pid" 2>/dev/null || true
  fi
  if [ -n "$rawosd_pid" ]; then
    kill "$rawosd_pid" 2>/dev/null || true
    wait "$rawosd_pid" 2>/dev/null || true
  fi
  rawosd_test_xvfb_stop
  rm -rf "$tmp"
}
trap cleanup EXIT HUP INT TERM

if ! rawosd_test_xvfb_start "$tmp"; then
  echo "skipped: unusable Xvfb fixture" >&2
  exit 77
fi

start_fake()
{
  ready=$1
  trigger=$2
  rm -f "$ready" "$trigger"
  "$fake" "$ready" "$trigger" >"$tmp/fake.log" 2>&1 &
  fake_pid=$!
  tries=0
  while [ ! -e "$ready" ]; do
    if ! kill -0 "$fake_pid" 2>/dev/null; then
      cat "$tmp/fake.log" >&2 || true
      exit 1
    fi
    tries=$((tries + 1))
    if [ "$tries" -gt 100 ]; then
      echo "renderer test failed: fake owner did not become ready" >&2
      exit 1
    fi
    sleep 0.02
  done
}

start_fake "$tmp/ready1" "$tmp/trigger1"
"$rawosd" -c "$config" >"$tmp/rawosd.log" 2>&1 &
rawosd_pid=$!

if ! "$probe" hidden 1500; then
  echo "renderer test failed: rawosd window did not remain hidden at baseline" >&2
  cat "$tmp/rawosd.log" >&2 || true
  exit 1
fi

touch "$tmp/trigger1"
if ! "$probe" viewable 1500; then
  echo "renderer test failed: feedback did not map rawosd window" >&2
  cat "$tmp/rawosd.log" >&2 || true
  exit 1
fi
if ! "$probe" cover-above 1500; then
  echo "renderer test failed: rawosd fought a later external surface" >&2
  cat "$tmp/rawosd.log" >&2 || true
  exit 1
fi
wait "$fake_pid"
fake_pid=
if ! "$probe" hidden 1500; then
  echo "renderer test failed: owner loss did not hide rawosd window" >&2
  exit 1
fi

start_fake "$tmp/ready2" "$tmp/trigger2"
sleep 0.7
if ! "$probe" hidden 500; then
  echo "renderer test failed: replacement baseline was replayed" >&2
  exit 1
fi
touch "$tmp/trigger2"
if ! "$probe" viewable 1500; then
  echo "renderer test failed: replacement owner feedback not rendered" >&2
  cat "$tmp/rawosd.log" >&2 || true
  exit 1
fi
wait "$fake_pid"
fake_pid=

kill "$rawosd_pid"
wait "$rawosd_pid" || true
rawosd_pid=
printf '%s\n' 'renderer integration tests: ok'
