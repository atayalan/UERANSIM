# G1-4379 review findings (2026-11-19)

**Status after the fix commit:** A1, A3, A4, A5 (pending cap, address filter, accept loop, fd leak), A6, B1 and B2 are fixed (details at the end). A2 (peer-built Handover Request is trusted), A7, B3-B5 and C1-C6 are open.

Two independent read-only reviews of `git diff master...HEAD` (vendored `third_party/` and `src/gnb/xnap/srsran_xnap/` excluded): a C++ code review and a security review. Neither built, ran or fuzzed anything, and the reviewers did not read `xnap/codec.cpp` or `test/`. Findings are unverified claims until fixed with a test or reproduced. The lab runs passed, so none of these showed up in the six scenarios.

## A. Xn peer trust (security 1, 2, 3, 5, 7; code 3, 11)
The Xn listener accepts a Setup Request from any host and trusts the self-asserted gNB ID.
- A1 HIGH: no check of the SCTP remote address against `neighbor.address`; an inbound Setup replaces a CONNECTED neighbor (`xnap/task.cpp` ~360-430).
- A2 HIGH: a peer-built Handover Request is trusted (sti, amfUeNgapId, AMBR, security capabilities, PDU sessions) and drives a Path Switch.
- A3 HIGH: a forged Handover Request Ack makes the source forward downlink GTP-U to any IPv4 address (`xnap/codec.cpp` 325-357, `gtp/task.cpp` ~626-657).
- A4 HIGH (code 3): Xn UE Context Release is accepted without state == COMMAND_SENT or a `targetUeXnapId` match, so a stray release deletes a live UE.
- A5 MED: unbounded, untimed pending inbound associations and an accept loop with no try/catch (`sctp/task.cpp`); `Accept()` can leak an fd.
- A6 LOW: an unsolicited Xn Setup Response marks a neighbor CONNECTED.
- A7 MED: Xn-U target forwarding TEIDs are sequential from 0xE0000000, accept packets from any source and skip the rate limiter.

Trust assumption today: Xn runs on a closed lab segment. Suggested fix: accept inbound only from the configured neighbor address, never replace a CONNECTED neighbor, require the endpoint address in the Ack to equal the peer, require COMMAND_SENT and a matching id on release, cap and time out pending associations, catch in the accept loop. Document that Xn has no authentication (no IPsec/DTLS).

## B. Handover context lifecycle (code 1, 2, 7, 12, 15)
- B1 HIGH: aborting a target-side preparation (cancel, association shutdown, forwarding allocation failure, failed Ack) erases `m_handovers` but leaves the NGAP UE context and GTP sessions, so every later Xn Handover Request for that UE fails with "Target NGAP UE context already exists".
- B2 HIGH: no deadline on PREPARING, COMMAND_SENT or PATH_SWITCH_PENDING; a failed Path Switch is never reported to the source, which keeps forwarding and refuses new handovers.
- B3 MED: `m_n2HandoverTargets` and placeholder N2 target contexts leak on early returns and throws in `receiveHandoverRequest`; no Handover Failure sent to the AMF.
- B4 MED: target `m_targetForwarding` is not cleaned after a successful Path Switch.
- B5 LOW: `handleHandoverRequest` returns silently for an unknown client, so the source waits forever.

## C. Smaller correctness issues
- C1 MED: `RlsUdpTask::reserveNewUeId()` is unsynchronised and returns the last id (0 if none), used as the fallback when the N2 container has no sti (`rls/udp_task.cpp` 97-100, `ngap/handover.cpp` ~296). Reject the request instead, or lock and return counter + 1.
- C2 MED: UE cell id from an untrusted heartbeat can collide or go negative (`ue/rls/udp_task.cpp`).
- C3 MED: `gnbTargetID << (32 - gnbIdLength)` shifts a signed int (`ngap/handover.cpp:152`); cast to `uint32_t` first.
- C4 MED: `amfUeNgapId` is truncated to int (`ngap/handover.cpp:331`, pre-existing type).
- C5 LOW: `RlsUdpTask::send()` reads `m_ueMap` without `m_idMutex` (pre-existing race; the new mutex looks complete but is not).
- C6 LOW: `HEARTBEAT_THRESHOLD` raised from 2000 to 10000 ms in gNB and UE; confirm intent (it slows dead-cell detection for all uses).

