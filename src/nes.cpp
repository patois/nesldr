/*

	Nintendo Entertainment System (NES) loader module
	------------------------------------------------------
	Copyright 2006, Dennis Elser (dennis@backtrace.de)


	IDA Pro loader module for Nintendo Enternainment System
    (NES) ROM images in iNES file format.


	todo list:
	----------
    - detect write access to PRG ROM and EXP ROM area and
      autocomment it as bank swapping mechanism ?

    - implement a bank-switching-plugin?

    - further division of several memory segments into
      "subsegments" ?


	license:
	--------

	- freeware, copyright messages have to stay intact.



	Copy compiled loader to idadir%/loaders/nes.ldw


    Please send me an e-mail if you find any bugs!

	(c) 2006, Dennis Elser (dennis@backtrace.de)

*/


#include "../idaldr.h"
#include "nes.h"
#include "ioregs.h"
#include "mappers.h"

#include <moves.hpp>
#include <bytes.hpp>


#define YES_NO( condition ) ( condition ? "yes" : "no" )


static ines_hdr hdr;






//----------------------------------------------------------------------
//
//      check input file format. if recognized, then return 1
//      and fill 'fileformatname'.
//      otherwise return 0
//
int accept_file(linput_t *li, char fileformatname[MAX_FILE_FORMAT_NAME], int n)
{
	if( n!= 0 )
		return 0;

	// quit if file is smaller than size of iNes header
	if (qlsize(li) < sizeof(ines_hdr))
		return 0;

	// set filepos to offset 0
	qlseek(li, 0, SEEK_SET);

	// read NES header
	if(qlread(li, &hdr, INES_HDR_SIZE) != INES_HDR_SIZE)
		return 0;

	// is it a valid ROM image in iNes format?
	if( memcmp("NES", &hdr.id, sizeof(hdr.id)) != 0 || hdr.term != 0x1A )
		return 0;

	// this is the name of the file format which will be
	// displayed in IDA's dialog
	qstrncpy(fileformatname, "Nintendo Entertainment System ROM", MAX_FILE_FORMAT_NAME);

	// set processor to 6502
	if ( ph.id != PLFM_6502 )
	{
		msg("Nintendo Entertainment System ROM detected: setting processor type to M6502.\n");
		set_processor_type("M6502", SETPROC_ALL|SETPROC_FATAL);
	}


	return (1 | ACCEPT_FIRST);
}




//----------------------------------------------------------------------
//
//      load file into the database.
//
void load_file(linput_t *li, ushort /*neflag*/, const char * /*fileformatname*/)
{
    load_ines_file( li );
}




//----------------------------------------------------------------------
//
//
int write_file(FILE *fp, const char *)
{
    warning("[debug-msg] when am I being called?\n");
    return 0;
}




//----------------------------------------------------------------------
//
//      LOADER DESCRIPTION BLOCK
//
//----------------------------------------------------------------------
loader_t LDSC =
{
  IDP_INTERFACE_VERSION,
  0,                            // loader flags
//
//      check input file format. if recognized, then return 1
//      and fill 'fileformatname'.
//      otherwise return 0
//
  accept_file,
//
//      load file into the database.
//
  load_file,
//
//      create output file from the database.
//      this function may be absent.
//
  write_file,
};





//----------------------------------------------------------------------
//
//      loads the whole file into IDA
//      this is a wrapper function, which:
//
//      - checks the header for validity and fixes broken headers
//      - creates all necessary segments
//      - saves the whole file to blobs
//      - loads prg pages/banks
//      - adds informational descriptions to the database
//
static void load_ines_file( linput_t *li )
{
    // go to file offset 0 - just to be sure
	qlseek(li, 0, SEEK_SET);

	// read the whole header
	if( qlread(li, &hdr, sizeof(ines_hdr)) != sizeof(ines_hdr) )
        vloader_failure("File read error!",0);

    // check if header is corrupt
    // show a warning msg, but load the rom nonetheless
    if( is_corrupt_ines_hdr() )
    {
        //warning("The iNES header seems to be corrupt.\nLoader might give inaccurate results!");
        int code = askyn_c(1, "The iNES header seems to be corrupt.\n"
                              "The NES loader could produce wrong results!\n"
                              "Do you want to internally fix the header ?\n\n"
                              "(this will not affect the input file)");
        if( code == 1 )
            fix_ines_hdr();
    }

    // create NES segments
    create_segments( li );

    // save NES file to blobs
    save_image_as_blobs( li );
    
    // load relevant ROM banks into database
    load_rom_banks( li );
    
    // make vectors public
    add_entry_points( li );

    // fill inf structure
    set_ida_export_data();    

    // add information about the ROM image
	describe_rom_image();
  
    // let IDA add some information about the loaded file
    create_filename_cmt();
}



