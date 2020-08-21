// // NAME: Justin Yi
// // EMAIL: joostinyi00@gmail.com
// // ID: 905123893

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ulimit.h>
#include <zlib.h>

#define h_addr h_addr_list[0] /* for backward compatibility */

const int RECEIVED = 0;
const int SENT = 1;

struct termios tmp, restore;
char buffer[1024], compress_buf[1024];
char *program = "/bin/bash", *port = NULL, *logfile = NULL, *hostname = "localhost";
int i = 0, shut_down, closed, ret, compression, sockfd, log_file;
ssize_t readIn;
pid_t proc = -1;
int status = 0;
struct pollfd pollfds[2];
z_stream to_server, from_server;

const struct option args[] =
    {
        /*		option		has arg			flag		return value */
        {"port=", 1, NULL, 'p'},
        {"log=", 1, NULL, 'l'},
        {"compress", 0, NULL, 'c'},
        {0, 0, 0, 0}};

// optional_actions: how to set it (e.g. waits for all buffered input/output to finish?)
// TCSADRAIN        TCSAFLUSH

void terminal_restore();
void terminal_setup();
void processOpts(int argc, char *argv[]);
void processToSocket();
void processFromSocket();
int client_connect(char *host_name, unsigned int port);
void logData(char *buffer, int sent, int bytes);
void init_compress_stream(z_stream *stream);
void compress_stream(z_stream *stream, void *orig_buf, int orig_len,
                     void *out_buf, int out_len);
void fini_compress_stream(z_stream *stream);
void init_decompress_stream(z_stream *stream);
void decompress_stream(z_stream *stream, void *orig_buf, int orig_len,
                       void *out_buf, int out_len);
void fini_decompress_stream(z_stream *stream);

int main(int argc, char *argv[])
{
    processOpts(argc, argv);
    sockfd = client_connect(hostname, atoi(port));
    terminal_setup();
    shut_down = 0;
    pollfds[0].fd = 0;
    pollfds[0].events = POLLIN | POLLHUP | POLLERR;
    pollfds[1].fd = sockfd;
    pollfds[1].events = POLLIN | POLLHUP | POLLERR;

    while (1)
    {
        if ((ret = poll(pollfds, 2, -1)) < 0)
        {
            fprintf(stderr, "socket polling error %s\n", strerror(errno));
            exit(1);
        }
        if (ret > 0)
        {
            if (pollfds[0].revents & POLLIN) // process stdin
            {
                processToSocket();
                if (pread(pollfds[0].fd, buffer, 1, 0) == 0) // check for EOF
                    shut_down = 1;
                memset(buffer, 0, 1);
                break;
            }
            if (pollfds[1].revents & POLLIN) // process from socket
            {
                processFromSocket();
                if (pread(pollfds[1].fd, buffer, 1, 0) == 0) // check for EOF
                    shut_down = 1;
                memset(buffer, 0, 1);
                break;
            }
            if (pollfds[1].revents & (POLLHUP | POLLERR))
                shut_down = 1;
            if (pollfds[0].revents & (POLLHUP | POLLERR))
                shut_down = 1;
        }
        if (shut_down)
            break;
    }
    close(sockfd);
    close(log_file);
}

int client_connect(char *host_name, unsigned int port)
{
    struct sockaddr_in serv_addr;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Error creating client side socket: %s\n", strerror(errno));
        exit(1);
    }
    // AF_INET: IPv4, SOCK_STREAM: TCP connection
    struct hostent *server = gethostbyname(host_name);
    if (!server)
    {
        fprintf(stderr, "Error creating host name: %s\n", strerror(errno));
        exit(1);
    }
    // convert host_name to IP addr
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET; //address is Ipv4
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    serv_addr.sin_port = htons(port);
    if ((connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0)
    {
        fprintf(stderr, "Error connecting to server: %s\n", strerror(errno));
        exit(1);
    }
    return sockfd;
}

void terminal_restore()
{
    if (tcsetattr(1, TCSANOW, &restore) < 0) // would get bad fd when leaving first arg 0, not sure why
    {
        fprintf(stderr, "Error restoring terminal mode: %s\n", strerror(errno));
        exit(1);
    }
}

