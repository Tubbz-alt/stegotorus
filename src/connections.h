/* Copyright 2011 Nick Mathewson, George Kadianakis, Zack Weinberg
   See LICENSE for other credits and copying information
*/

#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <event2/bufferevent.h>

/**
   This struct defines the state of one downstream socket-level
   connection.  Each protocol may extend this structure with
   additional private data by embedding it as the first member of a
   larger structure.  The protocol's conn_create() method is
   responsible only for filling in the |cfg| field of this structure,
   plus any private data of course.

   Connections are associated with circuits (and thus with upstream
   socket-level connections) as quickly as possible.
 */
struct conn_t {
  config_t           *cfg;
  circuit_t          *circuit;
  const char         *peername;
  struct bufferevent *buffer;
  unsigned int        serial;
};

/** Initialize connection and circuit tracking.  Must be called before
    any function that creates connections or circuits is called. */
void conn_initialize(void);

/** When all currently-open connections and circuits are closed, stop
    the main event loop and exit the program.  If 'barbaric' is true,
    forcibly close them all now, then stop the event loop.  It
    is a bug to call any function that creates connections or circuits
    after conn_start_shutdown has been called. */
void conn_start_shutdown(int barbaric);

/** Create a new inbound connection from a configuration and a
    bufferevent wrapping a socket. */
conn_t *conn_create(config_t *cfg, struct bufferevent *buf,
                    const char *peername);

/** Close and deallocate a connection.  If the connection is part of a
    circuit, close the other side of that circuit as well. */
void conn_close(conn_t *conn);

/** Report the number of currently-open connections. */
unsigned long conn_count(void);

/** Retrieve the inbound evbuffer for a connection. */
static inline struct evbuffer *conn_get_inbound(conn_t *conn)
{ return conn->buffer ? bufferevent_get_input(conn->buffer) : NULL; }

/** Retrieve the outbound evbuffer for a connection. */
static inline struct evbuffer *conn_get_outbound(conn_t *conn)
{ return conn->buffer ? bufferevent_get_output(conn->buffer) : NULL; }

/** Connect to upstream, if it is possible to do so without receiving
    data from the downstream peer first. */
int conn_maybe_open_upstream(conn_t *conn);

/** Transmit the protocol-specific handshake message (if any) for a
    connection. */
int conn_handshake(conn_t *conn);

/** Receive data from SOURCE, decode it, and write it to upstream. */
int conn_recv(conn_t *source);

/** No more data will be received from the peer; flush any internally
    buffered data to your upstream. */
int conn_recv_eof(conn_t *source);

void conn_send_eof(conn_t *conn);
void conn_do_flush(conn_t *conn);

/* The next several conn_t methods are used by steganography modules to
   provide hints about appropriate higher-level behavior.  */

/** The peer is expected to close CONN without any further
    transmissions. */
void conn_expect_close(conn_t *conn);

/** Do not transmit any more data on this connection after the outbound
    queue has drained.  However, the peer may still send data back. */
void conn_cease_transmission(conn_t *conn);

/** Close CONN after all pending data is transmitted. */
void conn_close_after_transmit(conn_t *conn);

/** We must transmit something on this connection within TIMEOUT
    milliseconds. */
void conn_transmit_soon(conn_t *conn, unsigned long timeout);

/**
   This struct holds all the state for an "upstream" connection to the
   higher-level client or server that we are proxying traffic for. It
   will normally have one or more "downstream" connections (conn_t's)
   with the remote peer, but these are private to the protocol.  A
   circuit that's waiting for SOCKS directives from its upstream will
   have a non-null socks_state field and no downstream connections.

   Like conn_t, the protocol has an opportunity to add information to
   this structure, and will certainly add at least one conn_t pointer.
 */

struct circuit_t {
  config_t           *cfg;
  struct event       *flush_timer;
  struct event       *axe_timer;
  struct bufferevent *up_buffer;
  const char         *up_peer;
  socks_state_t      *socks_state;
  unsigned int        serial;

  unsigned int        connected : 1;
  unsigned int        pending_eof : 1;
};

circuit_t *circuit_create(config_t *cfg);

void circuit_add_upstream(circuit_t *ckt,
                          struct bufferevent *buf, const char *peer);
int circuit_open_upstream(circuit_t *ckt);

void circuit_add_downstream(circuit_t *ckt, conn_t *down);
void circuit_drop_downstream(circuit_t *ckt, conn_t *down);

void circuit_reopen_downstreams(circuit_t *ckt);

void circuit_close(circuit_t *ckt);
void circuit_recv_eof(circuit_t *ckt);

void circuit_send(circuit_t *ckt);
void circuit_send_eof(circuit_t *ckt);

void circuit_arm_flush_timer(circuit_t *ckt, unsigned int milliseconds);
void circuit_disarm_flush_timer(circuit_t *ckt);

void circuit_arm_axe_timer(circuit_t *ckt, unsigned int milliseconds);
void circuit_disarm_axe_timer(circuit_t *ckt);

void circuit_do_flush(circuit_t *ckt);

unsigned long circuit_count(void);

#endif
