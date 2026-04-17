import argparse
import csv
import serial
import matplotlib.pyplot as pyplot

parser = argparse.ArgumentParser()
parser.add_argument('device', help='Serial port, e.g. /dev/ttyUSB0')
parser.add_argument('--baud', type=int, default=115200)
parser.add_argument('--csv', default='sweep_data.csv', help='CSV output path for Excel import')
args = parser.parse_args()

port = serial.Serial(args.device, baudrate=args.baud)
port.reset_input_buffer()

print('Waiting for four sweeps. Press the board button when prompted.')
sweeps = {}
current_sweep = None
current_label = None

while True:
    line = port.readline().decode().strip()
    if not line:
        continue

    print(line)

    if line == 'SESSION COMPLETE':
        break

    if line.startswith('START '):
        parts = line.split()
        current_sweep = int(parts[1])
        current_label = parts[2] if len(parts) > 2 else f'SWEEP_{current_sweep}'
        sweeps[current_sweep] = {
            'label': current_label,
            'frequencies': [],
            'magnitudes': [],
        }
        continue

    if line.startswith('END '):
        current_sweep = None
        current_label = None
        continue

    if line.startswith('PROGRAM STARTED') or line.startswith('FOUR-SWEEP SESSION READY'):
        continue

    if line.startswith('SET LOAD TO '):
        continue

    if current_sweep is None:
        continue

    frequency, magnitude = line.split()
    sweeps[current_sweep]['frequencies'].append(int(frequency))
    sweeps[current_sweep]['magnitudes'].append(int(magnitude))

port.close()

with open(args.csv, 'w', newline='') as csv_file:
    writer = csv.writer(csv_file)
    writer.writerow(['sweep_id', 'label', 'frequency_hz', 'magnitude'])
    for sweep_id in sorted(sweeps):
        label = sweeps[sweep_id]['label']
        frequencies = sweeps[sweep_id]['frequencies']
        magnitudes = sweeps[sweep_id]['magnitudes']
        for frequency, magnitude in zip(frequencies, magnitudes):
            writer.writerow([sweep_id, label, frequency, magnitude])

print(f'Saved CSV data to {args.csv}')

for sweep_id in sorted(sweeps):
    label = sweeps[sweep_id]['label'].title()
    frequencies = sweeps[sweep_id]['frequencies']
    magnitudes = sweeps[sweep_id]['magnitudes']
    pyplot.plot(frequencies, magnitudes, label=label)
pyplot.xlabel('Frequency (Hz)')
pyplot.ylabel('Magnitude (ADC counts)')
pyplot.title('Open / Short / Match / Antenna Sweep Response')
pyplot.legend()
pyplot.tight_layout()
pyplot.show()
