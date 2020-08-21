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
#include <zlib.h>

struct termios tmp, restore;
char buffer[1024], compress_buf[1024];
char *program = "/bin/bash", *port = NULL, *hostname = "localhost";
int i = 0, shut_down, closed, ret, compression, sockfd, listenfd, retfd;
ssize_t readIn;
pid_t proc = -1;
int status = 0;
struct pollfd pollfds[2];
int to_shell[2];
int from_shell[2];
z_stream to_server, from_server;

const struct option args[] =
    {
        /*		option		has arg			flag		return value */
        {"shell=", 1, NULL, 's'},
        {"port=", 1, NULL, 'p'},
        {"compress", 0, NULL, 'c'},
        {0, 0, 0, 0}};

// optional_actions: how to set it (e.g. waits for all buffered input/output to finish?)
// TCSADRAIN        TCSAFLUSH

int server_connect(unsigned int port_num);
void processOpts(int argc, char *argv[]);
void processToShell();
void standardUsage();
void processFromShell();
void handleSigpipe(int signum);
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
    sockfd = server_connect(atoi(port));
    pipe(to_shell);
    pipe(from_shell);
    signal(SIGPIPE, handleSigpipe);

    if ((proc = fork()) < 0)
    {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        exit(1);
    }
    signal(SIGPIPE, handleSigpipe);

    if (proc == 0) // child process
    {
        close(to_shell[1]);   // unused
        close(from_shell[0]); // unused

        close(0);
        dup(to_shell[0]); // set pipe read end to program stdin
        close(to_shell[0]);

        //stdout
        close(1);
        dup(from_shell[1]); // pipe program stdout to terminal
        close(2);
        dup(from_shell[1]); // pipe program stderr to terminal
        close(from_shell[1]);

        // program execution
        execlp(program, program, NULL);
        fprintf(stderr, "program execution failed\n");
        exit(1);
    }
    else // parent process
    {
        close(to_shell[0]);   // unused
        close(from_shell[1]); // unused

        shut_down = 0;
        pollfds[0].fd = sockfd;
        pollfds[0].events = POLLIN | POLLHUP | POLLERR;
        pollfds[1].fd = from_shell[0];
        pollfds[1].events = POLLIN | POLLHUP | POLLERR;

        while (1)
        {
            // if (waitpid(proc, &status, WNOHANG) != 0)
            // {
            //     close(sockfd);
            //     close(listenfd);
            //     close(retfd);
            //     close(from_shell[0]);
            //     close(to_shell[1]);
            //     fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WIFSIGNALED(status), WEXITSTATUS(status));
            //     exit(0);
            // }

            if ((ret = poll(pollfds, 2, -1)) < 0)
            {
                fprintf(stderr, "Pipe polling error %s\n", strerror(errno));
                exit(1);
            }
            if (ret > 0)
            {
                if (pollfds[0].revents & POLLIN) // process stdin
                    processToShell();
                if (pollfds[1].revents & POLLIN) // process from socket
                    processFromShell();
                if (pollfds[0].revents & (POLLHUP | POLLERR))
                    shut_down = 1;
                if (pollfds[1].revents & (POLLHUP | POLLERR))
                    shut_down = 1;
            }
            if (shut_down)
                break;
        }
        close(to_shell[1]);
        close(from_shell[0]);

        if (waitpid(proc, &status, 0) < 0)
        {
            fprintf(stderr, "child process waiting error %s\n", strerror(errno));
            exit(1);
        }

        fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));

        close(sockfd);
        close(listenfd);
        close(retfd);
        exit(0);
    }
}

int server_connect(unsigned int port_num)
{
    struct sockaddr_in serv_addr, cli_addr; //server_address, client_address
    unsigned int cli_len = sizeof(struct sockaddr_in);
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        fprintf(stderr, "Error creating server side socket: %s\n", strerror(errno));
        exit(1);
    }
    retfd = 0;
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;         //ipv4 address
    serv_addr.sin_addr.s_addr = INADDR_ANY; //server can accept connection from any client IP
    serv_addr.sin_port = htons(port_num);   //setup port number
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "Error binding socket to port: %s\n", strerror(errno));
        exit(1);
    }
    if (listen(listenfd, 5) < 0)
    {
        fprintf(stderr, "Error listening to socket: %s\n", strerror(errno));
        exit(1);
    }
    retfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
    if (retfd < 0)
    {
        fprintf(stderr, "Error accepting client connection: %s\n", strerror(errno));
        exit(1);
    }
    return retfd;
}