## Rejected by the reviewer
CLI-thread access to `m_neighbors`/`m_handovers`: safe, `GnbCmdHandler::handleCmd` pauses all tasks first. `snapshot.cpp` parsing and `handover_identity.cpp` reviewed with no findings.

## Fixed (2026-11-19)
- A1: an inbound Xn connection is accepted only from a configured neighbor's IP (`SctpClient::peerAddress`, `xnap::SameHostAddress`), the Xn Setup must come from that neighbor's address, and a CONNECTED neighbor is never replaced.
- A3: a Handover Request Ack with an unusable forwarding address (not 4-octet IPv4, or unspecified, loopback, multicast, reserved) or TEID 0 drops the handover and sends a Handover Cancel (`xnap::IsUsableForwardingAddress`). The address is not required to equal the neighbor's Xn address, because the neighbor's GTP-U address can differ from its Xn address.
- A4: Xn UE Context Release is honoured only in COMMAND_SENT and with the target UE id the target gave in its Ack.
- A5: at most 16 inbound connections wait for Xn Setup; the SCTP accept loop catches errors and `Accept()` closes the fd if event setup fails. A pending-connection timeout was not added.
- A6: an Xn Setup Response is ignored unless a Setup Request is outstanding.
- B1: every handover abort goes through `XnapTask::abortHandover`, which releases the target NGAP/GTP context once target preparation had completed. A prepare ack that arrives after the handover was dropped also releases it.
- B2: a handover older than 15 s is dropped on the reconnect timer (superseded: a target in PATH_SWITCH_PENDING has 60 s from the Path Switch, see the fourth and fifth rounds). A failed Path Switch drops the target context; the source drops its record on its own timeout (the target does not tell the source).
Tests: `test/gnb/xnap/policy.cpp` (2 cases); all 79 tests pass with ASan and UBSan on Ubuntu 24.04. Lab (build with the fixes on vm10211): Xn and N2 Ethernet handover on one VM pass every check, Xn Ethernet handover across VMs passes (gNB2 on vm10213 with the older binary), and a connection from a non-neighbor address is rejected in the gNB log. The abort and timeout paths (B1, B2) and the A4 and A6 checks have no unit test or lab run of their own; they are covered by review only.

## PR review round (`/review-pr 6`, six agents) and its fixes
No Critical findings. Fixed in the same commit:
- Xn Handover Cancel only undoes a preparation (PREPARING, or TARGET_PREPARED with the target UE id the target gave); it is ignored once the UE is moving to the target (`xnap::CancelAccepted`).
- A Handover Request Ack without a target UE id is rejected (`xnap::AckUsable`); a Handover Request without an AMF UE NGAP ID is refused with a Preparation Failure, and the source refuses to start one.
- A source handover dropped while its command was outstanding leaves a 120 s record, so a late Xn UE Context Release still releases the source NGAP/GTP/RRC contexts.
- N2 target no longer overwrites `ranUeNgapId` with the radio-link UE id (it could repeat the id of another UE); the Handover Notify finds the UE by its context id.
- A Path Switch Acknowledge with no switched session, an undecodable transfer or no usable uplink tunnel is a Path Switch failure instead of a success.
- A failed RRC Handover Command build is reported back: XnAP sends a Handover Cancel to the target and drops the handover, NGAP sends a Handover Cancel to the AMF.
- `nr-cli n2-handover` reports a handover that could not be sent (`sendHandoverRequired` returns an error).
- Tests: 7 new cases (`policy` decision rules, CLI parsing of `mobility-update` and the handover commands); 86 tests pass with ASan and UBSan. Lab on one VM (build with these fixes): Xn and N2 Ethernet handover pass every check. Cross-VM and the new failure paths (RRC command failure, tombstone release, Cancel after path switch) were not run in the lab.

