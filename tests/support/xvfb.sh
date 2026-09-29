# Shared Xvfb lifecycle for rawosd integration tests.

rawosd_test_xvfb_pid=
rawosd_test_xvfb_lock=

rawosd_test_xvfb_stop()
{
  if [ -n "$rawosd_test_xvfb_pid" ]; then
    kill "$rawosd_test_xvfb_pid" 2>/dev/null || true
    wait "$rawosd_test_xvfb_pid" 2>/dev/null || true
    rawosd_test_xvfb_pid=
  fi
  if [ -n "$rawosd_test_xvfb_lock" ]; then
    rmdir "$rawosd_test_xvfb_lock" 2>/dev/null || true
    rawosd_test_xvfb_lock=
  fi
}

rawosd_test_xvfb_start()
{
  state_dir=$1
  lock_root=${TMPDIR:-/tmp}/rawosd-xvfb-displays
  number=190

  mkdir -p "$lock_root" || return 1
  while [ "$number" -lt 240 ]; do
    lock=$lock_root/$number
    if mkdir "$lock" 2>/dev/null; then
      if [ -e "/tmp/.X${number}-lock" ] || [ -e "/tmp/.X11-unix/X${number}" ]; then
        rmdir "$lock" 2>/dev/null || true
        number=$((number + 1))
        continue
      fi
      display=:$number
      ${RAWOSD_TEST_XVFB:-Xvfb} "$display" -screen 0 800x600x24 -nolisten tcp \
        >"$state_dir/xvfb.log" 2>&1 &
      pid=$!
      tries=0
      while [ "$tries" -lt 100 ]; do
        if DISPLAY=$display ${RAWOSD_TEST_XWININFO:-xwininfo} -root >/dev/null 2>&1; then
          rawosd_test_xvfb_pid=$pid
          rawosd_test_xvfb_lock=$lock
          DISPLAY=$display
          export DISPLAY
          return 0
        fi
        if ! kill -0 "$pid" 2>/dev/null; then
          break
        fi
        tries=$((tries + 1))
        sleep 0.02
      done
      kill "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
      rmdir "$lock" 2>/dev/null || true
      cat "$state_dir/xvfb.log" >&2 || true
      return 1
    fi
    number=$((number + 1))
  done
  return 1
}
