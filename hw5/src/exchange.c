#include <stdlib.h>
#include <csapp.h>
#include <time.h>
#include "exchange.h"
#include "debug.h"
#include "protocol.h"
#include "trader.h"


struct order{
    uint32_t id;
    TRADER * trader;
    int type;
    quantity_t q;           // Quantity bought/sold/traded/canceled
    funds_t p;
    struct order* next;
    pthread_mutex_t order_mutex; //needed coz cancel and matchmaker can access a order simultaneously
    pthread_mutexattr_t ma;
};

pthread_t tid;
int loop =1;
void remove_empty_and_update_exchange(struct order * , EXCHANGE * );

struct exchange{
    uint32_t total_id;
    funds_t bid;                   // Current highest bid price
    funds_t ask;                   // Current lowest ask price
    funds_t last;                  // Last trade price
    // orderid_t orderid;             // Order ID (for BUY, SELL, CANCEL)
    // quantity_t quantity;
    struct order * next;
    struct order ** order_list;
    sem_t exchange_mutex;
    sem_t matchmaker_mutex;
    sem_t wait;
};
//uncomment this to print orderlist before n after each order
void printallorders(EXCHANGE * ex){
    // struct order * head = *(ex->order_list);
    // struct order * it = head;
    // printf("Buy Orders\n");
    // while(it){
    //     pthread_mutex_lock(&(it->order_mutex));
    //     if(it->type==1)
    //         printf("[id: %d, trader: %p, type: 1, quant: %u, price: %u]",it->id, it->trader, it->q, it->p);
    //     pthread_mutex_unlock(&(it->order_mutex));
    //     it=it->next;
    // }
    // it = head;
    //  printf("\nSell Orders\n");
    // while(it){
    //     pthread_mutex_lock(&(it->order_mutex));
    //     if(it->type==2)
    //         printf("[id: %d, trader: %p, type: 2, quant: %u, price: %u]",it->id, it->trader, it->q, it->p);
    //     pthread_mutex_unlock(&(it->order_mutex));
    //     it=it->next;
    // }
    // printf("\n\n");
    // fflush(stdout);
}

BRS_PACKET_HEADER * getheader(int type){
    //debug("HEADER  malloc 12 bytes");
    struct timespec curr_time;
    clock_gettime(CLOCK_MONOTONIC,&curr_time);
    BRS_PACKET_HEADER * ack = malloc(sizeof(BRS_PACKET_HEADER));
    ack->type = type;
    ack->size = htons(sizeof(BRS_NOTIFY_INFO));
    ack->timestamp_sec = htonl(curr_time.tv_sec);
    ack->timestamp_nsec = htonl(curr_time.tv_nsec);
    return ack;
}

BRS_NOTIFY_INFO * getbody(uint32_t b_oid, uint32_t s_oid, quantity_t quantity, funds_t price){
    //debug("NOTIFY  malloc 16 bytes");
    BRS_NOTIFY_INFO * data = malloc(sizeof(BRS_NOTIFY_INFO));
    data->buyer = htonl(b_oid);
    data->seller = htonl(s_oid);
    data->quantity = htonl(quantity);
    data->price = htonl(price);
    return data;
}

