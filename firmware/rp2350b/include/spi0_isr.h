/*
 * spi0_isr.h — SPI0 Slave Interrupt Service Routine API
 *
 * Copyright (C) 2026 GhostBlade Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This header defines the public API for the SPI0 slave interrupt
 * handler that assembles incoming frames from the RK3576 host.
 * The ISR uses a state machine to process bytes from the SPI0 RX
 * FIFO, validate CRC checksums, and queue complete frames for
 * the main loop to dispatch.
 *
 * Reference: docs/spi-protocol-timing.md
 */

#ifndef SPI0_ISR_H
#define SPI0_ISR_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * SPI0 Frame Assembly Public API
 * ======================================================================== */

/**
 * spi0_rx_get_frame — Get the next assembled frame for dispatch
 *
 * Called from the main loop to check if a complete frame has been
 * assembled by the ISR. If a frame is ready, the caller can extract
 * the command and payload using validate_spi_frame() from spi_protocol.h.
 *
 * @buf:   Output: pointer to the frame data (within ISR buffer).
 *         The pointer is valid until spi0_rx_release_frame() is called.
 * @len:   Output: length of the frame in bytes.
 *
 * Returns: true if a frame is available, false otherwise.
 */
bool spi0_rx_get_frame(const uint8_t **buf, uint16_t *len);

/**
 * spi0_rx_release_frame — Release the current frame and reset state machine
 *
 * Called after the main loop has finished processing a frame.
 * Resets the assembly state machine to IDLE so the ISR can
 * begin receiving the next frame.
 */
void spi0_rx_release_frame(void);

/**
 * spi0_tx_queue_response — Queue a response frame for transmission
 *
 * The response is loaded into the SPI0 TX FIFO on the next
 * SPI transaction. An INT_REQ signal is asserted to notify
 * the host that data is available.
 *
 * @frame: Pointer to the response frame data (must be a valid
 *         SPI frame built with build_spi_frame())
 * @len:   Length of the response frame in bytes
 */
void spi0_tx_queue_response(const uint8_t *frame, uint16_t len);

/**
 * spi0_rx_reset — Reset the SPI0 receive state machine
 *
 * Clears all buffers and resets the state machine to IDLE.
 * Called on initialization and after error recovery.
 */
void spi0_rx_reset(void);

/**
 * spi0_tx_reset — Reset the SPI0 transmit state
 *
 * Clears the response buffer and deasserts INT_REQ.
 */
void spi0_tx_reset(void);

/**
 * spi0_get_stats — Get SPI0 communication statistics
 *
 * @frames_received:      Total bytes/frames received (ISR triggers)
 * @frames_validated:     Frames that passed all CRC validation
 * @frames_rejected_sync: Bytes rejected for bad sync byte
 * @frames_rejected_hdr:  Frames rejected for bad header CRC-64
 * @frames_rejected_pay:  Frames rejected for bad payload CRC-32
 * @frames_rejected_len:  Frames rejected for invalid payload length
 * @rx_overflows:         Bytes dropped due to frame not consumed
 * @bytes_received:       Total bytes received from SPI0
 */
void spi0_get_stats(uint32_t *frames_received,
                     uint32_t *frames_validated,
                     uint32_t *frames_rejected_sync,
                     uint32_t *frames_rejected_hdr,
                     uint32_t *frames_rejected_pay,
                     uint32_t *frames_rejected_len,
                     uint32_t *rx_overflows,
                     uint32_t *bytes_received);

#endif /* SPI0_ISR_H */