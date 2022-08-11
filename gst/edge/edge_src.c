/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * @file    edge_src.c
 * @date    02 Aug 2022
 * @brief   Subscribe and push incoming data to the GStreamer pipeline
 * @author  Yechan Choi <yechan9.choi@samsung.com>
 * @see     http://github.com/nnstreamer/nnstreamer
 * @bug     No known bugs
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "edge_src.h"

GST_DEBUG_CATEGORY_STATIC (gst_edgesrc_debug);
#define GST_CAT_DEFAULT gst_edgesrc_debug

/**
 * @brief the capabilities of the outputs 
 */
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

/**
 * @brief edgesrc properties
 */
enum
{
  PROP_0,

  PROP_HOST,
  PROP_PORT,
  PROP_CONNECT_TYPE,

  PROP_LAST
};

#define gst_edgesrc_parent_class parent_class
G_DEFINE_TYPE (GstEdgeSrc, gst_edgesrc, GST_TYPE_BASE_SRC);

static void gst_edgesrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_edgesrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_edgesrc_class_finalize (GObject * object);

static GstStateChangeReturn gst_edgesrc_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_edgesrc_start (GstBaseSrc * basesrc);
static gboolean gst_edgesrc_stop (GstBaseSrc * basesrc);
static GstCaps *gst_edgesrc_get_caps (GstBaseSrc * basesrc, GstCaps * filter);
static void gst_edgesrc_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_edgesrc_is_seekable (GstBaseSrc * basesrc);
static GstFlowReturn gst_edgesrc_create (GstBaseSrc * basesrc, guint64 offset,
    guint size, GstBuffer ** out_buf);
static gboolean gst_edgesrc_query (GstBaseSrc * basesrc, GstQuery * query);

static gchar *gst_edgesrc_get_host (GstEdgeSrc * self);
static void gst_edgesrc_set_host (GstEdgeSrc * self, const gchar * host);

static guint16 gst_edgesrc_get_port (GstEdgeSrc * self);
static void gst_edgesrc_set_port (GstEdgeSrc * self, const guint16 port);

static nns_edge_connect_type_e gst_edgesrc_get_connect_type (GstEdgeSrc * self);
static void gst_edgesrc_set_connect_type (GstEdgeSrc * self,
    const nns_edge_connect_type_e connect_type);

/**
 * @brief initialize the class
 */
static void
gst_edgesrc_class_init (GstEdgeSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);

  gobject_class->set_property = gst_edgesrc_set_property;
  gobject_class->get_property = gst_edgesrc_get_property;
  gobject_class->finalize = gst_edgesrc_class_finalize;

  g_object_class_install_property (gobject_class, PROP_HOST,
      g_param_spec_string ("host", "Host",
          "A host address to receive the packets from edgesink",
          DEFAULT_HOST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PORT,
      g_param_spec_uint ("port", "Port",
          "A port to receive the packets from edgesink (0=random available port)",
          0, 65535, DEFAULT_PORT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CONNECT_TYPE,
      g_param_spec_enum ("connect-type", "Connect Type",
          "The connections type between edgesink and edgesrc.",
          GST_TYPE_EDGE_CONNECT_TYPE, DEFAULT_CONNECT_TYPE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&srctemplate));

  gst_element_class_set_static_metadata (gstelement_class,
      "EdgeSrc", "Source/Edge",
      "Subscribe and push incoming streams", "Samsung Electronics Co., Ltd.");

  gstelement_class->change_state = gst_edgesrc_change_state;

  gstbasesrc_class->start = gst_edgesrc_start;
  gstbasesrc_class->stop = gst_edgesrc_stop;
  gstbasesrc_class->get_caps = gst_edgesrc_get_caps;
  gstbasesrc_class->get_times = gst_edgesrc_get_times;
  gstbasesrc_class->is_seekable = gst_edgesrc_is_seekable;
  gstbasesrc_class->create = gst_edgesrc_create;
  gstbasesrc_class->query = gst_edgesrc_query;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT,
      GST_EDGE_ELEM_NAME_SRC, 0, "Edge src");
}

/**
 * @brief initialize edgesrc element 
 */
static void
gst_edgesrc_init (GstEdgeSrc * self)
{
  GstBaseSrc *basesrc = GST_BASE_SRC (self);

  gst_base_src_set_format (basesrc, GST_FORMAT_TIME);
  gst_base_src_set_async (basesrc, FALSE);

  self->host = g_strdup (DEFAULT_HOST);
  self->port = DEFAULT_PORT;
  self->msg_queue = g_async_queue_new ();
  self->connect_type = DEFAULT_CONNECT_TYPE;
}

