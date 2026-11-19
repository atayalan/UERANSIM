# Lab test plan: G1-4379 handover and Mobility Registration Update

Covers T6, T10, T11, T13 of `tasks/plan.md`. Lab state below was read on 2026-11-18 (read-only); re-check before each run.

## Lab state found

| VM | Role | Observed |
|---|---|---|
| vm10211 | UERANSIM (gNBs + UE) | 8 CPU, passwordless sudo. `~/ueransim-ether` is the old Ethernet-only source (no `.git`), binaries in `cmake-build-release/`. cmake at `~/ueransim-ether-tools/cmake-3.28.6-linux-x86_64/bin`; `sctp-root` is **missing** (only the two `.deb` files remain). `ens224` already carries `192.168.10.31/24`, `.32`, `.33` and a block of `10.50.x.x/32` (not ours, leave alone). `ens192` has `192.168.10.1/32` and a host route to the AMF. No nr-gnb/nr-ue running. |
| vm10212 | Ataya 5GC | AMF, SMF, UDM, UPF pods running (core up ~7 h, UPF ~6 h, so past the C1-2261 60 s window). AMF N2 `192.168.10.10:38412`, UPF N3 `192.168.10.30`, PLMN 999/99, TAC 100-110, SST1/SD000001, DNN internet. |
| vm10213 | N6 gateway | `ens256` `192.168.60.1/24`, `ip_forward=1`, route `192.168.210.0/24` via `192.168.60.10`. dnsmasq **installed but inactive**, no config. Passwordless sudo. |
| vm10214 | candidate target-gNB VM (T11) | Ubuntu 22.04, 8 CPU, sudo, build-essential. **No cmake, no `sctp.h`**. NICs `ens192/224/256` have no IPs (same layout as vm10211). Idle for 21 days. |

## Decisions needed before starting

1. **vm10214 as the cross-VM target gNB host** (closes open question 2). Reachability of its NICs to the AMF/UPF segment is unverified.
2. **Build method.** Preferred: build on vm10211 into a new `~/ueransim-xn` (keeps the old tree for baseline), extract libsctp from the `.deb` files into `sctp-root` (no system install). For vm10214, copy the binaries plus libsctp1 (same Ubuntu release, so no rebuild).
3. **dnsmasq on vm10213**, bound to `ens256` only, started for the DHCP-lease check and stopped afterwards.

## Addressing (same VM, T6/T10/T13)

Reuse the segment plan in `~/ether-e2e/eth-gnb-seg.yaml`; PLMN is 999/99 (the committed `xn-gnb*.yaml` placeholders use 001/01 and 172.16.x, so copy and edit them in `~/ether-e2e`, do not edit the repo files).

| | gNB1 | gNB2 |
|---|---|---|
| nci | `0x000000010` | `0x000000020` |
| linkIp (RLS) | `127.0.0.1` | `127.0.0.2` |
| ngapIp (N2, `ens192`) | `192.168.10.1` | `192.168.10.2` (`/32` alias on `ens192`, remove after) |
| gtpIp (N3, `ens224`) | `192.168.30.1` | `192.168.30.2` (`/24` alias on `ens224`, remove after) |
| Xn bind | `192.168.10.1:38422` | `192.168.10.2:38422` |

Corrected 2026-11-18 after the first baseline attempt: the AMF is `192.168.10.10` via `ens192`, the UPF N3 is `192.168.30.10` via `ens224`. The `192.168.10.31-33` addresses on `ens224` reach nothing and must not be used.

UE config: `gnbSearchList: [127.0.0.1, 127.0.0.2]`, `opType: OPC`. Cross-VM (T11): gNB2 on vm10214 gets its own ngap/gtp/Xn addresses and a `linkIp` the UE can reach; the RLS path (UDP) between UE and the remote gNB is the main unknown to document.

## Order of runs

