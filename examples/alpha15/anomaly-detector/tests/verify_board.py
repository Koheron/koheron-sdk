#!/usr/bin/env python3
"""Exercise the looped-back board, including a captured event across DDR wrap."""
import argparse
import time
from koheron import command, connect

RATE = 15_000_000
RING = 33_554_432


class AnomalyDetector:
    def __init__(self, client):
        self.client = client

    @command()
    def get_state(self):
        return self.client.recv_tuple('I' * 13)

    @command()
    def get_live(self):
        return self.client.recv_vector(dtype='uint32')

    @command()
    def get_event(self, start, span):
        return self.client.recv_vector(dtype='uint32')

    @command()
    def get_event_raw(self, start, count):
        return self.client.recv_vector(dtype='uint32')

    @command()
    def get_model(self):
        return self.client.recv_vector(dtype='uint32')

    @command()
    def get_diagnostics(self):
        return self.client.recv_vector(dtype='uint32')

    @command()
    def get_live_trace(self):
        return self.client.recv_vector(dtype='uint32')

    @command()
    def set_threshold(self, value):
        return self.client.recv_uint32()

    @command()
    def learn(self):
        return self.client.recv_uint32()

    @command()
    def set_learning(self, enabled):
        return self.client.recv_uint32()

    @command()
    def arm(self):
        return self.client.recv_uint32()

    @command()
    def inject(self):
        return self.client.recv_uint32()

    @command()
    def try_again(self):
        return self.client.recv_uint32()


def wait_for(device, predicate, seconds, label):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        state = device.get_state()
        if state[1]:
            raise AssertionError(f'Board acquisition error {state[1]} while {label}: {state}')
        if predicate(state):
            return state
        time.sleep(.1)
    raise AssertionError(f'Timed out waiting for {label}: {device.get_state()}')


def signed32(value):
    return int(value) - (1 << 32) if value & (1 << 31) else int(value)


def signed_adc(word):
    raw = int(word) & 0x3ffff
    return raw - 262144 if raw & 0x20000 else raw


def predict(history, weights):
    total = 0
    for lag in range(4):
        q = signed_adc(history[-1-lag]) >> 2
        total += max(0, q) * signed32(weights[2*lag])
        total += max(0, -q) * signed32(weights[2*lag+1])
    return max(-131072, min(131071, ((total >> 12) + signed32(weights[8])) * 4))


