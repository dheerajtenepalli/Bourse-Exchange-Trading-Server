#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "protocol.h"
#include "trader.h"
#include "debug.h"



struct trader{
    uint32_t balance;
    uint32_t inventory;
    int ref; 
    int logged_in; //stores whether logged in or not
    int fd;
    char * name ;
    pthread_mutex_t trader_mutex;
    pthread_mutexattr_t ma;

}*all_trader;


int trader_init(void){
    debug(" YInitialize trader module");
    // I have taken a array of traders rather than dynamically allocation memory
    // because the no. of traders to support were low, so allocating a chunk of memory 
    //before hand does not matter. This makes implementation easier.
    all_trader = (TRADER *)malloc(sizeof(TRADER)*64);
    if(!all_trader)return -1;
    for(int i=0;i<64;i++){
        all_trader[i].balance =0;
        all_trader[i].inventory = 0;
        all_trader[i].ref = 0;
        all_trader[i].logged_in = 0;
        all_trader[i].fd = i+4; //will be always overwritten
        all_trader[i].name = NULL;
        pthread_mutexattr_init(&(all_trader[i].ma));
        pthread_mutexattr_settype(&(all_trader[i].ma), PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&(all_trader[i].trader_mutex),&(all_trader[i].ma));

    }
    return 0;
}


void trader_fini(void){
    debug("Finalize trader module");
    for(int i=0;i<64;i++){
        if(all_trader[i].ref!=0){
            trader_unref(&(all_trader[i]),"because server is shuttingdown");
        }
        if(all_trader[i].name){
            free(all_trader[i].name); //free space used by names
        }
    }
    debug("Free all traders");
    free(all_trader);
}


TRADER *trader_login(int fd, char *name){

    //check if old trader
    for(int i=0;i<64;i++){
        pthread_mutex_lock(&(all_trader[i].trader_mutex));
        if(all_trader[i].name!=NULL && all_trader[i].ref >=1 && all_trader[i].logged_in==0 && strcmp(name,all_trader[i].name)==0){
            debug("YLogging old trader %p named %s",&(all_trader[i]), name);
            all_trader[i].fd = fd;
            all_trader[i].logged_in = 1;
            TRADER * ref = trader_ref(&(all_trader[i]), "because trader has logged in");
            pthread_mutex_unlock(&(all_trader[i].trader_mutex));
            return ref;
        }
        //check if duplicate trader is already logged in
        else if(all_trader[i].name!=NULL && all_trader[i].ref>1 && all_trader[i].logged_in==1 && strcmp(name,all_trader[i].name)==0){
            debug("Duplicate name trader logged in %s", name);
            pthread_mutex_unlock(&(all_trader[i].trader_mutex));
            return NULL;

        }
        pthread_mutex_unlock(&(all_trader[i].trader_mutex));
    }
    int pos =-1;
    for(int i=0;i<64;i++){
        pthread_mutex_lock(&(all_trader[i].trader_mutex));
        if(all_trader[i].ref==0 && all_trader[i].name==NULL){
            debug("YLogging new trader %p named %s",&(all_trader[i]), name);
            pos = i;
            pthread_mutex_unlock(&(all_trader[i].trader_mutex));
            break;
        }
        pthread_mutex_unlock(&(all_trader[i].trader_mutex));
    }

    if(pos==-1){
        debug("Max trader exceeded");
        return NULL;
    }
    pthread_mutex_lock(&(all_trader[pos].trader_mutex));
    all_trader[pos].name = malloc(sizeof(char)*(strlen(name)+1));
    strcpy(all_trader[pos].name, name);
    all_trader[pos].fd = fd;
    all_trader[pos].logged_in = 1;
    TRADER * ref = trader_ref(&(all_trader[pos]), "for newly created trader");
    ref = trader_ref(&(all_trader[pos]), "because trader has logged in");
    pthread_mutex_unlock(&(all_trader[pos].trader_mutex));
    return ref;
}


void trader_logout(TRADER *trader){
    pthread_mutex_lock(&(trader->trader_mutex));
    debug("YLog out trader %p named %s",trader, trader->name);
    trader_unref(trader, "because trader has logged out");
    trader->logged_in = 0;
    pthread_mutex_unlock(&(trader->trader_mutex));
}


TRADER *trader_ref(TRADER *trader, char *why){
    pthread_mutex_lock(&(trader->trader_mutex));
    debug("YIncrease count reference on  trader %p from  (%d->%d) %s",
        trader, trader->ref, trader->ref +1, why);
    trader->ref++;
    pthread_mutex_unlock(&(trader->trader_mutex));
    return trader;
}


void trader_unref(TRADER *trader, char *why){
    pthread_mutex_lock(&(trader->trader_mutex));
    debug("YDecrease count reference on  trader %p from  (%d->%d) %s",
        trader, trader->ref, trader->ref -1, why);
    trader->ref--;
    pthread_mutex_unlock(&(trader->trader_mutex));
}



