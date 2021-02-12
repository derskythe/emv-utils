/**
 * @file emv-tool.c
 * @brief Simple EMV processing tool
 *
 * Copyright (c) 2021 Leon Lynch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "pcsc.h"
#include "iso7816.h"

#include <stdio.h>

static void print_buf(const char* buf_name, const void* buf, size_t length)
{
	const uint8_t* ptr = buf;
	printf("%s: ", buf_name);
	for (size_t i = 0; i < length; i++) {
		printf("%02X", ptr[i]);
	}
	printf("\n");
}

static const char* pcsc_get_reader_state_string(unsigned int reader_state)
{
	if (reader_state & PCSC_STATE_UNAVAILABLE) {
		return "Status unavailable";
	}
	if (reader_state & PCSC_STATE_EMPTY) {
		return "No card";
	}
	if (reader_state & PCSC_STATE_PRESENT) {
		if (reader_state & PCSC_STATE_MUTE) {
			return "Unresponsive card";
		}
		if (reader_state & PCSC_STATE_UNPOWERED) {
			return "Unpowered card";
		}

		return "Card present";
	}

	return NULL;
}

static void print_atr(pcsc_reader_ctx_t reader)
{
	int r;
	uint8_t atr[PCSC_MAX_ATR_SIZE];
	size_t atr_len = 0;
	struct iso7816_atr_info_t atr_info;

	r = pcsc_reader_get_atr(reader, atr, &atr_len);
	if (r) {
		printf("Failed to retrieve ATR\n");
		return;
	}

	print_buf("\nATR", atr, atr_len);

	r = iso7816_atr_parse(atr, atr_len, &atr_info);
	if (r) {
		printf("Failed to parse ATR\n");
		return;
	}

	// Print ATR info
	printf("  TS  = 0x%02X\n", atr_info.TS);
	printf("  T0  = 0x%02X\n", atr_info.T0);
	for (size_t i = 1; i < 5; ++i) {
		if (atr_info.TA[i]) {
			printf("  TA%zu = 0x%02X\n", i, *atr_info.TA[i]);
		}
		if (atr_info.TB[i]) {
			printf("  TB%zu = 0x%02X\n", i, *atr_info.TB[i]);
		}
		if (atr_info.TC[i]) {
			printf("  TC%zu = 0x%02X\n", i, *atr_info.TC[i]);
		}
		if (atr_info.TD[i]) {
			printf("  TD%zu = 0x%02X\n", i, *atr_info.TD[i]);
		}
	}
}

int main(void)
{
	int r;
	pcsc_ctx_t pcsc;
	size_t pcsc_count;
	pcsc_reader_ctx_t reader;
	unsigned int reader_state;
	const char* reader_state_str;
	size_t reader_idx;

	r = pcsc_init(&pcsc);
	if (r) {
		printf("PC/SC initialisation failed\n");
		goto exit;
	}

	pcsc_count = pcsc_get_reader_count(pcsc);
	if (!pcsc_count) {
		printf("No PC/SC readers detected\n");
		goto exit;
	}

	printf("PC/SC readers:\n");
	for (size_t i = 0; i < pcsc_count; ++i) {
		reader = pcsc_get_reader(pcsc, i);
		printf("%zu: %s", i, pcsc_reader_get_name(reader));

		reader_state = 0;
		r = pcsc_reader_get_state(reader, &reader_state);
		if (r == 0) {
			reader_state_str = pcsc_get_reader_state_string(reader_state);
			if (reader_state) {
				printf("; %s", reader_state_str);
			} else {
				printf("; Unknown state");
			}
		}

		printf("\n");
	}

	// Wait for card presentation
	printf("\nPresent card\n");
	r = pcsc_wait_for_card(pcsc, 5000, &reader_idx);
	if (r < 0) {
		printf("PC/SC error\n");
		goto exit;
	}
	if (r > 0) {
		printf("No card; exiting\n");
		goto exit;
	}

	reader = pcsc_get_reader(pcsc, reader_idx);
	printf("Card detected\n");

	r = pcsc_reader_connect(reader);
	if (r) {
		printf("PC/SC reader activation failed\n");
		goto exit;
	}
	printf("Card activated\n");

	print_atr(reader);

	r = pcsc_reader_disconnect(reader);
	if (r) {
		printf("PC/SC reader deactivation failed\n");
		goto exit;
	}
	printf("\nCard deactivated\n");

exit:
	pcsc_release(&pcsc);
}
