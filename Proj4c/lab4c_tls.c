// NAME: Justin Yi
// EMAIL: joostinyi00@gmail.com
// ID: 905123893

#ifdef DUMMY
#define MRAA_GPIO_IN 0
#define MRAA_GPIO_EDGE_RISING 2
typedef int mraa_aio_context;
typedef int mraa_gpio_context;
typedef int mraa_result_t;
typedef int mraa_gpio_edge_t;

mraa_aio_context mraa_aio_init(unsigned int pin)
{
    return 1;
}
int mraa_aio_read(mraa_aio_context c)
{
    return 650;
}
mraa_result_t mraa_aio_close(mraa_aio_context c)
{
    return 0;
}

mraa_gpio_context mraa_gpio_init(int pin)
{
    return 1;
}

int mraa_gpio_dir(mraa_gpio_context dev, int dir)
{
    return 0;
}

int mraa_gpio_read(mraa_gpio_context c)
{
    return 0;
}

int mraa_gpio_isr(mraa_gpio_context c, mraa_gpio_edge_t edge, void *fptr, void *args)
{
    return 1;
}

void mraa_gpio_close(mraa_gpio_context c)
{
}
int mraa_deinit()
{
    return 0;
}

#else
#include <mraa.h>
#include <mraa/aio.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

int opt, celsius = 0, period = 1, stop = 0, sockfd = 0, log_file, logfd = 0;
unsigned int port = 0;
mraa_gpio_context button;
mraa_aio_context temp;
char *logfile = NULL, *arg, *str, *token, *host, *id;
struct pollfd polls[1];
SSL * ssl_client;
SSL_CTX * context;
SSL *attach_ssl_to_socket(int socket, SSL_CTX *context);

const int B = 4275;
const int R0 = 100000.0;
const struct option args[] =
    {
        /*		option		has arg			flag		return value */
        {"period=", 1, NULL, 'p'},
        {"scale=", 1, NULL, 's'},
        {"log=", 1, NULL, 'l'},
        {"id=", 1, NULL, 'i'},
        {"host=", 1, NULL, 'h'},
        {0, 0, 0, 0}};
float convert_temperature_reading(int reading);
void processOpts(int argc, char *argv[]);
void processCommands(char *buffer);
void init();
void print_temp();
int lookup(const char *str);
int checkTime();
int client_connect(char *host_name, unsigned int port);
void ssl_clean_client(SSL *client);
SSL_CTX *ssl_init(void);


int main(int argc, char *argv[])
{
    processOpts(argc, argv);
    init();
    char buffer[128];
    time_t start;
    time(&start);

    polls[0].fd = sockfd;
    polls[0].events = POLLIN | POLLERR | POLLHUP;
    int iter = 0;

    while (1)
    {
        if (checkTime() && !stop && (difftime(time(NULL), start) >= period))
        {
            time(&start);
            print_temp();
        }
        if (poll(polls, 1, 0) < 0)
        {
            fprintf(stderr, "error polling server\n");
            exit(2);
        }
        if (polls[0].revents & POLLIN)
        {
            int ret_read;
            if ((ret_read = SSL_read(ssl_client, buffer, sizeof(buffer))) < 0)
            {
                fprintf(stderr, "error reading from server\n");
                exit(2);
            }
            else if (ret_read > 0)
            {
                token = strtok(buffer, "\n");
                processCommands(token);
                memset(buffer, 0, sizeof(buffer));
            }
            else if (ret_read == 0)
                exit(0);
        }
    }
}

void processOpts(int argc, char *argv[])
{
    while ((opt = getopt_long(argc, argv, "", args, NULL)) != -1)
    {
        switch (opt)
        {
        case 'p':
            period = atoi(optarg);
            break;
        case 's':
            celsius = (*optarg == 'C') ? 1 : 0;
            break;
        case 'l':
            logfile = optarg;
            if ((logfd = creat(logfile, 0666)) < 0)
            {
                fprintf(stderr, "error opening/creating logfile\n");
                exit(1);
            }
            break;
        case 'i':
            id = optarg;
            if (strlen(id) != 9)
            {
                fprintf(stderr, "Invalid ID");
                exit(1);
            }
            break;
        case 'h':
            host = optarg;
            break;
        default:
            fprintf(stderr, "usage: %s [--period=#] [--scale=C/F] --log=filename --id=9-digit-number --host=name/address port number\n",
                    argv[0]);
            exit(1);
        }
    }
    if (logfd == 0)
    {
        fprintf(stderr, "usage: %s [--period=#] [--scale=C/F] --log=filename --id=9-digit-number --host=name/address port number\n",
                argv[0]);
        exit(1);
    }
    port = atoi(argv[optind]);
    if (port <= 0)
    {
        fprintf(stderr, "Invalid port number");
        exit(1);
    }
}

