/*

	Nintendo Entertainment System (NES) loader module
	------------------------------------------------------
	Copyright 2006, Dennis Elser (dennis@backtrace.de)

*/


#ifndef _NES_H
#define _NES_H




//----------------------------------------------------------------------
//
//      general NES info:
//


#define RAM_START_ADDRESS                   0x0
#define RAM_SIZE                            0x2000

#define IOREGS_START_ADDRESS                0x2000
#define IOREGS_SIZE                         0x2020

#define EXPROM_START_ADDRESS                0x4020
#define EXPROM_SIZE                         0x1FE0

#define SRAM_START_ADDRESS                  0x6000
#define SRAM_SIZE                           0x2000

// start address and size of a trainer, if present
#define TRAINER_START_ADDRESS               0x7000
#define TRAINER_SIZE                        0x0200

#define ROM_START_ADDRESS                   0x8000
#define ROM_SIZE                            0x8000

#define PRG_PAGE_SIZE                       0x4000
#define CHR_PAGE_SIZE                       0x2000


#define PRG_ROM_BANK_SIZE                   PRG_PAGE_SIZE
#define PRG_ROM_8K_BANK_SIZE                0x2000
#define PRG_ROM_BANK_LOW_ADDRESS            ROM_START_ADDRESS
#define PRG_ROM_BANK_HIGH_ADDRESS           PRG_ROM_BANK_LOW_ADDRESS + PRG_ROM_BANK_SIZE
#define PRG_ROM_BANK_8000                   0x8000
#define PRG_ROM_BANK_A000                   0xA000
#define PRG_ROM_BANK_C000                   0xC000
#define PRG_ROM_BANK_E000                   0xE000


#define CHR_ROM_BANK_SIZE                   CHR_PAGE_SIZE
#define CHR_ROM_BANK_ADDRESS                RAM_START_ADDRESS



// start address of vectors
#define NMI_VECTOR_START_ADDRESS            0xFFFA
#define RESET_VECTOR_START_ADDRESS          0xFFFC
#define IRQ_VECTOR_START_ADDRESS            0xFFFE



// PPU RAM layout bottom-up

#define PATTERN_TABLE_SIZE                  0x1000
#define ATTRIBUTE_TABLE_SIZE                0x40
#define NAME_TABLE_SIZE                     0x3C0
#define MIRRORS_0_SIZE                      0xF00
#define MIRRORS_1_SIZE                      0xE0
#define MIRRORS_2_SIZE                      0xC000

#ifdef PALETTE_SIZE // defined in lines.hpp
    #undef PALETTE_SIZE
    #define PALETTE_SIZE                    0x10
#endif


#define PATTERN_TABLE_0_ADDRESS             0x0
#define PATTERN_TABLE_1_ADDRESS             0x1000

#define NAME_TABLE_0_ADDRESS                0x2000
#define ATTRIBUTE_TABLE_0_ADDRESS           0x23C0

#define NAME_TABLE_1_ADDRESS                0x2400
#define ATTRIBUTE_TABLE_1_ADDRESS           0x27C0

#define NAME_TABLE_2_ADDRESS                0x2800
#define ATTRIBUTE_TABLE_2_ADDRESS           0x2BC0

#define NAME_TABLE_3_ADDRESS                0x2C00
#define ATTRIBUTE_TABLE_3_ADDRESS           0x2CF0

#define MIRRORS_0_ADDRESS                   0x3000

#define IMAGE_PALETTE_ADDRESS               0x3F00

#define SPRITE_PALETTE_ADDRESS              0x3F10

#define MIRRORS_1_ADDRESS                   0x3F20

#define MIRRORS_2_ADDRESS                   0x4000



//----------------------------------------------------------------------
//
//      iNES file format specific information
//

// structure of iNES header
typedef struct _ines_hdr_t {

	uchar id[0x3];                          // NES
	uchar term;                             // 0x1A
	uchar prg_page_count_16k;               // number of PRG-ROM pages
	uchar chr_page_count_8k;                // number of CHR-ROM pages
	uchar rom_control_byte_0;               // flags describing ROM image
	uchar rom_control_byte_1;               // flags describing ROM image
	uchar ram_bank_count_8k;                // not used by this loader currently
	uchar reserved[7];                      // should all be zero (not checked by loader)

} ines_hdr;


// size of iNES header
#define INES_HDR_SIZE                       sizeof( ines_hdr )


// node name for iNES header
#define INES_HDR_NODE                       "$ iNES ROM header"

#define BANK_NUM_8000                       "$ Bank 8000"
#define BANK_NUM_C000                       "$ Bank C000"

// macros for masking control byte (cb) flags of the header
#define INES_MASK_V_MIRRORING( cb )         ( cb & 0x1 )
#define INES_MASK_H_MIRRORING( cb )         !INES_MASK_V_MIRRORING( cb )
#define INES_MASK_SRAM( cb )                ( (cb & 0x2) >> 1 )
#define INES_MASK_TRAINER( cb )             ( (cb & 0x4) >> 2 )
#define INES_MASK_VRAM_LAYOUT( cb )         ( (cb & 0x8) >> 3 )


// macro for getting the version of the mapper used by ROM image
#define INES_MASK_MAPPER_VERSION(cb0, cb1)  ( ((cb0 & 0xF0) >> 4) |  (cb1 & 0xF0) )




//----------------------------------------------------------------------
//
//      function prototypes for nes.cpp
//

static void load_ines_file( linput_t *li ); // convenience function for all below

static bool is_corrupt_ines_hdr( void );
static void fix_ines_hdr( void );

static void create_segments( linput_t *li ); // convenience function for the following few
static void create_sram_segment( void );
static void create_ram_segment( void );
static void create_ioreg_segment( void );
static void create_rom_segment( void );
static void create_exprom_segment( void );

static void load_trainer( linput_t *li );
static void load_chr_rom_bank( linput_t *li, uchar banknr, ea_t address );
static void load_prg_rom_bank( linput_t *li, uchar banknr, ea_t address );
static void load_8k_prg_rom_bank( linput_t *li, uchar banknr, ea_t address );
static void load_rom_banks( linput_t *li );

static void save_image_as_blobs( linput_t *li ); // convenience function for the following few
static bool save_ines_hdr_as_blob( void );
static bool save_trainer_as_blob( linput_t *li );
static bool save_prg_rom_pages_as_blobs( linput_t *li, uchar count );
static bool save_chr_rom_pages_as_blobs( linput_t *li, uchar count );


static char *get_mapper_name( uchar mapper );
static void define_item( ushort address, asize_t size, char *shortdesc, char *comment );
static ea_t get_vector( ea_t vec );
static void name_vector( ushort address, const char *name );
static bool add_entry_points( linput_t *li );
static void set_ida_export_data( void );
static void describe_rom_image( void );

#endif // _NES_H