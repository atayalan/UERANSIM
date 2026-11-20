# G1-4379 Task List (see tasks/plan.md for detail)

## Phase 1 — M1: IPv4 mobility parity
- [x] T1 Import third_party/srsran_xnap + CMake/SCTP plumbing
- [x] T2 Apply daren non-overlapping sources (gNB / UE / lib)
- [x] T3 Resolve 12 overlapping files (gtp/task, ue udp_task, lib/udp, cli_cmd, CMake...)
- [x] T4 Port 3 unit tests to doctest (xnap codec, gtp session tree, cli options)
- [x] T5 Configs xn-gnb1/2, Docker/chart/CI check
- [ ] **Checkpoint 1**: Linux build + all tests + CI green
- [x] T6 Lab e2e IPv4 scenarios 1, 2, 5 + Ethernet baseline regression

## Phase 2 — M2: Ethernet handover
- [x] T7 Session-type-aware Xn codec + Xn-U forwarding (TDD)
- [x] T8 Session-type-aware N2 / Path Switch (TDD)
- [x] T9 MAC + 3-ping check helper
- [x] T10 Lab e2e scenarios 3, 4 + DHCP lease
- [ ] **Checkpoint 2**: scenarios 1-4 pass, MAC identical, baseline passes
- [x] T11 Cross-VM scenarios 1-4; close N3/Xn open question (gNB2 on vm10213; vm10214 is not on the N2/N3 segments)

## Phase 3 — M3: Ethernet Mobility Registration Update
- [x] T12 PDU session status on MRU with Ethernet; no TAP teardown
- [x] T13 Lab e2e scenario 6 (+5), same and cross VM (nr-cli mobility-update added to trigger an MRU with a session up)
- [ ] **Checkpoint 3**: 6/6 scenarios, both topologies

## Phase 4
- [x] T14 CHANGELOG (generated from commits at release, not edited), Confluence guide (done: child page 2058485761), THIRD_PARTY_NOTICES (done), review (done, findings in tasks/review-g1-4379.md, HIGH and the PR-review Important fixes done, rest open), Jira comment done, PR body updated
