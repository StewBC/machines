#include "platform_socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET platform_socket_handle;
#define PLATFORM_INVALID_SOCKET INVALID_SOCKET
#define platform_close_socket closesocket
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int platform_socket_handle;
#define PLATFORM_INVALID_SOCKET (-1)
#define platform_close_socket close
#endif

struct platform_socket_listener {
    platform_socket_handle handle;
};

struct platform_socket_connection {
    platform_socket_handle handle;
};

static bool set_handle_nonblocking(platform_socket_handle handle, bool enabled)
{
#if defined(_WIN32)
    u_long mode = enabled ? 1ul : 0ul;
    return ioctlsocket(handle, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(handle, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    if (enabled) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return fcntl(handle, F_SETFL, flags) == 0;
#endif
}

static int wait_handle(platform_socket_handle handle, bool want_write, uint32_t timeout_ms)
{
#if defined(_WIN32)
    fd_set fds;
    struct timeval tv;
    int result;

    FD_ZERO(&fds);
    FD_SET(handle, &fds);
    tv.tv_sec = (long)(timeout_ms / 1000u);
    tv.tv_usec = (long)((timeout_ms % 1000u) * 1000u);
    if (want_write) {
        result = select(0, NULL, &fds, NULL, &tv);
    } else {
        result = select(0, &fds, NULL, NULL, &tv);
    }
    if (result < 0) {
        return -1;
    }
    if (result == 0) {
        return 0;
    }
    return 1;
#else
    struct pollfd pfd;
    int result;

    pfd.fd = handle;
    pfd.events = (short)((want_write ? POLLOUT : POLLIN) | POLLERR | POLLHUP);
    pfd.revents = 0;
    result = poll(&pfd, 1, (int)timeout_ms);
    if (result < 0) {
        return -1;
    }
    if (result == 0) {
        return 0;
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return -1;
    }
    if (want_write) {
        return (pfd.revents & POLLOUT) ? 1 : 0;
    }
    return (pfd.revents & POLLIN) ? 1 : 0;
#endif
}

bool platform_socket_startup(void)
{
#if defined(_WIN32)
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    return true;
#endif
}

void platform_socket_shutdown(void)
{
#if defined(_WIN32)
    WSACleanup();
#endif
}

platform_socket_listener *platform_socket_listen_localhost(uint16_t port)
{
    platform_socket_listener *listener;
    platform_socket_handle handle;
    struct sockaddr_in addr;
    int one = 1;

    handle = socket(AF_INET, SOCK_STREAM, 0);
    if (handle == PLATFORM_INVALID_SOCKET) {
        return NULL;
    }

    setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(handle, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        listen(handle, 1) != 0) {
        platform_close_socket(handle);
        return NULL;
    }

    listener = (platform_socket_listener *)calloc(1, sizeof(*listener));
    if (listener == NULL) {
        platform_close_socket(handle);
        return NULL;
    }
    listener->handle = handle;
    return listener;
}

uint16_t platform_socket_listener_bound_port(const platform_socket_listener *listener)
{
    struct sockaddr_in addr;
#if defined(_WIN32)
    int len = (int)sizeof(addr);
#else
    socklen_t len = (socklen_t)sizeof(addr);
#endif

    if (listener == NULL || listener->handle == PLATFORM_INVALID_SOCKET) {
        return 0;
    }
    memset(&addr, 0, sizeof(addr));
    if (getsockname(listener->handle, (struct sockaddr *)&addr, &len) != 0) {
        return 0;
    }
    return ntohs(addr.sin_port);
}

bool platform_socket_listener_set_nonblocking(platform_socket_listener *listener, bool enabled)
{
    if (listener == NULL || listener->handle == PLATFORM_INVALID_SOCKET) {
        return false;
    }
    return set_handle_nonblocking(listener->handle, enabled);
}

void platform_socket_listener_close(platform_socket_listener *listener)
{
    if (listener == NULL || listener->handle == PLATFORM_INVALID_SOCKET) {
        return;
    }
#if defined(_WIN32)
    shutdown(listener->handle, SD_BOTH);
#else
    shutdown(listener->handle, SHUT_RDWR);
#endif
    platform_close_socket(listener->handle);
    listener->handle = PLATFORM_INVALID_SOCKET;
}

void platform_socket_listener_destroy(platform_socket_listener *listener)
{
    if (listener == NULL) {
        return;
    }
    platform_socket_listener_close(listener);
    free(listener);
}

platform_socket_connection *platform_socket_accept(platform_socket_listener *listener)
{
    platform_socket_connection *connection;
    platform_socket_handle handle;

    if (listener == NULL || listener->handle == PLATFORM_INVALID_SOCKET) {
        return NULL;
    }

    handle = accept(listener->handle, NULL, NULL);
    if (handle == PLATFORM_INVALID_SOCKET) {
        return NULL;
    }
#if defined(SO_NOSIGPIPE)
    {
        int one = 1;
        (void)setsockopt(handle, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
    }
#endif

    connection = (platform_socket_connection *)calloc(1, sizeof(*connection));
    if (connection == NULL) {
        platform_close_socket(handle);
        return NULL;
    }
    connection->handle = handle;
    return connection;
}

platform_socket_connection *platform_socket_connect(
    const char *host,
    uint16_t port,
    uint32_t timeout_ms)
{
    char port_str[16];
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *ai;
    platform_socket_connection *connection;
    int gai;

    if (host == NULL || host[0] == '\0' || port == 0) {
        return NULL;
    }

    (void)snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    gai = getaddrinfo(host, port_str, &hints, &res);
    if (gai != 0 || res == NULL) {
        return NULL;
    }

    for (ai = res; ai != NULL; ai = ai->ai_next) {
        platform_socket_handle handle;
        int rc;

        handle = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (handle == PLATFORM_INVALID_SOCKET) {
            continue;
        }
#if defined(SO_NOSIGPIPE)
        {
            int one = 1;
            (void)setsockopt(handle, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
        }
#endif
        if (!set_handle_nonblocking(handle, true)) {
            platform_close_socket(handle);
            continue;
        }

#if defined(_WIN32)
        rc = connect(handle, ai->ai_addr, (int)ai->ai_addrlen);
#else
        rc = connect(handle, ai->ai_addr, ai->ai_addrlen);
#endif
        if (rc != 0) {
#if defined(_WIN32)
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
                platform_close_socket(handle);
                continue;
            }
#else
            if (errno != EINPROGRESS && errno != EWOULDBLOCK) {
                platform_close_socket(handle);
                continue;
            }
#endif
            {
                int ready = wait_handle(handle, true, timeout_ms);
                int so_error = 0;
#if defined(_WIN32)
                int len = (int)sizeof(so_error);
#else
                socklen_t len = (socklen_t)sizeof(so_error);
#endif
                if (ready <= 0) {
                    platform_close_socket(handle);
                    continue;
                }
                if (getsockopt(
                        handle,
                        SOL_SOCKET,
                        SO_ERROR,
                        (char *)&so_error,
                        &len) != 0 ||
                    so_error != 0) {
                    platform_close_socket(handle);
                    continue;
                }
            }
        }

        connection = (platform_socket_connection *)calloc(1, sizeof(*connection));
        if (connection == NULL) {
            platform_close_socket(handle);
            freeaddrinfo(res);
            return NULL;
        }
        connection->handle = handle;
        freeaddrinfo(res);
        return connection;
    }

    freeaddrinfo(res);
    return NULL;
}

int platform_socket_read(platform_socket_connection *connection, void *buffer, size_t size)
{
    int received;

    if (connection == NULL || connection->handle == PLATFORM_INVALID_SOCKET ||
        buffer == NULL || size == 0) {
        return -1;
    }

    received = (int)recv(connection->handle, (char *)buffer, (int)size, 0);
    if (received == 0) {
        return 0;
    }
    if (received < 0) {
#if defined(_WIN32)
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return -2; /* would block */
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -2; /* would block */
        }
#endif
        return -1;
    }
    return received;
}

bool platform_socket_write_all(
    platform_socket_connection *connection,
    const void *buffer,
    size_t size)
{
    const char *cursor = (const char *)buffer;
    size_t remaining = size;

    if (connection == NULL || connection->handle == PLATFORM_INVALID_SOCKET ||
        (buffer == NULL && size > 0)) {
        return false;
    }

    while (remaining > 0) {
#if defined(MSG_NOSIGNAL)
        int sent = (int)send(connection->handle, cursor, (int)remaining, MSG_NOSIGNAL);
#else
        int sent = (int)send(connection->handle, cursor, (int)remaining, 0);
#endif
        if (sent > 0) {
            cursor += sent;
            remaining -= (size_t)sent;
            continue;
        }
        if (sent == 0) {
            return false;
        }
#if defined(_WIN32)
        {
            int err = WSAGetLastError();
            if (err == WSAEINTR) {
                continue;
            }
            if (err == WSAEWOULDBLOCK) {
                fd_set write_set;
                struct timeval timeout;
                int ready;
                FD_ZERO(&write_set);
                FD_SET(connection->handle, &write_set);
                timeout.tv_sec = 5;
                timeout.tv_usec = 0;
                ready = select(0, NULL, &write_set, NULL, &timeout);
                if (ready > 0) {
                    continue;
                }
            }
        }
#else
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            struct pollfd pfd;
            int ready;
            pfd.fd = connection->handle;
            pfd.events = POLLOUT | POLLERR | POLLHUP;
            pfd.revents = 0;
            ready = poll(&pfd, 1, 5000);
            if (ready > 0 &&
                (pfd.revents & POLLOUT) != 0 &&
                (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) == 0) {
                continue;
            }
        }
#endif
        return false;
    }
    return true;
}

int platform_socket_write(
    platform_socket_connection *connection,
    const void *buffer,
    size_t size)
{
    int sent;

    if (connection == NULL || connection->handle == PLATFORM_INVALID_SOCKET ||
        (buffer == NULL && size > 0) || size == 0) {
        return -1;
    }

#if defined(MSG_NOSIGNAL)
    sent = (int)send(connection->handle, (const char *)buffer, (int)size, MSG_NOSIGNAL);
#else
    sent = (int)send(connection->handle, (const char *)buffer, (int)size, 0);
#endif
    if (sent > 0) {
        return sent;
    }
    if (sent == 0) {
        return -1;
    }
#if defined(_WIN32)
    {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return 0;
        }
    }
#else
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;
    }
    if (errno == EINTR) {
        return 0;
    }
