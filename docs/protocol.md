# rawosd protocol consumption

rawosd is a consumer of rawm's public control membrane. It does not define or
own rawm protocol versions.

## Discovery

On X screen `N`, rawosd resolves `_RAWM_CONTROL_SN` and calls
`XGetSelectionOwner()`. The returned InputOnly window is the current rawm
authority. rawosd validates `_RAWM_CONTROL_VERSION` as CARDINAL/32 version 1,
selects property and structure notifications on that owner, and never retains
the XID after owner destruction or replacement.

## Feedback

`_RAWM_FEEDBACK` must contain one UTF-8 record:

```
rawm-feedback-v1<TAB>SERIAL<TAB>EVENT<TAB>MONITOR<TAB>ARG0<TAB>ARG1
```

rawosd accepts the version-1 event names published by rawm: `monitor`,
`master-count`, `master-factor`, `move`, `resize`, `view`, `layout`, `floating`,
`fullscreen`, `tag`, `toggle-tag`, and `tag-monitor`.

The serial is used only to reject stale/repeated property observations. Gaps are
allowed. The latest value is baselined when a control owner is attached and is
not displayed.

## State

For every new feedback event, rawosd rereads `_RAWM_STATE`. It accepts only the
version-1 snapshot emitted by rawm:

```
rawm-state-v2
tag-count<TAB>N
selected-monitor<TAB>N
```

Each monitor then has exactly one `monitor` record, one `stack` record, and one
`tag` record for every configured tag. rawosd validates monitor geometry, tag
masks, selected-monitor consistency, canonical layout names, stack counts, and
field counts. Unknown record kinds are rejected.

The consumed records are:

```
monitor<TAB>NUMBER<TAB>SELECTED<TAB>MX<TAB>MY<TAB>MW<TAB>MH<TAB>VIEWED<TAB>OCCUPIED<TAB>URGENT<TAB>LAYOUT<TAB>FOCUSED<TAB>NMASTER<TAB>MFACT
stack<TAB>MONITOR<TAB>FOCUSED-INDEX<TAB>VISIBLE-COUNT
tag<TAB>MONITOR<TAB>ONE-BASED-INDEX<TAB>NAME
```

Tag text decodes `%HH` escapes as defined by rawm. rawosd uses full monitor
and work-area geometry from `monitor`; presentation is placed inside rawm's
authoritative work area and never reconstructs dock/strut reservations.

## Failure model

Feedback is presentation input and may be lost. Loss does not compromise rawm
truth. rawosd never uses event history to rebuild tags, layouts, geometry, or
focus. If state cannot be parsed for a feedback event, that event is not
rendered.

An unsupported control owner or malformed record is rejected. Owner loss hides
any currently visible surface. A replacement owner starts a new process-local
feedback serial domain and is baselined independently.