0. **Setup (once):** rsync branch to `~/ueransim-xn`, build `nr-gnb nr-ue nr-cli nr-tests`, run `nr-tests` on the VM, add the extra address, write configs.
1. **Baseline regression:** old `~/ueransim-ether` binaries, one gNB, Ethernet session (`ps-establish Ethernet --sst 1 --sd 1 --dnn internet`, once only), three pings. Then the same with the new binaries and no handover. Both must pass before any mobility run.
2. **T6, IPv4:** scenario 5 (MRU, `nr-ue --mobi_update`), 1 (Xn HO), 2 (N2 HO). After each: ping via `uesimtun0`. Expect up to 1 lost ping at the N2 boundary (daren parity).
3. **T10, Ethernet same VM:** scenario 3 (Xn HO), 4 (N2 HO), with `tools/ether-mobility-check.py record` before and `check` after. Add DHCP lease check (dnsmasq on vm10213, client in the TAP namespace).
4. **T13, Ethernet MRU:** scenario 6, then re-run 5. Watch the UE log for "releasing locally" (T12 risk).
5. **T11, cross VM:** repeat 1-4 with gNB2 on vm10214, then 5-6 if time allows.

Per scenario: start gNB1 and gNB2, check both NG Setup and Xn established (`nr-cli <gnb> -e xn-status`), start UE, establish session, record, trigger `xn-handover <ue-id> <target-gnb-id>` or `n2-handover ...` via `nr-cli`, check, capture UPF/SMF/AMF logs.

Pass criteria are the PRD acceptance: IPv4 ping after the procedure; for Ethernet UE-to-UE, UE-to-gateway and gateway-to-UE pings, TAP MAC identical, lease still valid.

## Known hazards (from memory and the plan)

- Do not send UE-to-UE unicast while both UEs have flows: crashes the UPF VPP (C1-2236). Use UE-to-gateway and gateway-to-UE for the pass check; run UE-to-UE as a separate, last step per run, and after a UPF crash restart gNBs and UEs.
- Request the Ethernet session **once**; the first attempt logs "No response from RRC from UAC checks" and the ~16 s retransmit succeeds.
- Do not use `pkill -f` / `pgrep -f` over ssh; use `pkill -x nr-ue`.
- Leave the unrelated n6lab tcpdump (vm10213) and chorus-upf (vm10211) alone.
- Cleanup after each run: stop nr-ue/nr-gnb/tcpdump, remove added addresses, flush `192.168.60.50/51` neighbours on vm10213, stop dnsmasq.
- Capture UPF crash cores from `/var/crash` in the UPF pod if the UPF restarts.

## Risks specific to this change

| Risk | Where it would show |
|---|---|
| Core reports an Ethernet session inactive in Registration Accept, UE now releases the TAP (T12) | scenario 6: UE log "releasing locally", TAP gone |
| UPF path switch for Ethernet differs from IPv4 (C1-2235/2236/2260) | scenarios 3, 4: pings fail after HO while Xn/N2 logs look fine |
| Second gNB on same VM misroutes N2/N3 because of the per-NIC portgroups | NG Setup of gNB2 fails, or N3 packets leave the wrong NIC |
| RLS UDP between UE and remote gNB blocked | T11: UE never sees the target cell |

## Output

A results table (scenario, topology, MAC before/after, three pings, notes) in the PR description and the Jira comment, plus logs under `~/ether-e2e/runN` on vm10211.

## Results

### T6 IPv4 (2026-11-18, vm10211 + Ataya core on vm10212, build `db998db`)

One UE, IPv4 session (DNN internet, SST1/SD000001, UE address 192.168.210.10), continuous ping to the N6 gateway 192.168.60.1 through `uesimtun0`.

| # | Scenario | Result | Evidence |
|---|---|---|---|
| 5 | MRU (`--mobi_update`, one gNB) | PASS | "Mobility Registration is successful", session up afterwards, 5/5 pings |
| 1 | Xn handover gNB1 to gNB2 | PASS | 100/100 pings, 0% loss. gNB log: Xn-U forwarding, Path Switch Request/Ack; AMF log: Path Switch Request and Acknowledge |
| 2 | N2 handover gNB1 to gNB2 | PASS | 99/100 pings (1 lost at the boundary, daren parity). AMF log: Handover Required, Request, Acknowledge, Command, Notify |

