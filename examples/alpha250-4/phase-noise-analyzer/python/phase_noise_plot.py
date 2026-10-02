import numpy as np
from matplotlib import pyplot as plt


def db_to_linear(y_db):
    return 10.0 ** (y_db / 10.0)


def linear_to_db(y_linear):
    return 10.0 * np.log10(y_linear)


def smoothstep(w):
    w = np.clip(w, 0.0, 1.0)
    return w * w * (3.0 - 2.0 * w)


def blend_db(y_low_db, y_high_db, w):
    w = smoothstep(w)

    p_low = db_to_linear(y_low_db)
    p_high = db_to_linear(y_high_db)

    return linear_to_db((1.0 - w) * p_low + w * p_high)


def find_crossover_by_floor(low_seg, high_seg, margin_db=3.0, search_decades=0.6):
    overlap_min = max(low_seg["f_min"], high_seg["f_min"])
    overlap_max = min(low_seg["f_max"], high_seg["f_max"])

    if overlap_min >= overlap_max:
        return np.sqrt(low_seg["f_max"] * high_seg["f_min"])

    search_min = max(overlap_min, overlap_max / (10.0 ** search_decades))
    search_max = overlap_max

    f_low = low_seg["freqs"]
    y_low = low_seg["smoothed"]

    mask = (
        np.isfinite(y_low)
        & (f_low >= search_min)
        & (f_low <= search_max)
    )

    if not np.any(mask):
        return np.sqrt(search_min * search_max)

    f = f_low[mask]

    y_high = np.interp(
        f,
        high_seg["freqs"],
        high_seg["smoothed"],
        left=np.nan,
        right=np.nan,
    )

    valid = np.isfinite(y_high)
    if not np.any(valid):
        return np.sqrt(search_min * search_max)

    f = f[valid]
    y_low = y_low[mask][valid]
    y_high = y_high[valid]

    better = y_high < y_low - margin_db

    if np.any(better):
        return f[np.argmax(better)]

    diff = np.abs(y_low - y_high)
    return f[np.argmin(diff)]


def compute_crossovers(segments, margin_db=3.0, search_decades=0.6):
    crossovers = []

    for low_seg, high_seg in zip(segments[:-1], segments[1:]):
        fc = find_crossover_by_floor(
            low_seg,
            high_seg,
            margin_db=margin_db,
            search_decades=search_decades,
        )

        crossovers.append(fc)

        print(
            f"Crossover "
            f"{low_seg['min_frequency']} Hz -> "
            f"{high_seg['min_frequency']} Hz: "
            f"{fc:.1f} Hz"
        )

    return crossovers


def build_publication_trace(segments, crossovers, blend_decades=0.12):
    stitched_freqs = []
    stitched_raw = []
    stitched_smooth = []

    for i, seg in enumerate(segments):
        freqs = seg["freqs"]
        raw = seg["phase_noise"]
        smooth = seg["smoothed"]

        raw_left = seg["f_min"] if i == 0 else crossovers[i - 1]
        raw_right = seg["f_max"] if i == len(segments) - 1 else crossovers[i]

        smooth_left = seg["f_min"]
        smooth_right = seg["f_max"]

        if i > 0:
            smooth_left = crossovers[i - 1] * (10.0 ** blend_decades)

        if i < len(segments) - 1:
            smooth_right = crossovers[i] / (10.0 ** blend_decades)

        raw_mask = (
            np.isfinite(freqs)
            & np.isfinite(raw)
            & (freqs >= raw_left)
            & (freqs <= raw_right)
        )

        smooth_mask = (
            np.isfinite(freqs)
            & np.isfinite(smooth)
            & (freqs >= smooth_left)
            & (freqs <= smooth_right)
        )

        stitched_freqs.append(freqs[raw_mask])
        stitched_raw.append(raw[raw_mask])
        stitched_smooth.append(np.full(np.count_nonzero(raw_mask), np.nan))

        stitched_freqs.append(freqs[smooth_mask])
        stitched_raw.append(np.full(np.count_nonzero(smooth_mask), np.nan))
        stitched_smooth.append(smooth[smooth_mask])

        if i < len(segments) - 1:
            next_seg = segments[i + 1]
            fc = crossovers[i]

            blend_min = fc / (10.0 ** blend_decades)
            blend_max = fc * (10.0 ** blend_decades)

            overlap_min = max(seg["f_min"], next_seg["f_min"], blend_min)
            overlap_max = min(seg["f_max"], next_seg["f_max"], blend_max)

            if overlap_min >= overlap_max:
                continue

            blend_mask = (
                np.isfinite(freqs)
                & np.isfinite(smooth)
                & (freqs >= overlap_min)
                & (freqs <= overlap_max)
            )

            f_blend = freqs[blend_mask]

            if f_blend.size == 0:
                continue

            y_low = smooth[blend_mask]

            y_high = np.interp(
                f_blend,
                next_seg["freqs"],
                next_seg["smoothed"],
                left=np.nan,
                right=np.nan,
            )

            valid = np.isfinite(y_low) & np.isfinite(y_high)

            if not np.any(valid):
                continue

            f_blend = f_blend[valid]
            y_low = y_low[valid]
            y_high = y_high[valid]

            log_f = np.log10(f_blend)
            log_min = np.log10(overlap_min)
            log_max = np.log10(overlap_max)

            w = (log_f - log_min) / (log_max - log_min)
            y_blend = blend_db(y_low, y_high, w)

            stitched_freqs.append(f_blend)
            stitched_raw.append(np.full_like(y_blend, np.nan))
            stitched_smooth.append(y_blend)

    f = np.concatenate(stitched_freqs)
    y_raw = np.concatenate(stitched_raw)
    y_smooth = np.concatenate(stitched_smooth)

    order = np.argsort(f)

    return f[order], y_raw[order], y_smooth[order]


