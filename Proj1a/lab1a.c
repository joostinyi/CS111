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

struct termios tmp, restore;
char buffer[1024];
char *program = NULL;
int i = 0, shutdown, closed, ret;
ssize_t readIn;
pid_t proc = -1;
int status = 0;
int to_shell[2];
int from_shell[2];
struct pollfd pollfds[2];

const struct option args[] =
    {
        /*		option		has arg			flag		return value */
        {"shell=", 1, NULL, 's'},
        {0, 0, 0, 0}};

// optional_actions: how to set it (e.g. waits for all buffered input/output to finish?)
// TCSADRAIN        TCSAFLUSH

void terminal_restore()
{
    if (tcsetattr(0, TCSANOW, &restore) < 0)
    {
        fprintf(stderr, "Error restoring terminal mode: %s\n", strerror(errno));
        exit(1);
    }
}

void terminal_setup(void)
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
        if (i == 's')
        {
            program = optarg;
            continue;
        }
        else
        {
            fprintf(stderr, "usage: %s [--shell=program] [--debug]\n", argv[0]);
            exit(1);
        }
    }
}

// from terminal stdin to shell program
void processToShell()
{
    memset(buffer, 0, sizeof(buffer));
    if ((readIn = read(0, buffer, sizeof(buffer))) < 0)
    {
        fprintf(stderr, "%s\n", strerror(errno));
        exit(1);
    }
    for (i = 0; i < readIn; i++)
    {
        if (buffer[i] == 0x4) // ^D
        {
            if (write(1, "^D", 2) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }

            closed = 1;
            close(to_shell[1]); // close write end to shell
        }
        else if (buffer[i] == 0x3) // ^C
        {
            if (write(1, "^C", 2) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
            if (kill(proc, SIGINT) < 0)
            {
                fprintf(stderr, "Error killing child process: %s\n", strerror(errno));
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
            // pass linefeed to shell program
            if (write(to_shell[1], "\n", 1) < 0)
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
            if (write(to_shell[1], &buffer[i], 1) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
        }
    }
}

// optionless functionality
void standardUsage()
{
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        if ((readIn = read(0, buffer, sizeof(buffer))) < 0)
        {
            fprintf(stderr, "%s\n", strerror(errno));
            exit(1);
        }
        for (i = 0; i < readIn; i++)
        {
            if (buffer[i] == 0x4) // ^D
            {
                if (write(1, "^D", 2) < 0)
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
                kill(proc, SIGINT);
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
                if (write(1, buffer + i, 1) < 0)
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
    for (i = 0; i < readIn; i++)
    {
        if (buffer[i] == 0x4) // ^D
        {
            if (write(1, "^D", 2) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
            shutdown = 1;
        }
        else if (buffer[i] == '\n')
        {
            if (write(1, "\r\n", 2) < 0)
            {
                fprintf(stderr, "%s\n", strerror(errno));
                exit(1);
            }
        }
        else if (write(1, &buffer[i], 1) < 0)
        {
            fprintf(stderr, "%s\n", strerror(errno));
            exit(1);
        }
    }
}

void handleSigpipe(int signum)
{
    shutdown = (signum == SIGPIPE) ? 1 : 0;
}

int main(int argc, char *argv[])
{
    processOpts(argc, argv);
    terminal_setup();
    pipe(to_shell);
    pipe(from_shell);

    if (program)
    {
        if ((proc = fork()) < 0)
        {
            fprintf(stderr, "fork failed: %s\n", strerror(errno));
            exit(1);
        }
        signal(SIGPIPE, handleSigpipe);
    }
    else
    {
        standardUsage();
        fprintf(stderr, "unreachable section: error in optionless functionality\n");
        exit(1);
    }

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
        close(to_shell[0]); // unused
        close(from_shell[1]); // unused

        shutdown = 0;
        pollfds[0].fd = 0;
        pollfds[0].events = POLLIN + POLLHUP + POLLERR;
        pollfds[1].fd = from_shell[0];
        pollfds[1].events = POLLIN + POLLHUP + POLLERR;

        while (1)
        {
            if ((ret = poll(pollfds, 2, -1)) < 0)
            {
                fprintf(stderr, "Pipe polling error %s\n", strerror(errno));
                exit(1);
            }
            if (ret > 0)
            {
                if (pollfds[0].revents & POLLIN) // process stdin
                    processToShell();
                if (pollfds[1].revents & POLLIN) // process from shell
                    processFromShell();
                if (pollfds[0].revents & (POLLHUP | POLLERR))
                    shutdown = 1;
                if (pollfds[1].revents & (POLLHUP | POLLERR))
                    shutdown = 1;
            }
            if (shutdown)
                break;
        }
        if (!closed){
            closed = 1;
            close(to_shell[1]);
        }

        if (waitpid(proc, &status, 0) < 0)
        {
            fprintf(stderr, "child process waiting error %s\n", strerror(errno));
            exit(1);
        }
        fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
        exit(0);
    }
}