Observations (not failures):
- `--mobi_update` fires right after the initial registration, before the PDU session exists, so scenario 5 does not test a session surviving an MRU. T13 needs the MRU with the Ethernet session already up to cover that.
- gNB log `[error] UE context not found with id: 1` on the N2 target at Handover Request: `NgapTask::findUeContext` logs an error when probing for a context it then creates. Cosmetic.
- gNB1 logs `Connection refused` to the Xn neighbour while gNB2 is still starting; it connects on retry.
- UPF pod did not restart during the runs. Logs are in `~/ether-e2e/xn/t6-s1`, `t6-s2`, `t6-s5` on vm10211.

### T10 Ethernet, same VM (2026-11-18, build `db998db`)

Tested UE: imsi-999990000000002 (both gNBs in range, attaches to gNB1 first, DHCP lease 192.168.60.124 from the lab dnsmasq through the Ethernet session). Peer UE: imsi-999990000000001 (gNB1 only, static 192.168.60.50, TAP MAC set to 6e:d3:79:59:2c:c4 inside its netns because two `nr-ue` processes get the same TAP MAC). Both TAPs live in their own netns so the UE-to-UE ping really crosses the UPF. Checks by `tools/ether-mobility-check.py` after the handover, plus 150 pings at 0.2 s intervals to the gateway across the handover, and a DHCP re-request (INIT-REBOOT) after it.

| # | Scenario | Continuous ping across HO | MAC | IPv4 / DHCP | UE-UE | UE-GW | GW-UE | Core evidence |
|---|---|---|---|---|---|---|---|---|
| 3 | Xn handover gNB1 to gNB2 | 150/150 | unchanged | same lease, renewed | pass | pass | pass | AMF: Path Switch Request for imsi-002 |
| 4 | N2 handover gNB1 to gNB2 | 149/150 (1 lost at the boundary, daren parity) | unchanged | same lease, renewed | pass | pass | pass | AMF: Handover Required and Handover Notify for imsi-002 |

UPF pod did not restart. Logs: `~/ether-e2e/xn/t10-s3e` and `t10-s4` on vm10211.

Findings:
1. **Multi-UE id mismatch on the Xn target (real defect, daren design).** `NgapTask::prepareHandoverTarget` creates the target NGAP context with the source gNB's UE id (`sourceUeXnapId`), but the RRC Handover Confirm arrives with the id the target's own RLS layer gave that UE. With one UE both are 1. With two UEs in range of both gNBs they can differ: run `t10-s3c` moved a UE that was id 1 on gNB1 and id 2 on gNB2, the target logged `Unexpected RRC handover confirm for UE[2]`, never sent the Path Switch, and dropped uplink (`Uplink data failure, PDU session not found. UE[2]`): 86.7% ping loss, 0/5 afterwards, and gNB1 never released the source context. An earlier two-UE run that moved the other UE passed only because the ids happened to match. Scope of the ticket is one UE and two gNBs, so T10 keeps the peer UE out of gNB2's range; a fix needs a target-side id mapping and is a follow-up. The same assumption is likely in the N2 target path.
2. **UE-to-UE, first ping of a fresh run fails** in this core (old and new binaries alike; the other direction and later pings work). Looks like MAC learning at the UPF: warm up each UE with a ping to the gateway first.
3. **TAP MAC is not set by the UE**: no MAC code in `src/ue`; the same MACs repeat on every restart (6a:e4:19:b0:c7:d2 for the first UE of a process, 6e:d3:79:59:2c:c4 for the second). Handover never touches the TAP, so "MAC unchanged" holds by construction; it does not show that the UE guarantees a stable MAC across restarts.
4. **Initial cell choice is racy** when both gNBs are up before the UE starts (it picked gNB2 in one run). The runs start gNB2 after the tested UE has attached to gNB1.
5. **Test environment:** dhclient is AppArmor-confined (cannot run a script or write a lease file from the home directory), so the runs use a private copy of the binary. The lab already runs its own dnsmasq on vm10213 ens256 (`~/dhcp-lab`, range .100-.150, 1 h lease); it was used as is. A second dnsmasq was started for a few seconds by mistake and stopped before any client used it.

