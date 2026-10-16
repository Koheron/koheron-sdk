"""Read CPUFreq policies without changing scaling or starting an instrument."""
import json
import time

from native_fixture import NativeFixture
from test_native_management import WebSocket


class NativeCpuHealthTest(NativeFixture):
    def setUp(self):
        super().setUp()
        self.policies = self.root / 'sys/devices/system/cpu/cpufreq'
        self.policies.mkdir(parents=True)
        self.save_state(active=False)

    def policy(self, name='policy0', **attributes):
        path = self.policies / name
        path.mkdir(exist_ok=True)
        for key, value in attributes.items():
            (path / key).write_text(str(value) + '\n')
        return path

    def health(self):
        status, body, _ = self.request('/api/system/status')
        self.assertEqual(status, 200, body)
        return json.loads(body)['health']

    def test_hardware_frequency_preferred_and_requested_fallback_is_explicit(self):
        self.policy(cpuinfo_cur_freq=1200000, scaling_cur_freq=1000000,
                    scaling_governor='schedutil', affected_cpus='0 1')
        self.policy('policy2', scaling_cur_freq=800000, scaling_governor='performance', affected_cpus='2 3')
        self.start_api()
        self.assertEqual(self.health()['cpu']['policies'], [
            {'label': 'policy0', 'frequency_hz': 1200000000, 'frequency_source': 'hardware',
             'governor': 'schedutil', 'affected_cpus': '0 1'},
            {'label': 'policy2', 'frequency_hz': 800000000, 'frequency_source': 'requested',
             'governor': 'performance', 'affected_cpus': '2 3'}])
        self.assertFalse(any(line.startswith(('start ', 'restart ')) for line in self.operations()))

    def test_missing_and_invalid_values_do_not_become_zero_frequencies(self):
        for i, value in enumerate(('', '0', '-1', 'nan', '1000x', '1.5', '18446744073709551616', 'x' * 129)):
            self.policy(f'policy{i}', cpuinfo_cur_freq=value, scaling_cur_freq=value)
        self.policy('policy8', cpuinfo_cur_freq='bad', scaling_cur_freq=600000)
        self.policy('policy9', scaling_governor='x' * 65)
        self.start_api()
        policies = self.health()['cpu']['policies']
        for policy in policies[:8] + policies[9:]:
            self.assertIsNone(policy['frequency_hz']); self.assertIsNone(policy['frequency_source'])
        self.assertEqual(policies[8]['frequency_hz'], 600000000)
        self.assertEqual(policies[8]['frequency_source'], 'requested')
        self.assertEqual(policies[9]['governor'], '')

    def test_absent_cpufreq_and_bounded_policy_discovery(self):
        self.policies.rmdir(); self.start_api()
        self.assertEqual(self.health()['cpu'], {'policies': []})
        self.stop_api(); self.policies.mkdir()
        for i in range(40): self.policy(f'policy{i}', scaling_cur_freq=800000)
        self.policy('policybad', scaling_cur_freq=800000)
        self.start_api()
        policies = self.health()['cpu']['policies']
        self.assertEqual(len(policies), 32)
        self.assertTrue(all(p['frequency_hz'] == 800000000 and p['label'] != 'policybad' for p in policies))

    def test_frequency_updates_on_existing_websocket_with_instrument_stopped(self):
        path = self.policy(scaling_cur_freq=600000)
        self.start_api()
        socket = WebSocket(self.port); self.addCleanup(socket.close)
        first = json.loads(socket.frame()[1])
        self.assertIsNone(first['instruments']['live_instrument'])
        self.assertEqual(first['health']['cpu']['policies'][0]['frequency_hz'], 600000000)
        (path / 'scaling_cur_freq').write_text('1000000\n')
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            opcode, payload = socket.frame()
            if opcode == 1 and json.loads(payload)['health']['cpu']['policies'][0]['frequency_hz'] == 1000000000:
                break
        else:
            self.fail('CPU frequency did not update on the status WebSocket')
