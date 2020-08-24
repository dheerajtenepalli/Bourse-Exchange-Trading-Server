#include <stdlib.h>
#include <csapp.h>
#include <time.h>
#include "server.h"
#include "debug.h"
#include "protocol.h"
#include "trader.h"

void print_infop(BRS_STATUS_INFO *i){
    debug("balance %u %u",i->balance, ntohl(i->balance));
    debug("inventory %u %u",i->inventory, ntohl(i->inventory));
    debug("bid %u %u",i->bid, ntohl(i->bid));
    debug("ask %u %u",i->ask, ntohl(i->ask));
    debug("last %u %u",i->last, ntohl(i->last));
    debug("orderid %u %u",i->orderid, ntohl(i->orderid));
    debug("quantity %u %u",i->quantity, ntohl(i->quantity));
}

void *brs_client_service(void *arg){
    int fd = *((int *)arg);
    int login=0;
    free(arg);
    debug("[%d] Starting client service",fd);
    if(creg_register(client_registry,fd)<0){
        debug("creg register error");
        return NULL;
    }
    BRS_PACKET_HEADER *header = malloc(sizeof(BRS_PACKET_HEADER));
    void *payload=NULL;
    TRADER * tdr=NULL;
    while(1){
        if(proto_recv_packet(fd, header, &payload) < 0){
            if(login==1){
                debug("[%d] Logging out trader",fd);
                trader_logout(tdr);
            }
            debug("[%d] Ending client service",fd);
            creg_unregister(client_registry, fd);
            free(header);
            return NULL;
        }
        uint8_t type = header->type;
        void * body = NULL;
        if(payload==NULL)debug("payload is null");
        if(payload)
            body = payload;

        struct timespec curr_time;
        clock_gettime(CLOCK_MONOTONIC,&curr_time);
        BRS_PACKET_HEADER * ack = malloc(sizeof(BRS_PACKET_HEADER));
        ack->size = htons(0);
        ack->timestamp_sec = htonl(curr_time.tv_sec);
        ack->timestamp_nsec = htonl(curr_time.tv_nsec);
        void * ack_pl = NULL;

        if(type==1){
            debug("[%d] LOGIN packet recieved",fd);

            if(login==1){
                debug("[%d] Already logged in",fd);
                ack->type = BRS_NACK_PKT;
                proto_send_packet(fd,ack,ack_pl);
            }
            else{
                tdr = trader_login(fd,body);
                if(tdr==NULL){
                    debug("empty trader");
                    ack->type = BRS_NACK_PKT;
                    proto_send_packet(fd,ack,ack_pl);
                }
                else{
                    login=1;
                    ack->type = BRS_ACK_PKT;
                    trader_send_packet(tdr,ack,NULL);
                } 
            }
        }
        else if(type==2){
             debug("[%d] STATUS packet recieved",fd);
             if(login==0){
                debug("Login Required");
                // ack->type = BRS_NACK_PKT;
                // proto_send_packet(fd,ack,ack_pl);
             }
             else{
                BRS_STATUS_INFO *infop = malloc(sizeof(BRS_STATUS_INFO));
                exchange_get_status(exchange, infop);
                trader_send_ack(tdr,infop);
             }
        }
        else if(type==3){
             debug("[%d] DEPOSIT packet recieved",fd);
             if(login==0){
                debug("Login Required");
                // ack->type = BRS_NACK_PKT;
                // proto_send_packet(fd,ack,ack_pl);
             }
             else{
                trader_increase_balance(tdr, ntohl(((BRS_FUNDS_INFO *)body)->amount));
                BRS_STATUS_INFO *infop = malloc(sizeof(BRS_STATUS_INFO));
                //exchange_get_status(exchange, infop);
                BRS_STATUS_INFO * ack_pl = infop;
                trader_send_ack(tdr,ack_pl);
             }
        }
        else if(type==4){
             debug("[%d] WITHDRAW packet recieved",fd);
             if(login==0){
                debug("Login Required");
                // ack->type = BRS_NACK_PKT;
                // proto_send_packet(fd,ack,ack_pl);
             }
             else{
                
                if(trader_decrease_balance(tdr, ntohl(((BRS_FUNDS_INFO *)body)->amount))<0){
                    trader_send_nack(tdr);
                }
                else{
                    BRS_STATUS_INFO *infop = malloc(sizeof(BRS_STATUS_INFO));
                    ack_pl = infop;
                    trader_send_ack(tdr,ack_pl);
                }
                
             }
        }
        else if(type==5){
             debug("[%d] ESCROW packet recieved",fd);
             if(login==0){
                debug("Login Required");
                // ack->type = BRS_NACK_PKT;
                // proto_send_packet(fd,ack,ack_pl);
             }
             else{
                trader_increase_inventory(tdr, ntohl(((BRS_ESCROW_INFO *)body)->quantity));
                BRS_STATUS_INFO *infop = malloc(sizeof(BRS_STATUS_INFO));
                //exchange_get_status(exchange, infop);
                ack_pl = infop;
                trader_send_ack(tdr,ack_pl);
             }
        }
        else if(type==6){
             debug("[%d] RELEASE packet recieved",fd);
             if(login==0){
                debug("Login Required");
                // ack->type = BRS_NACK_PKT;
                // proto_send_packet(fd,ack,ack_pl);
             }
             else{
                if(trader_decrease_inventory(tdr, ntohl(((BRS_ESCROW_INFO *)body)->quantity))<0){
                    trader_send_nack(tdr);
                }
                else{
                    BRS_STATUS_INFO *infop = malloc(sizeof(BRS_STATUS_INFO));
                    ack_pl = infop;
                    trader_send_ack(tdr,ack_pl);
                }
                
             }
        }
        else if(type==7){
             debug("[%d] BUY packet recieved",fd);
             if(login==0){
                debug("Login Required");
                // ack->type = BRS_NACK_PKT;
                // proto_send_packet(fd,ack,ack_pl);
             }
             else{
                uint32_t q = ntohl(((BRS_ORDER_INFO *)body)->quantity);
                uint32_t p = ntohl(((BRS_ORDER_INFO *)body)->price);
                int id = exchange_post_buy(exchange, tdr,  q, p);
                if(id==0){
                   trader_send_nack(tdr);
                }
                else{
                    BRS_STATUS_INFO *infop = malloc(sizeof(BRS_STATUS_INFO));
                    exchange_get_status(exchange, infop);
                    infop->orderid = htonl(id);
                    trader_send_ack(tdr,infop);
                }
                
             }
        }
        else if(type==8){
             debug("[%d] SELL packet recieved",fd);
             if(login==0){
                debug("Login Required");
                // ack->type = BRS_NACK_PKT;
                // proto_send_packet(fd,ack,ack_pl);
             }
             else{
                uint32_t q = ntohl(((BRS_ORDER_INFO *)body)->quantity);
                uint32_t p = ntohl(((BRS_ORDER_INFO *)body)->price);
                int id = exchange_post_sell(exchange, tdr,  q, p);
                if(id==0){
                   trader_send_nack(tdr);
                }
                else{

                    BRS_STATUS_INFO *infop = malloc(sizeof(BRS_STATUS_INFO));
                    exchange_get_status(exchange, infop);
                    infop->orderid = htonl(id);
                    //print_infop(infop);
                    trader_send_ack(tdr,infop);
                }
                
             }
        }
        else if(type==9){
             debug("[%d] CANCEL packet recieved",fd);
             if(login==0){
                debug("Login Required");
                // ack->type = BRS_NACK_PKT;
                // proto_send_packet(fd,ack,ack_pl);
             }
             else{
                uint32_t * quan = malloc(sizeof(uint32_t));
                uint32_t id = ntohl(((BRS_CANCEL_INFO *)body)->order);
                debug("brs_cancel: order: %d",id);
                if(exchange_cancel(exchange, tdr, id, quan)<0){
                   trader_send_nack(tdr);
                }
                else{
                    BRS_STATUS_INFO *infop = malloc(sizeof(BRS_STATUS_INFO));
                    exchange_get_status(exchange, infop);
                    infop->orderid = htonl(id);
                    infop->quantity = htonl(*quan);
                    debug("Quantity cancelled %u",*quan);
                    trader_send_ack(tdr,infop);
                }
                free(quan);
                
             }
        }
    }
    return NULL;
}