void terminal_setup()
{
    // save current modes for restoration
    if (tcgetattr(0, &restore) < 0)
    {
        fprintf(stderr, "Error fetching terminal attributes: %s\n", strerror(errno));
        exit(1);
    }

    atexit(terminal_restore);

    // change modes

    if (tcgetattr(0, &tmp) < 0)
    {
        fprintf(stderr, "Error fetching terminal attributes: %s\n", strerror(errno));
        exit(1);
    }

    tmp.c_iflag = ISTRIP; // only lower 7 bits
    tmp.c_oflag = 0;      // no processing
    tmp.c_lflag = 0;      // no processing

    if (tcsetattr(0, TCSANOW, &tmp) < 0)
    {
        fprintf(stderr, "Error setting no echo, non-canonical mode: %s\n", strerror(errno));
        exit(1);
    }
}

void processOpts(int argc, char *argv[])
{
    while ((i = getopt_long(argc, argv, "", args, NULL)) != -1)
    {
        if (!port && i == 'p')
            port = optarg;
        else if (!logfile && i == 'l')
        {
            logfile = optarg;
            if ((log_file = open(logfile, O_WRONLY | O_APPEND | O_CREAT, 0777)) < 0)
            {
                fprintf(stderr, "error opening/creating logfile\n");
                exit(1);
            }
            ulimit(UL_SETFSIZE, 10000);
        }
        else if (i == 'c')
        {
            compression = 1;
        }
        else
        {
            fprintf(stderr, "usage: %s --port=port# [--log=filename] [--compress]\n", argv[0]);
            exit(1);
        }
    }
    if (!port)
    {
        fprintf(stderr, "usage: %s --port=port# [--log=filename] [--compress]\n", argv[0]);
        exit(1);
    }
}

void processToSocket()
{
    memset(buffer, 0, sizeof(buffer));
    if ((readIn = read(0, buffer, sizeof(buffer))) < 0)
    {
        fprintf(stderr, "%s\n", strerror(errno));
        exit(1);
    }

    if (readIn == 0)
        return;
    if (readIn > 0)
    {
        if (compression)
        {
            init_compress_stream(&to_server); //init a compress stream.
            compress_stream(&to_server, buffer, readIn, compress_buf, sizeof(compress_buf));
            //Use the zlib to compress the data from orig_buf and put it in out_buf.
            // output the out_buf to the network.
            int compressed_size = sizeof(compress_buf) - to_server.avail_out;
            logData(compress_buf, SENT, compressed_size);
            write(sockfd, compress_buf, compressed_size);
            fini_compress_stream(&to_server); // Close the compress stream
            memset(compress_buf, 0, sizeof(compress_buf));
        }
        else
            logData(buffer, SENT, readIn);
    }
    for (i = 0; i < readIn; i++)
    {
        if (buffer[i] == '\r' || buffer[i] == '\n')
        {
            if (write(1, "\r\n", 2) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
            if (!compression)
                if (write(sockfd, "\n", 1) < 0)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    exit(1);
                }
        }
        else if (buffer[i] == 0x4) // ^D
        {
            if (write(1, "^D", 2) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
            if (!compression)
                if (write(sockfd, &buffer[i], 1) < 0)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    exit(1);
                }
            exit(0);
        }
        else if (buffer[i] == 0x3) // ^C
        {
            if (write(1, "^C", 2) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
            if (!compression)
                if (write(sockfd, &buffer[i], 1) < 0)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    exit(1);
                }
        }
        else if (buffer[i] == '\r' || buffer[i] == '\n')
        {
            if (write(1, "\r\n", 2) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
        }
        else
        {
            if (write(1, &buffer[i], 1) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
            if (!compression)
                if (write(sockfd, &buffer[i], 1) < 0)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    exit(1);
                }
        }
    }
}

void processFromSocket()
{
    memset(buffer, 0, sizeof(buffer));
    if ((readIn = read(sockfd, buffer, sizeof(buffer))) < 0)
    {
        fprintf(stderr, "%s\n", strerror(errno));
        exit(1);
    }
    if (readIn == 0)
        return;
    if (readIn > 0 && compression)
    {
        init_decompress_stream(&from_server); //init a compress stream.
        decompress_stream(&from_server, buffer, readIn, compress_buf, sizeof(compress_buf));
        int compressed_size = sizeof(compress_buf) - from_server.avail_out;
        // while (compressed_size <= 0)
        // {
        //     fprintf(stderr, "%d", compressed_size);

        //     compress_buf = (char *) realloc(compress_buf, 2*sizeof(compress_buf));
        //     sizeof(compress_buf) = 2*sizeof(compress_buf);
        //     decompress_stream(&from_server, buffer, readIn, compress_buf, sizeof(compress_buf));
        //     compressed_size = sizeof(compress_buf) - from_server.avail_out;
        // }
        fprintf(stderr, compress_buf, compressed_size);
        logData(compress_buf, RECEIVED, compressed_size);
        for (i = 0; i < compressed_size; i++)
        {
            if (compress_buf[i] == '\n')
            {
                if (write(1, "\r\n", 2) < 0)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    exit(1);
                }
            }
            else if (write(1, &compress_buf[i], 1) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
        }
        fini_decompress_stream(&from_server); // Close the compress stream
        memset(compress_buf, 0, sizeof(compress_buf));
    }
    else
    {
        if (readIn > 0)
            logData(buffer, RECEIVED, readIn);
        for (i = 0; i < readIn; i++)
        {
            if (buffer[i] == '\n')
            {
                if (write(1, "\r\n", 2) < 0)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    exit(1);
                }
            }
            else if (buffer[i] == 0x4) // ^D
            {
                if (write(1, "^D", 2) < 0)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    exit(1);
                }
                shut_down = 1;
            }
            else if (buffer[i] == 0x3) // ^C
            {
                if (write(1, "^C", 2) < 0)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    exit(1);
                }
                shut_down = 1;
            }
            else if (write(1, &buffer[i], 1) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
        }
    }
}

