#include "protocol.h"
#include "csapp.h"
#include "debug.h"

int proto_send_packet(int fd, BRS_PACKET_HEADER *hdr, void *payload){
    // wrte from packet to fd | nton
    //debug("---proto_send_packet------");
    int n = sizeof(BRS_PACKET_HEADER);

    debug("%d",n);
    // debug("type %hhu",hdr->type);
    // debug("size %hu",ntohs(hdr->size));
    // debug("sec %u",ntohl(hdr->timestamp_sec)); 
    int retval = rio_writen(fd, (void *)hdr, n);

    if(retval==-1){
        return -1;
    }

    size_t sz = ntohs(hdr->size);

    if(sz>0 && payload){
        //  debug("balance %hhu",ex->balance);
        // debug("balance %hhu",ntohl(ex->balance));
        // debug("inventory %hu",ntohl(ex->inventory));
        retval = rio_writen(fd, payload, sz);
        
        if(retval==-1){
            return -1;
        }
    }

    return 0;
}

int proto_recv_packet(int fd, BRS_PACKET_HEADER *hdr, void **payloadp){
    //debug("---proto_recv_packet------");
    //fd -> packet | nton

    int  n = sizeof(BRS_PACKET_HEADER);
    int retval = rio_readn(fd, (void *)hdr, n);

    if(retval==-1){
        debug("readn error");
        return -1;
    }
    //return -1;
    if(retval==0){
        debug("EOF on fd %d",fd);
        return -1;
    }

    // debug("type %hhu",hdr->type);
    // debug("size %hu",ntohs(hdr->size));
    // debug("sec %u",ntohl(hdr->timestamp_sec));
    size_t sz = ntohs(hdr->size);
    if(sz<=0)
        return 0;

    char * buf = malloc(sizeof(char) * (sz));
    retval = rio_readn(fd,buf,sz);
    if(retval==-1){
        debug("readn error");
        return -1;
    }
    *payloadp = (void *)buf;
    
    return 0;
}