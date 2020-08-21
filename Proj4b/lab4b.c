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

int mraa_gpio_isr(mraa_gpio_context c, mraa_gpio_edge_t edge, void* fptr, void * args)
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

int opt, celsius = 0, period = 1, stop = 0;
mraa_gpio_context button;
mraa_aio_context temp;
FILE *log_file, *output = NULL;
char *logfile = NULL, *arg, *str, *token;

const int B = 4275;
const int R0 = 100000.0;
const struct option args[] =
    {
        /*		option		has arg			flag		return value */
        {"period=", 1, NULL, 'p'},
        {"scale=", 1, NULL, 's'},
        {"log=", 1, NULL, 'l'},
        {0, 0, 0, 0}};
float convert_temperature_reading(int reading);
void print_current_time();
void processOpts(int argc, char *argv[]);
void processCommands(char *buffer);
void init();
void print_temp();
int lookup(const char *str);
int checkTime();
void do_when_pushed();

int main(int argc, char *argv[])
{
    processOpts(argc, argv);
    if (!output)
        output = stdout;
    init();
    fcntl(0, F_SETFL, O_NONBLOCK);
    char buffer[256];
    time_t start;
    time(&start);

    while (1)
    {
        if (checkTime() && !stop && (difftime(time(NULL), start) >= period))
        {
            time(&start);
            print_temp();
        }
        memset(buffer, 0, 256);
        int ret = read(0, buffer, 256);
        if (ret < 0 && errno != EAGAIN)
        {
            fprintf(stderr, "error reading from stdin\n");
            exit(2);
        }
        else if (ret > 0)
        {
            token = strtok(buffer, "\n");
            while (token)
            {
                processCommands(token);
                token = strtok(NULL, "\n");
            }
        }
        else if (ret == 0)
            exit(0);
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
            if ((log_file = fopen(logfile, "w")) == NULL)
            {
                fprintf(stderr, "error opening/creating logfile\n");
                exit(1);
            }
            output = log_file;
            break;
        default:
            fprintf(stderr, "usage: %s [--period=#] [--scale=C/F] [--log=filename]\n", argv[0]);
            exit(1);
        }
    }
}

void init()
{
    mraa_gpio_context button = mraa_gpio_init(73);
    if (!button)
    {
        fprintf(stderr, "ERROR: Could not initialize button\n");
        mraa_deinit();
        exit(2);
    }

    mraa_aio_context temp = mraa_aio_init(0);
    if (!temp)
    {
        fprintf(stderr, "ERROR: Could not initialize temp sensor\n");
        mraa_deinit();
        exit(2);
    }
    mraa_gpio_dir(button, MRAA_GPIO_IN);
    mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, do_when_pushed, NULL);
}

void processCommands(char *buffer)
{
    switch (lookup(buffer))
    {
    case -1: // default or period or log
        if (strncmp(buffer, "PERIOD=", 7) == 0)
        {
            period = (int)atoi(buffer + 7);
            fprintf(output, "%s\n", buffer);
            break;
        }
        if (strncmp(buffer, "LOG", 3) == 0)
        {
            fprintf(output, "%s\n", buffer);
            break;
        }
        fprintf(output, "%s\n", buffer);
        break;
    case 0:
        celsius = 0;
        fprintf(output, "%s\n", buffer);
        break;
    case 1:
        celsius = 1;
        fprintf(output, "%s\n", buffer);
        break;
    case 2:
        stop = 1;
        fprintf(output, "%s\n", buffer);
        break;
    case 3:
        stop = 0;
        fprintf(output, "%s\n", buffer);
        break;
    case 4:
        fprintf(output, "OFF\n");
        do_when_pushed();
    }
}

void print_temp()
{
    float conv_temp = convert_temperature_reading(mraa_aio_read(temp));
    if (mraa_aio_read(temp) < 0)
        conv_temp = convert_temperature_reading(650);
    print_current_time();
    fprintf(output, "%4.1f\n", conv_temp);
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

void print_current_time()
{
    struct timespec ts;
    struct tm *tm;
    clock_gettime(CLOCK_REALTIME, &ts);
    tm = localtime(&(ts.tv_sec));
    fprintf(output, "%02d:%02d:%02d ", tm->tm_hour,
            tm->tm_min, tm->tm_sec);
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

void do_when_pushed()
{
    print_current_time();
    fprintf(output, "SHUTDOWN\n");
    exit(0);
}