void logData(char *buffer, int sent, int bytes)
{
    char temp[bytes];
    memcpy(&temp, buffer, bytes);
    if (logfile)
    {
        if (sent)
        {
            if (dprintf(log_file, "SENT %d bytes: %s\n", bytes, temp) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
        }
        else
        {
            if (dprintf(log_file, "RECEIVED %d bytes: %s\n", bytes, temp) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
        }
    }
    memset(temp, 0, sizeof(temp));
}

void init_compress_stream(z_stream *stream)
{
    int ret = 0;
    stream->zalloc = Z_NULL, stream->zfree = Z_NULL, stream->opaque = Z_NULL;
    if ((ret = deflateInit(stream, Z_DEFAULT_COMPRESSION)) != Z_OK)
    {
        fprintf(stderr, "Error initializing client side compression: %s\n", strerror(errno));
        exit(1);
    }
}

void init_decompress_stream(z_stream *stream)
{
    int ret = 0;
    stream->zalloc = Z_NULL, stream->zfree = Z_NULL, stream->opaque = Z_NULL;
    if ((ret = inflateInit(stream)) != Z_OK)
    {
        fprintf(stderr, "Error initializing client side decompression: %s\n", strerror(errno));
        exit(1);
    }
}

void compress_stream(z_stream *stream, void *orig_buf, int orig_len,
                     void *out_buf, int out_len)
{
    stream->next_in = orig_buf, stream->avail_in = orig_len;
    stream->next_out = out_buf, stream->avail_out = out_len;
    //deflate will update next_in, avail_in, next_out, avail_out accordingly.
    while (stream->avail_in > 0)
        if (deflate(stream, Z_SYNC_FLUSH) != Z_OK)
        {
            fprintf(stderr, "Error client side deflation: %s\n", strerror(errno));
            exit(1);
        }
    //out_buf contains the compressed data.
    //out_len - stream->avail_out contains the size of the compressed data.
}

void decompress_stream(z_stream *stream, void *orig_buf, int orig_len,
                       void *out_buf, int out_len)
{
    stream->next_in = orig_buf, stream->avail_in = orig_len;
    stream->next_out = out_buf, stream->avail_out = out_len;
    //FIXME: What if avail_out is not enough?
    while (stream->avail_in > 0)
        if (inflate(stream, Z_SYNC_FLUSH) < 0)
        {
            fprintf(stderr, "Error client side inflation: %s\n", strerror(errno));
            exit(1);
        }
    //out_buf contains the decompressed data.
    //out_len - stream->avail_out contains the size of the decompressed data.
}

void fini_compress_stream(z_stream *stream)
{
    deflateEnd(stream);
}

void fini_decompress_stream(z_stream *stream)
{
    inflateEnd(stream);
}
