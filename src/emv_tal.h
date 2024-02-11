/**
 * @file emv_tal.h
 * @brief EMV Terminal Application Layer (TAL)
 *
 * Copyright (c) 2021, 2024 Leon Lynch
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

#ifndef EMV_TAL_H
#define EMV_TAL_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

// Forward declarations
struct emv_ttl_t;
struct emv_app_list_t;
struct emv_app_t;
struct emv_tlv_list_t;
struct emv_tlv_t;

/**
 * EMV Terminal Application Layer (TAL) errors.
 *
 * These errors indicate that the session should be terminated.
 */
enum emv_tal_error_t {
	EMV_TAL_ERROR_INTERNAL = -1, ///< Internal error
	EMV_TAL_ERROR_INVALID_PARAMETER = -2, ///< Invalid function parameter
	EMV_TAL_ERROR_TTL_FAILURE = -3, ///< Terminal Transport Layer (TTL) failure
	EMV_TAL_ERROR_CARD_BLOCKED = -4, ///< Card blocked
};

/**
 * EMV Terminal Application Layer (TAL) results.
 *
 * These results indicate the reason why processing did not succeed but also
 * that the session may continue.
 */
enum emv_tal_result_t {
	EMV_TAL_RESULT_PSE_NOT_FOUND = 1, ///< Payment System Environment (PSE) not found
	EMV_TAL_RESULT_PSE_BLOCKED, ///< Payment System Environment (PSE) is blocked
	EMV_TAL_RESULT_PSE_SELECT_FAILED, ///< Failed to select Payment System Environment (PSE)
	EMV_TAL_RESULT_PSE_FCI_PARSE_FAILED, ///< Failed to parse File Control Information (FCI) for Payment System Environment (PSE)
	EMV_TAL_RESULT_PSE_SFI_NOT_FOUND, ///< Failed to find Short File Identifier (SFI) for Payment System Environment (PSE)
	EMV_TAL_RESULT_PSE_AEF_PARSE_FAILED, ///< Failed to parse Application Elementary File (AEF) of Payment System Environment (PSE)
	EMV_TAL_RESULT_PSE_AEF_INVALID, ///< Invalid Payment System Environment (PSE) Application Elementary File (AEF) record
	EMV_TAL_RESULT_APP_NOT_FOUND, ///< Selected application not found
	EMV_TAL_RESULT_APP_BLOCKED, ///< Selected application is blocked
	EMV_TAL_RESULT_APP_SELECTION_FAILED, ///< Application selection failed
	EMV_TAL_RESULT_APP_FCI_PARSE_FAILED, ///< Failed to parse File Control Information (FCI) for selected application
};

/**
 * Read Payment System Environment (PSE) records and build candidate
 * application list
 * @remark See EMV 4.4 Book 1, 12.3.2
 *
 * @param ttl EMV Terminal Transport Layer context
 * @param supported_aids Supported AID (field 9F06) list including ASI flags
 * @param app_list Candidate application list output
 *
 * @return Zero for success
 * @return Less than zero indicates that the terminal should terminate the
 *         card session. See @ref emv_tal_error_t
 * @return Greater than zero indicates that reading of PSE records failed and
 *         that the terminal may continue the card session. Typically the list
 *         of AIDs method (@ref emv_tal_find_supported_apps()) would be next.
 *         See @ref emv_tal_result_t
 */
int emv_tal_read_pse(
	struct emv_ttl_t* ttl,
	const struct emv_tlv_list_t* supported_aids,
	struct emv_app_list_t* app_list
);

/**
 * Find supported applications and build candidate application list
 * @remark See EMV 4.4 Book 1, 12.3.3
 *
 * @param ttl EMV Terminal Transport Layer context
 * @param supported_aids Supported AID (field 9F06) list including ASI flags
 * @param app_list Candidate application list output
 *
 * @return Zero for success
 * @return Less than zero indicates that the terminal should terminate the
 *         card session. See @ref emv_tal_error_t
 */
int emv_tal_find_supported_apps(
	struct emv_ttl_t* ttl,
	const struct emv_tlv_list_t* supported_aids,
	struct emv_app_list_t* app_list
);

/**
 * SELECT application and validate File Control Information (FCI)
 * @remark See EMV 4.4 Book 1, 12.4
 *
 * @param ttl EMV Terminal Transport Layer context
 * @param aid Application Identifier (AID) field. Must be 5 to 16 bytes.
 * @param aid_len Length of Application Identifier (AID) field. Must be 5 to 16 bytes.
 * @param selected_app Selected EMV application output. Use @ref emv_app_free() to free memory.
 *
 * @return Zero for success
 * @return Less than zero indicates that the terminal should terminate the
 *         card session. See @ref emv_tal_error_t
 * @return Greater than zero indicates that selection or validation of the
 *         application failed and that the terminal may continue the card
 *         session. Typically the candidate application list should be
 *         consulted next for remaining application options.
 */
int emv_tal_select_app(
	struct emv_ttl_t* ttl,
	const uint8_t* aid,
	size_t aid_len,
	struct emv_app_t** selected_app
);

/**
 * Parse GET PROCESSING OPTIONS response
 * @param buf Buffer containing GET PROCESSING OPTIONS response data
 * @param len Length of buffer in bytes
 * @param list List to which decoded EMV TLV fields will be appended
 * @param aip Pointer to Application Interchange Profile (AIP) field on
 *            @c list for convenience. Do not free. NULL to ignore.
 * @param afl Pointer to Application File Locator (AFL) field on @c list
 *            for convenience. Do not free. NULL to ignore.
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_tal_parse_gpo_response(
	const void* buf,
	size_t len,
	struct emv_tlv_list_t* list,
	struct emv_tlv_t** aip,
	struct emv_tlv_t** afl
);

__END_DECLS

#endif