void init()
{
    sockfd = client_connect(host, port);
    context = ssl_init(); // newContext, init library, error string, algorithms  
	ssl_client = attach_ssl_to_socket(sockfd, context); //SSL_new(), SSL_set_fd(),SSL_connect()
    char buffer[128];
    sprintf(buffer, "ID=%s\n", id);
    SSL_write(ssl_client, buffer, strlen(buffer));
    dprintf(logfd, "ID=%s\n", id);

    mraa_aio_context temp = mraa_aio_init(0);
    if (!temp)
    {
        fprintf(stderr, "ERROR: Could not initialize temp sensor\n");
        mraa_deinit();
        exit(2);
    }
}

void processCommands(char *buffer)
{
    switch (lookup(buffer))
    {
    case -1: // default or period or log
        if (strncmp(buffer, "PERIOD=", 7) == 0)
        {
            period = (int)atoi(buffer + 7);
            dprintf(logfd, "%s\n", buffer);
            return;
        }
        if (strncmp(buffer, "LOG", 3) == 0)
        {
            dprintf(logfd, "%s\n", buffer);
            return;
        }
        dprintf(logfd, "%s\n", buffer);
        break;
    case 0:
        celsius = 0;
        dprintf(logfd, "%s\n", buffer);
        break;
    case 1:
        celsius = 1;
        dprintf(logfd, "%s\n", buffer);
        break;
    case 2:
        stop = 1;
        dprintf(logfd, "%s\n", buffer);
        break;
    case 3:
        stop = 0;
        dprintf(logfd, "%s\n", buffer);
        break;
    case 4:
        dprintf(logfd, "OFF\n");
        struct timespec ts;
        struct tm *tm;
        clock_gettime(CLOCK_REALTIME, &ts);
        tm = localtime(&(ts.tv_sec));
        dprintf(logfd, "%02d:%02d:%02d SHUTDOWN\n", tm->tm_hour,
                tm->tm_min, tm->tm_sec);
        char buffer[128];
        sprintf(buffer, "%02d:%02d:%02d SHUTDOWN\n", tm->tm_hour,
                tm->tm_min, tm->tm_sec);
        SSL_write(ssl_client, buffer, strlen(buffer));
        ssl_clean_client(ssl_client);
        exit(0);
    }
}

void print_temp()
{
    float conv_temp = convert_temperature_reading(mraa_aio_read(temp));
    if (mraa_aio_read(temp) < 0)
        conv_temp = convert_temperature_reading(650);

    struct timespec ts;
    struct tm *tm;
    clock_gettime(CLOCK_REALTIME, &ts);
    tm = localtime(&(ts.tv_sec));
    dprintf(logfd, "%02d:%02d:%02d %4.1f\n", tm->tm_hour,
            tm->tm_min, tm->tm_sec, conv_temp);
    char buffer[128];
    sprintf(buffer, "%02d:%02d:%02d %4.1f\n", tm->tm_hour,
            tm->tm_min, tm->tm_sec, conv_temp);
    SSL_write(ssl_client, buffer, strlen(buffer));
}

float convert_temperature_reading(int reading)
{
    float R = 1023.0 / ((float)reading) - 1.0;
    R = R0 * R;
    //C is the temperature in Celcious
    float C = 1.0 / (log(R / R0) / B + 1 / 298.15) - 273.15;
    //F is the temperature in Fahrenheit
    float F = (C * 9) / 5 + 32;
    // lock celsius
    if (celsius)
        return C;
    return F;
}

int checkTime()
{
    struct timespec ts;
    struct tm *tm;
    clock_gettime(CLOCK_REALTIME, &ts);
    tm = localtime(&(ts.tv_sec));
    return ((tm->tm_sec % period) == 0);
}
int lookup(const char *str)
{
    typedef struct item_t
    {
        const char *str;
        int value;
    } item_t;
    item_t table[] = {
        {"SCALE=F", 0},
        {"SCALE=C", 1},
        {"STOP", 2},
        {"START", 3},
        {"OFF", 4},
        {NULL, -1}};

    item_t *p = table;
    for (; p->str != NULL; ++p)
        if (strcmp(p->str, str) == 0)
            return p->value;
    return -1;
}

//e.g. host_name:”lever.cs.ucla.edu”, port:18000, return the socket for subsequent communication
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

void ssl_clean_client(SSL *client)
{
    SSL_shutdown(client);
    SSL_free(client);
}

SSL_CTX *ssl_init(void)
{
    SSL_CTX *newContext = NULL;
    SSL_library_init();
    //Initialize the error message
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    //TLS version: v1, one context per server.
    newContext = SSL_CTX_new(TLSv1_client_method());
    return newContext;
}

SSL *attach_ssl_to_socket(int socket, SSL_CTX *context)
{
    SSL *sslClient = SSL_new(context);
    SSL_set_fd(sslClient, socket);
    SSL_connect(sslClient);
    return sslClient;
}