int mm_helper(struct order * head, EXCHANGE * ex){
    int flag=0;
    pthread_mutex_lock(&(head->order_mutex));
    if(head->type ==1){ //When the latest order = BUY
        TRADER * buyer = head->trader;
        uint32_t buyer_price = head->p;
        struct order * it = head->next;

        while(it && head->q>0){
            //debug("%u",head->q);
            uint32_t seller_price = it->p;
            pthread_mutex_lock(&(it->order_mutex)); 
            if(it->type==2 && (seller_price <= buyer_price)){
                
                TRADER * seller = it->trader;
                if(head->q >= it->q){
                    trader_increase_inventory(buyer,it->q);
                    trader_increase_balance(seller, it->q * seller_price);
                    trader_increase_balance(buyer, (it->q)*(buyer_price - seller_price));

                    //notification body 
                    BRS_NOTIFY_INFO * data = getbody(head->id, it->id , it->q, seller_price);
                    //send BOUGHT packet
                    BRS_PACKET_HEADER * packet_head = getheader(BRS_BOUGHT_PKT);
                    trader_send_packet(buyer,packet_head, data);
                    //send SOLD packet
                    packet_head = getheader(BRS_SOLD_PKT);
                    trader_send_packet(seller,packet_head, data);
                    //broadcast TRADED packet
                    packet_head = getheader(BRS_TRADED_PKT);
                    trader_broadcast_packet(packet_head, data);

                    head->q -= it->q;
                    it->q = 0;
                }
                else{ 
                    trader_increase_inventory(buyer,head->q);
                    trader_increase_balance(seller, head->q * seller_price);
                    trader_increase_balance(buyer, (head->q)*(buyer_price - seller_price));
                    
                    //notification body 
                    BRS_NOTIFY_INFO * data = getbody(head->id, it->id , head->q, seller_price);
                    //send BOUGHT packet
                    BRS_PACKET_HEADER * packet_head = getheader(BRS_BOUGHT_PKT);
                    trader_send_packet(buyer,packet_head, data);
                    //send SOLD packet
                    packet_head = getheader(BRS_SOLD_PKT);
                    trader_send_packet(seller,packet_head, data);
                    //broadcast TRADED packet
                    packet_head = getheader(BRS_TRADED_PKT);
                    trader_broadcast_packet(packet_head, data);

                    it->q -=head->q;
                    head->q = 0;
                    
                }
                flag=1;
                //update last trade price
                ex->last = seller_price;
            }
            pthread_mutex_unlock(&(it->order_mutex));
            it=it->next; 
        }    
    }
    else{ //When the latest order = SELL
        TRADER * seller = head->trader;
        uint32_t seller_price = head->p;
        struct order * it = head->next;
        while(it && head->q > 0){
            pthread_mutex_lock(&(it->order_mutex));
            debug("%u",head->q);
            uint32_t buyer_price = it->p; 
            if(it->type==1 && (seller_price <= buyer_price)){
                TRADER * buyer = it->trader;
                if(head->q >= it->q){ //sq>=bq
                    trader_increase_inventory(buyer,it->q);
                    trader_increase_balance(seller, it->q * seller_price);
                    trader_increase_balance(buyer, (it->q)*(buyer_price - seller_price));

                    //notification body 
                    BRS_NOTIFY_INFO * data = getbody(it->id, head->id , it->q, seller_price);
                    //send BOUGHT packet
                    BRS_PACKET_HEADER * packet_head = getheader(BRS_BOUGHT_PKT);
                    trader_send_packet(buyer,packet_head, data);
                    //send SOLD packet
                    packet_head = getheader(BRS_SOLD_PKT);
                    trader_send_packet(seller,packet_head, data);
                    //broadcast TRADED packet
                    packet_head = getheader(BRS_TRADED_PKT);
                    trader_broadcast_packet(packet_head, data);


                    head->q -= it->q;
                    it->q = 0;
                }else{
                    trader_increase_inventory(buyer,head->q);
                    trader_increase_balance(seller, head->q * seller_price);
                    trader_increase_balance(buyer, (head->q)*(buyer_price - seller_price));
                    
                    //notification body 
                    BRS_NOTIFY_INFO * data = getbody(it->id, head->id , head->q, seller_price);
                    //send BOUGHT packet
                    BRS_PACKET_HEADER * packet_head = getheader(BRS_BOUGHT_PKT);
                    trader_send_packet(buyer,packet_head, data);
                    //send SOLD packet
                    packet_head = getheader(BRS_SOLD_PKT);
                    trader_send_packet(seller,packet_head, data);
                    //broadcast TRADED packet
                    packet_head = getheader(BRS_TRADED_PKT);
                    trader_broadcast_packet(packet_head, data);

                    it->q -=head->q;
                    head->q = 0;
                    
                }
                flag=1;
                //update last trade price
                ex->last = seller_price;
            }
            pthread_mutex_unlock(&(it->order_mutex));
            it=it->next;
        }  
    }
    pthread_mutex_unlock(&(head->order_mutex));
    remove_empty_and_update_exchange(head,ex);
    return flag;
}
void remove_empty_and_update_exchange(struct order * head, EXCHANGE * ex){
    //remove zero quantities from order_list
    struct order ** root = ex->order_list;
    while((*root)!=NULL){
        struct order * it = *root;
        pthread_mutex_lock(&(it->order_mutex));
        if(it->q == 0){
            (*root) = (*root)->next;
            trader_unref(it->trader,"order removed from list");
            free(it);
        }else{
            root = &((*root)->next);
        }
        pthread_mutex_unlock(&(it->order_mutex));
    }
    //update current max bid and current min ask
    printallorders(ex);
    struct order * itt = *(ex->order_list);
    ex->bid = 0;
    ex->ask = 0;
    while(itt){
        pthread_mutex_lock(&(itt->order_mutex));
        if(itt->type==1){
            ex->bid = (itt->p > ex->bid)? itt->p : ex->bid;
        }
        else if(itt->type==2){
            if(ex->ask==0)
                ex->ask = itt->p;
            else
                ex->ask = (itt->p < ex->ask)? itt->p : ex->ask;
        }
        pthread_mutex_unlock(&(itt->order_mutex));
        itt=itt->next;
    }
}
void * matchmaker(void * ex){
    //Pthread_detach(pthread_self());
    while(loop){

        sem_wait(&(((EXCHANGE *)ex)->matchmaker_mutex));
        sem_wait(&(((EXCHANGE *)ex)->exchange_mutex));
        debug("Matchmaker %lu for exchange %p starting",tid, ex);
        struct order * head = *(((EXCHANGE *)ex)->order_list);
        if(!head || mm_helper(head, ex)==0){
            debug("Matchmaker %lu sees no possible trade",tid);
        }
        debug("Matchmaker %lu for exchange %p sleeping",tid, ex);
        sem_post(&(((EXCHANGE *)ex)->exchange_mutex));
        sem_post(&(((EXCHANGE *)ex)->wait));

    }
    debug("Match MAker Terminated");
    return NULL;
}

