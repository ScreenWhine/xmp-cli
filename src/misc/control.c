/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "driver.h"
#include "mixer.h"
#include "formats.h"

static int drv_parm = 0;
extern struct xmp_drv_info drv_callback;
extern struct xmp_ord_info xxo_info[XMP_DEF_MAXORD];

void xmp_init_callback (struct xmp_control *ctl, void (*callback)(void *, int))
{
    xmp_drv_register (&drv_callback);
    xmp_init_formats ();
    xmp_register_driver_callback (callback);
    xmp_drv_mutelloc (64);
    ctl->drv_id = "callback";
}

void xmp_init (int argc, char **argv, struct xmp_control *ctl)
{
    int num;

    xmp_init_drivers ();
    xmp_init_formats ();

    memset (ctl, 0, sizeof (struct xmp_control));
    xmp_event_callback = NULL;

    /* Set defaults */
    ctl->freq = 44100;
    ctl->mix = 80;
    ctl->outfmt = XMP_FMT_MONO;
    ctl->resol = 16;					/* Was 0. why? */
    ctl->flags = XMP_CTL_DYNPAN | XMP_CTL_FILTER;	/* Default mode */

    /* Turn on channel mute control, alloc default 64 max track control */
    xmp_drv_mutelloc (64);

    /* Set max number of voices per channel */
    ctl->maxvoc = 16;

    /* Okay, this is kludgy. But I must parse this _before_ loading
     * the rc file.
     */
    for (num = 1; num < argc; num++) {
	if (!strcmp (argv[num], "--norc"))
	    break;
    }
    if (num >= argc)
	xmpi_read_rc (ctl);

    xmpi_tell_wait ();
}


inline int xmp_open_audio (struct xmp_control *ctl)
{
    return xmp_drv_open (ctl);
}


inline void xmp_close_audio ()
{
    xmp_drv_close ();
}


void xmp_set_driver_parameter (struct xmp_control *ctl, char *s)
{
    ctl->parm[drv_parm] = s;
    while (isspace (*ctl->parm[drv_parm]))
	ctl->parm[drv_parm]++;
    drv_parm++;
}


inline void xmp_register_event_callback (void (*cb)())
{
    xmp_event_callback = cb;
}


void xmp_channel_mute (int from, int num, int on)
{
    from += num - 1;
    if (num > 0)
	while (num--)
	    xmp_drv_mute (from - num, on);
}


int xmp_player_ctl (int cmd, int arg)
{
    switch (cmd) {
    case XMP_ORD_PREV:
	if (xmp_ctl->pos > 0)
	    xmp_ctl->pos--;
	return xmp_ctl->pos;
    case XMP_ORD_NEXT:
	if (xmp_ctl->pos < xxh->len)
	    xmp_ctl->pos++;
	return xmp_ctl->pos;
    case XMP_ORD_SET:
	if (arg < xxh->len && arg >= 0)
	    xmp_ctl->pos = arg;
	return xmp_ctl->pos;
    case XMP_MOD_STOP:
	xmp_ctl->pos = -2;
	break;
    case XMP_MOD_PAUSE:
	xmp_ctl->pause ^= 1;
	return xmp_ctl->pause;
    case XMP_MOD_RESTART:
	xmp_ctl->pos = -1;
	break;
    case XMP_GVOL_DEC:
	if (xmp_ctl->volume > 0)
	    xmp_ctl->volume--;
	return xmp_ctl->volume;
    case XMP_GVOL_INC:
	if (xmp_ctl->volume < 64)
	    xmp_ctl->volume++;
	return xmp_ctl->volume;
    case XMP_TIMER_STOP:
	xmp_drv_stoptimer ();
	break;
    case XMP_TIMER_RESTART:
	xmp_drv_starttimer ();
	break;
    }

    return XMP_OK;
}


int xmp_play_module ()
{
    time_t t0, t1;
    int t, i;

    time (&t0);
    xmpi_player_start ();
    time (&t1);
    t = difftime (t1, t0);

    xmp_ctl->start = 0;

    if (med_vol_table) {
	for (i = 0; i < xxh->ins; i++)
	     if (med_vol_table[i])
		free (med_vol_table[i]);
	free (med_vol_table);
    }

    if (med_wav_table) {
	for (i = 0; i < xxh->ins; i++)
	     if (med_wav_table[i])
		free (med_wav_table[i]);
	free (med_wav_table);
    }

    _D (_D_INFO "Freeing memory");

    for (i = 0; i < xxh->trk; i++)
	free (xxt[i]);
    for (i = 0; i < xxh->pat; i++)
	free (xxp[i]);
    for (i = 0; i < xxh->ins; i++) {
	free (xxfe[i]);
	free (xxpe[i]);
	free (xxae[i]);
	free (xxi[i]);
    }
    free (xxt);
    free (xxp);
    free (xxi);
    if (xxh->smp > 0)
	free (xxs);
    free (xxim);
    free (xxih);
    free (xxfe);
    free (xxpe);
    free (xxae);
    free (xxh);

    return t;
}


void xmp_get_driver_cfg (int *srate, int *res, int *chn, int *itpt)
{
    *srate = xmp_ctl->memavl ? 0 : xmp_ctl->freq;
    *res = xmp_ctl->resol ? xmp_ctl->resol : 8 /* U_LAW */;
    *chn = xmp_ctl->outfmt & XMP_FMT_MONO ? 1 : 2;
    *itpt = !!(xmp_ctl->fetch & XMP_CTL_ITPT);
}


int xmp_verbosity_level (int i)
{
    int tmp;

    tmp = xmp_ctl->verbose;
    xmp_ctl->verbose = i;

    return tmp;
}


int xmp_seek_time (int time)
{
    int i, t;
    /* _D("seek to %d, total %d", time, xmp_cfg.time); */

    time *= 1000;
    for (i = 0; i < xxh->len; i++) {
	t = xxo_info[i].time;

	_D("%2d: %d %d", i, time, t);

	if (t > time) {
	    if (i > 0)
		i--;
	    xmp_ord_set (i);
	    return XMP_OK;
	}
    }
    return -1;
}