//----------------------------------------------------------------------
//
//      check if ROM image header is corrupt
//
static bool is_corrupt_ines_hdr(void)
{
    char empty[sizeof(hdr.reserved)];

    memset( &empty, 0, sizeof(empty) );
    return ( memcmp(&empty, &hdr.reserved, sizeof(empty)) != 0 );    
}



//----------------------------------------------------------------------
//
//      fix iNES header internally
//
static void fix_ines_hdr(void)
{
    char *diskdude = "DiskDude";

    if(memcmp(&hdr.rom_control_byte_1, diskdude, strlen(diskdude)) == 0)
    {
        memset(&hdr.rom_control_byte_1, 0, 9);
    }
    memset( &hdr.reserved, 0, sizeof(hdr.reserved));
    return;
}




//----------------------------------------------------------------------
//
//      creates all necessary segments and initializes them, if possible
//
static void create_segments( linput_t *li )
{
    // create RAM segment
    create_ram_segment();

    // create segment for I/O registers
    // NES uses memory mapped I/O
    create_ioreg_segment();

    // create SRAM segment if supported by cartridge
    //if( INES_MASK_SRAM( hdr.rom_control_byte_0 ) )
        create_sram_segment();

    // create segment for expansion ROM
    create_exprom_segment();

    // load trainer, if one is present
    if( INES_MASK_TRAINER(hdr.rom_control_byte_0) )
    {
        warning("This ROM image seems to have a trainer.\n"
                "By default, this loader assumes the trainer to be mapped to $7000.\n");
        load_trainer( li );  
    }

    // create segment for PRG ROMs
    create_rom_segment();

}


//----------------------------------------------------------------------
//
//      creates an SRAM segment, if available on cartridge
//
static void create_sram_segment( void )
{
    bool success = add_segm( 0, SRAM_START_ADDRESS, SRAM_START_ADDRESS + SRAM_SIZE, "SRAM", NULL ) == 1;
    msg("creating SRAM segment..%s", success ? "ok!\n" : "failure!\n");
    if(!success)
        return;
    set_segm_addressing( getseg( SRAM_START_ADDRESS ), 0 );
}



//----------------------------------------------------------------------
//
//      creates a RAM segment
//
static void create_ram_segment( void )
{
    bool success = add_segm( 0, RAM_START_ADDRESS, RAM_START_ADDRESS + RAM_SIZE, "RAM", NULL ) == 1;
    msg("creating RAM segment..%s", success ? "ok!\n" : "failure!\n");
    if(!success)
        return;
    set_segm_addressing( getseg( RAM_START_ADDRESS ), 0 );
    
    // how do I properly initialize a segment ?
    //for( unsigned int ea = SRAM_START_ADDRESS; ea<= SRAM_START_ADDRESS + SRAM_SIZE; ea++ )
    //    put_byte( ea, 0 );
}



