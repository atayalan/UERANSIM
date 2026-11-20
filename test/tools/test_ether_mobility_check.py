"""Tests for tools/ether-mobility-check.py, run through ctest; the module is loaded by path because of its file name."""
import importlib.util
import json
import os
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location(
    "ether_mobility_check", os.path.join(HERE, "..", "..", "tools", "ether-mobility-check.py"))
emc = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(emc)

MAC = "6a:e4:19:b0:c7:d2"
ADDR_LINE = "3: uesimtap0    inet 192.168.60.124/24 brd 192.168.60.255 scope global dynamic uesimtap0\n"


class FakeRunner:
    """Answers commands from a table keyed by a substring of the joined command; records every call."""

    def __init__(self, answers, overrides=None):
        self.answers = dict(overrides or {})  # an override is looked up before the answer it replaces
        for key, answer in answers.items():
            self.answers.setdefault(key, answer)
        self.calls = []

    def __call__(self, argv):
        line = " ".join(argv)
        self.calls.append(line)
        for key, (rc, out) in self.answers.items():
            # A key ending in $ must match the end of the command, so that ...60.1 does not match ...60.123.
            if line.endswith(key[:-1]) if key.endswith("$") else key in line:
                return rc, out
        return 1, "no answer for: " + line


def ping_ok(received=3):
    return 0, "3 packets transmitted, %d received, 0%% packet loss\n" % received


def args(**over):
    base = dict(tap="uesimtap0", netns=None, peer_ip="192.168.60.123", gateway_ip="192.168.60.1",
                gw_ping_cmd="ssh gw ping", ue_ip=None, count=3, wait=2)
    base.update(over)
    return emc.CheckConfig(**base)


class RecordTest(unittest.TestCase):
    def test_records_mac_and_ipv4_of_the_tap(self):
        runner = FakeRunner({"address": (0, MAC.upper() + "\n"), "addr show": (0, ADDR_LINE)})
        state = emc.record_state("uesimtap0", None, runner)
        self.assertEqual(state, {"tap": "uesimtap0", "mac": MAC, "ipv4": ["192.168.60.124/24"]})

    def test_runs_inside_the_netns_when_given(self):
        runner = FakeRunner({"address": (0, MAC + "\n"), "addr show": (0, "")})
        emc.record_state("uesimtap0", "ue1", runner)
        self.assertTrue(all(c.startswith("ip netns exec ue1 ") for c in runner.calls), runner.calls)

    def test_a_missing_tap_is_an_error(self):
        runner = FakeRunner({"address": (1, "No such device")})
        with self.assertRaises(emc.CheckError):
            emc.record_state("uesimtap0", None, runner)