def check_event(device, state, weights):
    assert state[0] == 5, state
    start = RATE - 256
    raw = device.get_event_raw(start, 1024)
    assert len(raw) == 1024
    first_sequence = (state[6] - RATE + start) & 0xffffffff
    for i, word in enumerate(raw):
        assert int(word) >> 18 == ((first_sequence + i) & 0x3fff), f'tag at {i}'
    errors = []
    for i in range(4, len(raw)):
        errors.append(abs(signed_adc(raw[i]) - predict(raw[i-4:i], weights)))
    trigger_error = errors[256 - 4]
    assert trigger_error > state[4], (trigger_error, state[4])
    # The on-board scan checked every sample. Independently check the sequence
    # where this event crosses the physical end of the 128 MiB ring.
    ring_start = (state[6] - RATE) % RING
    wrap = (RING - ring_start) % RING
    if 128 <= wrap < 2 * RATE - 128:
        crossing = device.get_event_raw(wrap - 128, 256)
        first = (state[6] - RATE + wrap - 128) & 0xffffffff
        for i, word in enumerate(crossing):
            assert int(word) >> 18 == ((first+i) & 0x3fff), f'wrap tag at {i}'
    overview = device.get_event(0, 2 * RATE)
    assert len(overview) == 2000
    print(f'event valid: trigger sequence {state[6]}, trigger error {trigger_error} counts, '
          f'threshold {state[4]}, ring boundary at event sample {wrap}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host')
    parser.add_argument('--normal-seconds', type=float, default=10)
    args = parser.parse_args()
    device = AnomalyDetector(connect(args.host, name='anomaly-detector'))
    warm = wait_for(device, lambda s: s[2] * 131072 >= RATE + 131072, 10, 'normal history')
    live = device.get_live()
    assert len(live) == 2000 and max(live) - min(live) > 1000, 'DAC0→ADC0 loopback is missing'
    print('loopback amplitude:', max(live) - min(live), 'ADC counts')
    assert device.learn() == 0, 'learning failed'
    learned = wait_for(device, lambda s: s[9] == 1, 20, 'first adaptive fit')
    for _ in range(5):
        diagnostics = device.get_diagnostics()
        weights = device.get_model()
        trace = device.get_live_trace()
        if device.get_diagnostics()[1] == diagnostics[1]:
            break
    else:
        raise AssertionError('Model changed during live reference read')
    assert len(trace) == 3013 and len(diagnostics) == 26
    assert diagnostics[0] == 1 and diagnostics[1] >= 1 and diagnostics[2] > 10000
    assert max(trace[:1000]) - min(trace[:1000]) > 1000
    for i in range(1000):
        assert trace[2000+i] == abs(int(trace[i])-int(trace[1000+i])), 'live residual mismatch'
    recent = [int(trace[3000+3*lag]) & 0x3ffff for lag in range(4)]
    assert signed32(trace[3012]) == predict(recent[::-1], weights), 'live predictor mismatch'
    print('holdout p99.9:', learned[11], 'suggested threshold:', learned[12],
          'adaptive updates:', diagnostics[1], 'weights:', weights)
    assert len(weights) == 9 and learned[9] == 1
    adjusted = min(131071, learned[12] + 100)
    assert device.set_threshold(adjusted) == 0
    assert device.get_state()[4] == adjusted
    assert device.set_threshold(learned[12]) == 0
    assert device.arm() == 0
    wait_for(device, lambda s: s[0] == 3, 5, 'ready')
    end = time.monotonic() + args.normal_seconds
    while time.monotonic() < end:
        state = device.get_state()
        assert state[0] == 3, f'false trigger during normal operation: {state}'
        time.sleep(.2)
    print(f'normal operation: no trigger for {args.normal_seconds:g} s')
    assert device.get_diagnostics()[1] > diagnostics[1], 'model did not adapt during detection'
    assert device.inject() == 0
    event = wait_for(device, lambda s: s[0] == 5, 20, 'first event')
    check_event(device, event, device.get_model())
    assert device.inject() != 0 and device.get_state()[6] == event[6], 'held event was replaced'
    assert device.try_again() == 0
    wait_for(device, lambda s: s[0] == 3, 5, 'second ready')
    # Make the DMA writer lap the whole 128 MiB ring before the next trigger.
    time.sleep(3)
    assert device.get_state()[0] == 3, 'unexpected trigger before second injection'
    assert device.inject() == 0
    event = wait_for(device, lambda s: s[0] == 5, 20, 'second event after wrap')
    check_event(device, event, device.get_model())
    assert device.try_again() == 0
    wait_for(device, lambda s: s[2] * 131072 >= RATE + 131072, 5, 'new training data')
    previous_updates = device.get_diagnostics()[1]
    deadline = time.monotonic() + 10
    while device.get_diagnostics()[1] <= previous_updates and time.monotonic() < deadline:
        time.sleep(.2)
    assert device.get_diagnostics()[1] > previous_updates, 'runtime retraining failed'
    wait_for(device, lambda s: s[0] == 3, 5, 'ready after retraining')
    assert device.inject() == 0
    event = wait_for(device, lambda s: s[0] == 5, 20, 'event after runtime retraining')
    check_event(device, event, device.get_model())
    assert device.set_learning(0) == 0
    assert device.get_diagnostics()[0] == 0
    print('PASS: loopback, live trace, adaptive training during detection, '
          'quiet normal operation, three verified events, DDR wrap and pause')


if __name__ == '__main__':
    main()
