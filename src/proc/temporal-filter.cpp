// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include "../include/librealsense2/hpp/rs_sensor.hpp"
#include "../include/librealsense2/hpp/rs_processing.hpp"
#include "source.h"
#include "option.h"
#include "environment.h"
#include "context.h"
#include "proc/synthetic-stream.h"
#include "proc/temporal-filter.h"

namespace librealsense
{
    const size_t PERSISTENCE_MAP_NUM = 9;

    // Look-up table that establishes pixel persistency criteria. Utilized in hole-filling phase
    static const std::vector<std::array<uint8_t, PRESISTENCY_LUT_SIZE>> _persistence_lut =
    {
        // index 0
        {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        },
        // index 1
        {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
        },

        // index 2
        {
        0x00, 0x00, 0x00, 0x0c, 0x00, 0x08, 0x18, 0x1c, 0x00, 0x00, 0x10, 0x1c, 0x30, 0x38, 0x38, 0x3c,
        0x00, 0x00, 0x00, 0x0c, 0x20, 0x28, 0x38, 0x3c, 0x60, 0x60, 0x70, 0x7c, 0x70, 0x78, 0x78, 0x7c,
        0x00, 0x00, 0x00, 0x0c, 0x00, 0x08, 0x18, 0x1c, 0x40, 0x40, 0x50, 0x5c, 0x70, 0x78, 0x78, 0x7c,
        0xc0, 0xc0, 0xc0, 0xcc, 0xe0, 0xe8, 0xf8, 0xfc, 0xe0, 0xe0, 0xf0, 0xfc, 0xf0, 0xf8, 0xf8, 0xfc,
        0x00, 0x02, 0x00, 0x0e, 0x00, 0x0a, 0x18, 0x1e, 0x00, 0x02, 0x10, 0x1e, 0x30, 0x3a, 0x38, 0x3e,
        0x80, 0x82, 0x80, 0x8e, 0xa0, 0xaa, 0xb8, 0xbe, 0xe0, 0xe2, 0xf0, 0xfe, 0xf0, 0xfa, 0xf8, 0xfe,
        0x81, 0x83, 0x81, 0x8f, 0x81, 0x8b, 0x99, 0x9f, 0xc1, 0xc3, 0xd1, 0xdf, 0xf1, 0xfb, 0xf9, 0xff,
        0xc1, 0xc3, 0xc1, 0xcf, 0xe1, 0xeb, 0xf9, 0xff, 0xe1, 0xe3, 0xf1, 0xff, 0xf1, 0xfb, 0xf9, 0xff,
        0x00, 0x06, 0x04, 0x0e, 0x00, 0x0e, 0x1c, 0x1e, 0x00, 0x06, 0x14, 0x1e, 0x30, 0x3e, 0x3c, 0x3e,
        0x00, 0x06, 0x04, 0x0e, 0x20, 0x2e, 0x3c, 0x3e, 0x60, 0x66, 0x74, 0x7e, 0x70, 0x7e, 0x7c, 0x7e,
        0x01, 0x07, 0x05, 0x0f, 0x01, 0x0f, 0x1d, 0x1f, 0x41, 0x47, 0x55, 0x5f, 0x71, 0x7f, 0x7d, 0x7f,
        0xc1, 0xc7, 0xc5, 0xcf, 0xe1, 0xef, 0xfd, 0xff, 0xe1, 0xe7, 0xf5, 0xff, 0xf1, 0xff, 0xfd, 0xff,
        0x03, 0x07, 0x07, 0x0f, 0x03, 0x0f, 0x1f, 0x1f, 0x03, 0x07, 0x17, 0x1f, 0x33, 0x3f, 0x3f, 0x3f,
        0x83, 0x87, 0x87, 0x8f, 0xa3, 0xaf, 0xbf, 0xbf, 0xe3, 0xe7, 0xf7, 0xff, 0xf3, 0xff, 0xff, 0xff,
        0x83, 0x87, 0x87, 0x8f, 0x83, 0x8f, 0x9f, 0x9f, 0xc3, 0xc7, 0xd7, 0xdf, 0xf3, 0xff, 0xff, 0xff,
        0xc3, 0xc7, 0xc7, 0xcf, 0xe3, 0xef, 0xff, 0xff, 0xe3, 0xe7, 0xf7, 0xff, 0xf3, 0xff, 0xff, 0xff,
        },

        // index 3
        {
        0x00, 0x00, 0x00, 0x1c, 0x00, 0x18, 0x38, 0x3c, 0x00, 0x10, 0x30, 0x3c, 0x70, 0x78, 0x78, 0x7c,
        0x00, 0x00, 0x20, 0x3c, 0x60, 0x78, 0x78, 0x7c, 0xe0, 0xf0, 0xf0, 0xfc, 0xf0, 0xf8, 0xf8, 0xfc,
        0x00, 0x02, 0x00, 0x1e, 0x40, 0x5a, 0x78, 0x7e, 0xc0, 0xd2, 0xf0, 0xfe, 0xf0, 0xfa, 0xf8, 0xfe,
        0xc1, 0xc3, 0xe1, 0xff, 0xe1, 0xfb, 0xf9, 0xff, 0xe1, 0xf3, 0xf1, 0xff, 0xf1, 0xfb, 0xf9, 0xff,
        0x00, 0x06, 0x04, 0x1e, 0x00, 0x1e, 0x3c, 0x3e, 0x80, 0x96, 0xb4, 0xbe, 0xf0, 0xfe, 0xfc, 0xfe,
        0x81, 0x87, 0xa5, 0xbf, 0xe1, 0xff, 0xfd, 0xff, 0xe1, 0xf7, 0xf5, 0xff, 0xf1, 0xff, 0xfd, 0xff,
        0x83, 0x87, 0x87, 0x9f, 0xc3, 0xdf, 0xff, 0xff, 0xc3, 0xd7, 0xf7, 0xff, 0xf3, 0xff, 0xff, 0xff,
        0xc3, 0xc7, 0xe7, 0xff, 0xe3, 0xff, 0xff, 0xff, 0xe3, 0xf7, 0xf7, 0xff, 0xf3, 0xff, 0xff, 0xff,
        0x00, 0x0e, 0x0c, 0x1e, 0x08, 0x1e, 0x3c, 0x3e, 0x00, 0x1e, 0x3c, 0x3e, 0x78, 0x7e, 0x7c, 0x7e,
        0x01, 0x0f, 0x2d, 0x3f, 0x69, 0x7f, 0x7d, 0x7f, 0xe1, 0xff, 0xfd, 0xff, 0xf9, 0xff, 0xfd, 0xff,
        0x03, 0x0f, 0x0f, 0x1f, 0x4b, 0x5f, 0x7f, 0x7f, 0xc3, 0xdf, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff,
        0xc3, 0xcf, 0xef, 0xff, 0xeb, 0xff, 0xff, 0xff, 0xe3, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff,
        0x07, 0x0f, 0x0f, 0x1f, 0x0f, 0x1f, 0x3f, 0x3f, 0x87, 0x9f, 0xbf, 0xbf, 0xff, 0xff, 0xff, 0xff,
        0x87, 0x8f, 0xaf, 0xbf, 0xef, 0xff, 0xff, 0xff, 0xe7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x87, 0x8f, 0x8f, 0x9f, 0xcf, 0xdf, 0xff, 0xff, 0xc7, 0xdf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xc7, 0xcf, 0xef, 0xff, 0xef, 0xff, 0xff, 0xff, 0xe7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        },
        // index 4
        {
        0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        },
        // index 5
        {
        0x00, 0x06, 0x0c, 0x0e, 0x18, 0x1e, 0x1c, 0x1e, 0x30, 0x36, 0x3c, 0x3e, 0x38, 0x3e, 0x3c, 0x3e,
        0x60, 0x66, 0x6c, 0x6e, 0x78, 0x7e, 0x7c, 0x7e, 0x70, 0x76, 0x7c, 0x7e, 0x78, 0x7e, 0x7c, 0x7e,
        0xc0, 0xc6, 0xcc, 0xce, 0xd8, 0xde, 0xdc, 0xde, 0xf0, 0xf6, 0xfc, 0xfe, 0xf8, 0xfe, 0xfc, 0xfe,
        0xe0, 0xe6, 0xec, 0xee, 0xf8, 0xfe, 0xfc, 0xfe, 0xf0, 0xf6, 0xfc, 0xfe, 0xf8, 0xfe, 0xfc, 0xfe,
        0x81, 0x87, 0x8d, 0x8f, 0x99, 0x9f, 0x9d, 0x9f, 0xb1, 0xb7, 0xbd, 0xbf, 0xb9, 0xbf, 0xbd, 0xbf,
        0xe1, 0xe7, 0xed, 0xef, 0xf9, 0xff, 0xfd, 0xff, 0xf1, 0xf7, 0xfd, 0xff, 0xf9, 0xff, 0xfd, 0xff,
        0xc1, 0xc7, 0xcd, 0xcf, 0xd9, 0xdf, 0xdd, 0xdf, 0xf1, 0xf7, 0xfd, 0xff, 0xf9, 0xff, 0xfd, 0xff,
        0xe1, 0xe7, 0xed, 0xef, 0xf9, 0xff, 0xfd, 0xff, 0xf1, 0xf7, 0xfd, 0xff, 0xf9, 0xff, 0xfd, 0xff,
        0x03, 0x07, 0x0f, 0x0f, 0x1b, 0x1f, 0x1f, 0x1f, 0x33, 0x37, 0x3f, 0x3f, 0x3b, 0x3f, 0x3f, 0x3f,
        0x63, 0x67, 0x6f, 0x6f, 0x7b, 0x7f, 0x7f, 0x7f, 0x73, 0x77, 0x7f, 0x7f, 0x7b, 0x7f, 0x7f, 0x7f,
        0xc3, 0xc7, 0xcf, 0xcf, 0xdb, 0xdf, 0xdf, 0xdf, 0xf3, 0xf7, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff,
        0xe3, 0xe7, 0xef, 0xef, 0xfb, 0xff, 0xff, 0xff, 0xf3, 0xf7, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff,
        0x83, 0x87, 0x8f, 0x8f, 0x9b, 0x9f, 0x9f, 0x9f, 0xb3, 0xb7, 0xbf, 0xbf, 0xbb, 0xbf, 0xbf, 0xbf,
        0xe3, 0xe7, 0xef, 0xef, 0xfb, 0xff, 0xff, 0xff, 0xf3, 0xf7, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff,
        0xc3, 0xc7, 0xcf, 0xcf, 0xdb, 0xdf, 0xdf, 0xdf, 0xf3, 0xf7, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff,
        0xe3, 0xe7, 0xef, 0xef, 0xfb, 0xff, 0xff, 0xff, 0xf3, 0xf7, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff,
        },
        // index 6
        {
        0x00, 0x3e, 0x7c, 0x7e, 0xf8, 0xfe, 0xfc, 0xfe, 0xf1, 0xff, 0xfd, 0xff, 0xf9, 0xff, 0xfd, 0xff,
        0xe3, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xf3, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff,
        0xc7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xe7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x8f, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xcf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x1f, 0x3f, 0x7f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xdf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x9f, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xdf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        },
        // index 7
        {
        0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        },
        // index 8
        {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        }
    };