EXCHANGE * exchange_init(){
    EXCHANGE * ex = malloc(sizeof(EXCHANGE));
    ex->total_id=0;
    ex->bid = 0;
    ex->ask = 0;
    ex->last = 0;
    // ex->orderid = 0;
    // ex->quantity = 0;
    ex->next = NULL;
    ex->order_list = &(ex->next);
    debug("YInitialized Exchange %p",ex);
    sem_init(&(ex->exchange_mutex),0,1);
    sem_init(&(ex->matchmaker_mutex),0,1);
    sem_init(&(ex->wait),0,0);
    pthread_create(&tid, NULL, matchmaker, (void *)ex);
    sem_wait(&(ex->wait));
    return ex;
}

/*
 * Finalize an exchange, freeing all associated resources.
 *
 * @param xchg  The exchange to be finalized, which must not
 * be referenced again.
 */
void free_order_list(struct order * it){
    while(it){
        struct order * next = it->next;
        free(it);
        it = next;
    }
}
void exchange_fini(EXCHANGE *xchg){
    debug("YExchange Finish");
    loop =0; //will break match maker service loop
    sem_post(&(xchg->matchmaker_mutex)); // allow matchmaker to terminate
    pthread_join(tid,NULL); // reap matchmaker thread
    free_order_list(*(xchg->order_list)); //free order list
    debug("remaining order after free all");
    printallorders(xchg);
    free(xchg); //free exchange
    

}

/*
 * Get the current status of the exchange.
 */
void exchange_get_status(EXCHANGE *xchg, BRS_STATUS_INFO *infop){
    sem_wait(&(xchg->exchange_mutex));
    debug("YGet Status of Exchange %p",xchg);

    infop->bid = htonl(xchg->bid);
    infop->ask = htonl(xchg->ask);
    infop->last = htonl(xchg->last);
    sem_post(&(xchg->exchange_mutex)); 
}

