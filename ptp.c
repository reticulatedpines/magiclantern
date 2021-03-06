/** \file
 * PTP handlers to extend Magic Lantern to the USB port.
 *
 * These handlers are registered to allow Magic Lantern to interact with
 * a PTP client on the USB port.
 */

#include "dryos.h"
#include "ptp.h"
#include "tasks.h"
#include "menu.h"
#include "bmp.h"
#include "hotplug.h"
#include "property.h"
#include "lens.h"


PTP_HANDLER( 0x9999, 0 )
{
	struct ptp_msg msg = {
		.id		= PTP_RC_OK,
		.session	= session,
		.transaction	= transaction,
		.param_count	= 4,
		.param		= { 1, 2, 0xdeadbeef, 3 },
	};

	//call( "FA_StartLiveView" );
	bmp_printf( FONT_MED, 0, 30, "usb %08x %08x", context, context->handle );
	bmp_printf( FONT_MED, 0, 50, "%08x %08x %08x %08x %08x",
		(unsigned) param1,
		(unsigned) param2,
		(unsigned) param3,
		(unsigned) param4,
		(unsigned) param5
	);

#if 0
	int len = context->len( context->handle );
	bmp_printf( FONT_LARGE, 0, 50, "Len = %d", len );
	if( !len )
	{
		context->send(
			context->handle,
			&msg
		);

		return 0;
	}

	void * buf = AllocateMemory( len );
	if( !buf )
		return 1;

	bmp_printf( FONT_LARGE, 0, 60, "buf = %08x", buf );
	context->recv(
		context->handle,
		buf,
		len,
		0,
		0
	);


	bmp_hexdump( FONT_LARGE, 0, 50, buf, len );
	FreeMemory( buf );
#endif

	context->send(
		context->handle,
		&msg
	);

	// Try to disable the USB lock
	gui_unlock();

	return 0;
}


/** Start recording when we get a PTP operation 0x9997
 * MovieStop doesn't seem to do anything, but MovieStart
 * toggles recording on and off
 */
PTP_HANDLER( 0x9997, 0 )
{
	call( "MovieStart" );

	struct ptp_msg msg = {
		.id		= PTP_RC_OK,
		.session	= session,
		.transaction	= transaction,
		.param_count	= 1,
		.param		= { param1 },
	};

	context->send(
		context->handle,
		&msg
	);

	return 0;
}


/** Dump memory */
PTP_HANDLER( 0x9996, 0 )
{
	const uint32_t * const buf = (void*) param1;

	struct ptp_msg msg = {
		.id		= PTP_RC_OK,
		.session	= session,
		.transaction	= transaction,
		.param_count	= 5,
		.param		= {
			buf[0],
			buf[1],
			buf[2],
			buf[3],
			buf[4],
		},
	};

	context->send(
		context->handle,
		&msg
	);

	return 0;
}

/** Write to memory, returning the old value */
PTP_HANDLER( 0x9995, 0 )
{
	uint32_t * const buf = (void*) param1;
	const uint32_t val = (void*) param2;

	const uint32_t old = *buf;
	*buf = val;

	struct ptp_msg msg = {
		.id		= PTP_RC_OK,
		.session	= session,
		.transaction	= transaction,
		.param_count	= 2,
		.param		= {
			param1,
			old,
		},
	};

	context->send(
		context->handle,
		&msg
	);

	return 0;
}

static void
ptp_state_display(
	void *			priv,
	int			x,
	int			y,
	int			selected
)
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		//23456789012
		"PTP State:  %x %08x",
		hotplug_struct.usb_state,
		*(uint32_t*)( 0xC0220000 + 0x34 )
	);
}


static void
ptp_state_toggle( void * priv )
{
	hotplug_struct.usb_state = !hotplug_struct.usb_state;
	prop_deliver(
		hotplug_struct.usb_prop,
		&hotplug_usb_buf,
		sizeof(hotplug_usb_buf),
		0
	);
}

static struct menu_entry ptp_menus[] = {
	{
		.display	= ptp_state_display,
		.select		= ptp_state_toggle,
	},
};


static void
ptp_init( void )
{
	extern struct ptp_handler _ptp_handlers_start[];
	extern struct ptp_handler _ptp_handlers_end[];
	struct ptp_handler * handler = _ptp_handlers_start;

	for( ; handler < _ptp_handlers_end ; handler++ )
	{
		ptp_register_handler(
			handler->id,
			handler->handler,
			handler->priv
		);
	}

	menu_add( "PTP", ptp_menus, COUNT(ptp_menus) );
}


INIT_FUNC( __FILE__, ptp_init );
