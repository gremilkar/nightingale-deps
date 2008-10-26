/* GStreamer
 * Copyright (C)  2005 Sebastien Moutte <sebastien@moutte.net>
 * Copyright (C) 2007 Pioneers of the Inevitable <songbird@songbirdnest.com>
 *
 * gstdirectsoundsink.h: 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * The development of this code was made possible due to the involvement
 * of Pioneers of the Inevitable, the creators of the Songbird Music player
 *
 * 
 */

#ifndef __GST_DIRECTSOUNDSINK_H__
#define __GST_DIRECTSOUNDSINK_H__

#include <gst/gst.h>
#include <gst/audio/gstaudiosink.h>
#include <gst/interfaces/mixer.h>

#include <windows.h>
#include <dxerr9.h>
#include <dsound.h>

G_BEGIN_DECLS
#define GST_TYPE_DIRECTSOUND_SINK            (gst_directsound_sink_get_type())
#define GST_DIRECTSOUND_SINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DIRECTSOUND_SINK,GstDirectSoundSink))
#define GST_DIRECTSOUND_SINK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DIRECTSOUND_SINK,GstDirectSoundSinkClass))
#define GST_IS_DIRECTSOUND_SINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DIRECTSOUND_SINK))
#define GST_IS_DIRECTSOUND_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DIRECTSOUND_SINK))
typedef struct _GstDirectSoundSink GstDirectSoundSink;
typedef struct _GstDirectSoundSinkClass GstDirectSoundSinkClass;

#define GST_DSOUND_LOCK(obj)	(g_mutex_lock (obj->dsound_lock))
#define GST_DSOUND_UNLOCK(obj)	(g_mutex_unlock (obj->dsound_lock))

struct _GstDirectSoundSink
{
  GstAudioSink sink;

  /* directsound object interface pointer */
  LPDIRECTSOUND pDS;

  /* directsound sound object interface pointer */
  LPDIRECTSOUNDBUFFER pDSBSecondary;

  /* directSound buffer size */
  guint buffer_size;

  /* offset of the circular buffer where we must write next */
  guint current_circular_offset;

  guint bytes_per_sample;

  /* current volume setup by mixer interface */
  glong volume;

  /* tracks list of our mixer interface implementation */
  GList *tracks;

  /* lock used to protect writes and resets */
  GMutex *dsound_lock;

  gboolean first_buffer_after_reset;

  gboolean has_set_thread_priority;
};

struct _GstDirectSoundSinkClass
{
  GstAudioSinkClass parent_class;
};

GType gst_directsound_sink_get_type (void);

G_END_DECLS
#endif /* __GST_DIRECTSOUNDSINK_H__ */
