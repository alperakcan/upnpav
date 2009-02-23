/***************************************************************************
    begin                : Sun Jun 01 2008
    copyright            : (C) 2008 by Alper Akcan
    email                : alper.akcan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License as        *
 *   published by the Free Software Foundation; either version 2.1 of the  *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ithread.h>

#include <gst/gst.h>

#include "common.h"

static GstElement *play;
static char *gsuri = NULL;
static int gendofstream = 0;
static gchar *audiosink = NULL;
static gchar *videosink = NULL;

static void scan_caps (render_module_t *gstreamer, const GstCaps *caps)
{
	guint i;

	g_return_if_fail(caps != NULL);

	if (gst_caps_is_any(caps)) {
		return;
	}
	if (gst_caps_is_empty(caps)) {
		return;
	}

	for (i = 0; i < gst_caps_get_size(caps); i++) {
		GstStructure *structure = gst_caps_get_structure(caps, i);
		render_register_mimetype(gstreamer->render, gst_structure_get_name(structure));
	}
}

static void scan_pad_templates_info (render_module_t *gstreamer, GstElement *element, GstElementFactory *factory)
{
	const GList *pads;
	GstPadTemplate *padtemplate;
	GstPad *pad;
	GstElementClass *class;
	
	class = GST_ELEMENT_GET_CLASS(element);

	if (!class->numpadtemplates) {
		return;
	}

	pads = class->padtemplates;
	while (pads) {
		padtemplate = (GstPadTemplate *) (pads->data);
		pad = (GstPad *) (pads->data);
		pads = g_list_next(pads);

		if ((padtemplate->direction == GST_PAD_SINK) &&
		    ((padtemplate->presence == GST_PAD_ALWAYS) ||
		     (padtemplate->presence == GST_PAD_SOMETIMES) ||
		     (padtemplate->presence == GST_PAD_REQUEST)) &&
		    (padtemplate->caps)) {
			scan_caps(gstreamer, padtemplate->caps);
		}
	}
}

static void render_gstreamer_scan_mime_list (render_module_t *gstreamer)
{
	GList *plugins;
	GstRegistry *registry;
	
	debugf("scanning mime list");
	
	registry = gst_registry_get_default();
	plugins = gst_default_registry_get_plugin_list();

	debugf("looking for plugins");
	while (plugins) {
		GList *features;
		GstPlugin *plugin;

		plugin = (GstPlugin *) (plugins->data);
		plugins = g_list_next(plugins);

		features = gst_registry_get_feature_list_by_plugin(registry, gst_plugin_get_name(plugin));

		debugf("looking for features");
		while (features) {
			GstPluginFeature *feature;

			feature = GST_PLUGIN_FEATURE(features->data);

			if (GST_IS_ELEMENT_FACTORY(feature)) {
				GstElementFactory *factory;
				GstElement *element;
				factory = GST_ELEMENT_FACTORY(feature);
				element = gst_element_factory_create(factory, NULL);
				if (element) {
					scan_pad_templates_info(gstreamer, element, factory);
				}
			}

			features = g_list_next(features);
		}
	}
	
	debugf("done scanning");
}

static const char * gststate_get_name (GstState state)
{
	switch (state) {
		case GST_STATE_VOID_PENDING:
			return "VOID_PENDING";
		case GST_STATE_NULL:
			return "NULL";
		case GST_STATE_READY:
			return "READY";
		case GST_STATE_PAUSED:
			return "PAUSED";
		case GST_STATE_PLAYING:
			return "PLAYING";
		default:
			return "Unknown";
	}
}

static render_state_t gststate_get_renderstate (GstState state)
{
	switch (state) {
		case GST_STATE_VOID_PENDING:
			return RENDER_STATE_WAITING;
		case GST_STATE_NULL:
			return RENDER_STATE_WAITING;
		case GST_STATE_READY:
			return RENDER_STATE_STOPPED;
		case GST_STATE_PAUSED:
			return RENDER_STATE_PAUSED;
		case GST_STATE_PLAYING:
			return RENDER_STATE_PLAYING;
		default:
			return RENDER_STATE_UNKNOWN;
	}
}

static GstBusSyncReply mediarender_gstreamer_callback (GstBus *bus, GstMessage *msg, gpointer data)
{
	GError *err;
	gchar *debug;
	GstState pending;
	GstState oldstate;
	GstState newstate;
	GstObject *msgSrc;
	gchar *msgSrcName;
	GstMessageType msgType;

	msgType = GST_MESSAGE_TYPE(msg);
	msgSrc = GST_MESSAGE_SRC(msg);
	msgSrcName = GST_OBJECT_NAME(msgSrc);
	
	if (strcmp((char *) msgSrcName, "play") != 0) {
		return GST_BUS_PASS;
	}

	switch (msgType) {
		case GST_MESSAGE_EOS:
			debugf("GStreamer: %s: End-of-stream", msgSrcName);
			gendofstream = 1;
			break;
		case GST_MESSAGE_ERROR:
			gst_message_parse_error(msg, &err, &debug);
			g_free(debug);
			debugf("GStreamer: %s: Error: %s", msgSrcName, err->message);
			g_error_free(err);
			break;
		case GST_MESSAGE_STATE_CHANGED:
			gst_message_parse_state_changed(msg, &oldstate, &newstate, &pending);
			debugf("GStreamer: %s: State change: OLD: '%s', NEW: '%s', PENDING: '%s'", msgSrcName, gststate_get_name(oldstate), gststate_get_name(newstate), gststate_get_name(pending));
			break;
		default:
			//debugf("GStreamer: %s: unhandled message type %d (%s)", msgSrcName, msgType, gst_message_type_get_name(msgType));
			break;
	}
	
	return GST_BUS_PASS;
}

static int render_gstreamer_init (render_module_t *gstreamer, int argc, char *argv[])
{
	GstBus *bus;
	
	debugf("initializing gstreamer");

	gst_init(&argc, &argv);
	
	render_gstreamer_scan_mime_list(gstreamer);

	debugf("creating playbin factory");
	play = gst_element_factory_make("playbin", "play");

	bus = gst_pipeline_get_bus(GST_PIPELINE(play));
	gst_bus_set_sync_handler(bus, mediarender_gstreamer_callback, NULL);
	gst_object_unref(bus);

	if (audiosink != NULL) {
		GstElement *sink = NULL;
		debugf("setting audio sink to %s", audiosink);
		sink = gst_element_factory_make(audiosink, "sink");
		g_object_set(G_OBJECT (play), "audio-sink", sink, NULL);
	}
	if (videosink != NULL) {
		GstElement *sink = NULL;
		debugf("setting video sink to %s", videosink);
		sink = gst_element_factory_make(videosink, "sink");
		g_object_set(G_OBJECT (play), "video-sink", sink, NULL);
	}

	if (gst_element_set_state(play, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE) {
		debugf("pipeline doesn't want to get ready");
	}
	
	debugf("initialized gstreamer factory");

	return 0;
}

static  int render_gstreamer_uninit (render_module_t *gstreamer)
{
	debugf("uninitializing gstreamer");
	gst_element_set_state(play, GST_STATE_NULL);
	gst_object_unref(GST_OBJECT(play));
	gst_deinit();
	return 0;
}

static int render_gstreamer_seturi (char *uri)
{
	if (gsuri != NULL) {
		free(gsuri);
		gsuri = NULL;
	}
	gsuri = uri_escape(uri);
	debugf("setting gsuri as '%s'", gsuri);
	return 0;
}

static int render_gstreamer_play (render_module_t *gstreamer, char *uri)
{
	int result = -1;
	
	debugf("mediarender play '%s'", uri);
	if (render_gstreamer_seturi(uri)) {
		return -1;
	}
	if (gst_element_set_state(play, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE) {
		debugf("setting play state failed");
                goto out;
	}
	g_object_set(G_OBJECT(play), "uri", gsuri, NULL);
	if (gst_element_set_state(play, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		debugf("setting play state failed");
		goto out;
	}
	gendofstream = 0;
	result = 0;
out:	return result;
}

static int render_gstreamer_pause (render_module_t *gstreamer)
{
	return -1;
}

static int render_gstreamer_stop (render_module_t *gstreamer)
{
	debugf("mediarender stop");
	if (gst_element_set_state(play, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE) {
		return -1;
	} else {
		return 0;
	}
}

static int render_gstreamer_seek (render_module_t *gstreamer, long offset)
{
	return -1;
}

static int render_gstreamer_info (render_module_t *gstreamer, render_info_t *info)
{
	gint64 len;
	gint64 pos;
	GstFormat fmt;
	GstState state;
	GstState pending;
	GstStateChangeReturn rc;
	debugf("render gstreamer info");
	fmt = GST_FORMAT_TIME;
	if (gendofstream == 1) {
		render_gstreamer_stop(gstreamer);
	}
	if (gst_element_query_position(play, &fmt, &pos) == TRUE) {
		info->position = GST_TIME_AS_SECONDS(pos);
	} else {
		info->position = (unsigned long) -1;
	}
	if (gst_element_query_duration(play, &fmt, &len) == TRUE) {
		info->duration = GST_TIME_AS_SECONDS(len);
	} else {
		info->duration = (unsigned long) -1;
	}
	rc = gst_element_get_state(play, &state, &pending, GST_CLOCK_TIME_NONE);
	if (rc != GST_STATE_CHANGE_SUCCESS) {
		info->state = RENDER_STATE_UNKNOWN;
	} else {
		info->state = gststate_get_renderstate(state);
	}
	return 0;
}

render_module_t render_gstreamer = {
	.author = "Alper Akcan",
	.description = "GStreamer Rendering Plugin",
	.name = "gstreamer",
	.init = render_gstreamer_init,
	.uninit = render_gstreamer_uninit,
	.play = render_gstreamer_play,
	.seek = render_gstreamer_seek,
	.pause = render_gstreamer_pause,
	.stop = render_gstreamer_stop,
	.info = render_gstreamer_info,
};