    // The persistence parameter/holes filling mode
    const uint8_t persistence_min = 0;
    const uint8_t persistence_max = PERSISTENCE_MAP_NUM-1;
    const uint8_t persistence_default = 3;  // Credible if two of the last four frames are valid at this pixel
    const uint8_t persistence_step = 1;

    //alpha -  the weight with default value 0.4, between 1 and 0 -- 1 means 100% weight from the current pixel
    const float temp_alpha_min = 0.f;
    const float temp_alpha_max = 1.f;
    const float temp_alpha_default = 0.4f;
    const float temp_alpha_step = 0.01f;

    //delta -  the filter threshold for edge classification and preserving with default value of 20 depth increments
    const uint8_t temp_delta_min = 1;
    const uint8_t temp_delta_max = 100;
    const uint8_t temp_delta_default = 20;
    const uint8_t temp_delta_step = 1;

    temporal_filter::temporal_filter() : _persistence_param(persistence_default),
        _alpha_param(temp_alpha_default),
        _one_minus_alpha(1- _alpha_param),
        _delta_param(temp_delta_default),
        _width(0), _height(0), _stride(0), _bpp(0),
        _extension_type(RS2_EXTENSION_DEPTH_FRAME),
        _current_frm_size_pixels(0)
    {
        auto temporal_persistence_control = std::make_shared<ptr_option<uint8_t>>(
            persistence_min,
            persistence_max,
            persistence_step,
            persistence_default,
            &_persistence_param, "Persistency mode");

        temporal_persistence_control->set_description(0, "Disabled");
        temporal_persistence_control->set_description(1, "Valid in 8/8");
        temporal_persistence_control->set_description(2, "Valid in 2/last 3");
        temporal_persistence_control->set_description(3, "Valid in 2/last 4");
        temporal_persistence_control->set_description(4, "Valid in 2/8");
        temporal_persistence_control->set_description(5, "Valid in 1/last 2");
        temporal_persistence_control->set_description(6, "Valid in 1/last 5");
        temporal_persistence_control->set_description(7, "Valid in 1/8");
        temporal_persistence_control->set_description(8, "Valid in previous");

        temporal_persistence_control->on_set([this,temporal_persistence_control](float val)
        {
            if (!temporal_persistence_control->is_valid(val))
                throw invalid_value_exception(to_string()
                    << "Unsupported temporal persistence param "
                    << (int)val << " is out of range.");

            on_set_persistence_control(static_cast<uint8_t>(val));
        });

        register_option(RS2_OPTION_HOLES_FILL, temporal_persistence_control);

        auto temporal_filter_alpha = std::make_shared<ptr_option<float>>(
            temp_alpha_min,
            temp_alpha_max,
            temp_alpha_step,
            temp_alpha_default,
            &_alpha_param, "The normalized weight of the current pixel");
        temporal_filter_alpha->on_set([this](float val)
        {
            on_set_alpha(val);
        });

        auto temporal_filter_delta = std::make_shared<ptr_option<uint8_t>>(
            temp_delta_min,
            temp_delta_max,
            temp_delta_step,
            temp_delta_default,
            &_delta_param, "Depth edge (gradient) classification threshold");
        temporal_filter_delta->on_set([this, temporal_filter_delta](float val)
        {
            if (!temporal_filter_delta->is_valid(val))
                throw invalid_value_exception(to_string()
                    << "Unsupported temporal delta: " << val << " is out of range.");

            on_set_delta(val);
        });

        register_option(RS2_OPTION_FILTER_SMOOTH_ALPHA, temporal_filter_alpha);
        register_option(RS2_OPTION_FILTER_SMOOTH_DELTA, temporal_filter_delta);

        auto on_frame = [this](rs2::frame f, const rs2::frame_source& source)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            rs2::frame res = f, tgt, depth;

            bool composite = f.is<rs2::frameset>();

            depth = (composite) ? f.as<rs2::frameset>().first_or_default(RS2_STREAM_DEPTH) : f;
            if (depth) // Processing required
            {
                update_configuration(f);
                tgt = prepare_target_frame(depth, source);

                // Temporal filter execution
                if (_extension_type == RS2_EXTENSION_DISPARITY_FRAME)
                    temp_jw_smooth<float>(const_cast<void*>(tgt.get_data()), _last_frame.data(), _history.data());
                else
                    temp_jw_smooth<uint16_t>(const_cast<void*>(tgt.get_data()), _last_frame.data(), _history.data());
            }

            res = composite ? source.allocate_composite_frame({ tgt }) : tgt;

            source.frame_ready(res);
        };

