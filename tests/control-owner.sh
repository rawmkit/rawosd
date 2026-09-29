#!/bin/sh
set -eu

case $0 in
  */*) test_dir=${0%/*} ;;
  *) test_dir=. ;;
esac
. "$test_dir/support/xvfb.sh"

if [ "$#" -ne 1 ]; then
  echo "usage: $0 CONTROL-OWNER-TEST" >&2
  exit 2
fi

tmp=${TMPDIR:-/tmp}/rawosd-control-owner.$$
mkdir -p "$tmp"
cleanup()
{
  rawosd_test_xvfb_stop
  rm -rf "$tmp"
}
trap cleanup EXIT HUP INT TERM

if ! rawosd_test_xvfb_start "$tmp"; then
  echo "skipped: unusable Xvfb fixture" >&2
  exit 77
fi

exec "$1"
