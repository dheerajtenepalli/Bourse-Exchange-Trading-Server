/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * The "Bourse" trading platform protocol.
 *
 * This header file specifies the format of communication between the
 * Bourse server and its clients.  We will use the term "packet" to refer
 * to a single message sent at the protocol level.  A full-duplex,
 * stream-based (i.e. TCP) connection is used between a client and the
 * server.  Communication is effected by the client and server sending
 * "packets" to each other over this connection.  Each packet consists
 * of a fixed-length header, with fields in network byte order,
 * followed by an optional payload whose length is specified in the header.
 *
 * The following are the packet types in the protocol:
 *
 * Client-to-server requests:
 *   LOGIN:    Log a trader into the exchange
 *             Payload: trader username
 *   STATUS:   Request balance/inventory and bid/ask/last info
 *   DEPOSIT:  Deposit funds into trader's account
 *             Payload: amount to deposit
 *   WITHDRAW: Withdraw funds from trader's account
 *             Payload: amount to withdraw
 *   ESCROW:   Increase inventory in escrow for a trader
 *             Payload: quantity to place in escrow
 *   RELEASE:  Release inventory in escrow for a trader
 *             Payload: quantity to release from escrow
 *   BUY:      Post a buy order to the exchange
 *             Payload: quantity
 *                      max price
 *   SELL:     Post a sell order to the exchange
 *             Payload: quantity
 *                      min price
 *   CANCEL:   Attempt to cancel a pending order
 *             Payload: order id
 * 
 * Server-to-client responses (synchronous):
 *   ACK:      Sent in response to a successful request
 *             Payload (optional):
 *                      account balance
 *                      inventory
 *                      bid/ask/last
 *                      order ID (for BUY/SELL/CANCEL requests)
 *   NACK:     Sent in response to a failed request
 *
 * Server-to-client notifications (asynchronous):
 * Specific to client:
 *   BOUGHT    Sent when client's BUY order is fulfilled or partially fulfilled
 *             Payload: order ID
 *                      quantity
 *                      price
 *   SOLD      Sent when client's SELL order is fullfilled or partially fulfilled
 *             Payload: order ID
 *                      quantity
 *                      price
 *
 * General ("ticker tape"):
 *   POSTED    Notification that a buy or sell order has been posted for some client
 *             Payload: order type (buy/sell)
 *                      order ID
 *                      quantity
 *                      max/min
 *   CANCELED  Notification that a pending order has been canceled for some client
 *             Payload: order ID
 *                      quantity
 *   TRADED    Notification that a trade has occurred
 *             Payload: seller order ID
 *                      buyer order ID
 *                      quantity
 *                      price
 */

/*
 * Packet types.
 */
typedef enum {
    /* Unused */
    BRS_NO_PKT,
    /* Client-to-server*/
    BRS_LOGIN_PKT, BRS_STATUS_PKT,
    BRS_DEPOSIT_PKT, BRS_WITHDRAW_PKT,
    BRS_ESCROW_PKT, BRS_RELEASE_PKT,
    BRS_BUY_PKT, BRS_SELL_PKT, BRS_CANCEL_PKT,
    /* Server-to-client responses (synchronous) */
    BRS_ACK_PKT, BRS_NACK_PKT,
    /* Server-to_client notifications (asynchronous) */
    BRS_BOUGHT_PKT, BRS_SOLD_PKT,
    BRS_POSTED_PKT, BRS_CANCELED_PKT, BRS_TRADED_PKT
} BRS_PACKET_TYPE;

/*
 * Type definitions for fields in packets.
 */
typedef uint32_t funds_t;
typedef uint32_t quantity_t;
typedef uint32_t orderid_t;

/*
 * Fixed-size packet header (same for all packets).
 *
 * NOTE: all multibyte fields in all packets are assumed to be in
 * network byte order.
 */
typedef struct brs_packet_header {
    uint8_t type;		   // Type of the packet
    uint16_t size;                 // Payload size (zero if no payload)
    uint32_t timestamp_sec;        // Seconds field of time packet was sent
    uint32_t timestamp_nsec;       // Nanoseconds field of time packet was sent
} BRS_PACKET_HEADER;

/*
 * Payload structures (depends on packet type).
 */
typedef struct brs_funds_info {    // For DEPOSIT, WITHDRAW
    funds_t amount;                // Amount to deposit/withdraw
} BRS_FUNDS_INFO;

typedef struct brs_escrow_info {   // For ESCROW, RELEASE
    quantity_t quantity;           // Quantity to escrow/release
} BRS_ESCROW_INFO;

typedef struct brs_order_info {    // For BUY, SELL
    quantity_t quantity;           // Quantity to buy/sell
    funds_t price;                 // Price
} BRS_ORDER_INFO;

typedef struct brs_cancel_info {   // For CANCEL
    orderid_t order;               // Order to cancel
} BRS_CANCEL_INFO;

typedef struct brs_status_info {   // For ACK (optional)
    funds_t balance;               // Trader's account balance
    quantity_t inventory;          // Trader's inventory
    funds_t bid;                   // Current highest bid price
    funds_t ask;                   // Current lowest ask price
    funds_t last;                  // Last trade price
    orderid_t orderid;             // Order ID (for BUY, SELL, CANCEL)
    quantity_t quantity;           // Quantity canceled (for CANCEL)
} BRS_STATUS_INFO;

typedef struct brs_notify_info {   // For BOUGHT, SOLD, POSTED, CANCELED, TRADED
    orderid_t buyer;               // Buy order ID
    orderid_t seller;              // Sell order ID
    quantity_t quantity;           // Quantity bought/sold/traded/canceled
    funds_t price;                 // Price
} BRS_NOTIFY_INFO;

/*
 * Send a packet, which consists of a fixed-size header followed by an
 * optional associated data payload.
 *
 * @param fd  The file descriptor on which packet is to be sent.
 * @param pkt  The fixed-size packet header, with multi-byte fields
 *   in network byte order
 * @param data  The data payload, or NULL, if there is none.
 * @return  0 in case of successful transmission, -1 otherwise.
 *   In the latter case, errno is set to indicate the error.
 *
 * All multi-byte fields in the packet are assumed to be in network byte order.
 */
int proto_send_packet(int fd, BRS_PACKET_HEADER *hdr, void *payload);

/*
 * Receive a packet, blocking until one is available.
 *
 * @param fd  The file descriptor from which the packet is to be received.
 * @param pkt  Pointer to caller-supplied storage for the fixed-size
 *   portion of the packet.
 * @param datap  Pointer to a variable into which to store a pointer to any
 *   payload received.
 * @return  0 in case of successful reception, -1 otherwise.  In the
 *   latter case, errno is set to indicate the error.
 *
 * The returned packet has all multi-byte fields in network byte order.
 * If the returned payload pointer is non-NULL, then the caller has the
 * responsibility of freeing that storage.
 */
int proto_recv_packet(int fd, BRS_PACKET_HEADER *hdr, void **payloadp);

#endif