struct order * init_order(orderid_t oid, TRADER *trader, int type, quantity_t quantity,
    funds_t price){
    struct order * new_order = malloc(sizeof(struct order));
    new_order->id = oid;
    new_order->trader = trader;
    new_order->type = type;
    new_order->q = quantity;
    new_order->p = price;
    pthread_mutexattr_init(&(new_order->ma));
    pthread_mutexattr_settype(&(new_order->ma), PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&(new_order->order_mutex),&(new_order->ma));
    return new_order;
}
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
                funds_t price){
        if(trader_decrease_balance(trader, quantity*price)<0)
            return 0;
        trader = trader_ref(trader, "YTo place new buy order");
       

        sem_wait(&(((EXCHANGE *)xchg)->exchange_mutex));
        orderid_t oid = ++(xchg->total_id);
        struct order * new_order = init_order(oid , trader, 1, quantity, price);
        new_order->next = *(xchg->order_list);
        *(xchg->order_list) = new_order;

        debug("Exchange %p posting buy order %d for trader %p, quantity %u, max price %u Last trade price: %u", xchg , oid, trader,quantity, price, xchg->last);
        printallorders(xchg);        
        sem_post(&(((EXCHANGE *)xchg)->exchange_mutex));

        BRS_PACKET_HEADER * head = getheader(BRS_POSTED_PKT);
        BRS_NOTIFY_INFO * data = getbody(oid, 0, quantity, price);


        if(trader_broadcast_packet(head, data)<0) //broadcast POSTED before TRADED
            oid =0;

        sem_post(&(xchg->matchmaker_mutex)); //make matchmaker available
        sem_wait(&(xchg->wait)); //wait for matchmaker to finish  before sending ACK
        return oid;

}


 // * Post a sell order on the exchange on behalf of a trader.
 // * The trader is stored with the order, and its reference count is
 // * increased by one to account for the stored pointer.
 // * Inventory equal to the amount of the order is
 // * encumbered by removing it from the traders account.
 // * A POSTED packet containing details of the order is broadcast
 // * to all logged-in traders.
 // *
 // * @param xchg  The exchange to which the order is to be posted.
 // * @param trader  The trader on whose behalf the order is to be posted.
 // * @param quantity  The quantity to be sold.
 // * @param price  The minimum sale price per unit.
 // * @return  The order ID assigned to the new order, if successfully posted,
 // * otherwise 0.
 
orderid_t exchange_post_sell(EXCHANGE *xchg, TRADER *trader, quantity_t quantity,
                 funds_t price){
        if(trader_decrease_inventory(trader, quantity)<0)return 0;
        trader = trader_ref(trader, "YTo place new sell order");

        sem_wait(&(xchg->exchange_mutex));
        orderid_t oid = ++(xchg->total_id);
        struct order * new_order = init_order(oid ,trader, 2, quantity, price);
        new_order->next = *(xchg->order_list);
        *(xchg->order_list) = new_order;

        debug("Exchange %p posting sell order %d for trader %p, quantity %u, min price %u \n Last trade price: %u", xchg, oid, trader,quantity, price, xchg->last);
        printallorders(xchg);
        sem_post(&(xchg->exchange_mutex));

        BRS_PACKET_HEADER * head = getheader(BRS_POSTED_PKT);
        BRS_NOTIFY_INFO * data = getbody(0, oid, quantity, price);

        if(trader_broadcast_packet(head, data)<0) //broadcast POSTED before TRADED
            oid =0;

        sem_post(&(xchg->matchmaker_mutex)); //make matchmaker available
        sem_wait(&(xchg->wait)); //wait till matchmaker finish
        return oid;
}

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
            quantity_t *quantity){
    debug("Exchange %p trying to cancel order %u",xchg,order);
    sem_wait(&(xchg->exchange_mutex));
    int flag = 0 ;
    struct order * head = xchg->next;
    struct order * it = xchg->next;
    while(it){
        if(it->id==order){
            pthread_mutex_lock(&(it->order_mutex));
            int b_oid=0;
            int s_oid=0;
            if(it->trader !=trader){
                debug("This trader did not create this order");
                pthread_mutex_unlock(&(it->order_mutex));
                sem_post(&(xchg->exchange_mutex));
                return -1;
            }else{
                uint32_t quantity_left = it->q;
                if(it->type == 1){
                    b_oid = order;
                    trader_increase_balance(trader,quantity_left * it->p); 
                }
                else{
                    s_oid = order;
                    trader_increase_inventory(trader,quantity_left);
                }
                it->q = 0;
                remove_empty_and_update_exchange(head, xchg);

                flag = 1;
                *quantity = quantity_left;
                BRS_PACKET_HEADER * pkt_head = getheader(BRS_CANCELED_PKT);
                BRS_NOTIFY_INFO * pkt_data = getbody(b_oid, s_oid, quantity_left, it->p);

                if(trader_broadcast_packet(pkt_head, pkt_data)<0) //broadcast CANCELLED packet
                {
                    pthread_mutex_unlock(&(it->order_mutex));
                    sem_post(&(xchg->exchange_mutex));
                    return -1;
                }
            }
            pthread_mutex_unlock(&(it->order_mutex));
            break;
        }
        it = it->next;
    }

    sem_post(&(xchg->exchange_mutex));
    if(flag==0){
        debug("Couldn't find orderid/ already traded");
        return -1;
    }

    return 0;
}