//----------------------------------------------------------------------
//
//      creates an I/O registers segment and names all io registers
//
static void create_ioreg_segment( void )
{
    bool success = add_segm( 0, IOREGS_START_ADDRESS, IOREGS_START_ADDRESS + IOREGS_SIZE, "IO_REGS", NULL ) == 1;
    msg("creating IO_REGS segment..%s", success ? "ok!\n" : "failure!\n");
    if(!success)
        return;
    set_segm_addressing( getseg( IOREGS_START_ADDRESS ), 0 );

    define_item( PPU_CR_1_ADDRESS, PPU_CR_1_SIZE, PPU_CR_1_SHORT_DESCRIPTION, PPU_CR_1_COMMENT );
    define_item( PPU_CR_2_ADDRESS, PPU_CR_2_SIZE, PPU_CR_2_SHORT_DESCRIPTION, PPU_CR_2_COMMENT );
    define_item( PPU_SR_ADDRESS, PPU_SR_SIZE, PPU_SR_SHORT_DESCRIPTION, PPU_SR_COMMENT );

    define_item( SPR_RAM_AR_ADDRESS, SPR_RAM_AR_SIZE, SPR_RAM_AR_SHORT_DESCRIPTION, SPR_RAM_AR_COMMENT );
    define_item( SPR_RAM_IOR_ADDRESS, SPR_RAM_IOR_SIZE, SPR_RAM_IOR_SHORT_DESCRIPTION, SPR_RAM_IOR_COMMENT );

    define_item( VRAM_AR_1_ADDRESS, VRAM_AR_1_SIZE, VRAM_AR_1_SHORT_DESCRIPTION, VRAM_AR_1_COMMENT );
    define_item( VRAM_AR_2_ADDRESS, VRAM_AR_2_SIZE, VRAM_AR_2_SHORT_DESCRIPTION, VRAM_AR_2_COMMENT );
    define_item( VRAM_IOR_ADDRESS, VRAM_IOR_SIZE, VRAM_IOR_SHORT_DESCRIPTION, VRAM_IOR_COMMENT );

    define_item( PAPU_PULSE_1_CR_ADDRESS, PAPU_PULSE_1_CR_SIZE, PAPU_PULSE_1_CR_SHORT_DESCRIPTION, PAPU_PULSE_1_CR_COMMENT );
    define_item( PAPU_PULSE_1_RCR_ADDRESS, PAPU_PULSE_1_RCR_SIZE, PAPU_PULSE_1_RCR_SHORT_DESCRIPTION, PAPU_PULSE_1_RCR_COMMENT );
    define_item( PAPU_PULSE_1_FTR_ADDRESS, PAPU_PULSE_1_FTR_SIZE, PAPU_PULSE_1_FTR_SHORT_DESCRIPTION, PAPU_PULSE_1_FTR_COMMENT );
    define_item( PAPU_PULSE_1_CTR_ADDRESS, PAPU_PULSE_1_CTR_SIZE, PAPU_PULSE_1_CTR_SHORT_DESCRIPTION, PAPU_PULSE_1_CTR_COMMENT );

    define_item( PAPU_PULSE_2_CR_ADDRESS, PAPU_PULSE_2_CR_SIZE, PAPU_PULSE_2_CR_SHORT_DESCRIPTION, PAPU_PULSE_2_CR_COMMENT );
    define_item( PAPU_PULSE_2_RCR_ADDRESS, PAPU_PULSE_2_RCR_SIZE, PAPU_PULSE_2_RCR_SHORT_DESCRIPTION, PAPU_PULSE_2_RCR_COMMENT );
    define_item( PAPU_PULSE_2_FTR_ADDRESS, PAPU_PULSE_2_FTR_SIZE, PAPU_PULSE_2_FTR_SHORT_DESCRIPTION, PAPU_PULSE_2_FTR_COMMENT );
    define_item( PAPU_PULSE_2_CTR_ADDRESS, PAPU_PULSE_2_CTR_SIZE, PAPU_PULSE_2_CTR_SHORT_DESCRIPTION, PAPU_PULSE_2_CTR_COMMENT );

    define_item( PAPU_TRIANGLE_CR_1_ADDRESS, PAPU_TRIANGLE_CR_1_SIZE, PAPU_TRIANGLE_CR_1_SHORT_DESCRIPTION, PAPU_TRIANGLE_CR_1_COMMENT );
    define_item( PAPU_TRIANGLE_CR_2_ADDRESS, PAPU_TRIANGLE_CR_2_SIZE, PAPU_TRIANGLE_CR_2_SHORT_DESCRIPTION, PAPU_TRIANGLE_CR_2_COMMENT );
    define_item( PAPU_TRIANGLE_FR_1_ADDRESS, PAPU_TRIANGLE_FR_1_SIZE, PAPU_TRIANGLE_FR_1_SHORT_DESCRIPTION, PAPU_TRIANGLE_FR_1_COMMENT );
    define_item( PAPU_TRIANGLE_FR_2_ADDRESS, PAPU_TRIANGLE_FR_2_SIZE, PAPU_TRIANGLE_FR_2_SHORT_DESCRIPTION, PAPU_TRIANGLE_FR_2_COMMENT );
    
    define_item( PAPU_NOISE_CR_1_ADDRESS, PAPU_NOISE_CR_1_SIZE, PAPU_NOISE_CR_1_SHORT_DESCRIPTION, PAPU_NOISE_CR_1_COMMENT );
    define_item( PAPU_NOISE_CR_2_ADDRESS, PAPU_NOISE_CR_2_SIZE, PAPU_NOISE_CR_2_SHORT_DESCRIPTION, PAPU_NOISE_CR_2_COMMENT );
    define_item( PAPU_NOISE_FR_1_ADDRESS, PAPU_NOISE_FR_1_SIZE, PAPU_NOISE_FR_1_SHORT_DESCRIPTION, PAPU_NOISE_FR_1_COMMENT );
    define_item( PAPU_NOISE_FR_2_ADDRESS, PAPU_NOISE_FR_2_SIZE, PAPU_NOISE_FR_2_SHORT_DESCRIPTION, PAPU_NOISE_FR_2_COMMENT );

    
    define_item( PAPU_DM_CR_ADDRESS, PAPU_DM_CR_SIZE, PAPU_DM_CR_SHORT_DESCRIPTION, PAPU_DM_CR_COMMENT );
    define_item( PAPU_DM_DAR_ADDRESS, PAPU_DM_DAR_SIZE, PAPU_DM_DAR_SHORT_DESCRIPTION, PAPU_DM_DAR_COMMENT );
    define_item( PAPU_DM_AR_ADDRESS, PAPU_DM_AR_SIZE, PAPU_DM_AR_SHORT_DESCRIPTION, PAPU_DM_AR_COMMENT );
    define_item( PAPU_DM_DLR_ADDRESS, PAPU_DM_DLR_SIZE, PAPU_DM_DLR_SHORT_DESCRIPTION, PAPU_DM_DLR_COMMENT );

    define_item( PAPU_SV_CSR_ADDRESS, PAPU_SV_CSR_SIZE, PAPU_SV_CSR_SHORT_DESCRIPTION, PAPU_SV_CSR_COMMENT );    
    
    define_item( SPRITE_DMAR_ADDRESS, SPRITE_DMAR_SIZE, SPRITE_DMAR_SHORT_DESCRIPTION, SPRITE_DMAR_COMMENT );

    define_item( JOYPAD_1_ADDRESS, JOYPAD_1_SIZE, JOYPAD_1_SHORT_DESCRIPTION, JOYPAD_1_COMMENT );
    define_item( JOYPAD_2_ADDRESS, JOYPAD_2_SIZE, JOYPAD_2_SHORT_DESCRIPTION, JOYPAD_2_COMMENT );
}


