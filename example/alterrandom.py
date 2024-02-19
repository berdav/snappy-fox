import sys
import random

prob = int(sys.argv[3])
substbyte = int(sys.argv[4])

with open(sys.argv[1], "rb") as f:
	indata = bytearray(f.read())

for b in range(len(indata)):
	x = random.randint(0, 10000)
	if x < prob:
		# Alter the data
		indata[b] = substbyte

with open(sys.argv[2], "wb") as f:
	f.write(indata)

