#include <semaphore.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include "client_registry.h"
#include "debug.h"

struct client_registry{
    int n;
    fd_set client_set;
    sem_t mutex;
    sem_t count_zero;
};

CLIENT_REGISTRY *creg_init(){
    debug("YInitialize client registry");
    CLIENT_REGISTRY * ret = malloc(sizeof(CLIENT_REGISTRY));
    if(ret==NULL)return NULL;
    ret->n = 0;
    FD_ZERO(&(ret->client_set));
    sem_init(&(ret->mutex),0,1);
    sem_init(&(ret->count_zero),0,1);
    return ret;
}

int creg_register(CLIENT_REGISTRY *cr, int fd){
    
    if(sem_wait(&(cr->mutex))<0)
        return -1;
    FD_SET(fd,&(cr->client_set));
    cr->n = cr->n + 1;
    if(cr->n==1){
        sem_wait(&(cr->count_zero));
        int ans;
        sem_getvalue(&(cr->count_zero),&ans);
        //debug("Count Zero after first registering -> %d",ans);
    }

    debug("YRegister client fd %d (total connected: %d)",fd,cr->n);
    if(sem_post(&(cr->mutex))<0)
        return -1;

    return 0;
}

int creg_unregister(CLIENT_REGISTRY *cr, int fd){
    //debug("----creg unregister--------");
    if(sem_wait(&(cr->mutex))<0)
        return -1;
    FD_CLR(fd,&(cr->client_set));
    cr->n = cr->n - 1;
    //debug("Unregistered fd %d left %d",fd,cr->n);
    if((cr->n) == 0){
        sem_post(&(cr->count_zero));
    }
    debug("Unregister client fd %d (total connected: %d)",fd,cr->n);
    if(sem_post(&(cr->mutex))<0)
        return -1;
    return 0;
}

void creg_wait_for_empty(CLIENT_REGISTRY *cr){
    debug("-----creg wait for empty--------");

    sem_wait(&(cr->count_zero)); //wait for all clients to terminatate
}

void creg_shutdown_all(CLIENT_REGISTRY *cr){
    sem_wait(&(cr->mutex));
    for(int i=4; i<FD_SETSIZE; i++){
        if(FD_ISSET(i,&(cr->client_set))){
            debug("YShutting down fd %d",i);
            shutdown(i,SHUT_RD);
        }
    }
    sem_post(&(cr->mutex));
}

void creg_fini(CLIENT_REGISTRY *cr){
    debug("YFinalize client registry");
    free(cr);
}