        auto callback = new rs2::frame_processor_callback<decltype(on_frame)>(on_frame);
        processing_block::set_processing_callback(std::shared_ptr<rs2_frame_processor_callback>(callback));

        on_set_persistence_control(_persistence_param);
        on_set_delta(_delta_param);
        on_set_alpha(_alpha_param);
    }

    void temporal_filter::on_set_persistence_control(uint8_t val)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _persistence_param = val;
        recalc_persistence_map();
        _last_frame.clear();
        _history.clear();
    }

    void temporal_filter::on_set_alpha(float val)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _alpha_param = val;
        _one_minus_alpha = 1.f - _alpha_param;
        _cur_frame_index = 0;
        _last_frame.clear();
        _history.clear();
    }

    void temporal_filter::on_set_delta(float val)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _delta_param = static_cast<uint8_t>(val);
        _cur_frame_index = 0;
        _last_frame.clear();
        _history.clear();
    }

    void  temporal_filter::update_configuration(const rs2::frame& f)
    {
        if (f.get_profile().get() != _source_stream_profile.get())
        {
            _source_stream_profile = f.get_profile();
            _target_stream_profile = _source_stream_profile.clone(RS2_STREAM_DEPTH, 0, _source_stream_profile.format());

            environment::get_instance().get_extrinsics_graph().register_same_extrinsics(
                *(stream_interface*)(f.get_profile().get()->profile),
                *(stream_interface*)(_target_stream_profile.get()->profile));

            //TODO - reject any frame other than depth/disparity
            _extension_type = f.is<rs2::disparity_frame>() ? RS2_EXTENSION_DISPARITY_FRAME : RS2_EXTENSION_DEPTH_FRAME;
            _bpp = (_extension_type == RS2_EXTENSION_DISPARITY_FRAME) ? sizeof(float) : sizeof(uint16_t);
            auto vp = _target_stream_profile.as<rs2::video_stream_profile>();
            _width = vp.width();
            _height = vp.height();
            _stride = _width*_bpp;
            _current_frm_size_pixels = _width * _height;

            _last_frame.clear();
            _last_frame.resize(_current_frm_size_pixels*_bpp);

            _history.clear();
            _history.resize(_current_frm_size_pixels*_bpp);

        }
    }

    rs2::frame temporal_filter::prepare_target_frame(const rs2::frame& f, const rs2::frame_source& source)
    {
        // Allocate and copy the content of the original Depth data to the target
        rs2::frame tgt = source.allocate_video_frame(_target_stream_profile, f, (int)_bpp, (int)_width, (int)_height, (int)_stride, _extension_type);

        memmove(const_cast<void*>(tgt.get_data()), f.get_data(), _current_frm_size_pixels * _bpp);
        return tgt;
    }

    void temporal_filter::recalc_persistence_map()
    {
        _persistence_map.fill(0);
        _persistence_map = _persistence_lut[_persistence_param];
    }
}
