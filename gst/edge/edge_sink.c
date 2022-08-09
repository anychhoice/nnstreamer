/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * @file    edge_sink.h
 * @date    01 Aug 2022
 * @brief   Publish incoming streams
 * @author  Yechan Choi <yechan9.choi@samsung.com>
 * @see     http://github.com/nnstreamer/nnstreamer
 * @bug     No known bugs
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "edge_sink.h"

GST_DEBUG_CATEGORY_STATIC (gst_edgesink_debug);
#define GST_CAT_DEFAULT gst_edgesink_debug

/**
 * @brief the capabilities of the inputs.
 */
static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

enum
{
  PROP_0,

  PROP_HOST,
  PROP_PORT,

  /** @todo define props */

  PROP_LAST
};

#define gst_edgesink_parent_class parent_class
G_DEFINE_TYPE (GstEdgeSink, gst_edgesink, GST_TYPE_BASE_SINK);

static void gst_edgesink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSepc * pspec);

static void gst_edgesink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static void gst_edgesink_finalize (GObject * object);

static gboolean gst_edgesink_start (GstBaseSink * basesink);
static gboolean gst_edgesink_stop (GstBaseSink * basesink);
static gboolean gst_edgesink_query (GstBaseSink * basesink);
static GstFlowReturn gst_edgesink_render (GstBaseSink * basesink,
    GstBuffer * buffer);
static GstFlowReturn gst_edgesink_render_list(GstBaseSink * basesink, GstBufferList * list);
static gboolean gst_edgesink_event(GstBaseSink* basesink, GstEvent *event);
static gboolean gst_edgesink_set_caps (GstBaseSink * basesink, GstCaps * caps);

static gchar * gst_edgesink_get_host(GstEdgeSink * self);
static void gst_edgesink_set_host(GstEdgeSink * self, const gchar* host);

static guint16 gst_edgesink_get_port(GstEdgSink* self);
static void gst_edgesink_set_port(GstEdgeSink* self, const guint16 port);

/**
 * @brief initialize the class
 */
static void
gst_edgesink_class_init (GstEdgeSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gselement_class;
  GstBaseSinkClass *gstbasesink_class;

  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstelement_class = (GstElementClass *) gstbasesink_class;
  gobject_class = (GObjectClass *) gstelement_class;

  gobject_class->set_property = gst_edgesink_set_property;
  gobject_class->get_property = gst_edgesink_get_property;
  gobject_class->finalize = gst_edgesink_finalize;

  g_object_class_install_property(gobject_class, PROP_HOST, g_param_spec_string(
    "host", "Host", "The hostname to send", DEFAULT_HOST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
  ));
  g_object_class_install_property(gobject_class, PROP_PORT, g_param_spec_uint(
    "port", "Port", "The port to send (0=random available port)", 0, 65535, DEFAULT_PORT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
  ));

  /** @todo set props */

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));

  gst_element_class_set_static_metadata (gstelement_class,
      "EdgeSink", "Sink/Edge",
      "Publish incoming streams", "Samsung Electronics Co., Ltd.");

  gstbasesink_class->start = gst_edgesink_start;
  gstbasesink_class->stop = gst_edgesink_stop;
  gstbasesink_class->query = gst_edgesink_query;
  gstbasesink_class->render = gst_edgesink_render;
  gstbasesink_class->render_list = gst_edgesink_render_list;
  gstbasesink_class->event = gst_edgesink_event;
  gstbasesink_class->set_caps = gst_edgesink_set_caps;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT,
      GST_EDGE_ELEM_NAME_SINK, 0, "Edge sink");
}

/**
 * @brief initialize the new element
 */
static void
gst_edgesink_init (GstEdgeSink * sink)
{
  self->host = g_strdup(DEFAULT_HOST);
  self->port = DEFAULT_PORT;
  /** @todo set default value of props */
}

/**
 * @brief set property
 */
