/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * @file    edge_common.c
 * @date    01 Aug 2022
 * @brief   Common functions for edge sink and src
 * @author  Yechan Choi <yechan9.choi@samsung.com>
 * @see     http://github.com/nnstreamer/nnstreamer
 * @bug     No known bugs
 *
 */

/**
 * @brief register GEnumValue array for edge protocol property handling
 */
#include "edge_common.h"

GType
gst_edge_get_connect_type (void)
{
  static GType protocol = 0;
  if (protocol == 0) {
    static GEnumValue protocols[] = {
      {NNS_EDGE_CONNECT_TYPE_TCP, "TCP",
          "Directly sending stream frames via TCP connections."},
            /** @todo support UDP, MQTT and HYBRID
             *  {NNS_EDGE_CONNECT_TYPE_UDP, "UDP",
             *      "Directly sending stream frames via UDP connections."},
             *  {NNS_EDGE_CONNECT_TYPE_MQTT, "MQTT",
             *      "Connect and send stream frames with MQTT brokers."},
             *  {NNS_EDGE_CONNECT_TYPE_HYBRID, "HYBRID",
             *      "Connect with MQTT brokers and directly sending stream frames via TCP connections."},
             */
      {0, NULL, NULL},
    };
    protocol = g_enum_register_static ("edge_protocol", protocols);
  }

  return protocol;
}
