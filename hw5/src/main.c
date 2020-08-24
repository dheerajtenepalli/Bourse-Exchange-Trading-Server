#include <stdlib.h>
#include <csapp.h>

#include "client_registry.h"
#include "exchange.h"
#include "trader.h"
#include "debug.h"
#include "server.h"
#include "protocol.h"

extern EXCHANGE *exchange;
extern CLIENT_REGISTRY *client_registry;

static void terminate(int status);
// volatile int flag;
/*
 * "Bourse" exchange server.
 *
 * Usage: bourse <port>
 */

void * thread(void *argvs){
    pthread_detach(pthread_self());
    brs_client_service(argvs);
    //free(argvs);
    return NULL;
}
void handler(int sig){
    debug("Inside gihup handler");
    terminate(EXIT_SUCCESS);
}
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    if(argc < 3 || strcmp(argv[1],"-p")!=0){
        debug("Usage: util/demo_server -p <port>");
        exit(0);
    }
    // size_t port_number = str_to_num(argv[2]);
    // Perform required initializations of the client_registry,
    // maze, and player modules.
    client_registry = creg_init();
    exchange = exchange_init();
    trader_init();
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = &handler;
    sigaction(SIGHUP,&sa,NULL);

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function brs_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.
    int listenfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    listenfd = Open_listenfd(argv[2]);

    while(1){
        int *connfd;
        clientlen = sizeof(clientaddr);
        connfd = malloc(sizeof(int));
        //debug("hello1");
        *connfd = accept(listenfd, (SA*) &clientaddr, &clientlen);
        //debug("hello2");
        pthread_create(&tid,NULL, thread, connfd);
        //debug("hello3");
    }

    fprintf(stderr, "You have to finish implementing main() "
	    "before the Bourse server will function.\n");

    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
static void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);
    
    debug("Waiting for service threads to terminate...");
    creg_wait_for_empty(client_registry);
    debug("All service threads terminated.");

    // Finalize modules.
    creg_fini(client_registry);
    exchange_fini(exchange);
    trader_fini();

    debug("Bourse server terminating");
    exit(status);
}
