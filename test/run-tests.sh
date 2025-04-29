#!/bin/sh

set -eu

test000() {
	echo "[Test 000  ] check compilation"
	cd ..
	# Standard compilation
	echo "[Test 000 a] Debug compilation "
	make CFLAGS=-DDEBUG clean all
	file ./snappy-fox | grep -q ELF
	echo "[Test 000 b] Normal compilation "
	make clean all
	file ./snappy-fox | grep -q ELF
	echo "[Test 000  ] ok"
}

test001() {
	echo "[Test 001  ]check standard framed run"
	cd ..
	echo "[Test 001 a] normal image"
	./snappy-fox example/exampleimage.snappy example/exampleimage.jpg
	file example/exampleimage.jpg | grep -q JPEG

	# The example image is CRC corrupted (it uses firefox CRCs)
	echo "[Test 001 b] CRC Corruption"
	! ./snappy-fox --consider_crc_errors  \
		example/exampleimage.snappy example/exampleimage.jpg

	echo "[Test 001 b] Firefox CRC Test"
	./snappy-fox --consider_crc_errors --firefox \
		example/exampleimage.snappy example/exampleimage.jpg

	# Test on the corrupted image
	echo "[Test 001 c] Corrupted image"
	! ./snappy-fox \
		example/alteredimage.snappy example/alteredimage.jpg
	echo "[Test 001 c] Corrupted image ignoring offset errors"
	./snappy-fox --ignore_offset_errors \
		example/alteredimage.snappy example/alteredimage.jpg
	echo "[Test 001 c] Corrupted metadata image"
	! ./snappy-fox \
		example/nomagic.snappy example/nomagic.jpg
	echo "[Test 001 c] Corrupted metadata image ignoring magic"
	./snappy-fox --ignore_magic \
		example/nomagic.snappy example/nomagic.jpg
	echo "[Test 001  ] ok"
}

( test000 )
( test001 )
