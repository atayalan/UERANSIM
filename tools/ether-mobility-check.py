#!/usr/bin/env python3
"""Checks an Ethernet PDU session before and after a mobility procedure (Xn/N2 handover, Mobility Registration Update).

  record  Run before the procedure on the UE's host: stores the TAP's MAC and IPv4 addresses in a state file.
  check   Run after it: the MAC and the IPv4 addresses must be the same as recorded, and three pings must succeed:
            UE to UE (the peer UE), UE to the gateway, and gateway to UE (run on the gateway through --gw-ping-cmd).
          Exits 0 only if every check passed, 1 otherwise, 2 for a usage or environment error.

The TAP belongs to the UE, so it must keep its MAC through the procedure; a kept IPv4 address (DHCP lease) and a working
ping to the gateway show that the lease survived. If the TAP was moved into a network namespace, pass --netns.
Needs only the Python standard library, ip and ping; the gateway side needs whatever --gw-ping-cmd runs, e.g. ssh.

Example:
  sudo tools/ether-mobility-check.py record --tap uesimtap0 --netns ue1 --state /tmp/ue1.json
  ... run the handover ...
  sudo tools/ether-mobility-check.py check --tap uesimtap0 --netns ue1 --state /tmp/ue1.json \\
       --peer-ip 192.168.60.123 --gateway-ip 192.168.60.1 --gw-ping-cmd "ssh vm10213 ping"
"""
import argparse
import json
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
from typing import Callable, List, Optional, Tuple

Runner = Callable[[List[str]], Tuple[int, str]]

EXIT_FAILED = 1
EXIT_USAGE = 2


class CheckError(Exception):
    """The check could not be set up (as opposed to a check that ran and failed)."""


@dataclass(frozen=True)
class CheckConfig:
    tap: str
    netns: Optional[str]
    peer_ip: str
    gateway_ip: str
    gw_ping_cmd: str
    ue_ip: Optional[str] = None
    count: int = 3
    wait: int = 2


@dataclass(frozen=True)
class CheckResult:
    name: str
    ok: bool
    detail: str


def system_runner(argv: List[str]) -> Tuple[int, str]:
    proc = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True,
                          timeout=120, check=False)
    return proc.returncode, proc.stdout


def _in_netns(netns: Optional[str], argv: List[str]) -> List[str]:
    return ["ip", "netns", "exec", netns] + argv if netns else argv


def _tap_mac(tap: str, netns: Optional[str], runner: Runner) -> str:
    rc, out = runner(_in_netns(netns, ["cat", "/sys/class/net/%s/address" % tap]))
    mac = out.strip().lower()
    if rc != 0 or not re.fullmatch(r"([0-9a-f]{2}:){5}[0-9a-f]{2}", mac):
        raise CheckError("cannot read the MAC of %s%s: %s" % (tap, " in netns " + netns if netns else "", out.strip()))
    return mac


def _tap_ipv4(tap: str, netns: Optional[str], runner: Runner) -> List[str]:
    rc, out = runner(_in_netns(netns, ["ip", "-o", "-4", "addr", "show", "dev", tap]))
    if rc != 0:
        raise CheckError("cannot read the addresses of %s: %s" % (tap, out.strip()))
    return sorted(re.findall(r"inet (\d+\.\d+\.\d+\.\d+/\d+)", out))


def record_state(tap: str, netns: Optional[str], runner: Runner) -> dict:
    return {"tap": tap, "mac": _tap_mac(tap, netns, runner), "ipv4": _tap_ipv4(tap, netns, runner)}


def save_state(path: str, state: dict) -> None:
    with open(path, "w") as f:
        json.dump(state, f, indent=2)
        f.write("\n")


def load_state(path: str) -> dict:
    try:
        with open(path) as f:
            state = json.load(f)
    except (OSError, ValueError) as e:
        raise CheckError("cannot read the state file %s: %s" % (path, e))
    if not isinstance(state, dict) or not {"tap", "mac", "ipv4"} <= state.keys():
        raise CheckError("%s is not a state file written by 'record'" % path)
    return state