//----------------------------------------------------------------------
//
//      creates a ROM segment where all the code is being loaded to
//
static void create_rom_segment( void )
{
    bool success = add_segm( 0, ROM_START_ADDRESS, ROM_START_ADDRESS + ROM_SIZE, "ROM", CLASS_CODE ) == 1;
    msg("creating ROM segment..%s", success ? "ok!\n" : "failure!\n");
    if(!success)
        return;
    set_segm_addressing( getseg( ROM_START_ADDRESS ), 0 );
}


//----------------------------------------------------------------------
//
//      creates an EXPANSION ROM segment, I don't know when it is used
//
static void create_exprom_segment( void )
{
    bool success = add_segm( 0, EXPROM_START_ADDRESS, EXPROM_START_ADDRESS + EXPROM_SIZE, "EXP_ROM", NULL ) == 1;
    msg("creating EXP_ROM segment..%s", success ? "ok!\n" : "failure!\n");
    if(!success)
        return;
    set_segm_addressing( getseg( EXPROM_START_ADDRESS ), 0 );
}



//----------------------------------------------------------------------
//
//      loads a 512 byte trainer (located at file offset INES_HDR_SIZE)
//      to TRAINER_START_ADDRESS
//
static void load_trainer( linput_t *li )
{
    if( !INES_MASK_SRAM( hdr.rom_control_byte_0 ) )
    {
        bool success = add_segm( 0, TRAINER_START_ADDRESS, TRAINER_START_ADDRESS + TRAINER_SIZE, "TRAINER", CLASS_CODE ) == 1;
        msg("creating TRAINER segment..%s", success ? "ok!\n" : "failure!\n");
        set_segm_addressing( getseg( TRAINER_START_ADDRESS ), 0 );
    }
    file2base(li, INES_HDR_SIZE, TRAINER_START_ADDRESS, TRAINER_START_ADDRESS + TRAINER_SIZE, FILEREG_PATCHABLE);
}