class CheckTest(unittest.TestCase):
    STATE = {"tap": "uesimtap0", "mac": MAC, "ipv4": ["192.168.60.124/24"]}

    def runner(self, **over):
        answers = {"address": (0, MAC + "\n"), "addr show": (0, ADDR_LINE),
                   "ping -c 3 -W 2 -I uesimtap0 192.168.60.123": ping_ok(),
                   "ping -c 3 -W 2 -I uesimtap0 192.168.60.1$": ping_ok(),
                   "ssh gw ping -c 3 -W 2 192.168.60.124": ping_ok()}
        return FakeRunner(answers, over)

    def test_everything_unchanged_passes(self):
        results = emc.run_checks(args(), self.STATE, self.runner())
        self.assertEqual([r.name for r in results], ["mac", "ipv4", "ue-to-ue", "ue-to-gateway", "gateway-to-ue"])
        self.assertTrue(all(r.ok for r in results), results)

    def test_a_changed_mac_fails_only_the_mac_check(self):
        results = emc.run_checks(args(), self.STATE, self.runner(address=(0, "02:00:00:00:00:01\n")))
        failed = [r.name for r in results if not r.ok]
        self.assertEqual(failed, ["mac"])
        self.assertIn("02:00:00:00:00:01", results[0].detail)

    def test_a_lost_address_fails_the_ipv4_check(self):
        results = emc.run_checks(args(), self.STATE, self.runner(**{"addr show": (0, "")}))
        self.assertEqual([r.name for r in results if not r.ok], ["ipv4"])

    def test_each_ping_direction_is_reported_on_its_own(self):
        for key, name in (("-I uesimtap0 192.168.60.123", "ue-to-ue"), ("-I uesimtap0 192.168.60.1$", "ue-to-gateway"),
                          ("ssh gw ping", "gateway-to-ue")):
            results = emc.run_checks(args(), self.STATE, self.runner(**{key: (1, "100% packet loss")}))
            self.assertEqual([r.name for r in results if not r.ok], [name], key)

    def test_the_gateway_pings_the_recorded_ue_address_unless_one_is_given(self):
        runner = self.runner()
        emc.run_checks(args(), self.STATE, runner)
        self.assertTrue(any(c.endswith("-W 2 192.168.60.124") for c in runner.calls), runner.calls)
        runner = self.runner(**{"ssh gw ping -c 3 -W 2 10.0.0.9": ping_ok()})
        emc.run_checks(args(ue_ip="10.0.0.9"), self.STATE, runner)
        self.assertTrue(any(c.endswith("-W 2 10.0.0.9") for c in runner.calls), runner.calls)

    def test_an_exception_from_the_runner_is_a_failure_not_a_crash(self):
        def broken(argv):
            raise OSError("ssh not found")
        results = emc.run_checks(args(), self.STATE, broken)
        self.assertFalse(all(r.ok for r in results))


class StateFileTest(unittest.TestCase):
    def test_round_trip_and_rejection_of_a_bad_file(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "state.json")
            state = {"tap": "uesimtap0", "mac": MAC, "ipv4": []}
            emc.save_state(path, state)
            self.assertEqual(emc.load_state(path), state)
            with open(path, "w") as f:
                json.dump({"tap": "x"}, f)
            with self.assertRaises(emc.CheckError):
                emc.load_state(path)
            with self.assertRaises(emc.CheckError):
                emc.load_state(os.path.join(d, "missing.json"))


class ExitCodeTest(unittest.TestCase):
    def test_exit_status_is_zero_only_when_every_check_passed(self):
        ok = emc.CheckResult("a", True, "")
        bad = emc.CheckResult("b", False, "")
        self.assertEqual(emc.exit_status([ok, ok]), 0)
        self.assertEqual(emc.exit_status([ok, bad]), 1)
        self.assertEqual(emc.exit_status([]), 1)


class MainTest(unittest.TestCase):
    def test_record_then_check_through_the_command_line(self):
        runner = FakeRunner({"address": (0, MAC + "\n"), "addr show": (0, ADDR_LINE), "ping": ping_ok()})
        with tempfile.TemporaryDirectory() as d:
            state = os.path.join(d, "s.json")
            self.assertEqual(emc.main(["record", "--tap", "uesimtap0", "--state", state], runner), 0)
            check = ["check", "--tap", "uesimtap0", "--state", state, "--peer-ip", "192.168.60.123",
                     "--gateway-ip", "192.168.60.1", "--gw-ping-cmd", "ssh gw ping"]
            self.assertEqual(emc.main(check, runner), 0)
            runner.answers["address"] = (0, "02:00:00:00:00:01\n")
            self.assertEqual(emc.main(check, runner), 1)

    def test_a_missing_or_foreign_state_file_is_a_usage_error(self):
        runner = FakeRunner({})
        base = ["check", "--tap", "uesimtap0", "--peer-ip", "1.1.1.1", "--gateway-ip", "1.1.1.2", "--gw-ping-cmd", "x"]
        with tempfile.TemporaryDirectory() as d:
            self.assertEqual(emc.main(base + ["--state", os.path.join(d, "none.json")], runner), 2)
            path = os.path.join(d, "other.json")
            emc.save_state(path, {"tap": "uesimtap9", "mac": MAC, "ipv4": []})
            self.assertEqual(emc.main(base + ["--state", path], runner), 2)


if __name__ == "__main__":
    unittest.main()
