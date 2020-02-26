
# NESLdr

IDA Pro loader module for Nintendo Enternainment System (NES) ROM images.

## Requirements

This loader was written for and works with IDA 4.9. Support for newer IDA versions (>7.x)
is covered with the following repository which contains a port of this loader to IDAPython:

https://github.com/Jinmo/nesldr-py

## Installation

Copy compiled loader to %idadir%/loaders/nes.ldw

## Usage/Restrictions

With the loader installed, compatible files can be opened and disassembled with IDA.

Since the original NES hardware can address up to 16bit of memory, it uses page/bank swapping
mechanisms for larger ROM files. However, this loader reads at most two 16k PRG ROM banks into
the IDA database, in order to adhere to the original memory layout.

Doing so improves the disassembly but for certain ROMs it currently doesn't allow the entire ROM
to be loaded into a single IDA database. As a "workaround", a separate plugin has been developed
which simulates the bank/page swapping mechanism:

https://github.com/patois/bankswitch

## Development

This loader stores the entire ROM file within netnodes of the IDA Pro database.

As an example, the following example code shows how the original iNES header
can be accessed using the INES_HDR_NODE constant. 

```
#include "nes.h"

netnode node(INES_HDR_NODE);
ines_hdr hdr;

node.getblob(&hdr, &INES_HDR_SIZE, 0, 'I');
```

## Author

Dennis Elser
