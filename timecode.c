/** \file SMPTE timecode analyzer for the audio port
 */
#ifndef __ARM__
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h> // for ntohl
#define con_printf(font,...) /* Nothing */
unsigned offset;
#else
#include "dryos.h"
#include "tasks.h"
#include "audio.h"
#include "bmp.h"
#include "config.h"
#include "menu.h"
#define printf(fmt,...) /* Nothing */
#endif
#include <stdint.h>

// SMTPE timecode frame
static uint8_t smpte_frame[ 8 ];


// Returns 1 if there is a new frame available
static int
tc_sample(
	int16_t			sample
)
{
	static uint32_t last_transition;

	// Hold onto the last bit for hysteris
	static int bit = 0;
	static int bit_count = 0;

	static uint32_t word = 0;
	static uint32_t raw_word = 0;
	static int synced = 0;
	static int byte_count = 0;

	// Use some hysteris to avoid zero crosing errors
	int old_bit = bit;
	if( sample > 1000 )
		bit = 0;
	else
	if( sample < -1000 )
		bit = 1;

	static int x, y = 32;

	// Record any transition to help reconstruct the clock
	if( bit == old_bit )
		return 0;

#ifdef __ARM__
	unsigned (*read_clock)(void) = (void*) 0xff9948d8;
	unsigned now = read_clock();
#else
	unsigned now = offset;
#endif
	int delta = now - last_transition;
	last_transition = now;

	// Timer is only 24 bits?
	delta &= 0x00FFFFFF;

#if 0 //def __ARM__
	bmp_printf( FONT_SMALL, x, y, "%02x ", delta );
	x += 3 * 8;
	if( x > 700 )
	{
		x = 0;
		y += 12;
		if( y > 400 )
		{
			y = 32;
			msleep( 2000 );
		}
	}
	bit_count = 0;
	return 0;
#endif

	int new_bit;

	static unsigned last_bit;
	//printf( "%08x delta %x\n", offset, delta );

#ifdef __ARM__
// These deltas are in timer ticks
// Timer == 500 MHz?  0xD0 ==> 2400 Hz, 0x1A0 => 1200 Hz
#define ONE_LEN		0xD0
#define ZERO_LEN	0x1A0
#define EPS		0x30
#else
// These are in byte offsets in the file
#define ZERO_LEN	0x25
#define ONE_LEN		0x12
#define EPS		0x04
#endif

	if( ZERO_LEN-EPS < delta && delta < ZERO_LEN+EPS )
	{
		// Full bit width
		new_bit = 0;
	} else
	if( ONE_LEN-EPS < delta && delta < ONE_LEN+EPS )
	{
		// Skip the second transition if we're short
		if( ++last_bit & 1 )
			return 0;
		last_bit = 0;
		new_bit = 1;
	} else {
		bmp_printf(
			FONT(FONT_SMALL,COLOR_RED,0),
			delta < ONE_LEN ? 10 :
			delta > ZERO_LEN ? 210 :
			110,
			300,
			"%04x",
			delta
		);
		printf( "%08x: bad delta %x\n", offset, delta );
		// Lost sync somewhere
		synced = 0;
		word = 0;
		return 0;
	}

	word = (word << 1) | new_bit;

	//printf( " %04x\n", word & 0xFFFF );
	//return 0;

	// Check for SMPTE sync signal
	if( !synced )
	{
		if( (word & 0xFFFF) == 0x3FFD )
		{
			//fprintf( stderr, "%x: Got sync!\n", offset );
			printf( "\n%08x: synced", offset );
			synced = 1;
			word = 1;
			byte_count = 0;
		}

		return 0;
	}

	// We are locked; wait until we have 8 bits then write it
	if( (word & 0x100) == 0 )
		return 0;

	//printf( " %02x", word & 0xFF );

	// Reverse the word since we bit banged it in the wrong order
	word = 0
		| ( word & 0x01 ) << 7
		| ( word & 0x02 ) << 5
		| ( word & 0x04 ) << 3
		| ( word & 0x08 ) << 1
		| ( word & 0x10 ) >> 1
		| ( word & 0x20 ) >> 3
		| ( word & 0x40 ) >> 5
		| ( word & 0x80 ) >> 7
		;
#ifdef __ARM__
	//con_printf( FONT_SMALL, "%02x ", word );
#endif

	smpte_frame[ byte_count++ ] = word;
	word = 1;
	if( byte_count < 8 )
		return 0;

	// We have a complete frame
	synced = 0;
	return 1;
}


