# Homework 5 - CSE 320 - Fall 2019
#### Professor Eugene Stark

### **Due Date: Friday 12/6/2019 @ 11:59pm**

## Introduction

The goal of this assignment is to become familiar with low-level POSIX
threads, multi-threading safety, concurrency guarantees, and networking.
The overall objective is to implement a simple exchange server, such as
might be used to perform electronic trading of securities or currencies.
As you will probably find this somewhat difficult, to grease the way
I have provided you with a design for the server, as well as binary object
files for almost all the modules.  This means that you can build a
functioning server without initially facing too much complexity.
In each step of the assignment, you will replace one of my
binary modules with one built from your own source code.  If you succeed
in replacing all of my modules, you will have completed your own
version of the server.

It is probably best if you work on the modules in roughly the order
indicated below.  Turn in as many modules as you have been able to finish
and have confidence in.  Don't submit incomplete modules or modules
that don't function at some level, as these will negatively impact
the ability of the code to be compiled or to pass tests.

### Takeaways

After completing this homework, you should:

* Have a basic understanding of socket programming
* Understand thread execution, locks, and semaphores
* Have an advanced understanding of POSIX threads
* Have some insight into the design of concurrent data structures
* Have enhanced your C programming abilities

## Hints and Tips

* We strongly recommend you check the return codes of all system
  calls. This will help you catch errors.

* **BEAT UP YOUR OWN CODE!** Throw lots of concurrent calls at your
  data structure libraries to ensure safety.

* Your code should **NEVER** crash. We will be deducting points for
  each time your program crashes during grading. Make sure your code
  handles invalid usage gracefully.

* You should make use of the macros in `debug.h`. You would never
  expect a library to print arbitrary statements as it could interfere
  with the program using the library. **FOLLOW THIS CONVENTION!**
  `make debug` is your friend.

> :scream: **DO NOT** modify any of the header files provided to you in the base code.
> These have to remain unmodified so that the modules can interoperate correctly.
> We will replace these header files with the original versions during grading.
> You are of course welcome to create your own header files that contain anything
> you wish.

> :nerd: When writing your program, try to comment as much as possible
> and stay consistent with your formatting.

## Helpful Resources

### Textbook Readings

You should make sure that you understand the material covered in
chapters **11.4** and **12** of **Computer Systems: A Programmer's
Perspective 3rd Edition** before starting this assignment.  These
chapters cover networking and concurrency in great detail and will be
an invaluable resource for this assignment.

### pthread Man Pages

