# Product

<!-- impeccable:product-schema 1 -->

## Platform

adaptive

## Users

Adriel, operating a dual-machine desk (Windows PC + MacBook Air) with one keyboard and mouse. Daily job: move the cursor between machines as if they were one display, share clipboard, and keep shortcuts (Ctrl/Cmd) feeling native.

## Product Purpose

InputLeap shares keyboard, mouse, and clipboard across computers on the local network without a hardware KVM. Success is a session that starts from one window, stays connected, and lets the user place each machine on the correct screen edge.

## Positioning

A locally installed Qt control surface that owns the InputLeap server/client on Windows, including a Windows service so Start / Stop / Apply actually control the running core. Custom build adds PNG clipboard, Ctrl↔Cmd translation, ABNT2, and low-latency flushing.

## Operating Context

Desktop Qt 6 app on Windows 11. Portuguese (pt-BR) UI. Server mode is the daily path: this PC is the keyboard host, Mac is the client. Screen layout is configured either interactively (grid) or via `inputleap.conf`. Night/desk ambient light; the window sits beside other work apps.

## Capabilities and Constraints

- Must keep existing Qt object names (`m_pGroupServer`, `m_pButtonToggleStart`, etc.) so C++ slots and translations keep working.
- Shared folder: drop or send a file in `Documents/InputLeap Share` (or a chosen folder) and it is copied to the same folder on the other machine. Both computers must run this GUI build. Uses TCP on InputLeap port + 1 (24801).
- Server and Client are mutually exclusive checkable groups.
- Service/daemon process mode on Windows.
- TLS fingerprint display and trust flow must remain reachable.
- No invented product claims, prices, or benchmarks.

## Brand Commitments

Name: InputLeap. User asked to remove the “ugly default Qt dialog” look and raise the visual craft of this control window.

## Evidence on Hand

Runnable GUI: `src/gui`. Icons in `src/gui/res/icons`. Installed app at `C:\Program Files\InputLeap`. Live session with Mac `MacBook-Air-de-Eli-7.local`.

## Product Principles

- The live connection state must be obvious in one glance.
- Role (server vs client) is a choice, not a stack of disabled group boxes.
- Screen-edge layout is a first-class task, not a hidden radio option.
- Every control that exists must look and feel operable.
- Preserve working behavior while replacing the default Qt skin.

## Accessibility & Inclusion

Portuguese UI. Keyboard shortcuts already exist (Ctrl+S start, Ctrl+T stop). Contrast must stay readable on the new surface. Assumed: no additional a11y standard was specified.