static void
gst_edgesink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstEdgeSink *self = GST_EDGESINK (object);

  switch (prop_id) {
    case PROP_HOST:
      gst_edgesink_set_host(self, g_value_get_string(value));
      break;
    case PROP_PORT:
      gst_edgesink_set_port(self, g_value_get_uint(value));
      break;
    
    /** @todo set prop */
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * @brief get property
 */
static void
gst_edgesink_get_property (GObject * object, guint prop_id, GValue * value,
    GParmSpec * pspec)
{
  GstEdgeSInk *self = GST_EDGESINK (object);

  switch (prop_id) {
    case PROP_HOST:
      g_value_set_string(value, gst_edgesink_get_host(self));
      break;
    case PROP_PORT:
      g_value_set_uint(value, gst_edgesink_get_port(self));
      break;
    
    /** @todo props */
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * @brief finalize the object
 */
static void
gst_edgesink_finalize (GObject * object)
{
  GstEdgeSink *self = GST_EDGESINK (object);
  g_free(self->host);
  self->host = NULL;
  
  /** @todo finalize - free all pointer in element */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * @brief start processing of edgesink
 */
static gboolean
gst_edgesink_start (GstBaseSink * basesink)
{
  GstEdgeSink *sink = GST_EDGESINK (basesink);

  /** @todo start */
}

/**
 * @brief stop processing of edgesink
 */
static gboolean gst_edgesink_stop(GstBaseSink * basesink) {
  /** @todo stop */
}

static gboolean gst_edgesink_query(GstBaseSink * basesink, GstQuery *query) {
  /** @todo query */
}

/**
 * @brief render buffer, send buffer
 */
static GstFlowReturn
gst_edgesink_render (GstBaseSink * basesink, GstBuffer * bufffer)
{
  /** @todo render, send data */
}

/**
 * @brief process GstBufferList (instead of a single buffer)
 */
static GstFlowReturn gst_edgesink_render_list(GstBaseSink * basesink, GstBufferList *list) {
  /** @todo render list (instead of a single buffer) */
}

/**
 * @brief handle events
 */
static gboolean gst_edgesink_event(GstBaseSink * bassink, GstEvent *event) {
  /** @todo handle events */
}

/**
 * @brief An implementation of the set_caps vmethod in GstBaseSinkClass
 */
static gboolean
gst_edgesink_set_caps (GstBaseSink * basesink, GstCaps * caps)
{
  GstEdgeSink *sink = GST_EDGESINK(basesink);
  gchar *caps_str, *prev_caps_str, *new_caps_str;

  caps_str = gst_caps_to_string(caps);

  nns_edge_get_info(sink->edge_h, "CAPS", &prev_caps_str);
  if(!prev_caps_str) {
    prev_caps_str = g_strdup("");
  }
  new_caps_str = g_strdup_printf("%s@edge_sink_caps@%s", prev_caps_str, caps_str);
  int set_rst = nns_edge_set_info(sink->edge_h, "CAPS", new_caps_str);

  g_free(prev_caps_str);
  g_free(new_caps_str);
  g_free(caps_str);

  return set_rst == NNS_EDGE_ERROR_NONE;
}

/**
 * @brief getter for the 'host' property.
 */
static gchar * gst_edgesink_get_host(GstEdgeSink * self) {
  return self->host;
}

/**
 * @brief setter for the 'host' property.
 */
static void gst_edgesink_set_host(GstEdgeSink * self, const gchar* host) {
  if(!g_value_get_string(value)) {
    nns_logw("host propery cannot be NULL");
    return;
  }
  g_free (self->host);
  self->host = g_strdup(host);
}

/**
 * @brief getter for the 'port' property.
 */
static guint16 gst_edgesink_get_port(GstEdgSink* self) {
  return self->port;
}

/**
 * @brief setter for the 'port' property.
 */
static void gst_edgesink_set_port(GstEdgeSink* self, const guint16 port) {
  self->port = port;
}
