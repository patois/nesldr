	
	Nintendo Entertainment System (NES) loader module
	------------------------------------------------------
	Copyright 2006, Dennis Elser (dennis@backtrace.de)


	IDA Pro loader module for Nintendo Enternainment System
  (NES) ROM images in iNES file format.

  You need IDA Pro 4.9 at least in order to be able to
  use this loader. For installation instructions, please
  read below.

  This loader tells IDA Pro how to correctly load
	iNES ROM images. The disassembly is done by the
	M6502 processor module coming along with IDA Pro,
  which is compatible with the slightly modified
  cpu of the NES.
	
	Please note that the real NES hardware memory area
	ends after 0xFFFF as it can address 16bit only.
	The NES uses several page/bank swapping techniques in
  order to load additional ROM banks. However, this
  loader reads at most two 16k PRG ROM banks into the
  IDA database, because it takes care of the original
  memory layout.
	This significantly improves the disassembly, but currently
  doesn't allow to reassemble the disassembly.

  By the way: if you want the IDA wizard to show up
  the NES loader under the "Consoles" tab, you can edit
  the following files:
  
    file: %idadir%/cfg/templates.xml
    add the following lines below under the "Consoles" category:
    
      <template idc:icon="I_ROM" extensions="ROM_NES" loader="nes" processor="m6502">
        <xi:include xpointer="xpointer(/ida/templates/wizard[@X='default_w'])"/>
      </template>
    
    file: %idadir%/cfg/idagui.cfg
    add the following line below under "FILE_EXTENSIONS":
      
      ROM_NES, ".nes Nintendo NES ROM Images", "*.nes"
      
      
	Copy compiled loader to %idadir%/loaders/nes.ldw
	
	-------------------------------------------------------------------      
      
  Note to programmers:
  
  The whole file is stored within the IDA Pro database
  by the loader. You can access all parts of the original
  file using netnodes.
  
  Example:
  
  The original iNES header is stored in a netnode
  (INES_HDR_NODE). You can access the header from a
  plugin by reading the netnode's blob as shown below.
	

	Example code:

		#include "nes.h"

		netnode node(INES_HDR_NODE);
		ines_hdr hdr;

		node.getblob(&hdr, &INES_HDR_SIZE, 0, 'I');
      
      
      
  (c) 2006, Dennis Elser      