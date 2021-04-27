/**
 * @file emv_ttl.h
 * @brief EMV Terminal Transport Layer
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

#ifndef EMV_TTL_H
#define EMV_TTL_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

/// Card reader mode
enum emv_cardreader_mode_t {
	EMV_CARDREADER_MODE_APDU = 1,               ///< Card reader is in APDU mode
	EMV_CARDREADER_MODE_TPDU,                   ///< Card reader is in TPDU mode
};

/// Card reader transceive function type
typedef int (*emv_cardreader_trx_t)(
	void* ctx,
	const void* tx_buf,
	size_t tx_buf_len,
	void* rx_buf,
	size_t* rx_buf_len
);

/**
 * EMV Terminal Transport Layer (TTL) abstraction for card reader
 * @note the card reader mode determines whether the @ref trx function
 * operates on TPDU frames or APDU frames. Typically PC/SC card readers use
 * APDU mode.
 */
struct emv_cardreader_t {
	enum emv_cardreader_mode_t mode;            ///< Card reader mode (TPDU vs APDU)
	void* ctx;                                  ///< Card reader transceive function context
	emv_cardreader_trx_t trx;                   ///< Card reader transceive function
};

/// EMV Terminal Transport Layer context
struct emv_ttl_t {
	struct emv_cardreader_t cardreader;
};

/**
 * EMV Terminal Transport Layer (TTL) transceive function for sending a
 * Command Application Protocol Data Unit (C-APDU) and receiving a
 * Response Application Protocol Data Unit (R-APDU)
 * @param ctx EMV Terminal Transport Layer context
 * @param c_apdu Command Application Protocol Data Unit (C-APDU) buffer
 * @param c_apdu_len Length of C-APDU buffer in bytes
 * @param r_apdu Response Application Protocol Data Unit (R-APDU) buffer
 * @param r_apdu_len Length of R-APDU buffer in bytes
 * @return Zero for success. Less than zero for error. Greater than zero for invalid reader response.
 */
int emv_ttl_trx(
	struct emv_ttl_t* ctx,
	const void* c_apdu,
	size_t c_apdu_len,
	void* r_apdu,
	size_t* r_apdu_len
);

__END_DECLS

#endif