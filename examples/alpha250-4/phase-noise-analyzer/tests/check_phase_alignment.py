"""Board regression: split sinusoidal PM into IN1/IN3; reference into IN0/IN2.

Does not control the signal sources. Temporarily changes CIC rate and channel,
then restores acquisition settings. Keep the AWG settings fixed for the run.
"""
import argparse
import json
from pathlib import Path
import sys
import time
import numpy as np
from koheron import KoheronClient, command

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'python'))
from phase_noise_analyzer import PhaseNoiseAnalyzer as Base

class PhaseNoiseAnalyzer(Base):
    @command()
    def set_fft_navg(self, count):
        pass

def check(host, modulation_hz, peak_deg, rates, output, snapshots, averages):
    client = KoheronClient(host)
    client.sock.settimeout(15)
    analyzer = PhaseNoiseAnalyzer(client)
    initial = analyzer.get_parameters()
    initial_rate = round(100e6 / initial[1])
    output.mkdir(parents=True, exist_ok=True)
    results = []
    try:
        analyzer.set_channel(2)
        for rate in rates:
            analyzer.set_cic_rate(rate)
            time.sleep(1)
            parameters = analyzer.get_parameters()
            fs = parameters[1]
            t = np.arange(32000) / fs
            basis = np.column_stack((np.ones(t.size), t - t.mean(),
                                     np.sin(2*np.pi*modulation_hz*t),
                                     np.cos(2*np.pi*modulation_hz*t)))
            frames = []
            peaks = []
            phases = []
            deadline = time.monotonic() + 15
            while len(frames) < snapshots and time.monotonic() < deadline:
                pair = np.array(analyzer.get_phase_xy_sync())[:, :32000]
                if not frames or not np.array_equal(pair, frames[-1]):
                    coefficients = np.linalg.lstsq(basis, pair.T.astype(float), rcond=None)[0]
                    peaks.append(np.rad2deg(np.hypot(coefficients[2], coefficients[3])))
                    phases.append(np.angle(np.exp(1j*(np.arctan2(coefficients[3,0], coefficients[2,0]) -
                                                      np.arctan2(coefficients[3,1], coefficients[2,1])))))
                    frames.append(pair)
                time.sleep(.12)
            if len(frames) != snapshots:
                raise RuntimeError('Acquisition did not provide fresh phase pairs')
            analyzer.reset_cumulative_averager()
            time.sleep(.25)
            deadline = time.monotonic() + 30
            while analyzer.get_parameters()[-1] < averages and time.monotonic() < deadline:
                time.sleep(.2)
            parameters = analyzer.get_parameters()
            psd = analyzer.get_phase_noise().astype(float)
            df = fs / 30000
            frequencies = np.arange(psd.size) * df
            tone = np.abs(frequencies - modulation_hz) < max(250, 5*df)
            power = float(np.sum(psd[tone]) * df)
            mean_peak = np.mean(peaks, axis=0)
            phase_difference = float(np.rad2deg(np.angle(np.mean(np.exp(1j*np.array(phases))))))
            expected_power = np.deg2rad(peak_deg)**2 / 2
            result = dict(cic_rate=rate,fs=fs,mean_peak_deg=mean_peak.tolist(),
                          max_abs_phase_difference_deg=float(np.rad2deg(np.max(np.abs(phases)))),
                          mean_phase_difference_deg=phase_difference,
                          cross_tone_power_ratio=power/expected_power,
                          cross_equivalent_peak_deg=float(np.rad2deg(np.sqrt(power*2))) if power > 0 else None,
                          cross_averages=int(parameters[-1]))
            result['passed'] = bool(np.max(np.abs(mean_peak/peak_deg-1)) < .02 and
                                    result['max_abs_phase_difference_deg'] < 1 and
                                    .96 < result['cross_tone_power_ratio'] < 1.04 and
                                    result['cross_averages'] >= averages)
            results.append(result)
            np.savez_compressed(output/f'cic-{rate}-{len(results)}.npz', phase=frames,
                                parameters=parameters,psd=psd)
            (output/'summary.json').write_text(json.dumps(results,indent=2))
            print(json.dumps(result),flush=True)
    finally:
        analyzer.set_cic_rate(initial_rate)
        analyzer.set_channel(initial[2])
        analyzer.set_fft_navg(initial[4])
        client.sock.close()
    return all(result['passed'] for result in results)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host')
    parser.add_argument('--modulation-hz',type=float,default=10000)
    parser.add_argument('--peak-deg',type=float,default=1)
    parser.add_argument('--cic-rates',type=int,nargs='+',default=[20,67,100,133,67,133])
    parser.add_argument('--snapshots',type=int,default=12)
    parser.add_argument('--averages',type=int,default=100)
    parser.add_argument('--output',type=Path,default=Path('tmp/tests/pna-board-alignment'))
    args = parser.parse_args()
    if args.modulation_hz <= 0 or args.peak_deg <= 0 or args.snapshots < 1 or args.averages < 1:
        parser.error('Modulation frequency, phase deviation, snapshots and averages must be positive')
    if any(rate < 4 or rate > 8192 for rate in args.cic_rates):
        parser.error('CIC rates must be between 4 and 8192')
    sys.exit(0 if check(args.host,args.modulation_hz,args.peak_deg,args.cic_rates,
                       args.output,args.snapshots,args.averages) else 1)
