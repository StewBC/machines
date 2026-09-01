#include "platform_socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static void expect_true(const char *name, int cond) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", name);
        exit(1);
    }
}

int main(void) {
    platform_socket_listener *listener;
    platform_socket_connection *server;
    platform_socket_connection *client;
    uint16_t port;
    char buf[64];
    int n;
    int ready;

    expect_true("startup", platform_socket_startup());

    listener = platform_socket_listen_localhost(0);
    expect_true("listen", listener != NULL);
    port = platform_socket_listener_bound_port(listener);
    expect_true("ephemeral port", port != 0);

    client = platform_socket_connect("127.0.0.1", port, 3000);
    expect_true("connect", client != NULL);

    server = platform_socket_accept(listener);
    expect_true("accept", server != NULL);

    expect_true("client nonblock", platform_socket_set_nonblocking(client, true));
    expect_true("server nonblock", platform_socket_set_nonblocking(server, true));

    n = platform_socket_write(client, "ping", 4);
    expect_true("partial write", n == 4);

    ready = platform_socket_wait_readable(server, 1000);
    expect_true("server readable", ready == 1);
    n = platform_socket_read(server, buf, sizeof(buf));
    expect_true("read 4", n == 4);
    expect_true("payload", memcmp(buf, "ping", 4) == 0);

    ready = platform_socket_wait_writable(client, 1000);
    expect_true("client writable", ready == 1);

    /* write_all must not be required for the happy path above. */
    platform_socket_connection_destroy(client);
    platform_socket_connection_destroy(server);
    platform_socket_listener_destroy(listener);
    platform_socket_shutdown();

    /* Failed connect should return NULL without hanging forever. */
    expect_true("startup2", platform_socket_startup());
    client = platform_socket_connect("127.0.0.1", 1, 500);
    expect_true("connect refused/timeout", client == NULL);
    platform_socket_shutdown();

    printf("OK\n");
    return 0;
}