def _ping(name: str, argv: List[str], runner: Runner) -> CheckResult:
    try:
        rc, out = runner(argv)
    except Exception as e:  # a missing ssh or ping binary is a failed check, the others still run
        return CheckResult(name, False, "%s could not run: %s" % (argv[0], e))
    received = re.search(r"(\d+) (?:packets )?received", out)
    detail = "%s replies" % received.group(1) if received else out.strip().splitlines()[-1:] and out.strip().splitlines()[-1]
    return CheckResult(name, rc == 0, detail or "exit status %d" % rc)


def run_checks(cfg: CheckConfig, state: dict, runner: Runner) -> List[CheckResult]:
    results: List[CheckResult] = []

    try:
        mac = _tap_mac(cfg.tap, cfg.netns, runner)
        results.append(CheckResult("mac", mac == state["mac"], "%s (recorded %s)" % (mac, state["mac"])))
    except Exception as e:
        results.append(CheckResult("mac", False, str(e)))

    try:
        ipv4 = _tap_ipv4(cfg.tap, cfg.netns, runner)
        results.append(CheckResult("ipv4", ipv4 == sorted(state["ipv4"]),
                                   "%s (recorded %s)" % (ipv4 or "none", state["ipv4"] or "none")))
    except Exception as e:
        results.append(CheckResult("ipv4", False, str(e)))

    count, wait = str(cfg.count), str(cfg.wait)
    ue_ping = _in_netns(cfg.netns, ["ping", "-c", count, "-W", wait, "-I", cfg.tap])
    results.append(_ping("ue-to-ue", ue_ping + [cfg.peer_ip], runner))
    results.append(_ping("ue-to-gateway", ue_ping + [cfg.gateway_ip], runner))

    ue_ip = cfg.ue_ip or (state["ipv4"][0].split("/")[0] if state["ipv4"] else None)
    if ue_ip is None:
        results.append(CheckResult("gateway-to-ue", False, "the UE has no IPv4 address, pass --ue-ip"))
    else:
        results.append(_ping("gateway-to-ue",
                             shlex.split(cfg.gw_ping_cmd) + ["-c", count, "-W", wait, ue_ip], runner))
    return results


def exit_status(results: List[CheckResult]) -> int:
    return 0 if results and all(r.ok for r in results) else EXIT_FAILED


def _parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = p.add_subparsers(dest="mode", required=True)
    for mode in ("record", "check"):
        s = sub.add_parser(mode)
        s.add_argument("--tap", required=True, help="TAP interface of the UE, e.g. uesimtap0")
        s.add_argument("--netns", help="network namespace the TAP was moved into")
        s.add_argument("--state", required=True, help="state file written by record and read by check")
    c = sub.choices["check"]
    c.add_argument("--peer-ip", required=True, help="IPv4 address of the peer UE")
    c.add_argument("--gateway-ip", required=True, help="IPv4 address of the gateway on the data network")
    c.add_argument("--gw-ping-cmd", required=True,
                   help="command that runs ping on the gateway, e.g. 'ssh vm10213 ping'; -c, -W and the UE address "
                        "are appended")
    c.add_argument("--ue-ip", help="IPv4 address the gateway pings (default: the one recorded)")
    c.add_argument("--count", type=int, default=3)
    c.add_argument("--wait", type=int, default=2, help="seconds to wait for each reply")
    return p


def main(argv: Optional[List[str]] = None, runner: Runner = system_runner) -> int:
    args = _parser().parse_args(argv)
    try:
        if args.mode == "record":
            state = record_state(args.tap, args.netns, runner)
            save_state(args.state, state)
            print("recorded %s: mac %s, ipv4 %s" % (args.tap, state["mac"], ", ".join(state["ipv4"]) or "none"))
            return 0
        state = load_state(args.state)
        if state["tap"] != args.tap:
            raise CheckError("the state file is for %s, not %s" % (state["tap"], args.tap))
        cfg = CheckConfig(tap=args.tap, netns=args.netns, peer_ip=args.peer_ip, gateway_ip=args.gateway_ip,
                          gw_ping_cmd=args.gw_ping_cmd, ue_ip=args.ue_ip, count=args.count, wait=args.wait)
    except CheckError as e:
        print("error: %s" % e, file=sys.stderr)
        return EXIT_USAGE

    results = run_checks(cfg, state, runner)
    for r in results:
        print("%s %-14s %s" % ("PASS" if r.ok else "FAIL", r.name, r.detail))
    return exit_status(results)


if __name__ == "__main__":
    sys.exit(main())
