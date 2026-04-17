import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as pyplot


def load_sweep(csv_path):
    frequencies = []
    magnitudes = []

    with open(csv_path, newline='') as csv_file:
        reader = csv.DictReader(csv_file)
        for row in reader:
            frequencies.append(int(row["frequency_hz"]))
            magnitudes.append(int(row["magnitude"]))

    return frequencies, magnitudes


def subtract_series(reference, target):
    return [ref - value for ref, value in zip(reference, target)]


parser = argparse.ArgumentParser()
parser.add_argument(
    "--dir",
    default=".",
    help="Directory containing the sweep CSV files",
)
args = parser.parse_args()

base_dir = Path(args.dir)

all_sweeps = [
    ("Sweep 1", base_dir / "my_sweep.csv"),
    ("Sweep 2", base_dir / "my_sweep2.csv"),
    ("Comp 1", base_dir / "my_sweepcomp.csv"),
    ("Comp 2", base_dir / "my_sweepcomp2.csv"),
    ("Antenna", base_dir / "my_sweepcompAntennaComp.csv"),
    ("Match", base_dir / "my_sweepcompMatch.csv"),
    ("Short", base_dir / "my_sweepcompShort.csv"),
]

loaded = {}
for label, path in all_sweeps:
    frequencies, magnitudes = load_sweep(path)
    loaded[label] = (frequencies, magnitudes)

pyplot.figure()
for label, (frequencies, magnitudes) in loaded.items():
    pyplot.plot(frequencies, magnitudes, label=label)
pyplot.xlabel("Frequency (Hz)")
pyplot.ylabel("Magnitude (ADC counts)")
pyplot.title("All Sweep Data")
pyplot.legend()
pyplot.tight_layout()

match_frequencies, match_magnitudes = loaded["Match"]
_, short_magnitudes = loaded["Short"]
_, antenna_magnitudes = loaded["Antenna"]

match_minus_short = subtract_series(match_magnitudes, short_magnitudes)
match_minus_antenna = subtract_series(match_magnitudes, antenna_magnitudes)

pyplot.figure()
pyplot.plot(match_frequencies, match_minus_short, label="Match - Short")
pyplot.plot(match_frequencies, match_minus_antenna, label="Match - Antenna")
pyplot.xlabel("Frequency (Hz)")
pyplot.ylabel("Magnitude Difference (ADC counts)")
pyplot.title("Difference Sweeps")
pyplot.legend()
pyplot.tight_layout()

pyplot.show()
