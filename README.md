# rawosd

`rawosd` is the transient feedback renderer for the rawmkit desktop.

It deliberately owns presentation rather than window-management state. `rawm`
publishes semantic feedback through its public X11 control membrane; `rawosd`
subscribes to those events, rereads authoritative rawm state when needed, formats
the event, and presents it in a short-lived override-redirect window.

## Authority boundary

`rawosd` owns:

* transient feedback-window lifecycle;
* feedback text formatting;
* Xft/Fontconfig font selection and glyph fallback;
* foreground, background, and accent colors;
* layout display symbols;
* presentation duration, padding, position, offset, and minimum width;
* whether live move/resize geometry is shown.

`rawosd` does not own or reconstruct:

* admitted windows;
* focus or stacking;
* tags or views;
* layouts or geometry policy;
* monitor authority;
* keyboard bindings;
* persistent panel/status/systray presentation.

Those remain with their producer components. In particular, `_RAWM_FEEDBACK` is
advisory. Whenever presentation needs current tag names, layout, or monitor
geometry, `rawosd` rereads `_RAWM_STATE` instead of treating the event stream as
truth.

## Protocol

The daemon discovers rawm through `_RAWM_CONTROL_Sn` on its X screen. It requires
control protocol version 1, consumes `_RAWM_FEEDBACK` version 1, and parses
`_RAWM_STATE` version 1 fail-closed. Unknown versions, event kinds, state-record
kinds, and malformed field counts are rejected.

Repeated identical feedback is observable because rawm includes a process-local
monotonic serial. `rawosd` baselines the current serial when it attaches, so an
old event is not replayed at startup. It accepts serial gaps, since the feedback
channel is explicitly lossy, and resets the baseline when the rawm selection
owner changes.

If rawm disappears, rawosd hides its presentation immediately and waits for a
new control owner. A replacement owner is validated and baselined before new
events are rendered.

See `docs/protocol.md` for the consumed subset.

## Configuration

The system configuration is `/etc/rawosd/rawosd.conf`. User configuration is
loaded afterward from `$XDG_CONFIG_HOME/rawosd/rawosd.conf`, or
`~/.config/rawosd/rawosd.conf` when `XDG_CONFIG_HOME` is unset. Missing default
files are allowed. `rawosd -c PATH` loads only the explicit file and requires it
to exist.

Running the process is the enable/disable authority. There is intentionally no
`show = true` compatibility switch.

The stock policy preserves the presentation previously embedded in rawm:
monospace 9pt text, bottom placement, 800 ms visibility, 10 px padding, 24 px
offset, and the historical layout symbols. Live move/resize geometry is disabled
by default.

`SIGHUP` reloads the configuration. A failed reload leaves the previous valid
configuration active.

### Monitor placement

rawm state protocol v1 publishes full monitor rectangles. Therefore rawosd v0.1
positions relative to that authoritative rectangle. It does not inspect docks,
struts, the X window tree, or EWMH state to synthesize a work area. If panel-aware
placement becomes required, rawm should publish that authority explicitly in a
future protocol version.

## Build

Dependencies:

* a C99 compiler;
* Meson >= 1.6.0;
* Xlib;
* Xft;
* Fontconfig;
* libconfig;
* scdoc for manual pages.

Build and test with:

```sh
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

The X11 integration tests use Xvfb when available. An unusable Xvfb fixture is
reported as a skipped capability rather than changing production behavior.

Install with:

```sh
meson install -C build
```

Validate configuration without opening X11 with `rawosd -C` or
`rawosd -C -c /path/to/rawosd.conf`.

## Session integration

Run one `rawosd` instance in the graphical session, under the same session
supervisor as `rawm`, `rawpanel`, and `rawkeys`. It can start before or after
rawm; discovery and restart handling are part of the control-owner protocol.

## License

MIT/X. See `LICENSE`.