Open from that round (not fixed): stale N2 target context reused without refreshing its ids; failed Xn Setup encode is silent and `XN_SETUP_PENDING` has no timeout; Xn listener bind failure is not reported to XnAP; Path Switch Ack/Failure for an unknown UE leaves Xn pending until the timeout; no tests for the `XnapTask` message handlers themselves (only the extracted decision rules), the UE RRC/RLS handover code, the SCTP changes or negative codec input; type design (split source and target state, the sentinel 0 for "no AMF id" on the wire, `RELEASE_HANDOVER_SOURCE` also used for target cleanup); stray line in `ue/nas/mm/base.cpp:94`; helper extraction for repeated Preparation Failure and Cancel blocks.

## Third review round (`/review-pr`, six agents on the squashed commit) and its fixes
No Critical findings. Several Important ones came from the previous round's fixes. Fixed:
- Dropping a handover (Xn link lost, 15 s expiry) no longer deletes a target UE that is in PATH_SWITCH_PENDING: only the Xn record goes. A failed Path Switch still releases the context (`xnap::ReleaseTargetContextOnAbort`), and an unusable Path Switch Acknowledge also sends a UE Context Release Request to the AMF.
- A Path Switch Acknowledge without an uplink NG-U tunnel is a success (the field is optional, the tunnel stays); only an undecodable transfer or a present but unusable tunnel fails.
- The late-release record (`m_droppedSource`) is cleared when a new handover starts and when a release is accepted; the match is `xnap::LateReleaseAccepted`.
- An Xn Handover Preparation Failure is honoured only before the handover command (`PreparationFailureAccepted`); a target prepare failure after the Ack is ignored, and the valid one goes through `abortHandover`.
- On timeout the source sends an Xn Handover Cancel to the target while the target may still hold a prepared context (`CancelTargetOnTimeout`).
- N2: a handover command failure cancels only a handover this gNB started (`m_n2HandoverSources`), and the AMF's Handover Cancel Acknowledge is handled instead of logged as an error.
- The RRC-to-XnAP message handler is a `switch`. Comments fixed for `releaseHandoverSource`, the N2 fallback id, the existing-context note, the Cancel rule and the inbound-association rule.
- Tests: 12 new cases in total across the two rounds' extra work (policy rules, CLI parsing with exact messages, `xn-status`); 91 pass with ASan and UBSan. Lab on one VM: Xn and N2 Ethernet handover pass every check with this build. The Cancel, Release and abort paths are covered by the policy unit tests only, not exercised in the lab.

Open from this round: the `XnapTask` handlers themselves have no direct test (only the rules they call); `sendRrcMessage` returns nothing, so an RRC encode failure or a missing UE still counts as sent (HANDOVER_COMMAND_FAILED misses it); late or rejected Xn messages (Ack, Failure, Cancel for an unknown UE) are dropped without a log; the SCTP accept loop retries forever every 200 ms and a rejected connection leaves little trace; `EHandoverState` plus `sourceSide` still allows impossible pairs and `targetPreparationComplete` can disagree with the state; `sendHandoverRequired` does not refuse a missing AMF id or radio identity; `ParseInt` accepts a numeric prefix (`2abc`); simplification helpers (drop-inbound, Preparation Failure and Cancel senders, id range check), dead code in `ngap/handover.cpp`.

## Fourth review round (`/review-pr`, six agents on `cc4580d`) and its fixes
No Critical findings; the Important ones were lifecycle gaps at the edges of the previous round's fixes. Fixed:
- A Path Switch the AMF rejected now releases the attached target UE completely: XnAP asks NGAP for `RELEASE_FAILED_TARGET`, which sends an RRC Release (`AN_RELEASE`) and then releases the NGAP/GTP context. An unusable Path Switch Acknowledge (the AMF accepted the switch) is different: this gNB sends a UE Context Release Request, XnAP only drops its record (`PATH_SWITCH_UNUSABLE`), and the AMF's release command tears the UE down and is answered, so the two releases no longer race.
- A target in PATH_SWITCH_PENDING keeps its Xn record when the Xn association shuts down, and has 60 s from the Path Switch (not 15 s from the start) for the AMF to answer (`xnap::HandoverTimedOut`). The Xn UE Context Release uses the neighbor's current association and logs an error when the link is down. A late Path Switch Failure with no record releases the UE; a late Acknowledge with no record is logged (the source gNB then keeps its context, because nothing releases a source context on expiry; superseded for the link-down case in the fifth round).
- N2 sets: `deleteUeContext` clears `m_n2HandoverSources` and `m_n2HandoverTargets`, `releaseHandoverSource` goes through it, and the sources set is also cleared on a Handover Preparation Failure and on a Handover Cancel Acknowledge.
- A Handover Command without a TargetToSource container cancels the N2 handover (Handover Cancel to the AMF).
- Comments: the `reserveNewUeId` wording, the `m_n2HandoverSources` description, the `releaseHandoverSource` and `abortHandover` conditions. Tests: `HandoverTimedOut`; a comment in `test/gnb/xnap/policy.cpp` that an Ack without forwarding endpoints is valid (the same statement is now in `policy.hpp`). 92 tests pass with ASan and UBSan; Xn and N2 Ethernet handover pass on one VM with this build. The failed-Path-Switch teardown, the late Ack/Failure handling and the Cancel paths were not exercised in the lab.