### T10 follow-up: multi-UE id mismatch (2026-11-19, build `d14b9d2`)

Fix for finding 1: the Xn target now finds the UE by its RLS `sti`, carried in the Xn handover snapshot (layout version 2), instead of assuming the source's UE id. The target XnAP id is the target's own id. New unit `src/gnb/xnap/snapshot.{hpp,cpp}` with 5 doctests (74 tests pass in the container, 73 `nr-tests` cases on vm10211).

Re-run of the failing setup (two UEs in one `nr-ue -n 2`, both in range of both gNBs, Ethernet sessions, DHCP, tested UE imsi-002):

| Procedure | UE id on gNB1 / gNB2 | Continuous ping | MAC | Lease | UE-UE / UE-GW / GW-UE |
|---|---|---|---|---|---|
| Xn (`t10-m1`) | 1 / 2 | 150/150 (before the fix: 20/150, Path Switch never sent) | unchanged | same | all pass |
| N2 (`t10-m2`) | 1 / 2 | 149/150 | unchanged | same | all pass |
| N2, other UE moved (`t10-m3`) | 2 / 2 | moved UE to gateway 5/5 | n/a | n/a | n/a |

**N2 fixed the same way.** The source gNB puts the UE's RLS identity (`sti`) in the RRC `HandoverPreparationInformation` inside the Handover Required source-to-target container (`AS-Context.ueAssistanceInformation`, marker `UEID`, helper `src/gnb/ngap/handover_identity.*`); the AMF passes the container on untouched, and the target resolves its own id with `ueIdOf(sti)` instead of `reserveNewUeId()`. Without an identity (a source that is not this simulator) the target keeps the old single-UE guess and logs a warning. Lab, 2 Ethernet UEs, both detected on both gNBs, run `t10-n1`: the moved UE is id 1 on gNB1 and id 1 on gNB2 while the target had detected 2 UEs, so the old guess would have picked id 2. New code picked 1 ("Handover target UE ID: 1", no fallback warning), 149/150 continuous pings, post-HO pings 5/5 both ways, MAC, IPv4, UE-UE, UE-GW, GW-UE and DHCP lease all pass. Not run: a control run with the old binary.

### T11 cross-VM (2026-11-19, build `533d440`)

Source gNB1 and the UE on vm10211, target gNB2 on **vm10213**, the 5GC on vm10212. vm10214, the planned host, could not be used: its `ens192`/`ens224` are not on the N2/N3 segments (ARP to the AMF, to vm10211 and to the UPF fails in both directions, and vm10211 cannot reach an address added there). Changing a vSwitch port group is outside what I can do from the shell. vm10213 has the same NIC layout and its `ens192`/`ens224` are on the N2 and N3 segments, so it hosts gNB2. It is also the N6 gateway, which does not interfere (different NIC and addresses).