//----------------------------------------------------------------------
//
//      load 8k chr rom bank into database
//
static void load_chr_rom_bank( linput_t *li, uchar banknr, ea_t address )
{
    // todo: add support for PPU
    // this function currently is disabled, since no
    // segment for the PPU is created
    msg("The loader was trying to load a CHR bank but the PPU is not supported yet.\n");
    return;

    if( (banknr == 0) || (hdr.chr_page_count_8k == 0) )
        return;
    
    // this is the file offset to begin reading pages from
    long offset = INES_HDR_SIZE +
        (INES_MASK_TRAINER(hdr.rom_control_byte_0) ? TRAINER_SIZE : 0) +
        PRG_PAGE_SIZE * hdr.prg_page_count_16k + (banknr - 1) * CHR_ROM_BANK_SIZE;
    
    // load page from ROM file into segment
    msg("mapping CHR-ROM page %02d to %08x-%08x (file offset %08x) ..", banknr, address, address + CHR_PAGE_SIZE, offset);
    if( file2base(li, offset, address, address + CHR_ROM_BANK_SIZE, FILEREG_PATCHABLE) == 1)
        msg("ok\n");
    else
        msg("failure (corrupt ROM image?)\n");
}




//----------------------------------------------------------------------
//
//      load 16k prg rom bank into database
//
static void load_prg_rom_bank( linput_t *li, uchar banknr, ea_t address )
{

    if( (banknr == 0) || (hdr.prg_page_count_16k == 0) )
        return;

    // this is the file offset to begin reading pages from
    long offset = INES_HDR_SIZE +
        (INES_MASK_TRAINER(hdr.rom_control_byte_0) ? TRAINER_SIZE : 0) +
        (banknr - 1) * PRG_ROM_BANK_SIZE;
    
    // load page from ROM file into segment
    msg("mapping PRG-ROM page %02d to %08x-%08x (file offset %08x) ..", 1, address, address + PRG_ROM_BANK_SIZE, offset);
    if( file2base(li, offset, address, address + PRG_ROM_BANK_SIZE, FILEREG_PATCHABLE) == 1)
        msg("ok\n");
    else
        msg("failure (corrupt ROM image?)\n");                  
}



//----------------------------------------------------------------------
//
//      load 8k prg rom bank into database
//
static void load_8k_prg_rom_bank( linput_t *li, uchar banknr, ea_t address )
{

    if( (banknr == 0) || (hdr.prg_page_count_16k == 0) )
        return;

    // this is the file offset to begin reading pages from
    long offset = INES_HDR_SIZE +
        (INES_MASK_TRAINER(hdr.rom_control_byte_0) ? TRAINER_SIZE : 0) +
        (banknr - 1) * PRG_ROM_8K_BANK_SIZE;
    
    // load page from ROM file into segment
    msg("mapping 8k PRG-ROM page %02d to %08x-%08x (file offset %08x) ..", 1, address, address + PRG_ROM_8K_BANK_SIZE, offset);
    if( file2base(li, offset, address, address + PRG_ROM_8K_BANK_SIZE, FILEREG_PATCHABLE) == 1)
        msg("ok\n");
    else
        msg("failure (corrupt ROM image?)\n");                  
}


