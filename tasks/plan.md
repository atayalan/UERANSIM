# Implementation Plan: G1-4379 — Xn/N2 handover + Mobility Registration Update in UERANSIM with Ethernet PDU sessions

Jira: https://atayalan.atlassian.net/browse/G1-4379 · PRD: `.claude/prds/handover-with-ethernet-pdu.prd.md`
Branch: `G1-4379` (this repo, based on master incl. Ethernet/TAP PR #4). Source of the port: `/Users/chihshinlin/code/ueransim_daren` (v1.0, branch `main`).

## Overview
Bring daren's gNB-side Xn handover (XnAP via srsRAN codec), N2 handover, Path Switch, Xn-U forwarding, UE-side RRC handover and optional `--mobi_update` into this repo, then make every handover path PDU-session-type aware so Ethernet (TAP) sessions survive Xn/N2 handover and Mobility Registration Update. The TAP lives in the UE process (`UeAppTask::m_tapTasks`) and daren's UE handover code never touches TUN/TAP, so MAC stability should come for free; the real risk is IPv4 assumptions inside gNB handover/forwarding code.

## Findings that shape the plan
- **Common ancestor is upstream `48554b7` (v3.3.0-37) for both repos.** Daren's diff vs base is ~65 changed source files (+ `third_party/srsran_xnap`, ~190k generated lines); our diff vs base is Ethernet/TAP + CI + tests.
- **Only 12 files are touched by both** (>2 lines each): `CMakeLists.txt`, `Dockerfile`, `makefile`, `src/gnb/gtp/task.cpp`, `src/lib/app/cli_cmd.{cpp,hpp}`, `src/lib/rls/rls_pdu.hpp`, `src/lib/udp/server.cpp`, `src/ue/rls/udp_task.{cpp,hpp}`, `src/utils/libc_error.cpp`, `src/test/main.cpp`. The hard ones: `gnb/gtp/task.cpp`, `ue/rls/udp_task.cpp`, `lib/udp/server.cpp` (our security hardening + IPv6-optional vs daren's per-UE search window / exception handling).
- **IPv4 assumptions to remove for Ethernet** (in daren): `src/gnb/xnap/codec.cpp:143` hardcodes `pdu_session_type = ipv4` in the Xn session list; `src/gnb/gtp/task.cpp:~337` drops non-IPv4 uplink (our branch already fixed the main path in `handleUplinkData` by session type — re-check every other data path incl. Xn-U forwarding); verify N2 path in `ngap/handover.cpp:414` and `path_switch.cpp` carry `sessionType` end to end.
- **Test frameworks differ:** ours = doctest under `test/` + ctest discovery; daren = `src/test/*.cpp` with its own main. Daren's 3 tests (`xnap_codec`, `gtp_session_tree`, `cli_options`) must be ported to doctest.
- Daren requires libsctp (CMake hard fail) and `filter_appledouble` (macOS `._*` files); local macOS cannot run/validate — build and e2e on Linux lab VMs only.

## Architecture Decisions
1. **Import by squashed patch, not `git merge`.** `git diff 48554b7 HEAD` in daren (excluding manuals/docs) applied with 3-way onto this branch; avoids importing 1193 commits of Chinese-language history. Conflicts resolved in 12 known files. (Alternative: real merge — better provenance, noisier history. See open question 1.)
2. **Keep our test layout (doctest/ctest), our CI (`atayalan.ci`), our Helm chart.** Drop daren's `src/test/main.cpp`, port its tests.
3. **Session type is the single source of truth** (`PduSessionResource::sessionType` in gNB). Every handover codec/forwarder reads it; no literal IPv4 anywhere in mobility code.
4. **Vertical, milestone-ordered delivery** matching the Jira milestones; Ethernet baseline (UE↔UE, UE↔GW, DHCP) is a regression gate at every checkpoint.
5. **Do not touch UE TAP lifecycle on handover/MRU** — assert it with a lab MAC comparison instead of new code unless e2e proves otherwise.

## Task List

### Phase 1 — Milestone 1: IPv4 mobility parity (scenarios 1, 2, 5)
- [ ] T1: Import `third_party/srsran_xnap` + CMake/SCTP/`filter_appledouble` plumbing; builds with an empty Xn stub
- [ ] T2: Apply daren non-overlapping source files (gnb xnap/ngap handover/path_switch/gtp snapshot/rrc/sctp/types/nts, UE rrc/nas, lib/sctp)
- [ ] T3: Resolve the 12 overlapping files, keeping Ethernet behaviour + our hardening
- [ ] T4: Port daren's 3 unit tests to doctest under `test/`
- [ ] T5: Configs (`xn-gnb1/2.yaml`, ue `--mobi_update`), CLI help, Docker/chart check
### Checkpoint 1: Linux build clean, all unit tests pass (existing 50 + ported), CI green
- [ ] T6: Lab e2e IPv4 scenarios 1, 2, 5 (same VM) + Ethernet baseline regression

### Phase 2 — Milestone 2: Ethernet handover (scenarios 3, 4)
- [ ] T7: TDD: session-type-aware Xn codec + forwarding paths (unit tests first)
- [ ] T8: TDD: N2 handover (HandoverRequired/Request/Command/Notify) + Path Switch carry Ethernet session type
- [ ] T9: `tools/` helper to record/compare TAP MAC and run the three pings
- [ ] T10: Lab e2e scenario 3 (Xn) and 4 (N2), same VM; DHCP lease survives
### Checkpoint 2: scenarios 1–4 pass, TAP MAC identical, Ethernet baseline passes
- [x] T11: Cross-VM run of scenarios 1–4 (second gNB on separate VM); answer the N3/Xn open question

### Phase 3 — Milestone 3: Ethernet Mobility Registration Update (scenario 6)
- [ ] T12: Verify/fix PDU-session status handling on Mobility Registration Update with an Ethernet session (no TAP teardown)
- [x] T13: Lab e2e scenario 6 + scenario 5, same and cross VM
### Checkpoint 3: all 6 scenarios pass in both topologies

### Phase 4 — Wrap-up
- [ ] T14: Docs (CHANGELOG, user guide / Confluence 1988001794, THIRD_PARTY_NOTICES for srsRAN AGPL), PR, Jira comment

## Tasks (detail)

### T1: Import srsran_xnap and build plumbing
**Acceptance:** `third_party/srsran_xnap` (incl. LICENSE/ORIGIN.md) added; root CMake links `srsran-xnap`, finds libsctp, has `filter_appledouble`; project still builds.
**Verification:** `cmake --build` on vm10211 (use `~/ueransim-ether-tools` cmake + sctp-root per memory); `ctest` 50/50.
**Files:** `third_party/srsran_xnap/**`, `CMakeLists.txt`, `src/lib/CMakeLists.txt`, `src/gnb/CMakeLists.txt`, `src/ext/CMakeLists.txt`. **Scope:** M (mostly vendored). **Deps:** none.

### T2: Apply non-overlapping daren sources
**Acceptance:** all files unique to daren (`gnb/xnap/*`, `gnb/ngap/{handover,path_switch}.cpp`, `gnb/sctp/*` changes, UE `rrc/{connection,idle}.cpp`, `nas/mm/*`, `lib/sctp/*`) present and compile.
**Verification:** build; `nr-gnb --help`, `nr-ue --help` show `--mobi_update`; `nr-cli` exposes `xn-handover`/`n2-handover`.
**Dependencies:** T1. **Scope:** L by file count but mechanical — split by gNB / UE / lib commits.

### T3: Resolve overlapping files
**Acceptance:** `gnb/gtp/task.cpp` has daren's session-tree/forwarding **and** type-aware (not IPv4-only) uplink filter; `ue/rls/udp_task.cpp` keeps gnbSearchList heartbeat check + IPv6-optional socket and daren's per-UE search window; `lib/udp/server.cpp`, `cli_cmd` (keep `ParsePduSessionTypeName` + add handover commands) merged.
**Verification:** build + existing doctests (`test/lib/app/cli_cmd.cpp`, `test/ue/rls/udp_task.cpp`, `test/lib/udp/server.cpp`) pass unchanged. **Dependencies:** T2. **Scope:** M, highest merge risk — do early.

### T4: Port unit tests
**Acceptance:** `test_xnap_codec`, `test_gtp_session_tree`, `test_cli_options` rewritten as doctest cases under `test/`; `src/test/` removed.
**Verification:** `ctest --output-on-failure` lists and passes each case. **Dependencies:** T3.

### T5: Configs, docs-in-repo, Docker/chart
**Acceptance:** `config/xn-gnb1.yaml`, `xn-gnb2.yaml` present with TAC/PLMN matching lab (999/99, TAC 100); Dockerfile and `atayalan.ci` still build (libsctp-dev, srsran-xnap compile time acceptable). **Verification:** CI build on branch. **Deps:** T3.

### T6: Lab e2e, IPv4 scenarios 1, 2, 5
**Acceptance:** after Xn HO, N2 HO and MRU, `ping` via `uesimtun0` still works; behaviour matches daren v1.0 (note known daren limitation: ~1 ping lost at N2 boundary).
**Verification:** two gNBs on vm10211 (second needs its own N2/N3 address — extend the segment plan from memory: ngap + gtp /32 per gNB); core logs captured per scenario. **Deps:** Checkpoint 1.

### T7: Session-type-aware Xn path (TDD)
**Acceptance:** `FillSimulatorPduSession` sets type from the real session (IPv4/Ethernet); Xn session snapshot serialization round-trips `Ethernet`; Xn-U forwarded Ethernet frames are not discarded by any IPv4 check.
**Verification:** failing doctests first (codec with Ethernet; GTP forwarding of a non-IP frame), then green. **Files:** `src/gnb/xnap/codec.cpp`, `src/gnb/xnap/task.cpp`, `src/gnb/gtp/task.cpp`, `test/gnb/...`. **Scope:** M. **Deps:** Checkpoint 1.

### T8: Session-type-aware N2 / Path Switch (TDD)
**Acceptance:** HandoverRequired builds the right PDU session list, target `HandoverRequest` parsing keeps Ethernet type (`handover.cpp:414` verified), `PathSwitchRequest`/Ack and `HandoverNotify` preserve it; no IPv4-only assumptions.
**Verification:** doctests on the encode/decode helpers; review of `ngap_utils::PduSessionTypeFromAsn`. **Files:** `src/gnb/ngap/{handover,path_switch,utils}.cpp`. **Scope:** M. **Deps:** T7 (parallelizable after Checkpoint 1; touches different files).

### T9: MAC/ping check helper
**Acceptance:** script prints TAP MAC before/after and runs UE↔UE, UE→GW, GW→UE pings, exit non-zero on any mismatch/failure. Reuses `tools/ether-ping.py`. **Scope:** S.

### T10: Lab e2e scenarios 3 and 4 (+ DHCP)
**Acceptance:** per PRD: three pings succeed, MAC identical, DHCP lease (dnsmasq on vm10213) still valid and renewable after HO. Core must be started ≥60 s before UEs (C1-2261 N4 flap, per memory). **Deps:** T7, T8, T9.

### T11: Cross-VM
**Acceptance:** scenarios 1–4 pass with source gNB and target gNB on different VMs; UE reaches both via `gnbSearchList`; document the exact N3/Xn/RLS reachability requirements and close the open question. **Needs:** a third VM or a second lab VM for the target gNB (open question 2).

### T12: Mobility Registration Update with Ethernet
**Acceptance:** after `--mobi_update`, the UE keeps its Ethernet PDU session (PDU session status IE honoured), TAP interface is not recreated (same name and MAC), no spurious session release. **Verification:** read `ue/nas/mm/register.cpp`/`sm` paths for session-status handling; failing test first where unit-testable. **Deps:** Checkpoint 2.

### T13: Lab e2e scenario 6 (+ 5 re-run), same and cross VM. **Deps:** T12, T11.

### T14: Wrap-up — CHANGELOG entry, update Confluence guide, THIRD_PARTY_NOTICES (srsRAN XnAP, AGPLv3), PR against `atayalan/UERANSIM`, Jira comment with results table and PR link; `ecc:code-reviewer` + security review on the diff before PR.

## Parallelization
- After Checkpoint 1: T7 (Xn) and T8 (N2) and T9 (helper) are independent. T12 can start in parallel with T10/T11.
- T1→T2→T3 are strictly sequential (shared build files).

## Risks and Mitigations
| Risk | Impact | Mitigation |
|---|---|---|
| Overlap files (esp. UE `udp_task`, gNB `gtp/task`) merge badly and regress security hardening or Ethernet | High | Do T3 first; existing doctests as gate; review diff of those 3 files by hand |
| More IPv4 assumptions than found by grep (Xn-U forwarding, N2 transfers, rate limiter) | Med | T7/T8 are test-first; add an "Ethernet frame (non-IP) through every path" test |
| Core (UPF) Ethernet path switch behaves differently (known UPF issues C1-2235/2236/2260/2261) | Med | Capture UPF/SMF/AMF logs per scenario; keep UE↔UE results separated by UPF version (lab is on 1.0.33) |
| Second gNB on same VM needs extra N2/N3 addresses (per-NIC portgroups) | Med | Extend `~/ether-e2e/eth-gnb-seg.yaml` pattern with another /32 pair |
| Daren's known issues carried over (N2 boundary loses 1 ping; Xn HandoverRequest UEHistoryInformation placeholder malformed in Wireshark) | Low | Document as parity limitations, not regressions |
| srsRAN XnAP AGPL notice / 190k vendored lines slows CI & review | Low | Keep as isolated commit; notices file |
| No macOS validation (SCTP/TAP Linux-only) | Low | All builds/tests on Linux VM / fgcdev container |

## Open Questions
1. Squashed patch import (recommended) vs real `git merge` of daren `main` (keeps provenance, imports 1193 commits)?
2. Which VM hosts the target gNB for cross-VM runs (vm10213 is the N6 gateway)? Is a new/free VM available?
3. Should the daren manuals (`使用手冊*.md`) be ported (translated) or only referenced in the new guide?
4. Is a second subscriber/UE config needed for scenario 6 (MRU uses same UE), and may we alter the AMF side (TAC list covers 100–110, fine for two gNBs)?
