"""Board snapshots reach HTTP and WebSocket clients without an instrument."""
import json
import time

from native_fixture import NativeFixture
from test_native_management import WebSocket


class NativeTelemetryTest(NativeFixture):
    def setUp(self):
        super().setUp()
        self.telemetry = self.root / 'telemetry.json'
        self.api_options = ['--telemetry', str(self.telemetry)]
        self.save_state(active=False)

    def publish(self, **updates):
        value = {'sampled_monotonic_us': time.monotonic_ns() // 1000,
                 'supplies': [{'label': 'Input', 'voltage_v': 12, 'current_a': .5, 'power_w': 6}],
                 'temperatures': [{'label': 'Processor', 'celsius': 42}], **updates}
        temporary = self.telemetry.with_suffix('.tmp')
        temporary.write_text(json.dumps(value))
        temporary.replace(self.telemetry)

    def status(self):
        status, body, _ = self.request('/api/system/status')
        self.assertEqual(status, 200, body)
        return json.loads(body)

    def test_telemetry_streams_and_updates_while_instrument_is_stopped(self):
        self.publish(); self.start_api()
        value = self.status()
        self.assertIsNone(value['instruments']['live_instrument'])
        self.assertEqual(value['health']['telemetry']['state'], 'live')
        self.assertEqual(value['health']['telemetry']['supplies'][0]['power_w'], 6)
        socket = WebSocket(self.port); self.addCleanup(socket.close)
        opcode, payload = socket.frame()
        self.assertEqual(opcode, 1)
        self.assertEqual(json.loads(payload)['health']['telemetry'], value['health']['telemetry'])
        self.publish(temperatures=[{'label': 'Processor', 'celsius': 43.5}])
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            opcode, payload = socket.frame()
            if opcode == 1 and json.loads(payload)['health']['telemetry']['temperatures'][0]['celsius'] == 43.5:
                break
        else:
            self.fail('Telemetry did not update on the existing WebSocket')
        self.assertFalse(any(line.startswith(('start ', 'restart ')) for line in self.operations()))

    def test_missing_invalid_oversized_and_future_samples_are_unavailable(self):
        for content in (None, '{bad', 'null', '[]', '{}', 'x' * 16385,
                        json.dumps({'sampled_monotonic_us': time.monotonic_ns() // 1000 + 60000000})):
            with self.subTest(content=str(content)[:40]):
                if content is not None: self.telemetry.write_text(content)
                self.start_api()
                try:
                    self.assertEqual(self.status()['health']['telemetry'],
                                     {'state': 'unavailable', 'supplies': [], 'temperatures': [], 'voltages': []})
                finally:
                    self.stop_api()

    def test_stale_snapshot_keeps_last_readings_explicitly_stale(self):
        self.publish(sampled_monotonic_us=1); self.start_api()
        value = self.status()['health']['telemetry']
        self.assertEqual(value['state'], 'stale')
        self.assertEqual(value['temperatures'][0]['celsius'], 42)

    def test_independent_failures_types_and_channel_bounds(self):
        self.publish(supplies=[{'label': 'Input', 'power_w': None, 'voltage_v': '12', 'current_a': False}],
                     temperatures=[None, {'label': 'x' * 81, 'celsius': 42}] +
                     [{'label': '<img src=x>', 'celsius': -5}] * 30)
        self.start_api()
        value = self.status()['health']['telemetry']
        self.assertEqual(value['state'], 'live')
        self.assertEqual(value['supplies'][0], {'label': 'Input', 'power_w': None, 'voltage_v': None, 'current_a': None})
        self.assertEqual(len(value['temperatures']), 14)
        self.assertEqual(value['temperatures'][0], {'label': '<img src=x>', 'celsius': -5})

    def test_voltage_only_channels_are_bounded_typed_and_streamed(self):
        self.publish(supplies=[], temperatures=[], voltages=[
            {'label': 'Core', 'voltage_v': .85}, {'label': 'Auxiliary', 'voltage_v': None},
            {'label': 'Reference', 'voltage_v': '1.2'}] + [{'label': 'Rail', 'voltage_v': 1.8}] * 40)
        self.start_api()
        telemetry = self.status()['health']['telemetry']
        self.assertEqual(len(telemetry['voltages']), 32)
        self.assertEqual(telemetry['voltages'][:3], [
            {'label': 'Core', 'voltage_v': .85}, {'label': 'Auxiliary', 'voltage_v': None},
            {'label': 'Reference', 'voltage_v': None}])
        socket = WebSocket(self.port); self.addCleanup(socket.close)
        opcode, payload = socket.frame()
        self.assertEqual(opcode, 1)
        self.assertEqual(json.loads(payload)['health']['telemetry'], telemetry)
        self.assertEqual(telemetry['supplies'], [])