//----------------------------------------------------------------------
//
//      this function loads the image into the ida database
//      depending on the mapper in use
//
static void load_rom_banks( linput_t *li )
{
    uchar mapper = INES_MASK_MAPPER_VERSION(hdr.rom_control_byte_0, hdr.rom_control_byte_1);
    switch( mapper )
    {
    default: // 1st prg, last prg, 1st chr
        warning("Mapper %d is not supported by this loader!\n"
                "This could be a corrupt ROM image!\n"
                "Loading first and last PRG-ROM banks by default.", mapper);
    case MAPPER_NONE:
    case MAPPER_MMC1:
    case MAPPER_UNROM:
    case MAPPER_CNROM:
    case MAPPER_MMC3:
    case MAPPER_MMC5:
    case MAPPER_FFE_F4XXX:
    case MAPPER_MMC4:
    case MAPPER_BANDAI:
    case MAPPER_FFE_F8XXX:
    case MAPPER_JALECO_SS8806:
    case MAPPER_KONAMI_VRC4:
    case MAPPER_KONAMI_VRC2_TYPE_A:
    case MAPPER_KONAMI_VRC2_TYPE_B:
    case MAPPER_KONAMI_VRC6:
    case MAPPER_NAMCOT_106:
    case MAPPER_IREM_G_101:
    case MAPPER_TAITO_TC0190:
    case MAPPER_IREM_H_3001:
    case MAPPER_SUNSOFT_MAPPER_4:
    case MAPPER_SUNSOFT_FME7: // not sure about this mapper
    case MAPPER_CAMERICA:
    case MAPPER_IREM_74HC161_32:
    case MAPPER_GNROM:
        {
            load_prg_rom_bank( li, 1, PRG_ROM_BANK_LOW_ADDRESS );
            load_prg_rom_bank( li, hdr.prg_page_count_16k, PRG_ROM_BANK_HIGH_ADDRESS );
            load_chr_rom_bank( li, 1, CHR_ROM_BANK_ADDRESS );
        }
        break;
        
    case MAPPER_HK_SF3: // last prg, last prg, 1st chr
        {
            load_prg_rom_bank( li, hdr.prg_page_count_16k, PRG_ROM_BANK_LOW_ADDRESS );
            load_prg_rom_bank( li, hdr.prg_page_count_16k, PRG_ROM_BANK_HIGH_ADDRESS );
            load_chr_rom_bank( li, 1, CHR_ROM_BANK_ADDRESS );
        }
        break;

    case MAPPER_AOROM: // 1st prg, 2nd prg, 1st chr
    case MAPPER_FFE_F3XXX:
    case MAPPER_COLOR_DREAMS:
    case MAPPER_100_IN_1:
    case MAPPER_NINA_1:
        {
            load_prg_rom_bank( li, 1, PRG_ROM_BANK_LOW_ADDRESS );
            load_prg_rom_bank( li, 2, PRG_ROM_BANK_HIGH_ADDRESS );
            load_chr_rom_bank( li, 1, CHR_ROM_BANK_ADDRESS );
        }
        break;
    case MAPPER_MMC2: // 1st 8k prg, last three 8k prgs, 1st chr
        {
            load_8k_prg_rom_bank( li, 1, PRG_ROM_BANK_LOW_ADDRESS );
            load_8k_prg_rom_bank( li, hdr.prg_page_count_16k*2 - 2, PRG_ROM_BANK_A000 );
            load_prg_rom_bank( li, hdr.prg_page_count_16k, PRG_ROM_BANK_HIGH_ADDRESS );
            load_chr_rom_bank( li, 1, CHR_ROM_BANK_ADDRESS );
        }
        break;
              
    case MAPPER_TENGEN_RAMBO_1: // last 8k prg, last 8k prg, last 8k prg, last 8k prg, 1st chr
        {
            load_8k_prg_rom_bank( li, hdr.prg_page_count_16k*2 , PRG_ROM_BANK_8000 );
            load_8k_prg_rom_bank( li, hdr.prg_page_count_16k*2 , PRG_ROM_BANK_A000 );
            load_8k_prg_rom_bank( li, hdr.prg_page_count_16k*2 , PRG_ROM_BANK_C000 );
            load_8k_prg_rom_bank( li, hdr.prg_page_count_16k*2 , PRG_ROM_BANK_E000 );
            load_chr_rom_bank( li, 1, CHR_ROM_BANK_ADDRESS );           
        }
        break;
    }
}



