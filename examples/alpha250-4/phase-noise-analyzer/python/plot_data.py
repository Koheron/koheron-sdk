import sys

from phase_noise_plot import (
    load_segments,
    compute_crossovers,
    plot_publication_trace,
    plot_debug_segments,
)


def main():
    filename = sys.argv[1] if len(sys.argv) > 1 else "phase_noise_segments.npy"

    segments, metadata = load_segments(filename)

    print("Metadata:")
    for key, value in metadata.items():
        print(f"  {key}: {value}")

    segments = sorted(segments, key=lambda seg: seg["f_min"])

    crossovers = compute_crossovers(segments)

    plot_publication_trace(segments, crossovers)

    # Optional:
    # plot_debug_segments(segments, crossovers)


if __name__ == "__main__":
    main()