int trader_send_packet(TRADER *trader, BRS_PACKET_HEADER *pkt, void *data){
    pthread_mutex_lock(&(trader->trader_mutex));
    debug("YSend packet (clientfd=%d, type=ACK) for trader %p [%s]",
        trader->fd, trader,trader->name);
    int i = 0;
    //check if trader is logged in
    if(trader->ref >1 && trader->logged_in ==1 )
        i = proto_send_packet(trader->fd, pkt, data);
    pthread_mutex_unlock(&(trader->trader_mutex));
    return i;
}


int trader_broadcast_packet(BRS_PACKET_HEADER *pkt, void *data){
    debug("YBroadcast packet");
    for(int i=0;i<64;i++){
        pthread_mutex_lock(&(all_trader[i].trader_mutex));
        if(all_trader[i].ref >= 2 && all_trader[i].logged_in==1){
            if(trader_send_packet(&all_trader[i],pkt,data)<0){
                pthread_mutex_unlock(&(all_trader[i].trader_mutex));
                return -1;
            }
        }
        pthread_mutex_unlock(&(all_trader[i].trader_mutex));
    }

    return 0;
}


int trader_send_ack(TRADER *trader, BRS_STATUS_INFO *info){
    pthread_mutex_lock(&(trader->trader_mutex));
    debug("YSend packet (clientfd=%d, type=ACK) for trader %p [%s]",trader->fd, trader,trader->name);
    int ret =0;
    //check if trader logged in
    if(trader->ref >1 && trader->logged_in ==1 ){
        struct timespec curr_time;
        clock_gettime(CLOCK_MONOTONIC,&curr_time);
        //add header
        BRS_PACKET_HEADER * ack = malloc(sizeof(BRS_PACKET_HEADER));
        ack->type = BRS_ACK_PKT;
        ack->size = htons(sizeof(BRS_STATUS_INFO));
        ack->timestamp_sec = htonl(curr_time.tv_sec);
        ack->timestamp_nsec = htonl(curr_time.tv_nsec);
        if(info){
            info->balance = htonl(trader->balance);
            info->inventory = htonl(trader->inventory);
        }
        ret = proto_send_packet(trader->fd, ack, info);
    }
    pthread_mutex_unlock(&(trader->trader_mutex));
    return ret;
}


int trader_send_nack(TRADER *trader){
    pthread_mutex_lock(&(trader->trader_mutex));
    debug("YSend packet (clientfd=%d, type=NACK) for trader %p [%s]",
        trader->fd, trader,trader->name);

    int ret = 0;
    //check if trader logged in
    if(trader->ref >1 && trader->logged_in ==1 ){
        struct timespec curr_time;
        clock_gettime(CLOCK_MONOTONIC,&curr_time);
        BRS_PACKET_HEADER * ack = malloc(sizeof(BRS_PACKET_HEADER));
        ack->type = BRS_NACK_PKT;
        ack->size = htons(0);
        ack->timestamp_sec = htonl(curr_time.tv_sec);
        ack->timestamp_nsec = htonl(curr_time.tv_nsec);

        ret = proto_send_packet(trader->fd, ack, NULL);
    }
    
    pthread_mutex_unlock(&(trader->trader_mutex));
    return ret;
}


void trader_increase_balance(TRADER *trader, funds_t amount){
    pthread_mutex_lock(&(trader->trader_mutex));
    debug("YIncrease balance (%d->%d)",trader->balance, trader->balance + amount);
    trader->balance += amount;
    pthread_mutex_unlock(&(trader->trader_mutex));
}


int trader_decrease_balance(TRADER *trader, funds_t amount){
    pthread_mutex_lock(&(trader->trader_mutex));
    if(trader->balance < amount){
        debug("YBalance %d is less than debit amount %d",trader->balance, amount);
        pthread_mutex_unlock(&(trader->trader_mutex));
        return -1;
    }
    debug("YDecrease balance (%d->%d",trader->balance, trader->balance - amount );
    trader->balance -= amount;
    pthread_mutex_unlock(&(trader->trader_mutex));
    return 0;
}


void trader_increase_inventory(TRADER *trader, quantity_t quantity){
    pthread_mutex_lock(&(trader->trader_mutex));
    debug("YIncrease Inventory (%d->%d)",trader->inventory, trader->inventory + quantity);
    trader->inventory += quantity;
    pthread_mutex_unlock(&(trader->trader_mutex));
}


int trader_decrease_inventory(TRADER *trader, quantity_t quantity){
    pthread_mutex_lock(&(trader->trader_mutex));
    if(trader->inventory < quantity){
        debug("YInventory %d is less than debit quantity %d",trader->inventory, quantity);
        pthread_mutex_unlock(&(trader->trader_mutex));
        return -1;
    }
    debug("YDecrease Inventory (%d->%d)",trader->inventory, trader->inventory - quantity);
    trader->inventory -= quantity;
    pthread_mutex_unlock(&(trader->trader_mutex));
    return 0;
}

