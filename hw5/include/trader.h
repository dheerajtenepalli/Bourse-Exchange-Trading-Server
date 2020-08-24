/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */
#ifndef TRADER_H
#define TRADER_H

#include "protocol.h"

/*
 * The trader module mantains state information for all traders who have ever
 * logged in to the exchange since it was started.  This state information
 * includes information about traders' accounts and which traders are currently
 * logged-in.  Account information persists across login sessions.
 */

/*
 * The maximum number of traders supported by the exchange.
 */
#define MAX_TRADERS 64

/*
 * Structure that defines the state of a trader in the Bourse server.
 *
 * You will have to give a complete structure definition in trader.c.
 * The precise contents are up to you.  Be sure that all the operations
 * that might be called concurrently are thread-safe.
 */
typedef struct trader TRADER;

/*
 * Initialize the trader module.
 * This must be called before doing calling any other functions in this
 * module.
 *
 * @return 0 if initialization succeeds, -1 otherwise.
 */
int trader_init(void);

/*
 * Finalize the trader module, freeing all associated resources.
 * This should be called when the traders module is no longer required.
 */
void trader_fini(void);

/*
 * Attempt to log in a trader with a specified user name.
 *
 * @param clientfd  The file descriptor of the connection to the client.
 * @param name  The trader's user name, which is copied by this function.
 * @return A pointer to a TRADER object, in case of success, otherwise NULL.
 *
 * The login can fail if the specified user name is already logged in.
 * If the login succeeds then a mapping is recorded from the specified avatar
 * to a TRADER object that is created for this client and returned as the
 * result of the call.  The returned TRADER object has a reference count equal
 * to two.  One reference should be "owned" by the thread that is servicing
 * this client, and it should not be released until the client has logged out.
 * The other reference is owned by the traders map, and it is retained until
 * server shutdown.
 */
TRADER *trader_login(int fd, char *name);

/*
 * Log out a trader.
 *
 * @param trader  The trader to be logged out.
 *
 * This function "consumes" one reference to the TRADER object by calling
 * trader_unref().  However, the trader remains in the traders map, which
 * still holds one reference, th so the trader is not freed.
 */
void trader_logout(TRADER *trader);

/*
 * Increase the reference count on a trader by one.
 *
 * @param trader  The TRADER whose reference count is to be increased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 * @return  The same TRADER object that was passed as a parameter.
 */
TRADER *trader_ref(TRADER *trader, char *why);

/*
 * Decrease the reference count on a trader by one.
 *
 * @param trader  The TRADER whose reference count is to be decreased.
 * @param why  A string describing the reason why the reference count is
 * being increased.  This is used for debugging printout, to help trace
 * the reference counting.
 *
 * If after decrementing, the reference count has reached zero, then the
 * trader and its contents are freed.
 */
void trader_unref(TRADER *trader, char *why);

/*
 * Send a packet to the client for a trader.
 *
 * @param trader  The TRADER object for the client who should receive
 * the packet.
 * @param pkt  The packet to be sent.
 * @param data  Data payload to be sent, or NULL if none.
 * @return 0 if transmission succeeds, -1 otherwise.
 *
 * Once a client has connected and successfully logged in, this function
 * should be used to send packets to the client, as opposed to the lower-level
 * proto_send_packet() function.  The reason for this is that the present
 * function will obtain exclusive access to the trader before calling
 * proto_send_packet().  The fact that exclusive access is obtained before
 * sending means that multiple threads can safely call this function to send
 * to the client, and these calls will be properly serialized.
 */
int trader_send_packet(TRADER *trader, BRS_PACKET_HEADER *pkt, void *data);

/*
 * Broadcast a packet to all currently logged-in traders.
 *
 * @param pkt  The packet to be sent.
 * @param data  Data payload to be sent, or NULL if none.
 * @return 0 if broadcast succeeds, -1 otherwise.
 */
int trader_broadcast_packet(BRS_PACKET_HEADER *pkt, void *data);

/*
 * Send an ACK packet to the client for a trader.
 *
 * @param trader  The TRADER object for the client who should receive
 * the packet.
 * @param infop  Pointer to the optional data payload for this packet,
 * or NULL if there is to be no payload.
 * @return 0 if transmission succeeds, -1 otherwise.
 */
int trader_send_ack(TRADER *trader, BRS_STATUS_INFO *info);

/*
 * Send an NACK packet to the client for a trader.
 *
 * @param trader  The TRADER object for the client who should receive
 * the packet.
 * @return 0 if transmission succeeds, -1 otherwise.
 */
int trader_send_nack(TRADER *trader);

/*
 * Increase the balance for a trader.
 *
 * @param trader  The trader whose balance is to be increased.
 * @param amount  The amount by which the balance is to be increased.
 */
void trader_increase_balance(TRADER *trader, funds_t amount);

/*
 * Attempt to decrease the balance for a trader.
 *
 * @param trader  The trader whose balance is to be decreased.
 * @param amount  The amount by which the balance is to be decreased.
 * @return 0 if the original balance is at least as great as the
 * amount of decrease, -1 otherwise.
 */
int trader_decrease_balance(TRADER *trader, funds_t amount);

/*
 * Increase the inventory for a trader by a specified quantity.
 *
 * @param trader  The trader whose inventory is to be increased.
 * @param amount  The amount by which the inventory is to be increased.
 */
void trader_increase_inventory(TRADER *trader, quantity_t quantity);

/*
 * Attempt to decrease the inventory for a trader by a specified quantity.
 *
 * @param trader  The trader whose inventory is to be decreased.
 * @param amount  The amount by which the inventory is to be decreased.
 * @return 0 if the original inventory is at least as great as the
 * amount of decrease, -1 otherwise.
 */
int trader_decrease_inventory(TRADER *trader, quantity_t quantity);

#endif