| gNB2 on vm10213 | Value |
|---|---|
| N2 `ngapIp` / Xn `bindIp` | 192.168.10.3 on `ens192` (temporary `/24`, reaches the AMF 192.168.10.10) |
| N3 `gtpIp` | 192.168.30.3 on `ens224` (temporary `/24`, reaches the UPF 192.168.30.10 and vm10211's 192.168.30.1) |
| RLS `linkIp` | 10.22.102.13 (management network) |
| UE `gnbSearchList` | 127.0.0.1 (gNB1) and 10.22.102.13 (gNB2) |
| gNB1 Xn neighbour | gnbId 2 at 192.168.10.3:38422 |

Binaries were copied from vm10211 (same Ubuntu 22.04.5, same architecture, system libsctp1 on vm10213); no rebuild. Scripts: `~/ether-e2e/xn/t11.sh` (Ethernet, two UEs, DHCP) and `t11v4.sh` (IPv4, one UE) on vm10211, configs `gnb1x.yaml`, `gnb2x.yaml`, `uex.yaml`, `ue-ipv4x.yaml`.

| # | Scenario | Continuous ping across the procedure | After the procedure | Target gNB log (vm10213) |
|---|---|---|---|---|
| 1 | Xn HO IPv4 | 150/150 | 5/5 | Xn Setup inbound, Handover Request, Path Switch Ack, Xn UE Context Release |
| 2 | N2 HO IPv4 | 149/150 (one lost at the boundary, as on one VM) | 5/5 | Handover Request from the AMF, target UE ID 1, Handover Notify |
| 3 | Xn HO Ethernet | 150/150 | UE-GW 5/5, GW-UE 5/5 | same as 1; MAC, IPv4, UE-UE, UE-GW, GW-UE and the DHCP lease all pass |
| 4 | N2 HO Ethernet | 149/150 | UE-GW 5/5, GW-UE 5/5 | target UE ID 2 (id 1 on gNB1, found through the identity, not guessed); all five checks and the DHCP lease pass |

**Reachability requirements, from what worked** (this closes open question 2):
- **N2:** each gNB needs its own `ngapIp` with SCTP reachability to the AMF (38412). A second gNB on another VM needs nothing from the first.
- **N3:** each gNB needs its own `gtpIp` that can exchange GTP-U (UDP 2152) with the UPF. The Path Switch and Handover Request Ack carry the new gNB's address, so the UPF must reach it.
- **Xn:** SCTP on port 38422 in both directions between the two `xn.bindIp` addresses. They do not have to be on the N2 segment, but they did here. vm10211's N2 addresses are `/32`, so it needed a host route to the peer (`ip route add 192.168.10.3/32 dev ens192`) for the Xn association and the pings to answer.
- **RLS:** the UE must reach each gNB's `linkIp` on UDP 4997 and the gNB answers to the address the datagrams came from. A `linkIp` of 127.0.0.x only works on the UE's own host. The remote gNB is found through `gnbSearchList`.
- **Not exercised:** Xn-U data forwarding was not measured separately. The Xn runs show the Ack carrying one forwarding endpoint and no loss, but the bursts were too small to tell forwarding from the path switch alone.

Cleanup done: gNB processes stopped on both VMs, the temporary 192.168.10.3 and 192.168.30.3 addresses removed from vm10213, the host route removed on vm10211. `~/xn-t11` (binaries and logs) stays on vm10213, the 192.168.10.2/32 and 192.168.30.2/24 aliases stay on vm10211.

## T13 Mobility Registration Update with an Ethernet session up (2026-11-19)

`--mobi_update` only fires after the initial registration, before any PDU session exists, so it cannot show a session surviving an MRU. T13 therefore adds `nr-cli <imsi> mobility-update` (a test aid: it raises a Mobility Registration Update with cause ENTER-UNLISTED-TRACKING-AREA) and runs it with the Ethernet session already up. Scripts: `~/ether-e2e/xn/t13.sh` (same flow as `t10m.sh`, trigger = `mobility-update` under 150 pings) and `t13x.sh` (as `t11.sh`, gNB2 on vm10213, MRU sent after the handover so the gNB on vm10213 serves it).

| Scenario | Setup | Result |
|---|---|---|
| 6 MRU Ethernet | one gNB, UE with DHCP lease | PASS: "Mobility Registration is successful", 150/150 pings across the MRU, MAC/IPv4/UE-UE/UE-GW/GW-UE pass, lease re-requested and kept, "releasing locally" count 0, TAP unchanged |
| 5 MRU IPv4 re-run | one gNB, `--mobi_update` | PASS: MRU successful, 5/5 pings afterwards, no release |
| 6 after Xn HO | gNB2 on vm10213 | PASS: same checks, 150/150 pings across the HO and MRU |
| 6 after N2 HO | gNB2 on vm10213 | PASS: same checks, 149/150 pings (the usual one lost at the N2 boundary) |

Not covered: the UE is CM-CONNECTED during the MRU, so the Registration Request carries no PDU session status IE. The CM-IDLE path (status IE sent, T12 reconcile on the Accept) stays unit-tested only. Cosmetic: the N2 target gNB logs "Unexpected RRC handover confirm" (warning, no effect).