//----------------------------------------------------------------------
//
//      saves prg and chr ROM pages/banks to a binary large object (blob)
//
static void save_image_as_blobs( linput_t *li )
{
    // store ines header in a blob
	save_ines_hdr_as_blob();
 
    save_trainer_as_blob( li );

    // store rom image in blobs
    save_prg_rom_pages_as_blobs( li, hdr.prg_page_count_16k );
    save_chr_rom_pages_as_blobs( li, hdr.chr_page_count_8k );
}





//----------------------------------------------------------------------
//
//      store header to netnode
//
static bool save_ines_hdr_as_blob( void )
{
	netnode hdr_node;
    
	if( !hdr_node.create( INES_HDR_NODE ) )
		return false;
	return hdr_node.setblob(&hdr, INES_HDR_SIZE, 0, 'I');
}



//----------------------------------------------------------------------
//
//      store trainer to netnode
//
static bool save_trainer_as_blob( linput_t *li )
{
	netnode node;

    uchar *buffer;

    if( !INES_MASK_TRAINER( hdr.rom_control_byte_0 ) )
        return false;

    buffer = (uchar *)qalloc( TRAINER_SIZE );

    if( buffer == 0 )
        return false;

    qlseek( li, INES_HDR_SIZE );
    qlread( li, buffer, TRAINER_SIZE );
  	if( !node.create( "$ Trainer" ) )
    {
        qfree( buffer );
	  	return false;
    }
    if( !node.setblob( buffer, TRAINER_SIZE, 0, 'I' ) )
        msg("Could not store trainer to netnode!\n");
    
    qfree( buffer );    
	return true;
}


//----------------------------------------------------------------------
//
//      store PRG ROM pages to netnode
//
static bool save_prg_rom_pages_as_blobs( linput_t *li, uchar count )
{
	netnode node;

    char prg_node_name[MAXNAMESIZE];
    uchar *buffer = (uchar *)qalloc( PRG_PAGE_SIZE );

    if( buffer == 0 )
        return false;

    qlseek( li, INES_HDR_SIZE + (INES_MASK_TRAINER(hdr.rom_control_byte_0) ? TRAINER_SIZE : 0) );

    for(int i=0; i<count; i++)
    {
        qlread( li, buffer, PRG_PAGE_SIZE );
        qsnprintf( prg_node_name, sizeof(prg_node_name), "$ PRG-ROM page %d", i );
    	if( !node.create( prg_node_name ) )
        {
            qfree( buffer );
	    	return false;
        }
        if( !node.setblob( buffer, PRG_PAGE_SIZE, 0, 'I' ) )
            msg("Could not store PRG-ROM pages to netnode!\n");
    }
    
    qfree( buffer );    
	return true;
}



//----------------------------------------------------------------------
//
//      store CHR ROM pages to netnode
//
static bool save_chr_rom_pages_as_blobs( linput_t *li, uchar count )
{
	netnode node;

    char chr_node_name[MAXSTR];
    uchar *buffer = (uchar *)qalloc( CHR_PAGE_SIZE );

    if( buffer == 0 )
        return false;

    qlseek( li, INES_HDR_SIZE + (INES_MASK_TRAINER(hdr.rom_control_byte_0) ? TRAINER_SIZE : 0) + PRG_PAGE_SIZE * hdr.prg_page_count_16k );

    for(int i=0; i<count; i++)
    {
        qlread( li, buffer, CHR_PAGE_SIZE );
        qsnprintf( chr_node_name, sizeof(chr_node_name), "$ CHR-ROM page %d", i );
    	if( !node.create( chr_node_name ) )
        {
            qfree( buffer );
	    	return false;
        }
        if( !node.setblob( buffer, CHR_PAGE_SIZE, 0, 'I' ) )
            msg("Could not store CHR-ROM pages to netnode!\n");
    }
    
    qfree( buffer );    
	return true;
}



//----------------------------------------------------------------------
//
//      returns name of mapper
//
static char *get_mapper_name( uchar mapper )
{
    if(mapper > MAPPER_LAST)
        return MAPPER_NOT_SUPPORTED;
    return mapper_names[mapper];
}