#endif
    return -1;
}

void platform_socket_connection_close(platform_socket_connection *connection)
{
    if (connection == NULL || connection->handle == PLATFORM_INVALID_SOCKET) {
        return;
    }
#if defined(_WIN32)
    shutdown(connection->handle, SD_BOTH);
#else
    shutdown(connection->handle, SHUT_RDWR);
#endif
    platform_close_socket(connection->handle);
    connection->handle = PLATFORM_INVALID_SOCKET;
}

void platform_socket_connection_destroy(platform_socket_connection *connection)
{
    if (connection == NULL) {
        return;
    }
    platform_socket_connection_close(connection);
    free(connection);
}

bool platform_socket_set_nonblocking(platform_socket_connection *connection, bool enabled)
{
    if (connection == NULL || connection->handle == PLATFORM_INVALID_SOCKET) {
        return false;
    }
    return set_handle_nonblocking(connection->handle, enabled);
}

int platform_socket_wait_readable(
    platform_socket_connection *connection,
    uint32_t timeout_ms)
{
    if (connection == NULL || connection->handle == PLATFORM_INVALID_SOCKET) {
        return -1;
    }
    return wait_handle(connection->handle, false, timeout_ms);
}

int platform_socket_wait_writable(
    platform_socket_connection *connection,
    uint32_t timeout_ms)
{
    if (connection == NULL || connection->handle == PLATFORM_INVALID_SOCKET) {
        return -1;
    }
    return wait_handle(connection->handle, true, timeout_ms);
}
