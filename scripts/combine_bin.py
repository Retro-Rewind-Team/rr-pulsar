# Simple script to create Pulsar's expected combined binary output without
# using a modified version of kamek

from argparse import ArgumentParser

parser = ArgumentParser()
parser.add_argument("-p", "--pal", dest="pal",
                    help="pal binary", metavar="FILE")
parser.add_argument("-e", "--ntsc", dest="ntsc",
                    help="ntsc binary", metavar="FILE")
parser.add_argument("-j", "--jp", dest="jp",
                    help="jp binary", metavar="FILE")
parser.add_argument("-k", "--kr", dest="kr",
                    help="kr binary", metavar="FILE")
parser.add_argument("-o", "--output", dest="output",
                    help="combined output destiniation", metavar="FILE")

args = parser.parse_args()

names = ["P", "E", "J", "K"]
binaries = [None] * 4


def add_region_bin(fname, idx):
    if fname is None:
        return

    file = open(fname, "rb")
    content = file.read()

    binaries[idx] = content

    file.close()


add_region_bin(args.pal, 0)
add_region_bin(args.ntsc, 1)
add_region_bin(args.jp, 2)
add_region_bin(args.kr, 3)

out_file = open(args.output, "wb")

# Header is just each section's length in the order of P, N, J, K
for idx, binary in enumerate(binaries):
    if binary is not None:
        bin_length = len(binary)
        print(f"combining version {names[idx]} ({bin_length})...")
        out_file.write(bin_length.to_bytes(4))
    else:
        print(f"skipping version {names[idx]}...")
        out_file.write((0).to_bytes(4))

for binary in binaries:
    if binary is None:
        continue

    out_file.write(binary)

print(f"writing {args.output}")
out_file.close()