Open from this round (not fixed): the policy functions take adjacent same-typed arguments (`CancelAccepted`, `LateReleaseAccepted`, `HandoverExpired`, the bool flags of `abortHandover` and `ReleaseTargetContextOnAbort`), so a swap compiles; several decisions are still inline in `XnapTask` and untested (when `abortHandover` records the late-release entry, the Ack and Path Switch state guards, peer ownership of a handover, the 120 s record expiry); `m_n2HandoverSources` is not cleared on a successful command (RRC sends no success notice); CLI test holes (`handover` alias fields, `gnbId` overflow); simplification helpers (`onTargetPrepared` instead of a fake NGAP message, redundant peer lookup, `releaseNgapContext`, collect-then-abort, insert-then-erase in `handleRrcHandoverCommandFailed`).

## Fifth review round (`/review-pr`, six agents on `e342305`) and its fixes
The code reviewer found no new bug; the other findings were hardening and edge cases around the previous round's fixes. Fixed:
- A Path Switch the AMF never answered (60 s) now asks the AMF to release the attached UE (`REQUEST_UE_RELEASE` -> UE Context Release Request); if that request can not be sent the UE is released locally. Before this, the expiry left the UE attached with no record.
- `sendNgapUeAssociated` and `sendContextRelease` return whether they sent and log when the UE or AMF is unknown. A Path Switch Request that could not be sent fails the Path Switch at once; an unusable Acknowledge whose release request could not be sent releases the UE locally.
- A Path Switch Acknowledge with the Xn link down queues the UE Context Release (`m_pendingReleases`) and sends it when the neighbor is connected again (kept for 120 s, the source's late-release window). The earlier log claim that the source "keeps its context until its own record expires" was wrong and is removed.
- Path Switch Failure and Unusable are honoured only for a target in PATH_SWITCH_PENDING (`PathSwitchAnswerAccepted`), so a stale answer cannot hit a newer handover of the same UE.
- `releaseFailedTarget` checks that the NGAP context exists before sending the RRC Release; `deleteUeContext` no longer inserts null map entries.
- The abort decision is an enum (`xnap::AbortCause`, `TargetReleaseOnAbort` returning `NONE/CONTEXT/ATTACHED_UE/ASK_AMF`) instead of a bool plus a ternary; the "survives an Xn link loss" rule is `SurvivesAssociationShutdown`; the state and its timestamp change together in `enterPathSwitchPending`; a `static_assert` pins the 60 s > 15 s relation.
- Comments: the unusable-Acknowledge wording, `startedMs`, the `m_n2HandoverSources` description. Notes: the three statements above corrected.
- Tests: 94 pass with ASan and UBSan (`TargetReleaseOnAbort`, `PathSwitchAnswerAccepted`, `SurvivesAssociationShutdown`). Lab on one VM: Xn and N2 Ethernet handover pass every check. The release-request, pending-release and send-failure paths were not exercised in the lab.

Open from this round (not fixed): the N2 set cleanup has no unit test (a small tracker class would make it testable); `HandoverTimedOut` takes five adjacent `int64_t`; `m_n2HandoverSources` is not cleared on a successful command; simplification helpers (Preparation Failure and Cancel senders, a shared source-handover lookup, `validateTargetSnapshot`, the `prepareHandoverTarget` rollback calling `releaseHandoverSource`).
