/**
 * @file emv.c
 * @brief High level EMV library interface
 *
 * Copyright (c) 2023-2024 Leon Lynch
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

#include "emv.h"
#include "emv_utils_config.h"
#include "emv_tal.h"
#include "emv_app.h"

#include "iso7816.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_EMV
#include "emv_debug.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

const char* emv_lib_version_string(void)
{
	return EMV_UTILS_VERSION_STRING;
}

const char* emv_error_get_string(enum emv_error_t error)
{
	switch (error) {
		case EMV_ERROR_INTERNAL: return "Internal error";
		case EMV_ERROR_INVALID_PARAMETER: return "Invalid function parameter";
	}

	return "Unknown error";
}

const char* emv_outcome_get_string(enum emv_outcome_t outcome)
{
	// See EMV 4.4 Book 4, 11.2, table 8
	switch (outcome) {
		case EMV_OUTCOME_CARD_ERROR: return "Card error"; // Message 06
		case EMV_OUTCOME_CARD_BLOCKED: return "Card blocked"; // Not in EMV specification
		case EMV_OUTCOME_NOT_ACCEPTED: return "Not accepted"; // Message 0C
		case EMV_OUTCOME_TRY_AGAIN: return "Try again"; // Message 13
	}

	return "Invalid outcome";
}

int emv_atr_parse(const void* atr, size_t atr_len)
{
	int r;
	struct iso7816_atr_info_t atr_info;
	unsigned int TD1_protocol = 0; // Default is T=0
	unsigned int TD2_protocol = 0; // Default is T=0

	if (!atr || !atr_len) {
		emv_debug_trace_msg("atr=%p, atr_len=%zu", atr, atr_len);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	r = iso7816_atr_parse(atr, atr_len, &atr_info);
	if (r) {
		emv_debug_trace_msg("iso7816_atr_parse() failed; r=%d", r);

		if (r < 0) {
			emv_debug_error("Internal error");
			return EMV_ERROR_INTERNAL;
		}
		if (r > 0) {
			emv_debug_error("Failed to parse ATR");
			return EMV_OUTCOME_CARD_ERROR;
		}
	}
	emv_debug_atr_info(&atr_info);

	// The intention of this function is to validate the ATR in accordance with
	// EMV Level 1 Contact Interface Specification v1.0, 8.3. Some of the
	// validation may already be performed by iso7816_atr_parse() and should be
	// noted below in comments. The intention is also not to limit this
	// function to only the "basic ATR", but instead to allow all possible ATRs
	// that are allowed by the specification.

	// TS - Initial character
	// See EMV Level 1 Contact Interface v1.0, 8.3.1
	// Validated by iso7816_atr_parse()

	// T0 - Format character
	// See EMV Level 1 Contact Interface v1.0, 8.3.2
	// Validated by iso7816_atr_parse()

	// TA1 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.1
	if (atr_info.TA[1]) { // TA1 is present
		if (atr_info.TA[2] && // TA2 is present
			(*atr_info.TA[2] & ISO7816_ATR_TA2_IMPLICIT) == 0 && // Specific mode
			(*atr_info.TA[1] < 0x11 || *atr_info.TA[1] > 0x13) // TA1 must be in the range 0x11 to 0x13
		) {
			emv_debug_error("TA2 indicates specific mode but TA1 is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}

		if (!atr_info.TA[2]) { // TA2 is absent
			// Max frequency must be at least 5 MHz
			if ((*atr_info.TA[1] & ISO7816_ATR_TA1_FI_MASK) == 0) {
				emv_debug_error("TA2 indicates negotiable mode but TA1 is invalid");
				return EMV_OUTCOME_CARD_ERROR;
			}

			// Baud rate adjustment factor must be at least 4
			if ((*atr_info.TA[1] & ISO7816_ATR_TA1_DI_MASK) < 3) {
				emv_debug_error("TA2 indicates negotiable mode but TA1 is invalid");
				return EMV_OUTCOME_CARD_ERROR;
			}
		}
	}

	// TB1 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.2
	// Validated by iso7816_atr_parse()

	// TC1 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.3
	if (atr_info.TC[1]) { // TC1 is present
		// TC1 must be either 0x00 or 0xFF
		if (*atr_info.TC[1] != 0x00 && *atr_info.TC[1] != 0xFF) {
			emv_debug_error("TC1 is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}
	}

	// TD1 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.4
	if (atr_info.TD[1]) { // TD1 is present
		// TD1 protocol type must be T=0 or T=1
		if ((*atr_info.TD[1] & ISO7816_ATR_Tx_OTHER_MASK) > ISO7816_PROTOCOL_T1) {
			emv_debug_error("TD1 protocol is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}
		TD1_protocol = *atr_info.TD[1] & ISO7816_ATR_Tx_OTHER_MASK;
	}

	// TA2 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.5
	if (atr_info.TA[2]) { // TA2 is present
		unsigned int TA2_protocol;

		// TA2 protocol must be the same as the first indicated protocol
		TA2_protocol = *atr_info.TA[2] & ISO7816_ATR_TA2_PROTOCOL_MASK;
		if (TA2_protocol != TD1_protocol) {
			emv_debug_error("TA2 protocol differs from TD1 protocol");
			return EMV_OUTCOME_CARD_ERROR;
		}

		// TA2 must indicate specific mode, not implicit mode
		if (*atr_info.TA[2] & ISO7816_ATR_TA2_IMPLICIT) {
			emv_debug_error("TA2 implicit mode is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}
	}

	// TB2 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.6
	// Validated by iso7816_atr_parse()

	// TC2 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.7
	if (atr_info.TC[2]) { // TC2 is present
		// TC2 is specific to T=0
		if (TD1_protocol != ISO7816_PROTOCOL_T0) {
			emv_debug_error("TC2 is not allowed when protocol is not T=0");
			return EMV_OUTCOME_CARD_ERROR;
		}

		// TC2 for T=0 must be 0x0A
		if (*atr_info.TC[2] != 0x0A) {
			emv_debug_error("TC2 for T=0 is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}
	}

	// TD2 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.8
	if (atr_info.TD[2]) { // TD2 is present
		// TD2 protocol type must be T=15 if TD1 protocol type was T=0
		if (TD1_protocol == ISO7816_PROTOCOL_T0 &&
			(*atr_info.TD[2] & ISO7816_ATR_Tx_OTHER_MASK) != ISO7816_PROTOCOL_T15
		) {
			emv_debug_error("TD2 protocol is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}

		// TD2 protocol type must be T=1 if TD1 protocol type was T=1
		if (TD1_protocol == ISO7816_PROTOCOL_T1 &&
			(*atr_info.TD[2] & ISO7816_ATR_Tx_OTHER_MASK) != ISO7816_PROTOCOL_T1
		) {
			emv_debug_error("TD2 protocol is invalid");
			return EMV_OUTCOME_CARD_ERROR;
		}

		TD2_protocol = *atr_info.TD[2] & ISO7816_ATR_Tx_OTHER_MASK;

	} else { // TD2 is absent
		// TB3, and therefore TD2, must be present for T=1
		// See EMV Level 1 Contact Interface v1.0, 8.3.3.10
		if (TD1_protocol == ISO7816_PROTOCOL_T1) {
			emv_debug_error("TD2 for T=1 is absent");
			return EMV_OUTCOME_CARD_ERROR;
		}
	}

	// T=1 Interface Characters
	if (TD2_protocol == ISO7816_PROTOCOL_T1) {
		// TA3 - Interface Character
		// See EMV Level 1 Contact Interface v1.0, 8.3.3.9
		if (atr_info.TA[3]) { // TA3 is present
			// TA3 for T=1 must be in the range 0x10 to 0xFE
			// iso7816_atr_parse() already rejects 0xFF
			if (*atr_info.TA[3] < 0x10) {
				emv_debug_error("TA3 for T=1 is invalid");
				return EMV_OUTCOME_CARD_ERROR;
			}
		}

		// TB3 - Interface Character
		// See EMV Level 1 Contact Interface v1.0, 8.3.3.10
		if (atr_info.TB[3]) { // TB3 is present
			// TB3 for T=1 BWI must be 4 or less
			if (((*atr_info.TB[3] & ISO7816_ATR_TBi_BWI_MASK) >> ISO7816_ATR_TBi_BWI_SHIFT) > 4) {
				emv_debug_error("TB3 for T=1 has invalid BWI");
				return EMV_OUTCOME_CARD_ERROR;
			}

			// TB3 for T=1 CWI must be 5 or less
			if ((*atr_info.TB[3] & ISO7816_ATR_TBi_CWI_MASK) > 5) {
				emv_debug_error("TB3 for T=1 has invalid CWI");
				return EMV_OUTCOME_CARD_ERROR;
			}

			// For T=1, reject 2^CWI < (N + 1)
			// - if N==0xFF, consider N to be -1
			// - if N==0x00, consider CWI to be 1
			// See EMV Level 1 Contact Interface v1.0, 8.3.3.1
			int N = (atr_info.global.N != 0xFF) ? atr_info.global.N : -1;
			unsigned int CWI = atr_info.global.N ? atr_info.protocol_T1.CWI : 1;
			unsigned int pow_2_CWI = 1 << CWI;
			if (pow_2_CWI < (N + 1)) {
				emv_debug_error("2^CWI < (N + 1) for T=1 is not allowed");
				return EMV_OUTCOME_CARD_ERROR;
			}

		} else { // TB3 is absent
			emv_debug_error("TB3 for T=1 is absent");
			return EMV_OUTCOME_CARD_ERROR;
		}

		// TC3 - Interface Character
		// See EMV Level 1 Contact Interface v1.0, 8.3.3.11
		if (atr_info.TC[3]) { // TC3 is present
			// TC for T=1 must be 0x00
			if (*atr_info.TC[3] != 0x00) {
				emv_debug_error("TC3 for T=1 is invalid");
				return EMV_OUTCOME_CARD_ERROR;
			}
		}
	}

	// TCK - Check Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.4
	// Validated by iso7816_atr_parse()

	return 0;
}

int emv_build_candidate_list(
	struct emv_ttl_t* ttl,
	const struct emv_tlv_list_t* supported_aids,
	struct emv_app_list_t* app_list
)
{
	int r;

	if (!ttl || !supported_aids || !app_list) {
		emv_debug_trace_msg("ttl=%p, supported_aids=%p, app_list=%p", ttl, supported_aids, app_list);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}

	emv_debug_info("SELECT Payment System Environment (PSE)");
	r = emv_tal_read_pse(ttl, supported_aids, app_list);
	if (r < 0) {
		emv_debug_trace_msg("emv_tal_read_pse() failed; r=%d", r);
		emv_debug_error("Failed to read PSE; terminate session");
		if (r == EMV_TAL_ERROR_CARD_BLOCKED) {
			return EMV_OUTCOME_CARD_BLOCKED;
		} else {
			return EMV_OUTCOME_CARD_ERROR;
		}
	}
	if (r > 0) {
		emv_debug_trace_msg("emv_tal_read_pse() failed; r=%d", r);
		emv_debug_info("Failed to process PSE; continue session");
	}

	// If PSE failed or no apps found by PSE, use list of AIDs method
	// See EMV 4.4 Book 1, 12.3.2, step 5
	if (emv_app_list_is_empty(app_list)) {
		emv_debug_info("Discover list of AIDs");
		r = emv_tal_find_supported_apps(ttl, supported_aids, app_list);
		if (r) {
			emv_debug_trace_msg("emv_tal_find_supported_apps() failed; r=%d", r);
			emv_debug_error("Failed to find supported AIDs; terminate session");
			if (r == EMV_TAL_ERROR_CARD_BLOCKED) {
				return EMV_OUTCOME_CARD_BLOCKED;
			} else {
				return EMV_OUTCOME_CARD_ERROR;
			}
		}
	}

	// If there are no mutually supported applications, terminate session
	// See EMV 4.4 Book 1, 12.4, step 1
	if (emv_app_list_is_empty(app_list)) {
		emv_debug_info("Candidate list empty");
		return EMV_OUTCOME_NOT_ACCEPTED;
	}

	// Sort application list according to priority
	// See EMV 4.4 Book 1, 12.4, step 4
	r = emv_app_list_sort_priority(app_list);
	if (r) {
		emv_debug_trace_msg("emv_app_list_sort_priority() failed; r=%d", r);
		emv_debug_error("Failed to sort application list; terminate session");
		return EMV_ERROR_INTERNAL;
	}

	return 0;
}

int emv_select_application(
	struct emv_ttl_t* ttl,
	struct emv_app_list_t* app_list,
	unsigned int index,
	struct emv_app_t** selected_app
)
{
	int r;
	struct emv_app_t* current_app = NULL;
	uint8_t current_aid[16];
	size_t current_aid_len;

	if (!ttl || !app_list || !selected_app) {
		emv_debug_trace_msg("ttl=%p, app_list=%p, index=%u, selected_app=%p", ttl, app_list, index, selected_app);
		emv_debug_error("Invalid parameter");
		return EMV_ERROR_INVALID_PARAMETER;
	}
	*selected_app = NULL;

	current_app = emv_app_list_remove_index(app_list, index);
	if (!current_app) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	if (current_app->aid->length > sizeof(current_aid)) {
		goto try_again;
	}
	current_aid_len = current_app->aid->length;
	memcpy(current_aid, current_app->aid->value, current_app->aid->length);
	emv_app_free(current_app);
	current_app = NULL;

	r = emv_tal_select_app(
		ttl,
		current_aid,
		current_aid_len,
		selected_app
	);
	if (r) {
		emv_debug_trace_msg("emv_tal_select_app() failed; r=%d", r);
		if (r < 0) {
			emv_debug_error("Error during application selection; terminate session");
			if (r == EMV_TAL_ERROR_CARD_BLOCKED) {
				r = EMV_OUTCOME_CARD_BLOCKED;
			} else {
				r = EMV_OUTCOME_CARD_ERROR;
			}
			goto exit;
		}
		if (r > 0) {
			emv_debug_info("Failed to select application; continue session");
			goto try_again;
		}
	}

	// Success
	r = 0;
	goto exit;

try_again:
	// If no applications remain, terminate session
	// Otherwise, try again
	// See EMV 4.4 Book 1, 12.4
	// See EMV 4.4 Book 4, 11.3
	if (emv_app_list_is_empty(app_list)) {
		emv_debug_info("Candidate list empty");
		r = EMV_OUTCOME_NOT_ACCEPTED;
	} else {
		r = EMV_OUTCOME_TRY_AGAIN;
	}

exit:
	if (current_app) {
		emv_app_free(current_app);
		current_app = NULL;
	}

	return r;
}