The pthread man pages can be easily accessed through your terminal.
However, [this opengroup.org site](http://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread.h.html)
provides a list of all the available functions.  The same list is also
available for [semaphores](http://pubs.opengroup.org/onlinepubs/7908799/xsh/semaphore.h.html).

## Getting Started

Fetch and merge the base code for `hw5` as described in `hw0`. You can
find it at this link: https://gitlab02.cs.stonybrook.edu/cse320/hw5.
Remember to use the `--stategy-option=theirs` flag for the `git merge`
command to avoid merge conflicts in the Gitlab CI file.

## The Bourse Exchange Server: Overview

"Bourse" is a simple implementation of an exchange server, of the type
that might be used to perform electronic trading of securities or currencies.
In this simple design, the server is accessed concurrently by a collection
of **traders**, who buy and sell units of one particular item.
Each trade exchanges one or more units of **inventory** for some number
of units of **funds**.  Traders express their desire to make trades by
posting buy or sell orders on the exchange.  The exchange serves as a
"matchmaker", finding matching buy and sell orders and executing trades.
The server can be queried for the current bid/ask spread and the last trade
price.  Traders connected to the system receive messages that notify them
about the disposition of their own orders that they have posted, as well
as "ticker tape" notifications that inform them about other events that occur,
such as posting and cancellation of orders and execution of trades.

Perhaps the easiest way to understand what the server does is to try it out.
For this purpose, I have provided an executable (`util/demo_server`) for
a completely implemented demonstration version of the server.
Launch it using the following command:

```
$ util/demo_server -p 3333
```

The `-p 3333` option is required, and it specifies the port number on which
the server will listen.  It will be convenient to start the server in a
separate terminal window, because it has not been written to detach from the
terminal and run as a `daemon` like a normal server would do.  This is because
you will be starting and stopping it frequently and you will want to be able
to see the voluminous debugging messages it issues.
The server does not ignore `SIGINT` as a normal daemon would,
so you can ungracefully shut down the server at any time by typing CTRL-C.
Note that the only thing that the server prints are debugging messages;
in a non-debugging setting there should be no output from the server
while it is running.

Once the server is started, you can use a test client program to access it.
The test client is called `util/client` and it has been provided only as
a binary executable.  The client is invoked as follows:

```
util/client -p <port> [-h <host>]
```

The `-p` option is required, and it is used to specify the port
number of the server.  If the `-h` option is given, then it specifies
the name of the network host on which the server is running.
If it is not given, then `localhost` is used.
Once the client is invoked, it will attempt to connect to the server.
If this succeeds, you will be presented with a command prompt and a
help message that lists the available commands:

```
$ util/client -p 1234
'Bourse' test client.
Commands are:
	help
	login <username>
	status
	deposit <amount>
	withdraw <amount>
	escrow <quantity>
	release <quantity>
	buy <quantity> <max price>
	sell <quantity> <min price>
	cancel <order id>
Connected to server localhost:1234
>
```

Once connected to the server, it is necessary to log in before any commands
other than ``help`` or ``login`` can be used.  Logging in is accomplished
using the ``login`` command, which requires that a username be specified
as an argument.  Any username can be used, as long as it is not currently
in use by some other logged-in trader.

Multiple clients can connect to the server at one time.  You should try
opening several terminal windows and starting a client in each of them to see
how this works.  If your computer and/or LAN does not firewall the connections,
you will also be able to connect to a server running on one computer from a
server elsewhere in the Internet.  This will be most likely to work between two
computers on the same LAN (e.g. connected to the same WiFi router, if the
router is configured to allow connected computers to talk to each other).
If it doesn't, there isn't much I can do about it.  In that case you'll just have
to use it on your own computer.

The Bourse server architecture is that of a multi-threaded network server.
When the server is started, a **master** thread sets up a socket on which to
listen for connections from clients.  When a connection is accepted,
a **client service thread** is started to handle requests sent by the client
over that connection.  The client service thread executes a service loop in which
it repeatedly receives a **request packet** sent by the client, performs the request,
and possibly sends one or more packets in response.  The server will also
send packets to the client asynchronously as the result of actions performed by
other users.  For example, whenever the server executes a trade,
a packet is broadcast to all logged-in traders announcing the details.

> :nerd: One of the basic tenets of network programming is that a
> network connection can be broken at any time and the parties using
> such a connection must be able to handle this situation.  In the
> present context, the client's connection to the Bourse server may
> be broken at any time, either as a result of explicit action by the
> client or for other reasons.  When disconnection of the client is
> noticed by the client service thread, the corresponding trader is logged
> out of the server and the client service thread terminates.
> Any unfulfilled orders that were posted by the now-logged-out trader
> remain in the system, as does the trader's account balance and inventory.
> The next time that trader logs in to the system, the balance and inventory
> will again be available; possibly having been updated as a result of trades
> that were performed while the trader was logged out.

### The Base Code

Here is the structure of the base code:

```
.
├── .gitlab-ci.yml
└── hw5
    ├── lib
    │   ├── bourse_debug.a
    │   └── bourse.a
    ├── util
    │   ├── client
    │   └── demo_server
    ├── src
    │   └── main.c
    ├── include
    │   ├── debug.h
    │   ├── client_registry.h
    │   ├── protocol.h
    │   ├── server.h
    │   ├── trader.h
    │   └── exchange.h
    ├── tests
    │   └── bourse_tests.c
    └── Makefile
```

The base code consists of header files that define module interfaces,
a library `bourse.a` containing binary object code for my
implementations of the modules, and a source code file `main.c` that
contains containing a stub for function `main()`.  The `Makefile` is
designed to compile any existing source code files and then link them
against the provided library.  The result is that any modules for
which you provide source code will be included in the final
executable, but modules for which no source code is provided will be
pulled in from the library.  The `bourse.a` library was compiled
without `-DDEBUG`, so it does not produce any debugging printout.
Also provided is `bourse_debug.a`, which was compiled with `-DDEBUG`,
and which will produce a lot of debugging output.  The `Makefile`
is set up to use `bourse_debug.a` when you say `make debug` and
`bourse.a` when you just say `make`.

The `util` directory contains the executable for the text-based client
program, `client`.
Besides the `-h` and `-p` options discussed above, the `client` program
also supports the `-q` option, which takes no arguments.  If `-q` is given,
then `client` suppresses its normal prompt.  This may be useful for using
`client` to feed in pre-programmed commands written in a file.
The list of commands that `client` understands can be viewed by typing
`help` at the command prompt.

Most of the detailed specifications for the various modules and functions
that you are to implement are provided in the comments in the header
files in the `include` directory.  In the interests of brevity and avoiding
redundancy, those specifications are not reproduced in this document.
Nevertheless, the information they contain is very important, and constitutes
the authoritative specification of what you are to implement.

> :scream: The various functions and variables defined in the header files
> constitute the **entirety** of the interfaces between the modules in this program.
> Use these functions and variables as described and **do not** introduce any
> additional functions or global variables as "back door" communication paths
> between the modules.  If you do, the modules you implement will not interoperate
> properly with my implementations, and it will also likely negatively impact
> our ability to test your code.

The test file I have provided contains some code to start a server and
attempt to connect to it.  It will probably be useful while you are
working on `main.c`.

## Task I: Server Initialization

When the base code is compiled and run, it will print out a message
saying that the server will not function until `main()` is
implemented.  This is your first task.  The `main()` function will
need to do the following things:

- Obtain the port number to be used by the server from the command-line
  arguments.  The port number is to be supplied by the required option
  `-p <port>`.
  
- Install a `SIGHUP` handler so that clean termination of the server can
  be achieved by sending it a `SIGHUP`.  Note that you need to use
  `sigaction()` rather than `signal()`, as the behavior of the latter is
  not well-defined in a multithreaded context.

- Set up the server socket and enter a loop to accept connections
  on this socket.  For each connection, a thread should be started to
  run function `brs_client_service()`.

These things should be relatively straightforward to accomplish, given the
information presented in class and in the textbook.  If you do them properly,
the server should function and accept connections on the specified port,
and you should be able to connect to the server using the test client.
Note that if you build the server using `make debug`, then the binaries
I have supplied will produce a fairly extensive debugging trace of what
they are doing.  This, together with the specifications in this document
and in the header files, will likely be invaluable to you in understanding
the desired behavior of the various modules.

## Task II: Send and Receive Functions

The header file `include/protocol.h` defines the format of the packets
used in the Bourse network protocol.  The concept of a protocol is an
important one to understand.  A protocol creates a standard for
communication so that any program implementing the protocol will be able
to connect and operate with any other program implementing the same
protocol.  Any client should work with any server if they both
implement the same protocol correctly.  In the Bourse protocol,
clients and servers exchange **packets** with each other.  Each packet
has two parts: a fixed-size header that describes the packet, and an
optional **payload** that can carry arbitrary data.  The fixed-size
header always has the same size and format, which is given by the
`brs_packet` structure; however the payload can be of arbitrary size.
One of the fields in the header tells how long the payload is.

- The function `proto_send_packet` is used to send a packet over a
network connection.  The `fd` argument is the file descriptor of a
socket over which the packet is to be sent.  The `pkt` argument is a
pointer to the fixed-size packet header.  The `data` argument is a
pointer to the data payload, if there is one, otherwise it is `NULL`.
The `proto_send_packet` function uses the `write()` system call
write the packet header to the "wire" (i.e. the network connection).
If the length field of the header specifies a nonzero payload length,
then an additional `write()` call is used to write the payload data
to the wire immediately following the header.

> :nerd:  The `proto_send_packet` assumes that multi-byte fields in
> the packet passed to it are stored in **network byte order**,
> which is a standardized byte order for sending multi-byte values
> over the network.  Note that, as it happens, network byte order
> is different than the **host byte order** used on the x86-64 platforms
> we are using, so you must convert multi-byte quantities from host
> to network byte order when storing them in a packet, and you must
> convert from network to host byte order when reading multi-byte
> quantities out of a packet.  These conversions can be accomplished
> using the library functions `htons()`, `htonl()`, `ntohs()`, `ntohl()`,
> *etc*.  See the man page for `ntohl()` for details and a full list
> of the available functions.

- The function `proto_recv_packet()` reverses the procedure in order to
receive a packet.  It first uses the `read()` system call to read a
fixed-size packet header from the wire.  If the length field of the header
is nonzero then an additional `read()` is used to read the payload from the
wire (note that the length field arrives in network byte order!).
The `proto_recv_packet()` uses `malloc()` to allocate memory for the
payload (if any), whose length is not known until the packet header is read.
A pointer to the payload is stored in a variable supplied by the caller.
It is the caller's responsibility to `free()` the payload once it is
no longer needed.

**NOTE:** Remember that it is always possible for `read()` and `write()`
to read or write fewer bytes than requested.  You must check for and
handle these "short count" situations.

Implement these functions in a file `protocol.c`.  If you do it
correctly, the server should function as before.

## Task III: Client Registry

You probably noticed the initialization of the `client_registry`
variable in `main()` and the use of the `creg_wait_for_empty()`
function in `terminate()`.  The client registry provides a way of
keeping track of the number of client connections that currently exist,
and to allow a "master" thread to forcibly shut down all of the
connections and to await the termination of all server threads
before finally terminating itself.  It is much more organized and
modular to simply present to each of the server threads a condition
that they can't fail to notice (i.e. EOF on the client connection)
and to allow themselves to perform any necessary finalizations and shut
themselves down, than it is for the main thread to try to reach in
and understand what the server threads are doing at any given time
in order to shut them down.

The functions provided by a client registry are specified in the
`client_registry.h` header file.  Provide implementations for these
functions in a file `src/client_registry.c`.  Note that these functions
need to be thread-safe (as will most of the functions you implement
for this assignment), so synchronization will be required.  Use a
mutex to protect access to the thread counter data.  Use a semaphore
to perform the required blocking in the `creg_wait_for_empty()`
function.  To shut down a client connection, use the `shutdown()`
function described in Section 2 of the Linux manual pages.
It is sufficient to use `SHUT_RD` to shut down just the read-side
of the connection, as this will cause the client service thread to
see an EOF indication and terminate.

Implementing the client registry should be a fairly easy warm-up
exercise in concurrent programming.  If you do it correctly, the
Bourse server should still shut down cleanly in response to SIGHUP
using your version.

**Note:** You should test your client registry separately from the
server.  Create test threads that rapidly call `creg_register()` and
`creg_unregister()` methods concurrently and then check that a call to the
`creg_wait_for_empty()` function blocks until the number of registered
clients reaches zero, and then returns.

## Task IV: Client Service Thread

Next, you should implement the thread function that performs service
for a client.  This function is called `brs_client_service`, and
you should implement it in the `src/server.c` file.

The `brs_client_service` function is invoked as the **thread function**
for a thread that is created (using ``pthread_create()``) to service a
client connection.
The argument is a pointer to the integer file descriptor to be used
to communicate with the client.  Once this file descriptor has been
retrieved, the storage it occupied needs to be freed.
The thread must then become detached, so that it does not have to be
explicitly reaped, and it must register the client file descriptor with
the client registry.
Finally, the thread should enter a service loop in which it repeatedly
receives a request packet sent by the client, carries out the request,
and sends any response packets.
The possible types of packets that can be received are:

- `LOGIN`:  The payload portion of the packet contains the trader name
(**not** null-terminated) given by the user.
Upon receipt of a `LOGIN` packet, the `trader_login()`
function should be called, which in case of a successful login will return
a `TRADER` object.  This `TRADER` object should be retained and used as a
context for processing subsequent packets.
In case of a successful `LOGIN` an `ACK` packet with no payload should be
sent back to the client.  In case of an unsuccessful `LOGIN`, a `NACK` packet
(also with no payload) should be sent back to the client.

Until a `LOGIN` has been successfully processed, other packets sent by the
client should be silently discarded.  Once a `LOGIN` has been successfully processed,
other packets should be processed normally, and `LOGIN` packets should be discarded. 

- `STATUS`:  This type of packet has no payload.  The server responds by
sending an `ACK` packet whose payload consists of a structure that contains
current information about the status of the server, such as the current bid/ask
values, the last trade price, and the trader's account balance and inventory.

- `DEPOSIT`:  The payload of this type of packet consists of a structure with
a single field telling the amount of funds to be deposited in the trader's account.
The server increases the trader's account balance by the amount of the funds
and responds with an `ACK` packet in the same format as for `STATUS`.

- `WITHDRAW`:  The payload of this type of packet consists of a structure with
a single field telling the amount of funds to be withdrawn from the trader's account.
If the trader's account balance is at least as great as the amount of funds to
be withdrawn, the server debits the balance and responds with an `ACK` packet
as for `STATUS`.  Otherwise, the server makes no change and responds with
a `NACK` packet.

- `ESCROW`:  The payload of this type of packet consists of a structure with
a single field telling the number of units to be added to the trader's "inventory".
The idea here is that the Bourse server will be used to trade items such as
foreign currency or shares of stock.  In order to have items available for trades,
the trader must first send these items (*e.g.* stock certificates or banknotes)
to the exchange, where they are placed in escrow pending delivery to a buyer
once a trade has been executed.
The server increases the trader's inventory by the specified amount
and responds with an `ACK` packet in the same format as for `STATUS`.

- `RELEASE`:  The payload of this type of packet consists of a structure with
a single field telling the number of units to be released from the trader's
escrowed inventory and returned to the trader.
If the trader's inventory is at least as great as the amount of items to
be released, the server debits the inventory and responds with an `ACK` packet
as for `STATUS`.  Otherwise, the server makes no change and responds with
a `NACK` packet.

- `BUY`:  The payload of this type of packet consists of a structure whose
fields describe a **buy order** to be posted to the exchange.
These fields include the quantity of items to be bought and the maximum
per-item price the buyer is willing to pay (*i.e.* the order is a
"limit order").  If the trader's account balance contains sufficient funds
to cover the maximum possible cost of such an order, then the funds are
**encumbered** by debiting them from the trader's account, the order
is assigned an **order ID**, and it is posted to the exchange.  In this case,
the server responds with an ``ACK`` packet as for ``STATUS``, but with the
order ID in one of the fields of the structure that is returned.
If the trader's account balance does not contain sufficient funds, then no
order is posted and the server responds with a ``NACK`` packet.

- `SELL`:  The payload of this type of packet consists of a structure whose
fields describe a **sell order** to be posted to the exchange.
These fields include the quantity of items to be sold and the minimum
per-item price the buyer is willing to sell for.
If the trader's inventory contains at least as many items as specified
in the order, then the items are encumbered by removing them from the trader's
account, the order is assigned an **order ID**, and it is posted to the exchange.
In this case, the server responds with an ``ACK`` packet as for ``STATUS``,
but with the order ID in one of the fields of the structure that is returned.
If the trader's inventory does not contain sufficient items, then no order is
posted and the server responds with a ``NACK`` packet.

- `CANCEL`: The payload of this type of packet consists of a structure with
a single field that contains the order ID of an order to be canceled.
If the order is currently pending on the exchange and was posted on behalf
of the same trader who is sending the `CANCEL` packet, then the order is
removed from the exchange and the server responds with an `ACK` packet.
In this case, besides the usual status information the `ACK` packet also
contains the order ID of the order being canceled and the quantity of items
specified in the canceled order.
In addition, for a buy order, funds that were encumbered for the order are
unencumbered and credited back to the trader's account, and for a sell order,
inventory that was encumbered for the order is unencumbered and credited back
to the trader's inventory.
If the order cannot be canceled, then the server responds with a `NACK` packet.
Note that, due to the asynchronous nature by which trades occur on the server,
it is possible for an order that was still pending at the time a `CANCEL`
packet was sent to already have been fulfilled and removed from the server by the
time the server processes the packet.  In this case, the attempt to cancel
the order will fail.

## Task V: Trader Module

It is probably a toss-up as to whether it is simplest to implement the
`trader` module or the `exchange` module next.
Here we describe the `trader` module, which stores information about all
traders known to the system.
As this module contains mutable state that will be accessed concurrently by
all the server threads, it needs to be synchronized in order to make it thread-safe.
You will have to work out the details of the required synchronization.
You will also have to choose a suitable data structure for representing the
state of this module.
The following is an overview of the functions implemented by the `trader` module:

- `void trader_init(void)`:  This function initializes the `trader` module.
  It must be called once before any other functions of this module are used.

- `void trader_fini(void)`:  This function finalizes the `trader` module.
  It must be called once during server shutdown.  Once it has been called,
  no other functions of this module should be used.

- `TRADER *trader_login(int fd, char *name)`:  This function attempts to log
  in a trader with the specified user name.  The first argument is the file
  descriptor of the client connection that is to be logged in.
  If successful, a `TRADER` object is returned that represents the state of
  the trader, including account balance and inventory if the trader had
  previously accessed the system.  The login can fail if the specified user
  name is already logged-in or if the maximum number of traders has been
  reached.  In that case, `NULL` is returned.

- `void trader_logout(TRADER *trader)`:  This function logs out the specified
  trader.  The account balance and inventory of the trader are retained
  for a subsequent login.

- `TRADER *trader_ref(TRADER *trader, char *why)`:  This function increases
  the reference count on a trader by one.  The `why` argument is a string
  that explains the reason why the reference count is being increased, and
  it is only used in debugging messages.  The use of reference counts is
  explained further below.

- `void trader_unref(TRADER *trader, char *why)`:  This function decreases
  the reference count on a trader by one.  The `why` argument is as for
  `trader_ref`.  If the reference count on the trader reaches zero, then
  the trader and its state information are freed.  An attempt to call
  this function on a trader whose reference count is already zero is a
  fatal error, which will cause an abort.

- `int trader_send_packet(TRADER *trader, BRS_PACKET_HEADER *pkt, void *data)`:
  Once a client has connected and logged in, this function should be used
  to send packets to the client, as opposed to the lower-level
  `proto_send_packet` function.  The reason for this is that the
  `trader_send_packet` function will obtain exclusive access to the trader
  object before calling `proto_send_packet`.  Exclusive access ensures that
  multiple threads can safely call this function to send to the client.

- `int trader_broadcast_packet(BRS_PACKET_HEADER *pkt, void *data)`:
  This function is used to broadcast the same packet to all currently
  logged-in traders.  It is used by the exchange to send "ticker tape"
  information about orders and trades.

- `int trader_send_ack(TRADER *trader, BRS_STATUS_INFO *info)`:
  This function is used to send an `ACK` packet to a trader.
  If `info` is non-`NULL`, then it points to data to be used as the
  payload of the packet, otherwise there is no payload.

- `int trader_send_nack(TRADER *trader)`:
  This function is used to send a `NACK` packet to a trader.
  There is no payload.

- `void trader_increase_balance(TRADER *trader, funds_t amount)`:
  This function is used to increase the account balance of a trader
  by a specified amount of funds.

- `int trader_decrease_balance(TRADER *trader, funds_t amount)`:
  This function is used to decrease the account balance of a trader
  by a specified amount of funds.  The balance must have sufficient
  funds to cover the amount of the debit.

- `void trader_increase_inventory(TRADER *trader, quantity_t quantity)`:
  This function is used to increase the inventory of a trader
  by a specified quantity.

- `int trader_decrease_inventory(TRADER *trader, quantity_t quantity)`:
  This function is used to decrease the inventory of a trader
  by a specified quantity.  The inventory must be sufficient
  to cover the amount of the decrease.

## Task VI: Exchange Module

The last module is the `exchange` module, which manages buy and sell orders
and performs trades.  A **matchmaker** thread is used to find matching
trades and carry them out.  The matchmaker thread is created when the
exchange module is initialized, and is terminated when the exchange module
is finalized.  Normally, the matchmaker thread sleeps (*e.g.* on a semaphore)
awaiting a change in the set of posted orders.  When such a change occurs,
the matchmaker thread is awakened, it looks for matching buy and sell orders,
and it carries out trades until there are no orders that match.
Then it goes back to sleep again.

The exchange module implements the following functions:

- `EXCHANGE *exchange_init()`:  This function initializes an exchange and starts
  the associated matchmaker thread.  It returns the newly initialized exchange object,
  or `NULL` if initialization failed.

- `void exchange_fini(EXCHANGE *xchg)`:  This function finalizes an exchange.
  It arranges and waits for the termination of the associated matchmaker thread,
  and frees the exchange object and any memory or resources it contains.
  This function should only be called after any pending orders have been canceled
  and removed from the exchange.  Once an exchange has been finalized, it should
  not be used any more.

- `void exchange_get_status(EXCHANGE *xchg, BRS_STATUS_INFO *infop)`:
  This function fills in the fields of the object pointed to by `infop` with
  information (in network byte order) about the current state of the exchange.
  The information should reflect a consistent snapshot of the state of the
  exchange at a single point in time, though the exchange state can subsequently
  change arbitrarily after this function has returned.

- `orderid_t exchange_post_buy(EXCHANGE *xchg, TRADER *trader, quantity_t quantity, funds_t price)`:
  This function attempts to post a buy order on the exchange on behalf of a
  specified trader.  The trader's account balance is debited by the maximum possible
  price of a trade made for this order.  A reference to the `TRADER` object is retained
  as part of the order; to prevent the `TRADER` object from being freed as long as
  the order is pending, the reference count on the trader is increased by one when
  the order is created and decreased by one when the order is eventually removed from the
  exchange.  A `POSTED` packet containing the parameters of the posted order
  is broadcast to all logged-in traders.

- `orderid_t exchange_post_sell(EXCHANGE *xchg, TRADER *trader, quantity_t quantity, funds_t price)`:
  This function attempts to post a sell order on the exchange on behalf of a
  specified trader.  The trader's inventory is decreased by the specified quantity.
  A reference to the `TRADER` object is retained as part of the order; to prevent the `TRADER`
  object from being freed as long as the order is pending, the reference count on the trader
  is increased by one when the order is created and decreased by one when the order is eventually
  removed from the exchange.  A `POSTED` packet containing the parameters of the posted order
  is broadcast to all logged-in traders.

- `int exchange_cancel(EXCHANGE *xchg, TRADER *trader, orderid_t order, quantity_t *quantity)`:
  This function attempts to cancel a pending order on the exchange.
  If the order is present, and was created by the same trader as is requesting cancellation,
  then the order is removed from the exchange and any encumbered funds or inventory are
  restored to the trader's account.  In this case, a `CANCELED` packet containing the parameters
  of the canceled order is broadcast to all logged-in traders.

Trades are made by the exchange between matching buy and sell orders.
A buy and sell order **match** when the maximum buy price of the buy order is at least as
great as the minimum sell price of the sell order.
In that case, a trade involving the matching orders can be executed.  This occurs as follows:

- The trade price is set to be that price in the range of overlap between
  the ranges specified by the buy and sell orders which is closest to the
  last trade price.  In particular, if the last trade price is within
  the range of overlap, then the new trade will take place at the last
  trade price.

- The quantity to be traded is set to be the minimum of the quantities specified
  in the buy and sell orders.

- The quantities in the buy and sell orders are decreased by the quantity to
  be traded.  This will leave one or both orders with quantity zero.
  Such orders will subsequently be removed from the exchange as described below.
  Orders with nonzero remaining quantity are left pending, at the new quantity.

- The seller's account is credited with the proceeds of the trade (*i.e.* `quantity * price`).

- The buyer's inventory is credited with the number of units purchased (*i.e.* `quantity`).

- The buyer's account is credited with any remaining funds that should no longer
  be encumbered as a result of the trade.  Note that the buyer's account was debited
  by the maximum possible amount of the trade at the time the buy order was created,
  but the trade might have taken place for a lesser price.  Any difference is
  credited to the buyer's account.  If the buy order remains pending after
  the trade (because it was not completely fulfilled), then the amount of funds that
  remain encumbered must equal the maximum possible cost of a trade at the new quantity.

- Any order that now has quantity zero is removed from the exchange.

- The trader that posted the buy order is sent a `BOUGHT` packet.
  The trader that posted the sell order is sent a `SOLD` packet.
  A `TRADED` packet is broadcast to all logged-in traders.

The `trader` module has some complexities which will now be discussed.
Its function is to maintain the state of the traders, so that this state
persists across trader login sessions.  It maintains two types of objects:
a "trader map", of which there is just one that keeps track of all the
traders known to the system, and `TRADER` objects, which contain trader
state information and of which there is one for each known trader.
Both kinds of objects are subject to frequent concurrent access and must be
properly synchronized to be thread-safe.
The trader map can be protected by a mutex.  Each `TRADER` object will also require
a mutex to protect it.  However, because the functions in this module can
result in "self-calls" to other functions on the same `TRADER` object,
it will be necessary to make the mutex that protects a `TRADER` object a
so-called "recursive" mutex.  The difference between a recursive mutex and an
ordinary one is that a recursive mutex can be acquired multiple times by the same
thread that already holds the mutex, whereas an attempt to acquire a non-recursive
mutex again by the thread already holding the mutex, will deadlock that thread.
A mutex is made recursive by setting the `PTHREAD_MUTEX_RECURSIVE` attribute
when the mutex is initialized.  Refer to the pthreads documentation for more details.

Another complication in the `trader` module is that often actions performed by one
trader will need to send information to the clients associated with other traders.
For example, when a buy or sell order is posted by one trader, than a `POSTED`
packet will be sent to all currently logged-in traders.  Moreover, traders are
acting concurrently, so these notifications may be performed by a number of threads
at about the same time.
As the connection to a client thus constitutes data shared between threads, and we
cannot permit threads to access shared data concurrently, we need to synchronize.
This is done as follows: once a client is logged-in all sending of packets to that
client must go through `trader_send_packet()` (whereas before a client has logged
in and is without a `TRADER` object, packets to that client are are sent using the
lower-level `proto_send_packet()` function).
The `trader_send_packet()` function locks the mutex of the `TRADER` object to prevent
concurrent access to the client connection.
Since `TRADER` mutexes are recursive, it is OK for a thread to call this function while
holding a lock on the same `TRADER` object.  However, to avoid risk of deadlock,
it should be considered carefully whether any other mutexes can be held at the time
of call.

Yet another issue associated with the `trader` module is the need for
**reference counting**.  A reference count is a field maintained in an object to keep
track of the number of pointers extant to that object.  Each time a new pointer
to the object is created, the reference count is incremented.  Each time a pointer
is released, the reference count is decremented.  A reference-counted object is
freed when, and only when, the reference count reaches zero.  Using this scheme,
once a thread has obtained a pointer to an object, with the associated incremented
reference count, it can be sure that until it explicitly releases that object and
decrements the reference count, that the object will not be freed.

In the Bourse server, `TRADER` objects are reference counted.  One reference is
retained by the thread that serves the client associated with that `TRADER`,
for as long as that client remains logged in.  This enables that thread to perform
operations using that `TRADER` object without fear that the object might be freed.
A thread will also need to perform operations on a `TRADER` object other than the
one associated with the client it serves.  This occurs within the exchange module,
where the orders contain `TRADER` objects whose clients need to be notified about
events that occur.  In order to be sure that such `TRADER` objects will always
be valid, the reference count on a `TRADER` is increased by one (using `trader_ref()`)
when a pointer to the `TRADER` is stored in an order posted to the exchange.
When the order is subsequently removed from the exchange, the reference count
is decreased by one (using `trader_unref()`), which will allow the `TRADER` object
to potentially be freed.
In addition, `TRADER` objects have to persist across login sessions.
So when a trader logs in for the first time, one reference to the `TRADER` object
is returned to the `server` module, to be retained as long as the trader is
logged in, but another is held by the trader map.  This latter reference will be
retained until the server is shut down, at which time it will be released and the
`TRADER` object can be freed.
The result of this scheme is that the server will be able to shut down cleanly,
with all allocated memory freed.

Finally, note that, in a multi-threaded setting, the reference count in an object
is shared between threads and therefore need to be protected by a mutex if
it is to work reliably.


## Submission Instructions

Make sure your hw5 directory looks similarly to the way it did
initially and that your homework compiles (be sure to try compiling
both with and without "debug").
Note that you should omit any source files for modules that you did not
complete, and that you might have some source and header files in addition
to those shown.  You are also, of course, encouraged to create Criterion
tests for your code.

It would definitely be a good idea to use `valgrind` to check your program
for memory and file descriptor leaks.  Keeping track of allocated objects
and making sure to free them is one of the more challenging aspects of this
assignment.

To submit, run `git submit hw5`.
