/**
 * === DO NOT MODIFY THIS FILE ===
 * If you need some other prototypes or constants in a header, please put them
 * in another header file.
 *
 * When we grade, we will be replacing this file with our own copy.
 * You have been warned.
 * === DO NOT MODIFY THIS FILE ===
 */
#ifndef EXCHANGE_H
#define EXCHANGE_H

#include "trader.h"

/*
 * An exchange maintains a set of buy and sell orders,
 * matches buy and sell orders with each other, and takes care
 * of notifying traders and updating their accounts when a trade
 * is performed.
 *
 * Most of the behavior of the exchange is covered by the specifications of
 * the various functions below.  What these do not cover is the behavior of
 * "matchmaker" thread that is started when the exchange is initialized,
 * terminates when the exchange is finalized, and in between spends its time
 * finding matching trades and carrying them out.  Normally, the matchmaker
 * thread sleeps (*e.g.* on a semaphore) awaiting a change in the set of posted
 * orders.  When such a change occurs, the matchmaker thread is awakened,
 * it looks for matching buy and sell orders, and it carries out trades until
 * there are no orders that match. Then it goes back to sleep again.
 *
 * The matchmaker is activated whenever a change in the pending orders has
 * the potential of having created a pair of matching orders.  A buy order
 * "matches" a sell order when the minimum sell price is less than or equal
 * to the maximum buy price.  The matchmaker selects a matching buy and sell
 * order and carries out a trade as follows:
 *
 *   The trade price is set to be that price in the range of overlap between
 *   the ranges specified by the buy and sell orders which is closest to the
 *   last trade price.  This means that if the last trade price is within
 *   the range of overlap, then the new trade will take place at the last
 *   trade price.
 *
 *   The quantity to be traded is set to be the minimum of the quantities specified
 *   in the buy and sell orders.
 *
 *   The quantities in the buy and sell orders are decreased by the quantity to
 *   be traded.  This will leave one or both orders with quantity zero.
 *   Such orders will be removed from the exchange as described below.
 *   Orders with nonzero remaining quantity are left pending, but with quantity
 *   that has been reduced by the the amount of the trade.
 *
 *   The seller's account is credited with the proceeds of the trade (`quantity * price`).
 *
 *   The buyer's inventory is credited with the number of units purchased (`quantity`).
 *
 *   The buyer's account is credited with any remaining funds that should no longer
 *   be encumbered as a result of the trade.  The buyer's account was debited by the
 *   maximum possible amount of the trade at the time the buy order was created,
 *   but the trade might have taken place for a lesser price.  Any difference is
 *   credited to the buyer's account.  If the buy order has quantity remaining after
 *   the trade, then the amount of funds that remain encumbered must equal the
 *   maximum possible cost of a trade at the new quantity.
 *
 *   Any order that now has quantity zero is removed from the exchange.
 *
 *   The trader that posted the buy order is sent a `BOUGHT` packet containing details
 *   of the trade.
 *
 *   The trader that posted the sell order is sent a `SOLD` packet containing details
 *   of the trade.
 *
 *   A `TRADED` packet containing details of the trade is broadcast to all logged-in
 *   traders.
 *
 * Broadcasting of `POSTED` and `TRADED` packets should be carried out in such a way
 * that a `TRADED` packet resulting from a matching buy and sell order is sent out
 * *after* the `POSTED` packets for that buy and sell order.
 */
typedef struct exchange EXCHANGE;

/*
 * Initialize a new exchange.
 *
 * @return  the newly initialized exchange, or NULL if initialization failed.
 */
EXCHANGE *exchange_init();

/*
 * Finalize an exchange, freeing all associated resources.
 *
 * @param xchg  The exchange to be finalized, which must not
 * be referenced again.
 */
void exchange_fini(EXCHANGE *xchg);

/*
 * Get the current status of the exchange.
 */
void exchange_get_status(EXCHANGE *xchg, BRS_STATUS_INFO *infop);

/*
 * Post a buy order on the exchange on behalf of a trader.
 * The trader is stored with the order, and its reference count is
 * increased by one to account for the stored pointer.
 * Funds equal to the maximum possible cost of the order are
 * encumbered by removing them from the trader's account.
 * A POSTED packet containing details of the order is broadcast
 * to all logged-in traders.
 *
 * @param xchg  The exchange to which the order is to be posted.
 * @param trader  The trader on whose behalf the order is to be posted.
 * @param quantity  The quantity to be bought.
 * @param price  The maximum price to be paid per unit.
 * @return  The order ID assigned to the new order, if successfully posted,
 * otherwise 0.
 */
orderid_t exchange_post_buy(EXCHANGE *xchg, TRADER *trader, quantity_t quantity,
			    funds_t price);

/*
 * Post a sell order on the exchange on behalf of a trader.
 * The trader is stored with the order, and its reference count is
 * increased by one to account for the stored pointer.
 * Inventory equal to the amount of the order is
 * encumbered by removing it from the trader's account.
 * A POSTED packet containing details of the order is broadcast
 * to all logged-in traders.
 *
 * @param xchg  The exchange to which the order is to be posted.
 * @param trader  The trader on whose behalf the order is to be posted.
 * @param quantity  The quantity to be sold.
 * @param price  The minimum sale price per unit.
 * @return  The order ID assigned to the new order, if successfully posted,
 * otherwise 0.
 */
orderid_t exchange_post_sell(EXCHANGE *xchg, TRADER *trader, quantity_t quantity,
			     funds_t price);

/*
 * Attempt to cancel a pending order.
 * If successful, the quantity of the canceled order is returned in a variable,
 * and a CANCELED packet containing details of the canceled order is
 * broadcast to all logged-in traders.
 *
 * @param xchg  The exchange from which the order is to be cancelled.
 * @param trader  The trader cancelling the order is to be posted,
 * which must be the same as the trader who originally posted the order.
 * @param id  The order ID of the order to be cancelled.
 * @param quantity  Pointer to a variable in which to return the quantity
 * of the order that was canceled.  Note that this need not be the same as
 * the original order amount, as the order could have been partially
 * fulfilled by trades.
 * @return  0 if the order was successfully cancelled, -1 otherwise.
 * Note that cancellation might fail if a trade fulfills and removes the
 * order before this function attempts to cancel it.
 */
int exchange_cancel(EXCHANGE *xchg, TRADER *trader, orderid_t order,
		    quantity_t *quantity);

#endif