#ifndef __ARM__
struct au_hdr
{
	uint32_t	magic;
	uint32_t	offset;
	uint32_t	len;
	uint32_t	encoding;
	uint32_t	rate;
	uint32_t	channels;
};


int main( int argc, char ** argv )
{
	const char * filename = argc > 1 ? argv[1] : "timecode.au";
	int fd = open( filename, O_RDONLY );
	if( fd < 0 )
	{
		perror( filename );
		return -1;
	}

	struct au_hdr hdr;
	offset = read( fd, &hdr, sizeof(hdr) );

	hdr.magic	= ntohl( hdr.magic );
	hdr.offset	= ntohl( hdr.offset );
	hdr.len		= ntohl( hdr.len );
	hdr.encoding	= ntohl( hdr.encoding );
	hdr.rate	= ntohl( hdr.rate );
	hdr.channels	= ntohl( hdr.channels );
	fprintf( stderr,
		"magic=%08x encoding=%x\n",
		hdr.magic,
		hdr.encoding
	);

	if( hdr.magic != 0x2e736e64
	||  hdr.encoding != 3 )
	{
		fprintf( stderr, "Bad magic or unsupported format!\n" );
		return -1;
	}

	uint16_t sample;
	uint32_t last_transition = 0;

	// Seek to the start of the data
	while( offset < hdr.offset )
		offset += read( fd, &sample, 1 );

	while( offset < hdr.len )
	{
		ssize_t rc = read( fd, &sample, sizeof(sample) );
		offset += rc;
		if( rc < 1 )
			break;

		sample = ntohs( sample );
		// Convert to an unsigned value
		sample += 32768;
		if( tc_sample( sample ) == 0 )
			continue;

		printf( "%08x", offset );
		int i;
		for( i=0 ; i<8 ; i++ )
			printf( " %02x", smpte_frame[ i ] );

		// Decode it:
#define BCD_BITS(x) \
	(smpte_frame[x/8] & 0xF)

		int f = BCD_BITS(0) + 10 * (BCD_BITS(8) & 0x3);
		int s = BCD_BITS(16) + 10 * (BCD_BITS(24) & 0x7);
		int m = BCD_BITS(32) + 10 * BCD_BITS(40);
		int h = BCD_BITS(48) + 10 * (BCD_BITS(56) & 0x3);
		
		printf( ": %02d:%02d:%02d.%02d\n", h, m, s, f  );
	}

	return 0;
}
#else


static void
process_timecode( void )
{
	while( !gui_menu_task )
	{
		int sample = audio_read_level( 0 );
		if( tc_sample( sample ) == 0 )
			continue;

		// Print a timecode result!
		// Decode it:
#define BCD_BITS(x) \
	(smpte_frame[x/8] & 0xF)

		int f = BCD_BITS(0) + 10 * (BCD_BITS(8) & 0x3);
		int s = BCD_BITS(16) + 10 * (BCD_BITS(24) & 0x7);
		int m = BCD_BITS(32) + 10 * BCD_BITS(40);
		int h = BCD_BITS(48) + 10 * (BCD_BITS(56) & 0x3);
		
#if 0
		bmp_printf(
			FONT(FONT_LARGE,COLOR_WHITE,COLOR_BLUE),
			180, 150,
			"             \n"
			"  SMPTE LTC  \n"
			" %02d:%02d:%02d.%02d \n"
			"             \n",
			h, m, s, f
		);
#else
		bmp_printf(
			FONT_HUGE,
			0, 150,
			//23456789012
			//hh:mm:ss.ff \n
			" SMTPE LTC:  \n"
			" %02d:%02d:%02d.%02d ",
			h, m, s, f
		);
#endif
	}
}


static struct semaphore * timecode_sem;

static void
timecode_unlock( void * priv )
{
	gui_stop_menu();
	give_semaphore( timecode_sem );
}


static struct menu_entry timecode_menu[] = {
	{
		.display	= menu_print,
		.priv		= "Jam timecode",
		.select		= timecode_unlock,
	},
};



static void
tc_task( void )
{
	timecode_sem = create_named_semaphore( "timecode", 0 );
	menu_add( "Debug", timecode_menu, COUNT(timecode_menu) );

	while(1)
	{
		take_semaphore( timecode_sem, 0 );
		process_timecode();
	}
}


TASK_CREATE( __FILE__, tc_task, 0, 0x18, 0x1000 );
#endif