def plot_publication_trace(
    segments,
    crossovers=None,
    blend_decades=0.12,
    raw_color="0.35",
    raw_alpha=0.25,
):
    if crossovers is None:
        crossovers = compute_crossovers(segments)

    f, y_raw, y_smooth = build_publication_trace(
        segments,
        crossovers,
        blend_decades=blend_decades,
    )

    ax = plt.subplot(111)

    raw_mask = np.isfinite(y_raw)
    smooth_mask = np.isfinite(y_smooth)

    ax.semilogx(
        f[raw_mask],
        y_raw[raw_mask],
        linewidth=0.8,
        alpha=raw_alpha,
        color=raw_color,
    )

    ax.semilogx(
        f[smooth_mask],
        y_smooth[smooth_mask],
        linewidth=2.0,
        color="black",
        label="Phase noise",
    )

    ax.set_xlabel("FREQUENCY (Hz)")
    ax.set_ylabel("PHASE NOISE (dBc/Hz)")
    ax.set_xlim(segments[0]["f_min"], segments[-1]["f_max"])

    ax.grid(True, which="major", linestyle="-", linewidth=1.5, color="0.35")
    ax.grid(True, which="minor", linestyle="-", linewidth=0.8, color="0.35")

    # ax.legend()
    plt.show()


def plot_debug_segments(segments, crossovers=None):
    if crossovers is None:
        crossovers = compute_crossovers(segments)

    ax = plt.subplot(111)

    for seg in segments:
        freqs = seg["freqs"]

        raw_mask = np.isfinite(seg["phase_noise"])
        smooth_mask = np.isfinite(seg["smoothed"])

        ax.semilogx(
            freqs[raw_mask],
            seg["phase_noise"][raw_mask],
            linewidth=1,
            alpha=0.18,
        )

        ax.semilogx(
            freqs[smooth_mask],
            seg["smoothed"][smooth_mask],
            linewidth=2,
            label=f"min frequency {seg['min_frequency']} Hz",
        )

    for fc in crossovers:
        ax.axvline(fc, color="red", linestyle="--", linewidth=1)

    ax.set_xlabel("FREQUENCY (Hz)")
    ax.set_ylabel("PHASE NOISE (dBc/Hz)")
    ax.set_xlim(segments[0]["f_min"], segments[-1]["f_max"])

    ax.grid(True, which="major", linestyle="-", linewidth=1.5, color="0.35")
    ax.grid(True, which="minor", linestyle="-", color="0.35")

    ax.legend()
    plt.show()


def save_segments(filename, segments, metadata=None):
    payload = {
        "segments": segments,
        "metadata": metadata or {},
    }

    np.save(filename, payload, allow_pickle=True)
    print(f"Saved data to {filename}")


def load_segments(filename):
    payload = np.load(filename, allow_pickle=True).item()
    return payload["segments"], payload.get("metadata", {})