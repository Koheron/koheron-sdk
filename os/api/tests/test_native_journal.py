"""Persistent log batches against real, private journal files; host tests only."""
import json
from pathlib import Path
import select
import struct
import subprocess
import tempfile
import time
import unittest

from native_fixture import BIN_DIR

UNIT = 'koheron-journal-test.service'
RUN_A, RUN_B = 'a' * 32, 'b' * 32
BOOT = Path('/proc/sys/kernel/random/boot_id').read_text().strip().replace('-', '')
REMOTE = '/usr/lib/systemd/systemd-journal-remote'


class NativeJournalTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='koheron-journal-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.directory = self.root / 'journal'; self.directory.mkdir()
        self.file = self.directory / 'test.journal'
        self.sequence = 0

    def write(self, messages, invocation=RUN_A, priority=6):
        data = bytearray()
        for message in messages:
            self.sequence += 1
            timestamp = 1700000000000000 + self.sequence
            data.extend((f'__REALTIME_TIMESTAMP={timestamp}\n__MONOTONIC_TIMESTAMP={self.sequence}\n'
                         f'_BOOT_ID={BOOT}\n_MACHINE_ID={"c" * 32}\n'
                         f'_SYSTEMD_UNIT={UNIT}\n_SYSTEMD_INVOCATION_ID={invocation}\n'
                         f'PRIORITY={priority}\n').encode())
            payload = message.encode() if isinstance(message, str) else message
            data.extend(b'MESSAGE\n' + struct.pack('<Q', len(payload)) + payload + b'\n\n')
        result = subprocess.run([REMOTE, '--split-mode=none', '--output=' + str(self.file), '-'],
                                input=data, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr.decode())

    def reader(self, cursor='', invocation=''):
        process = subprocess.Popen([str(BIN_DIR / 'journal-probe'), str(self.directory), cursor, invocation],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   text=True, encoding='utf-8')
        def close():
            process.stdin.close()
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait(timeout=5)
            errors = process.stderr.read()
            process.stdout.close(); process.stderr.close()
            self.assertEqual(process.returncode, 0, errors)
        self.addCleanup(close)
        return process

    def batch(self, reader):
        reader.stdin.write('\n'); reader.stdin.flush()
        self.assertTrue(select.select([reader.stdout], [], [], 5)[0], 'Journal reader stalled')
        line = reader.stdout.readline()
        self.assertTrue(line, 'Journal reader stopped')
        self.assertLess(len(line.encode()), 65536)
        value = json.loads(line)
        if value:
            self.assertLessEqual(len(value['entries']), 200)
        return value

    def messages(self, batch):
        return [entry['msg'] for entry in batch['entries']]

    def wait_batch(self, reader):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            value = self.batch(reader)
            if value and (value['entries'] or value['reset']): return value
            time.sleep(.01)
        self.fail('No journal change observed')

    def test_initial_tail_is_latest_200_in_order(self):
        self.write([str(i) for i in range(230)])
        reader = self.reader()
        value = self.batch(reader)
        self.assertEqual(self.messages(value), [str(i) for i in range(30, 230)])
        self.assertFalse(value['reset'])
        self.assertIsNone(self.batch(reader))

    def test_backlog_is_drained_without_gaps_or_repeated_cursor(self):
        self.write(['seed'])
        reader = self.reader()
        seed = self.batch(reader)
        self.write([str(i) for i in range(430)])
        values = [self.wait_batch(reader) for _ in range(3)]
        self.assertEqual([len(value['entries']) for value in values], [200, 200, 30])
        self.assertEqual([msg for value in values for msg in self.messages(value)], [str(i) for i in range(430)])
        self.assertEqual(len({seed['cursor'], *(value['cursor'] for value in values)}), 4)
        self.assertTrue(all(not value['reset'] for value in values))
        resumed = self.reader(seed['cursor'])
        self.assertEqual(self.messages(self.batch(resumed)), [str(i) for i in range(200)])

    def test_truncation_is_explicit_and_keeps_complete_utf8(self):
        self.write(['x' * 4096, 'y' * 4097, 'z' * 4095 + '€', '€' * 1366])
        entries = self.batch(self.reader())['entries']
        self.assertEqual([entry['truncated'] for entry in entries], [False, True, True, True])
        self.assertEqual([entry['msg'] for entry in entries], ['x' * 4096, 'y' * 4096, 'z' * 4095, '€' * 1365])

    def test_instrument_labels_recover_severity_without_weakening_journal_priority(self):
        self.write(['PANIC: stopped', 'CRITICAL: session', 'ERROR: failed', 'WARNING: late', 'ready',
                    'contains ERROR: text', 'ERRORISH: text'])
        self.write(['ERROR: already urgent'], priority=1)
        entries = self.batch(self.reader())['entries']
        self.assertEqual([entry['prio'] for entry in entries], [0, 2, 3, 4, 6, 6, 6, 1])

    def test_json_escaping_budget_keeps_pending_entries(self):
        messages = [('\x01' * 4000) + str(i) for i in range(7)]
        self.write(messages)
        reader = self.reader()
        values = [self.batch(reader) for _ in range(4)]
        self.assertEqual([len(value['entries']) for value in values], [2, 2, 2, 1])
        self.assertEqual([msg for value in values for msg in self.messages(value)], messages)
        self.assertIsNone(self.batch(reader))

    def test_invocation_filter_never_falls_back_to_other_runs(self):
        self.write(['previous'], RUN_A); self.write(['current'], RUN_B)
        self.assertEqual(self.messages(self.batch(self.reader(invocation=RUN_B))), ['current'])
        empty = self.batch(self.reader(invocation='d' * 32))
        self.assertEqual(empty['entries'], []); self.assertIsNone(empty['cursor'])
        self.assertFalse(empty['reset'])
        old_cursor = self.batch(self.reader(invocation=RUN_A))['cursor']
        changed = self.batch(self.reader(cursor=old_cursor, invocation=RUN_B))
        self.assertTrue(changed['reset']); self.assertEqual(self.messages(changed), ['current'])

    def test_expired_cursor_resets_to_recent_history(self):
        self.write(['recent'])
        value = self.batch(self.reader(cursor='missing'))
        self.assertTrue(value['reset']); self.assertEqual(self.messages(value), ['recent'])

    def test_rotation_keeps_a_retained_cursor_and_delivers_new_file(self):
        self.write(['before'])
        reader = self.reader(); self.batch(reader)
        self.file.rename(self.directory / 'retained.journal')
        self.write(['after'])
        value = self.wait_batch(reader)
        self.assertFalse(value['reset']); self.assertEqual(self.messages(value), ['after'])
        self.assertIsNone(self.batch(reader))

    def test_vacuum_reports_lost_history_then_continues(self):
        self.write(['removed'])
        reader = self.reader(); self.batch(reader)
        self.file.rename(self.root / 'removed.journal')
        self.write(['recent'])
        value = self.wait_batch(reader)
        self.assertTrue(value['reset']); self.assertEqual(self.messages(value), ['recent'])
        self.write(['next'])
        self.assertEqual(self.messages(self.wait_batch(reader)), ['next'])


if __name__ == '__main__':
    unittest.main()
