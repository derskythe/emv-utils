/**
 * @file emv_build_candidate_list_test.c
 * @brief Unit tests for EMV PSE processing and AID discovery
 *
 * Copyright (c) 2024 Leon Lynch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "emv.h"
#include "emv_app.h"
#include "emv_cardreader_emul.h"
#include "emv_fields.h"
#include "emv_tags.h"
#include "emv_tlv.h"
#include "emv_ttl.h"

#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

static const struct xpdu_t test_pse_card_blocked[] = {
    {
        20, (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50,
                        0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
                        0x44, 0x44, 0x46, 0x30, 0x31, 0x00}, // SELECT
                                                             // 1PAY.SYS.DDF01
        2, (uint8_t[]){0x6A, 0x81}, // Function not supported
    },
    {0}};

static const struct xpdu_t test_aid_card_blocked[] = {
    {
        20, (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50,
                        0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
                        0x44, 0x44, 0x46, 0x30, 0x31, 0x00}, // SELECT
                                                             // 1PAY.SYS.DDF01
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x10, 0x00},    // SELECT A00000000310
        2, (uint8_t[]){0x6A, 0x81}, // Function not supported
    },
    {0}};

static const struct xpdu_t test_nothing_found[] = {
    {
        20, (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50,
                        0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
                        0x44, 0x44, 0x46, 0x30, 0x31, 0x00}, // SELECT
                                                             // 1PAY.SYS.DDF01
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x10, 0x00},    // SELECT A00000000310
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        13,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x20, 0x10, 0x00}, // SELECT A0000000032010
        2, (uint8_t[]){0x6A, 0x82},    // File or application not found
    },
    {
        13,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x20, 0x20, 0x00}, // SELECT A0000000032020
        2, (uint8_t[]){0x6A, 0x82},    // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x10, 0x00},    // SELECT A00000000410
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x30, 0x00},    // SELECT A00000000430
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {0}};

static const struct xpdu_t test_pse_blocked[] = {
    {
        20, (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50,
                        0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
                        0x44, 0x44, 0x46, 0x30, 0x31, 0x00}, // SELECT
                                                             // 1PAY.SYS.DDF01
        2, (uint8_t[]){0x62, 0x83}, // Selected file deactivated
    },
    {
        5, (uint8_t[]){0x00, 0xC0, 0x00, 0x00, 0x00}, // GET RESPONSE
        2, (uint8_t[]){0x6C, 0x1A},                   // 36 bytes available
    },
    {
        5, (uint8_t[]){0x00, 0xC0, 0x00, 0x00, 0x1A}, // GET RESPONSE Le=36
        36,
        (uint8_t[]){0x6F, 0x20, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53,
                    0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x0E,
                    0x88, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E,
                    0x9F, 0x11, 0x01, 0x01, 0x90, 0x00}, // FCI
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x10, 0x00},    // SELECT A00000000310
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        13,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x20, 0x10, 0x00}, // SELECT A0000000032010
        2, (uint8_t[]){0x6A, 0x82},    // File or application not found
    },
    {
        13,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x20, 0x20, 0x00}, // SELECT A0000000032020
        2, (uint8_t[]){0x6A, 0x82},    // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x10, 0x00},    // SELECT A00000000410
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x30, 0x00},    // SELECT A00000000430
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {0}};

static const struct xpdu_t test_aid_blocked[] = {
    {
        20, (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50,
                        0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
                        0x44, 0x44, 0x46, 0x30, 0x31, 0x00}, // SELECT
                                                             // 1PAY.SYS.DDF01
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x10, 0x00},    // SELECT A00000000310
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        13,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x20, 0x10, 0x00}, // SELECT A0000000032010
        2, (uint8_t[]){0x6A, 0x82},    // File or application not found
    },
    {
        13,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x20, 0x20, 0x00}, // SELECT A0000000032020
        2, (uint8_t[]){0x62, 0x83},    // Selected file deactivated
    },
    {
        5, (uint8_t[]){0x00, 0xC0, 0x00, 0x00, 0x00}, // GET RESPONSE
        2, (uint8_t[]){0x6C, 0x33},                   // 51 bytes available
    },
    {
        5, (uint8_t[]){0x00, 0xC0, 0x00, 0x00, 0x33}, // GET RESPONSE Le=51
        51, (uint8_t[]){0x6F, 0x2F, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                        0x20, 0x20, 0xA5, 0x24, 0x50, 0x05, 0x56, 0x20, 0x50,
                        0x41, 0x59, 0x87, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E,
                        0x6C, 0x65, 0x6E, 0xBF, 0x0C, 0x10, 0x9F, 0x4D, 0x02,
                        0x0B, 0x05, 0x9F, 0x0A, 0x08, 0x00, 0x01, 0x05, 0x01,
                        0x00, 0x00, 0x00, 0x00, 0x90, 0x00}, // FCI
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x10, 0x00},    // SELECT A00000000410
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x30, 0x00},    // SELECT A00000000430
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {0}};

static const struct xpdu_t test_pse_app_not_supported[] = {
    {
        20, (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50,
                        0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
                        0x44, 0x44, 0x46, 0x30, 0x31, 0x00}, // SELECT
                                                             // 1PAY.SYS.DDF01
        36,
        (uint8_t[]){0x6F, 0x20, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53,
                    0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x0E,
                    0x88, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E,
                    0x9F, 0x11, 0x01, 0x01, 0x90, 0x00}, // FCI
    },
    {
        5, (uint8_t[]){0x00, 0xB2, 0x01, 0x0C, 0x00}, // READ RECORD 1,1
        45,
        (uint8_t[]){0x70, 0x29, 0x61, 0x27, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00,
                    0x03, 0x30, 0x30, 0x50, 0x0B, 0x56, 0x49, 0x53, 0x41, 0x20,
                    0x43, 0x52, 0x45, 0x44, 0x49, 0x54, 0x87, 0x01, 0x01, 0x9F,
                    0x12, 0x0B, 0x56, 0x49, 0x53, 0x41, 0x20, 0x43, 0x52, 0x45,
                    0x44, 0x49, 0x54, 0x90, 0x00}, // AEF
    },
    {
        5, (uint8_t[]){0x00, 0xB2, 0x02, 0x0C, 0x00}, // READ RECORD 1,2
        2, (uint8_t[]){0x6A, 0x83},                   // Record not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x10, 0x00},    // SELECT A00000000310
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        13,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x20, 0x10, 0x00}, // SELECT A0000000032010
        2, (uint8_t[]){0x6A, 0x82},    // File or application not found
    },
    {
        13,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x20, 0x20, 0x00}, // SELECT A0000000032020
        2, (uint8_t[]){0x6A, 0x82},    // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x10, 0x00},    // SELECT A00000000410
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x30, 0x00},    // SELECT A00000000430
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {0}};

static const struct xpdu_t test_pse_app_supported[] = {
    {
        20, (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50,
                        0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
                        0x44, 0x44, 0x46, 0x30, 0x31, 0x00}, // SELECT
                                                             // 1PAY.SYS.DDF01
        36,
        (uint8_t[]){0x6F, 0x20, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53,
                    0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x0E,
                    0x88, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E,
                    0x9F, 0x11, 0x01, 0x01, 0x90, 0x00}, // FCI
    },
    {
        5, (uint8_t[]){0x00, 0xB2, 0x01, 0x0C, 0x00}, // READ RECORD 1,1
        45,
        (uint8_t[]){0x70, 0x29, 0x61, 0x27, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00,
                    0x03, 0x10, 0x10, 0x50, 0x0B, 0x56, 0x49, 0x53, 0x41, 0x20,
                    0x43, 0x52, 0x45, 0x44, 0x49, 0x54, 0x87, 0x01, 0x01, 0x9F,
                    0x12, 0x0B, 0x56, 0x49, 0x53, 0x41, 0x20, 0x43, 0x52, 0x45,
                    0x44, 0x49, 0x54, 0x90, 0x00}, // AEF
    },
    {
        5, (uint8_t[]){0x00, 0xB2, 0x02, 0x0C, 0x00}, // READ RECORD 1,2
        2, (uint8_t[]){0x6A, 0x83},                   // Record not found
    },
    {0}};

static const struct xpdu_t test_pse_multi_app_supported[] = {
    {
        20, (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50,
                        0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
                        0x44, 0x44, 0x46, 0x30, 0x31, 0x00}, // SELECT
                                                             // 1PAY.SYS.DDF01
        36,
        (uint8_t[]){0x6F, 0x20, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53,
                    0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x0E,
                    0x88, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E,
                    0x9F, 0x11, 0x01, 0x01, 0x90, 0x00}, // FCI
    },
    {
        5, (uint8_t[]){0x00, 0xB2, 0x01, 0x0C, 0x00}, // READ RECORD 1,1
        72,
        (uint8_t[]){0x70, 0x44, 0x61, 0x20, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00,
                    0x03, 0x20, 0x20, 0x50, 0x05, 0x56, 0x20, 0x50, 0x41, 0x59,
                    0x87, 0x01, 0x01, 0x73, 0x0B, 0x9F, 0x0A, 0x08, 0x00, 0x01,
                    0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x61, 0x20, 0x4F, 0x07,
                    0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x10, 0x50, 0x05, 0x56,
                    0x20, 0x50, 0x41, 0x59, 0x87, 0x01, 0x02, 0x73, 0x0B, 0x9F,
                    0x0A, 0x08, 0x00, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00,
                    0x90, 0x00}, // AEF
    },
    {
        5, (uint8_t[]){0x00, 0xB2, 0x02, 0x0C, 0x00}, // READ RECORD 1,2
        2, (uint8_t[]){0x6A, 0x83},                   // Record not found
    },
    {0}};

static const struct xpdu_t test_aid_multi_exact_match_app_supported[] = {
    {
        20, (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50,
                        0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
                        0x44, 0x44, 0x46, 0x30, 0x31, 0x00}, // SELECT
                                                             // 1PAY.SYS.DDF01
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x10, 0x00},    // SELECT A00000000310
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        13,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x20, 0x10, 0x00}, // SELECT A0000000032010
        51, (uint8_t[]){0x6F, 0x2F, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                        0x20, 0x10, 0xA5, 0x24, 0x50, 0x05, 0x56, 0x20, 0x50,
                        0x41, 0x59, 0x87, 0x01, 0x02, 0x5F, 0x2D, 0x04, 0x6E,
                        0x6C, 0x65, 0x6E, 0xBF, 0x0C, 0x10, 0x9F, 0x4D, 0x02,
                        0x0B, 0x05, 0x9F, 0x0A, 0x08, 0x00, 0x01, 0x05, 0x01,
                        0x00, 0x00, 0x00, 0x00, 0x90, 0x00}, // FCI
    },
    {
        13,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x20, 0x20, 0x00}, // SELECT A0000000032020
        51, (uint8_t[]){0x6F, 0x2F, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                        0x20, 0x20, 0xA5, 0x24, 0x50, 0x05, 0x56, 0x20, 0x50,
                        0x41, 0x59, 0x87, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E,
                        0x6C, 0x65, 0x6E, 0xBF, 0x0C, 0x10, 0x9F, 0x4D, 0x02,
                        0x0B, 0x05, 0x9F, 0x0A, 0x08, 0x00, 0x01, 0x05, 0x01,
                        0x00, 0x00, 0x00, 0x00, 0x90, 0x00}, // FCI
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x10, 0x00},    // SELECT A00000000410
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x30, 0x00},    // SELECT A00000000430
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {0}};

static const struct xpdu_t test_aid_multi_partial_match_app_supported[] = {
    {
        20, (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50,
                        0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
                        0x44, 0x44, 0x46, 0x30, 0x31, 0x00}, // SELECT
                                                             // 1PAY.SYS.DDF01
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x10, 0x00},    // SELECT A00000000310
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        13,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x20, 0x10, 0x00}, // SELECT A0000000032010
        2, (uint8_t[]){0x6A, 0x82},    // File or application not found
    },
    {
        13,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03,
                    0x20, 0x20, 0x00}, // SELECT A0000000032020
        2, (uint8_t[]){0x6A, 0x82},    // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x10, 0x00}, // SELECT first A00000000410
        72,
        (uint8_t[]){
            0x6F, 0x44, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10,
            0xA5, 0x39, 0x50, 0x09, 0x4D, 0x43, 0x20, 0x43, 0x52, 0x45, 0x44,
            0x49, 0x54, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E, 0x87, 0x01,
            0x01, 0x9F, 0x11, 0x01, 0x01, 0x9F, 0x12, 0x0A, 0x4D, 0x41, 0x53,
            0x54, 0x45, 0x52, 0x43, 0x41, 0x52, 0x44, 0xBF, 0x0C, 0x10, 0x9F,
            0x4D, 0x02, 0x0B, 0x0A, 0x9F, 0x0A, 0x08, 0x00, 0x01, 0x05, 0x02,
            0x00, 0x00, 0x00, 0x00, 0x90, 0x00,
        }, // FCI
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x02, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x10, 0x00}, // SELECT next A00000000410
        72,
        (uint8_t[]){
            0x6F, 0x44, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x20,
            0xA5, 0x39, 0x50, 0x09, 0x4D, 0x43, 0x20, 0x43, 0x52, 0x45, 0x44,
            0x49, 0x54, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E, 0x87, 0x01,
            0x02, 0x9F, 0x11, 0x01, 0x01, 0x9F, 0x12, 0x0A, 0x4D, 0x41, 0x53,
            0x54, 0x45, 0x52, 0x43, 0x41, 0x52, 0x44, 0xBF, 0x0C, 0x10, 0x9F,
            0x4D, 0x02, 0x0B, 0x0A, 0x9F, 0x0A, 0x08, 0x00, 0x01, 0x05, 0x02,
            0x00, 0x00, 0x00, 0x00, 0x90, 0x00,
        }, // FCI
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x02, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x10, 0x00},    // SELECT next A00000000410
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {
        12,
        (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x04,
                    0x30, 0x00},    // SELECT A00000000430
        2, (uint8_t[]){0x6A, 0x82}, // File or application not found
    },
    {0}};

static const struct xpdu_t test_sorted_app_priority[] = {
    {
        20, (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50,
                        0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
                        0x44, 0x44, 0x46, 0x30, 0x31, 0x00}, // SELECT
                                                             // 1PAY.SYS.DDF01
        36,
        (uint8_t[]){0x6F, 0x20, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53,
                    0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x0E,
                    0x88, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E,
                    0x9F, 0x11, 0x01, 0x01, 0x90, 0x00}, // FCI
    },
    {
        5, (uint8_t[]){0x00, 0xB2, 0x01, 0x0C, 0x00}, // READ RECORD 1,1
        72,
        (uint8_t[]){0x70, 0x44, 0x61, 0x20, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00,
                    0x03, 0x10, 0x05, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x35,
                    0x87, 0x01, 0x05, 0x73, 0x0B, 0x9F, 0x0A, 0x08, 0x00, 0x01,
                    0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x61, 0x20, 0x4F, 0x07,
                    0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x03, 0x50, 0x05, 0x41,
                    0x50, 0x50, 0x20, 0x33, 0x87, 0x01, 0x04, 0x73, 0x0B, 0x9F,
                    0x0A, 0x08, 0x00, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00,
                    0x90, 0x00}, // AEF
    },
    {
        5, (uint8_t[]){0x00, 0xB2, 0x02, 0x0C, 0x00}, // READ RECORD 1,2
        58, (uint8_t[]){0x70, 0x36, 0x61, 0x19, 0x4F, 0x07, 0xA0, 0x00, 0x00,
                        0x00, 0x05, 0x10, 0x10, 0x50, 0x05, 0x41, 0x50, 0x50,
                        0x20, 0x38, 0x73, 0x07, 0x9F, 0x0A, 0x04, 0x00, 0x01,
                        0x01, 0x04, 0x61, 0x19, 0x4F, 0x07, 0xA0, 0x00, 0x00,
                        0x00, 0x04, 0x10, 0x10, 0x50, 0x05, 0x41, 0x50, 0x50,
                        0x20, 0x37, 0x73, 0x07, 0x9F, 0x0A, 0x04, 0x00, 0x01,
                        0x01, 0x04, 0x90, 0x00}, // AEF without application
                                                 // priority indicator, of which
                                                 // one AID is not supported
    },
    {
        5, (uint8_t[]){0x00, 0xB2, 0x03, 0x0C, 0x00}, // READ RECORD 1,3
        72,
        (uint8_t[]){0x70, 0x44, 0x61, 0x20, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00,
                    0x03, 0x10, 0x01, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x31,
                    0x87, 0x01, 0x01, 0x73, 0x0B, 0x9F, 0x0A, 0x08, 0x00, 0x01,
                    0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x61, 0x20, 0x4F, 0x07,
                    0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x06, 0x50, 0x05, 0x41,
                    0x50, 0x50, 0x20, 0x36, 0x87, 0x01, 0x07, 0x73, 0x0B, 0x9F,
                    0x0A, 0x08, 0x00, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00,
                    0x90, 0x00}, // AEF
    },
    {
        5, (uint8_t[]){0x00, 0xB2, 0x04, 0x0C, 0x00}, // READ RECORD 1,4
        72,
        (uint8_t[]){0x70, 0x44, 0x61, 0x20, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00,
                    0x03, 0x10, 0x02, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x32,
                    0x87, 0x01, 0x01, 0x73, 0x0B, 0x9F, 0x0A, 0x08, 0x00, 0x01,
                    0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x61, 0x20, 0x4F, 0x07,
                    0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x04, 0x50, 0x05, 0x41,
                    0x50, 0x50, 0x20, 0x34, 0x87, 0x01, 0x04, 0x73, 0x0B, 0x9F,
                    0x0A, 0x08, 0x00, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00,
                    0x90, 0x00}, // AEF
    },
    {
        5, (uint8_t[]){0x00, 0xB2, 0x05, 0x0C, 0x00}, // READ RECORD 1,5
        2, (uint8_t[]){0x6A, 0x83},                   // Record not found
    },
    {0}};

static const struct xpdu_t test_app_cardholder_confirmation[] = {
    {
        20, (uint8_t[]){0x00, 0xA4, 0x04, 0x00, 0x0E, 0x31, 0x50,
                        0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
                        0x44, 0x44, 0x46, 0x30, 0x31, 0x00}, // SELECT
                                                             // 1PAY.SYS.DDF01
        36,
        (uint8_t[]){0x6F, 0x20, 0x84, 0x0E, 0x31, 0x50, 0x41, 0x59, 0x2E, 0x53,
                    0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x0E,
                    0x88, 0x01, 0x01, 0x5F, 0x2D, 0x04, 0x6E, 0x6C, 0x65, 0x6E,
                    0x9F, 0x11, 0x01, 0x01, 0x90, 0x00}, // FCI
    },
    {
        5, (uint8_t[]){0x00, 0xB2, 0x01, 0x0C, 0x00}, // READ RECORD 1,1
        45,
        (uint8_t[]){0x70, 0x29, 0x61, 0x27, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00,
                    0x03, 0x10, 0x10, 0x50, 0x0B, 0x56, 0x49, 0x53, 0x41, 0x20,
                    0x43, 0x52, 0x45, 0x44, 0x49, 0x54, 0x87, 0x01, 0x81, 0x9F,
                    0x12, 0x0B, 0x56, 0x49, 0x53, 0x41, 0x20, 0x43, 0x52, 0x45,
                    0x44, 0x49, 0x54, 0x90, 0x00}, // AEF
    },
    {
        5, (uint8_t[]){0x00, 0xB2, 0x02, 0x0C, 0x00}, // READ RECORD 1,2
        2, (uint8_t[]){0x6A, 0x83},                   // Record not found
    },
    {0}};

int main(void) {
  int r;
  struct emv_cardreader_emul_ctx_t emul_ctx;
  struct emv_ttl_t ttl;
  struct emv_tlv_list_t supported_aids = EMV_TLV_LIST_INIT;
  struct emv_app_list_t app_list = EMV_APP_LIST_INIT;
  size_t app_count;

  ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
  ttl.cardreader.ctx = &emul_ctx;
  ttl.cardreader.trx = &emv_cardreader_emul;

  // Supported applications
  emv_tlv_list_push(&supported_aids, EMV_TAG_9F06_AID, 6,
                    (uint8_t[]){0xA0, 0x00, 0x00, 0x00, 0x03, 0x10},
                    EMV_ASI_PARTIAL_MATCH); // Visa
  emv_tlv_list_push(&supported_aids, EMV_TAG_9F06_AID, 7,
                    (uint8_t[]){0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x10},
                    EMV_ASI_EXACT_MATCH); // Visa Electron
  emv_tlv_list_push(&supported_aids, EMV_TAG_9F06_AID, 7,
                    (uint8_t[]){0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x20},
                    EMV_ASI_EXACT_MATCH); // V Pay
  emv_tlv_list_push(&supported_aids, EMV_TAG_9F06_AID, 6,
                    (uint8_t[]){0xA0, 0x00, 0x00, 0x00, 0x04, 0x10},
                    EMV_ASI_PARTIAL_MATCH); // Mastercard
  emv_tlv_list_push(&supported_aids, EMV_TAG_9F06_AID, 6,
                    (uint8_t[]){0xA0, 0x00, 0x00, 0x00, 0x04, 0x30},
                    EMV_ASI_PARTIAL_MATCH); // Maestro

  r = emv_debug_init(EMV_DEBUG_SOURCE_ALL, EMV_DEBUG_CARD, &print_emv_debug);
  if (r) {
    printf("Failed to initialise EMV debugging\n");
    return 1;
  }

  printf("\nTesting PSE card blocked or SELECT not supported...\n");
  emul_ctx.xpdu_list = test_pse_card_blocked;
  emul_ctx.xpdu_current = NULL;
  emv_app_list_clear(&app_list);
  r = emv_build_candidate_list(&ttl, &supported_aids, &app_list);
  if (r != EMV_OUTCOME_CARD_BLOCKED) {
    fprintf(stderr,
            "Unexpected emv_build_candidate_list() result; error %d: %s\n", r,
            r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
    r = 1;
    goto exit;
  }
  if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
    fprintf(stderr, "Incomplete card interaction\n");
    r = 1;
    goto exit;
  }
  if (!emv_app_list_is_empty(&app_list)) {
    fprintf(stderr, "Candidate list unexpectedly NOT empty\n");
    for (struct emv_app_t *app = app_list.front; app != NULL; app = app->next) {
      print_emv_app(app);
    }
    r = 1;
    goto exit;
  }
  if (emv_app_list_selection_is_required(&app_list)) {
    fprintf(stderr, "Cardholder application selection unexpectedly required\n");
    r = 1;
    goto exit;
  }
  printf("Success\n");

  printf("\nTesting PSE not found and AID card blocked or SELECT not "
         "supported...\n");
  emul_ctx.xpdu_list = test_aid_card_blocked;
  emul_ctx.xpdu_current = NULL;
  emv_app_list_clear(&app_list);
  r = emv_build_candidate_list(&ttl, &supported_aids, &app_list);
  if (r != EMV_OUTCOME_CARD_BLOCKED) {
    fprintf(stderr,
            "Unexpected emv_build_candidate_list() result; error %d: %s\n", r,
            r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
    r = 1;
    goto exit;
  }
  if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
    fprintf(stderr, "Incomplete card interaction\n");
    r = 1;
    goto exit;
  }
  if (!emv_app_list_is_empty(&app_list)) {
    fprintf(stderr, "Candidate list unexpectedly NOT empty\n");
    for (struct emv_app_t *app = app_list.front; app != NULL; app = app->next) {
      print_emv_app(app);
    }
    r = 1;
    goto exit;
  }
  if (emv_app_list_selection_is_required(&app_list)) {
    fprintf(stderr, "Cardholder application selection unexpectedly required\n");
    r = 1;
    goto exit;
  }
  printf("Success\n");

  printf("\nTesting PSE not found and no supported applications...\n");
  emul_ctx.xpdu_list = test_nothing_found;
  emul_ctx.xpdu_current = NULL;
  emv_app_list_clear(&app_list);
  r = emv_build_candidate_list(&ttl, &supported_aids, &app_list);
  if (r != EMV_OUTCOME_NOT_ACCEPTED) {
    fprintf(stderr,
            "Unexpected emv_build_candidate_list() result; error %d: %s\n", r,
            r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
    r = 1;
    goto exit;
  }
  if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
    fprintf(stderr, "Incomplete card interaction\n");
    r = 1;
    goto exit;
  }
  if (!emv_app_list_is_empty(&app_list)) {
    fprintf(stderr, "Candidate list unexpectedly NOT empty\n");
    for (struct emv_app_t *app = app_list.front; app != NULL; app = app->next) {
      print_emv_app(app);
    }
    r = 1;
    goto exit;
  }
  if (emv_app_list_selection_is_required(&app_list)) {
    fprintf(stderr, "Cardholder application selection unexpectedly required\n");
    r = 1;
    goto exit;
  }
  printf("Success\n");

  printf("\nTesting PSE blocked and no supported applications...\n");
  emul_ctx.xpdu_list = test_pse_blocked;
  emul_ctx.xpdu_current = NULL;
  emv_app_list_clear(&app_list);
  r = emv_build_candidate_list(&ttl, &supported_aids, &app_list);
  if (r != EMV_OUTCOME_NOT_ACCEPTED) {
    fprintf(stderr,
            "Unexpected emv_build_candidate_list() result; error %d: %s\n", r,
            r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
    r = 1;
    goto exit;
  }
  if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
    fprintf(stderr, "Incomplete card interaction\n");
    r = 1;
    goto exit;
  }
  if (!emv_app_list_is_empty(&app_list)) {
    fprintf(stderr, "Candidate list unexpectedly NOT empty\n");
    for (struct emv_app_t *app = app_list.front; app != NULL; app = app->next) {
      print_emv_app(app);
    }
    r = 1;
    goto exit;
  }
  if (emv_app_list_selection_is_required(&app_list)) {
    fprintf(stderr, "Cardholder application selection unexpectedly required\n");
    r = 1;
    goto exit;
  }
  printf("Success\n");

  printf("\nTesting PSE not found and AID blocked...\n");
  emul_ctx.xpdu_list = test_aid_blocked;
  emul_ctx.xpdu_current = NULL;
  emv_app_list_clear(&app_list);
  r = emv_build_candidate_list(&ttl, &supported_aids, &app_list);
  if (r != EMV_OUTCOME_NOT_ACCEPTED) {
    fprintf(stderr,
            "Unexpected emv_build_candidate_list() result; error %d: %s\n", r,
            r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
    r = 1;
    goto exit;
  }
  if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
    fprintf(stderr, "Incomplete card interaction\n");
    r = 1;
    goto exit;
  }
  if (!emv_app_list_is_empty(&app_list)) {
    fprintf(stderr, "Candidate list unexpectedly NOT empty\n");
    for (struct emv_app_t *app = app_list.front; app != NULL; app = app->next) {
      print_emv_app(app);
    }
    r = 1;
    goto exit;
  }
  if (emv_app_list_selection_is_required(&app_list)) {
    fprintf(stderr, "Cardholder application selection unexpectedly required\n");
    r = 1;
    goto exit;
  }
  printf("Success\n");

  printf("\nTesting PSE app not supported...\n");
  emul_ctx.xpdu_list = test_pse_app_not_supported;
  emul_ctx.xpdu_current = NULL;
  emv_app_list_clear(&app_list);
  r = emv_build_candidate_list(&ttl, &supported_aids, &app_list);
  if (r != EMV_OUTCOME_NOT_ACCEPTED) {
    fprintf(stderr,
            "Unexpected emv_build_candidate_list() result; error %d: %s\n", r,
            r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
    r = 1;
    goto exit;
  }
  if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
    fprintf(stderr, "Incomplete card interaction\n");
    r = 1;
    goto exit;
  }
  if (!emv_app_list_is_empty(&app_list)) {
    fprintf(stderr, "Candidate list unexpectedly NOT empty\n");
    for (struct emv_app_t *app = app_list.front; app != NULL; app = app->next) {
      print_emv_app(app);
    }
    r = 1;
    goto exit;
  }
  if (emv_app_list_selection_is_required(&app_list)) {
    fprintf(stderr, "Cardholder application selection unexpectedly required\n");
    r = 1;
    goto exit;
  }
  printf("Success\n");

  printf("\nTesting PSE app supported...\n");
  emul_ctx.xpdu_list = test_pse_app_supported;
  emul_ctx.xpdu_current = NULL;
  emv_app_list_clear(&app_list);
  r = emv_build_candidate_list(&ttl, &supported_aids, &app_list);
  if (r) {
    fprintf(stderr,
            "Unexpected emv_build_candidate_list() result; error %d: %s\n", r,
            r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
    r = 1;
    goto exit;
  }
  if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
    fprintf(stderr, "Incomplete card interaction\n");
    r = 1;
    goto exit;
  }
  if (emv_app_list_is_empty(&app_list)) {
    fprintf(stderr, "Candidate list unexpectedly empty\n");
    r = 1;
    goto exit;
  }
  for (struct emv_app_t *app = app_list.front; app != NULL; app = app->next) {
    print_emv_app(app);
  }
  if (emv_app_list_selection_is_required(&app_list)) {
    fprintf(stderr, "Cardholder application selection unexpectedly required\n");
    r = 1;
    goto exit;
  }
  printf("Success\n");

  printf("\nTesting PSE multiple apps supported...\n");
  emul_ctx.xpdu_list = test_pse_multi_app_supported;
  emul_ctx.xpdu_current = NULL;
  emv_app_list_clear(&app_list);
  r = emv_build_candidate_list(&ttl, &supported_aids, &app_list);
  if (r) {
    fprintf(stderr,
            "Unexpected emv_build_candidate_list() result; error %d: %s\n", r,
            r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
    r = 1;
    goto exit;
  }
  if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
    fprintf(stderr, "Incomplete card interaction\n");
    r = 1;
    goto exit;
  }
  if (emv_app_list_is_empty(&app_list)) {
    fprintf(stderr, "Candidate list unexpectedly empty\n");
    r = 1;
    goto exit;
  }
  for (struct emv_app_t *app = app_list.front; app != NULL; app = app->next) {
    print_emv_app(app);
  }
  if (!emv_app_list_selection_is_required(&app_list)) {
    fprintf(stderr,
            "Cardholder application selection unexpectedly NOT required\n");
    r = 1;
    goto exit;
  }
  printf("Success\n");

  printf(
      "\nTesting PSE not found and multiple exact match AIDs supported...\n");
  emul_ctx.xpdu_list = test_aid_multi_exact_match_app_supported;
  emul_ctx.xpdu_current = NULL;
  emv_app_list_clear(&app_list);
  r = emv_build_candidate_list(&ttl, &supported_aids, &app_list);
  if (r) {
    fprintf(stderr,
            "Unexpected emv_build_candidate_list() result; error %d: %s\n", r,
            r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
    r = 1;
    goto exit;
  }
  if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
    fprintf(stderr, "Incomplete card interaction\n");
    r = 1;
    goto exit;
  }
  if (emv_app_list_is_empty(&app_list)) {
    fprintf(stderr, "Candidate list unexpectedly empty\n");
    r = 1;
    goto exit;
  }
  for (struct emv_app_t *app = app_list.front; app != NULL; app = app->next) {
    print_emv_app(app);
  }
  if (!emv_app_list_selection_is_required(&app_list)) {
    fprintf(stderr,
            "Cardholder application selection unexpectedly NOT required\n");
    r = 1;
    goto exit;
  }
  printf("Success\n");

  printf(
      "\nTesting PSE not found and multiple partial match AIDs supported...\n");
  emul_ctx.xpdu_list = test_aid_multi_partial_match_app_supported;
  emul_ctx.xpdu_current = NULL;
  emv_app_list_clear(&app_list);
  r = emv_build_candidate_list(&ttl, &supported_aids, &app_list);
  if (r) {
    fprintf(stderr,
            "Unexpected emv_build_candidate_list() result; error %d: %s\n", r,
            r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
    r = 1;
    goto exit;
  }
  if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
    fprintf(stderr, "Incomplete card interaction\n");
    r = 1;
    goto exit;
  }
  if (emv_app_list_is_empty(&app_list)) {
    fprintf(stderr, "Candidate list unexpectedly empty\n");
    r = 1;
    goto exit;
  }
  for (struct emv_app_t *app = app_list.front; app != NULL; app = app->next) {
    print_emv_app(app);
  }
  if (!emv_app_list_selection_is_required(&app_list)) {
    fprintf(stderr,
            "Cardholder application selection unexpectedly NOT required\n");
    r = 1;
    goto exit;
  }
  printf("Success\n");

  printf("\nTesting sorted app priority...\n");
  emul_ctx.xpdu_list = test_sorted_app_priority;
  emul_ctx.xpdu_current = NULL;
  emv_app_list_clear(&app_list);
  r = emv_build_candidate_list(&ttl, &supported_aids, &app_list);
  if (r) {
    fprintf(stderr,
            "Unexpected emv_build_candidate_list() result; error %d: %s\n", r,
            r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
    r = 1;
    goto exit;
  }
  if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
    fprintf(stderr, "Incomplete card interaction\n");
    r = 1;
    goto exit;
  }
  if (emv_app_list_is_empty(&app_list)) {
    fprintf(stderr, "Candidate list unexpectedly empty\n");
    r = 1;
    goto exit;
  }
  app_count = 0;
  for (struct emv_app_t *app = app_list.front; app != NULL; app = app->next) {
    print_emv_app(app);
    ++app_count;

    // Use application display name to validate sorted app order
    char tmp[] = "APP x";
    tmp[4] = '0' + app_count;
    if (strcmp(tmp, app->display_name) != 0) {
      fprintf(stderr, "Invalid candidate list order\n");
      r = 1;
      goto exit;
    }
  }
  if (!emv_app_list_selection_is_required(&app_list)) {
    fprintf(stderr,
            "Cardholder application selection unexpectedly NOT required\n");
    r = 1;
    goto exit;
  }
  printf("Success\n");

  printf("\nTesting cardholder confirmation required for single app...\n");
  emul_ctx.xpdu_list = test_app_cardholder_confirmation;
  emul_ctx.xpdu_current = NULL;
  emv_app_list_clear(&app_list);
  r = emv_build_candidate_list(&ttl, &supported_aids, &app_list);
  if (r) {
    fprintf(stderr,
            "Unexpected emv_build_candidate_list() result; error %d: %s\n", r,
            r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
    r = 1;
    goto exit;
  }
  if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
    fprintf(stderr, "Incomplete card interaction\n");
    r = 1;
    goto exit;
  }
  if (emv_app_list_is_empty(&app_list)) {
    fprintf(stderr, "Candidate list unexpectedly empty\n");
    r = 1;
    goto exit;
  }
  for (struct emv_app_t *app = app_list.front; app != NULL; app = app->next) {
    print_emv_app(app);
  }
  if (app_list.front != app_list.back) {
    fprintf(stderr, "Candidate list unexpectedly contains more than one app\n");
    r = 1;
    goto exit;
  }
  if (!emv_app_list_selection_is_required(&app_list)) {
    fprintf(stderr,
            "Cardholder application selection unexpectedly NOT required\n");
    r = 1;
    goto exit;
  }
  printf("Success\n");

  // Success
  r = 0;
  goto exit;

exit:
  emv_tlv_list_clear(&supported_aids);
  emv_app_list_clear(&app_list);

  return r;
}