//----------------------------------------------------------------------
//
//      add information about the ROM image to disassembly
//
static void describe_rom_image(void)
{
    uchar mapper = INES_MASK_MAPPER_VERSION( hdr.rom_control_byte_0, hdr.rom_control_byte_1 );

    describe(inf.minEA, true, "\n;   ROM information\n"
		                      ";   ---------------\n;");
    describe(inf.minEA, true, ";   Valid image header      : %s", YES_NO( !is_corrupt_ines_hdr() ) );
	describe(inf.minEA, true, ";   16K PRG-ROM page count  : %d", hdr.prg_page_count_16k);
	describe(inf.minEA, true, ";   8K CHR-ROM page count   : %d", hdr.chr_page_count_8k);
    describe(inf.minEA, true, ";   Mirroring               : %s", INES_MASK_H_MIRRORING(hdr.rom_control_byte_0) ? "horizontal" : "vertical");
	describe(inf.minEA, true, ";   SRAM enabled            : %s", YES_NO( INES_MASK_SRAM(hdr.rom_control_byte_0) ) );
	describe(inf.minEA, true, ";   512-byte trainer        : %s", YES_NO( INES_MASK_TRAINER(hdr.rom_control_byte_0) ) );
    describe(inf.minEA, true, ";   Four screen VRAM layout : %s", YES_NO( INES_MASK_VRAM_LAYOUT(hdr.rom_control_byte_0) ) );
	describe(inf.minEA, true, ";   Mapper                  : %s (Mapper #%d)", get_mapper_name( mapper ), mapper);
}



//----------------------------------------------------------------------
//
//      defines, names and comments an item
//
static void define_item( ushort address, asize_t size, char *shortdesc, char *comment )
{
    do_unknown( address, true );
    do_data_ex( address, (size == IOREG_16 ? wordflag() : byteflag() ), size, BADNODE );
    set_name( address, shortdesc );
    set_cmt( address, comment, true );
}



//----------------------------------------------------------------------
//
//      gets a vector's address
//      vec either is one of the following constants:
//
//      #define NMI_VECTOR_START_ADDRESS            0xFFFA
//      #define RESET_VECTOR_START_ADDRESS          0xFFFC
//      #define IRQ_VECTOR_START_ADDRESS            0xFFFE
//
static ea_t get_vector( ea_t vec )
{
    return get_word( vec );
}


//----------------------------------------------------------------------
//
//      define location as word (2 byte), convert it to an offset, rename it
//      and comment it with the file offset
//
static void name_vector( ushort address, const char *name )
{
    do_unknown( address, true );
    do_data_ex( address, wordflag(), 2, BADNODE );
    set_offset( address, 0, 0 );
    set_name( address, name );
}


//----------------------------------------------------------------------
//
//      add entrypoints to the database and name vectors
//
static bool add_entry_points( linput_t *li )
{
    ea_t ea;

    ea = get_vector( NMI_VECTOR_START_ADDRESS );
    add_entry( ea, ea, "NMI_routine", true );
    name_vector( NMI_VECTOR_START_ADDRESS, "NMI_vector" );

    ea = get_vector( RESET_VECTOR_START_ADDRESS );
    add_entry( ea, ea, "RESET_routine", true );
    name_vector( RESET_VECTOR_START_ADDRESS, "RESET_vector" );

    ea = get_vector( IRQ_VECTOR_START_ADDRESS );
    add_entry( ea, ea, "IRQ_routine", true );
    name_vector( IRQ_VECTOR_START_ADDRESS, "IRQ_vector" );

    return true;
}



//----------------------------------------------------------------------
//
//      set entrypoint, minEA, maxEA, start_cs and filetype
//
static void set_ida_export_data( void )
{
    // set entrypoint
    inf.startIP = inf.beginEA = get_vector( RESET_VECTOR_START_ADDRESS );
    
    // set minEA, maxEA, etc.
    inf.start_cs = 0;
    inf.minEA = RAM_START_ADDRESS;
    inf.maxEA = ROM_START_ADDRESS + ROM_SIZE;
}







