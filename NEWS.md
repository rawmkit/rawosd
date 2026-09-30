# rawosd NEWS

## 0.1.0 - 2026-09-29

* Keep the transient OSD above normal client windows while it is visible, even if another client is raised after the feedback window maps.
* Consume `_RAWM_STATE` version 2 work-area geometry so OSD placement stays clear of panel/dock reservations; restore selected/highlight colors for ordinary feedback and floating colors for elevated/live geometry.
Initial standalone release extracted from rawm's transient presentation
responsibility.

* Consume rawm control protocol version 1 and `_RAWM_FEEDBACK` version 1.
* Reread `_RAWM_STATE` for authoritative monitor, tag, and layout data.
* Own the transient X11 feedback window, visibility timer, formatting, colors,
  Xft/Fontconfig font fallback, and layout symbols.
* Baseline feedback serials at attachment and resynchronize across rawm owner
  loss/replacement without replaying stale events.
* Fail closed on incompatible protocol versions and malformed records.
* Support layered system/user configuration and SIGHUP reload.
* Cover protocol parsing, formatting, configuration, selection-owner lifecycle,
  and renderer integration with isolated Xvfb fixtures.