void processOpts(int argc, char *argv[])
{
    while ((i = getopt_long(argc, argv, "", args, NULL)) != -1)
    {
        if (i == 's')
            program = optarg;
        else if (!port && i == 'p')
            port = optarg;
        else if (i == 'c')
            compression = 1;
        else
        {
            fprintf(stderr, "usage: %s --port=port# [--shell=program] [--compress]\n", argv[0]);
            exit(1);
        }
    }
    if (!port)
    {
        fprintf(stderr, "usage: %s --port=port# [--shell=program] [--compress]\n", argv[0]);
        exit(1);
    }
}

// from terminal stdin to shell
void processToShell()
{
    memset(buffer, 0, sizeof(buffer));
    if ((readIn = read(sockfd, buffer, sizeof(buffer))) < 0)
    {
        fprintf(stderr, "%s\n", strerror(errno));
        exit(1);
    }
    if (readIn > 0 && compression)
    {
        init_decompress_stream(&to_server); //init a decompress stream.
        decompress_stream(&to_server, buffer, readIn, compress_buf, sizeof(compress_buf));
        //Use the zlib to compress the data from orig_buf and put it in out_buf.
        // output the out_buf to the network.
        int compressed_size = sizeof(compress_buf) - to_server.avail_out;
        for (i = 0; i < compressed_size; i++)
        {
            if (compress_buf[i] == 0x4) // ^D
            {
                // closed = 1;
                close(to_shell[1]); // close write end to socket
            }
            else if (compress_buf[i] == 0x3) // ^C
            {
                if (kill(proc, SIGINT) < 0)
                {
                    fprintf(stderr, "Error killing child process: %s\n", strerror(errno));
                    exit(1);
                }
                shut_down = 1;
            }
            else if (compress_buf[i] == '\r' || compress_buf[i] == '\n')
            {
                if (write(to_shell[1], "\n", 1) < 0)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    exit(1);
                }
            }
            else if (write(to_shell[1], &compress_buf[i], 1) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
        }
        fini_decompress_stream(&to_server); // Close the compress stream
        memset(compress_buf, 0, sizeof(compress_buf));
    }
    else
    {
        for (i = 0; i < readIn; i++)
        {
            if (buffer[i] == 0x4) // ^D
            {
                // closed = 1;
                close(to_shell[1]); // close write end to socket
            }
            else if (buffer[i] == 0x3) // ^C
            {
                if (kill(proc, SIGINT) < 0)
                {
                    fprintf(stderr, "Error killing child process: %s\n", strerror(errno));
                    exit(1);
                }
                shut_down = 1;
            }
            else if (buffer[i] == '\r' || buffer[i] == '\n')
            {
                // pass linefeed to socket program
                if (write(to_shell[1], "\n", 1) < 0)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    exit(1);
                }
            }
            else
            {
                if (write(to_shell[1], &buffer[i], 1) < 0)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    exit(1);
                }
            }
        }
    }
}

// from program stdout/stderr to terminal stdout
void processFromShell()
{
    memset(buffer, 0, sizeof(buffer));
    if ((readIn = read(from_shell[0], buffer, sizeof(buffer))) < 0)
    {
        fprintf(stderr, "%s\n", strerror(errno));
        exit(1);
    }

    if (readIn > 0 && compression)
    {
        init_compress_stream(&from_server); //init a compress stream.
        compress_stream(&from_server, buffer, readIn, compress_buf, sizeof(compress_buf));
        //Use the zlib to compress the data from orig_buf and put it in out_buf.
        // output the out_buf to the network.
        int compressed_size = sizeof(compress_buf) - to_server.avail_out;
        write(sockfd, compress_buf, compressed_size);
        fini_compress_stream(&from_server); // Close the compress stream
        memset(compress_buf, 0, sizeof(compress_buf));
    }
    else
    {
        for (i = 0; i < readIn; i++)
        {
            if (buffer[i] == 0x4) // ^D
            {
                if (write(sockfd, &buffer[i], 1) < 0)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    exit(1);
                }
                shut_down = 1;
            }
            else if (buffer[i] == '\n')
            {
                if (write(sockfd, "\r\n", 2) < 0)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    exit(1);
                }
            }
            else if (write(sockfd, &buffer[i], 1) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
        }
    }
}

void handleSigpipe(int signum)
{
    shut_down = (signum == SIGPIPE) ? 1 : 0;
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
            fprintf(stderr, "Error decompressing server side: %s\n", strerror(errno));
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