/**
 * @brief set property
 */
static void
gst_edgesrc_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstEdgeSrc *self = GST_EDGESRC (object);

  switch (prop_id) {
    case PROP_HOST:
      gst_edgesrc_set_host (self, g_value_get_string (value));
      break;
    case PROP_PORT:
      gst_edgesrc_set_port (self, g_value_get_uint (value));
      break;
    case PROP_CONNECT_TYPE:
      gst_edgesrc_set_connect_type (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * @brief get property
 */
static void
gst_edgesrc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstEdgeSrc *self = GST_EDGESRC (object);

  switch (prop_id) {
    case PROP_HOST:
      g_value_set_string (value, gst_edgesrc_get_host (self));
      break;
    case PROP_PORT:
      g_value_set_uint (value, gst_edgesrc_get_port (self));
      break;
    case PROP_CONNECT_TYPE:
      g_value_set_enum (value, gst_edgesrc_get_connect_type (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * @brief finalize the object
 */
static void
gst_edgesrc_class_finalize (GObject * object)
{
  GstEdgeSrc *self = GST_EDGESRC (object);
  nns_edge_data_h data_h;

  if (self->host) {
    g_free (self->host);
    self->host = NULL;
  }

  if (self->msg_queue) {
    while ((data_h = g_async_queue_try_pop (self->msg_queue))) {
      nns_edge_data_destroy (data_h);
    }
    g_async_queue_unref (self->msg_queue);
    self->msg_queue = NULL;
  }

  if (self->edge_h) {
    nns_edge_release_handle (self->edge_h);
    self->edge_h = NULL;
  }
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * @brief handle edgesrc's state change
 */
static GstStateChangeReturn
gst_edgesrc_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GstEdgeSrc *self = GST_EDGESRC (element);

  UNUSED(self);

  switch (transition) {
  /** @todo handle transition  */
  
  default:
    break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
  /** @todo handle transition */
  default:
    break;
  }

  return ret;
}

/**
 * @brief nnstreamer-edge event callback.
 */
static int
_nns_edge_event_cb (nns_edge_event_h event_h, void *user_data)
{
  nns_edge_event_e event_type;
  int ret = NNS_EDGE_ERROR_NONE;

  GstEdgeSrc *self = GST_EDGESRC (user_data);
  if (0 != nns_edge_event_get_type (event_h, &event_type)) {
    nns_loge ("Failed to get event type!");
    return NNS_EDGE_ERROR_UNKNOWN;
  }

  switch (event_type) {
    case NNS_EDGE_EVENT_NEW_DATA_RECEIVED:
    {
      nns_edge_data_h data;

      nns_edge_event_parse_new_data (event_h, &data);
      g_async_queue_push (self->msg_queue, data);
      break;
    }
    default:
      break;
  }

  return ret;
}


/**
 * @brief start edgesrc, called when state changed null to ready
 */
static gboolean
gst_edgesrc_start (GstBaseSrc * basesrc)
{
  GstEdgeSrc *self = GST_EDGESRC (basesrc);

  int ret;
  char *port = NULL;

  ret =
      nns_edge_create_handle ("TEMP_ID", self->connect_type,
      NNS_EDGE_FLAG_RECV, &self->edge_h);

  if (NNS_EDGE_ERROR_NONE != ret) {
    nns_loge ("Failed to get nnstreamer edge handle.");

    if (self->edge_h) {
      nns_edge_release_handle (self->edge_h);
      self->edge_h = NULL;
    }

    return FALSE;
  }

  nns_edge_set_info (self->edge_h, "DEST_HOST", self->host);
  port = g_strdup_printf ("%d", self->port);
  nns_edge_set_info (self->edge_h, "DEST_PORT", port);
  g_free (port);

  nns_edge_set_event_callback (self->edge_h, _nns_edge_event_cb, self);

  if (0 != nns_edge_start (self->edge_h)) {
    nns_loge
        ("Failed to start NNStreamer-edge. Please check server IP and port");
    return FALSE;
  }

  if (0 != nns_edge_connect (self->edge_h, self->host, self->port)) {
    nns_loge ("Failed to connect to edge server!");
    return FALSE;
  }

  return TRUE;
}

/**
 * @brief stop edgesrc, called when state changed ready to null
 */
static gboolean
gst_edgesrc_stop (GstBaseSrc * basesrc)
{
  GstEdgeSrc *self = GST_EDGESRC (basesrc);
  /** @todo stop */

  UNUSED(self);
  return TRUE;
}

/**
 * @brief Get caps of subclass
 */
static GstCaps *
gst_edgesrc_get_caps (GstBaseSrc * basesrc, GstCaps * filter)
{
  GstPad *pad = GST_BASE_SRC_PAD (basesrc);
  GstCaps *cur_caps = gst_pad_get_current_caps (pad);
  GstCaps *caps = gst_caps_new_any ();
  UNUSED (filter);

  if (cur_caps) {
    GstCaps *intersection =
        gst_caps_intersect_full (cur_caps, caps, GST_CAPS_INTERSECT_FIRST);

    gst_caps_unref (cur_caps);
    gst_caps_unref (caps);
    caps = intersection;
  }

  return caps;
}

/**
 * @brief Return the time information of the given buffer
 */
static void
gst_edgesrc_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstClockTime sync_ts;
  GstClockTime duration;
  UNUSED (basesrc);

  sync_ts = GST_BUFFER_DTS (buffer);
  duration = GST_BUFFER_DURATION (buffer);

  if (!GST_CLOCK_TIME_IS_VALID (sync_ts))
    sync_ts = GST_BUFFER_PTS (buffer);

  if (GST_CLOCK_TIME_IS_VALID (sync_ts)) {
    *start = sync_ts;
    if (GST_CLOCK_TIME_IS_VALID (duration)) {
      *end = sync_ts + duration;
    }
  }
}

/**
 * @brief Check if source supports seeking
 */
static gboolean
gst_edgesrc_is_seekable (GstBaseSrc * basesrc)
{
  UNUSED (basesrc);

  return FALSE;
}

/**
 * @brief Create a buffer containing the subscribed data
 */
static GstFlowReturn
gst_edgesrc_create (GstBaseSrc * basesrc, guint64 offset, guint size,
    GstBuffer ** out_buf)
{
  GstEdgeSrc *self = GST_EDGESRC (basesrc);

  nns_edge_data_h data_h;
  GstBuffer *buffer = NULL;
  guint i, num_data;
  int ret;

  UNUSED (offset);
  UNUSED (size);

  data_h = g_async_queue_pop (self->msg_queue);

  if (!data_h) {
    nns_loge ("Failed to get message from the edgesrc message queue.");
    goto done;
  }

  ret = nns_edge_data_get_count (data_h, &num_data);
  if (ret != NNS_EDGE_ERROR_NONE || num_data == 0) {
    nns_loge ("Failed to get the number of memories of the edge data.");
    goto done;
  }

  buffer = gst_buffer_new ();
  for (i = 0; i < num_data; i++) {
    void *data = NULL;
    size_t data_len = 0;
    gpointer new_data;

    nns_edge_data_get (data_h, i, &data, &data_len);
    new_data = _g_memdup (data, data_len);

    gst_buffer_append_memory (buffer,
        gst_memory_new_wrapped (0, new_data, data_len, 0, data_len, new_data,
            g_free));
  }

done:
  if (data_h)
    nns_edge_data_destroy (data_h);

  if (buffer == NULL) {
    nns_loge ("Failed to get buffer to push to the edgesrc.");
    return GST_FLOW_ERROR;
  }

  *out_buf = buffer;

  return GST_FLOW_OK;
}

/**
 * @brief An implementation of the GstBaseSrc vmethod that handles queries
 */
static gboolean
gst_edgesrc_query (GstBaseSrc * basesrc, GstQuery * query)
{
  gboolean res = FALSE;

  res = GST_BASE_SRC_CLASS (parent_class)->query (basesrc, query);

  return res;
}

/**
 * @brief getter for the 'host' property.
 */
static gchar *
gst_edgesrc_get_host (GstEdgeSrc * self)
{
  return self->host;
}

/**
 * @brief setter for the 'host' property.
 */
static void
gst_edgesrc_set_host (GstEdgeSrc * self, const gchar * host)
{
  g_free (self->host);
  self->host = g_strdup (host);
}

/**
 * @brief getter for the 'port' property.
 */
static guint16
gst_edgesrc_get_port (GstEdgeSrc * self)
{
  return self->port;
}

/**
 * @brief setter for the 'port' property.
 */
static void
gst_edgesrc_set_port (GstEdgeSrc * self, const guint16 port)
{
  self->port = port;
}

/**
 * @brief getter for the 'connect_type' property.
 */
static nns_edge_connect_type_e
gst_edgesrc_get_connect_type (GstEdgeSrc * self)
{
  return self->connect_type;
}

/**
 * @brief setter for the 'connect_type' property.
 */
static void
gst_edgesrc_set_connect_type (GstEdgeSrc * self,
    const nns_edge_connect_type_e connect_type)
{
  self->connect_type = connect_type;
}
