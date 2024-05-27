#include <pthread.h>
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <poll.h>

#include "conc_native.h"

Channel	*ChanAlloc(void) {
    Channel *ret = (Channel *)malloc(sizeof(Channel));
    pipe(ret->pipefd);
    return ret;
}

int	ChanInInt(Channel *c) {
    int i;
    int rc = read (c->pipefd[0], &i, sizeof(i));
    assert (rc == sizeof(i));
    return i;
}
void ChanIn(Channel *c,void *p,int s) {
    int rc = read (c->pipefd[0], p, s);
    assert (rc == s);
}

void ChanOutInt(Channel *c,int i) {
    int rc = write (c->pipefd[1], &i, sizeof(i));
    assert (rc == sizeof(i));
}
void ChanOut(Channel *c,void *p,int s) {
    int rc = write (c->pipefd[1], p, s);
    assert (rc==s);
}

int	ProcAltList(Channel **channels) {
    int rc,i;
    Channel **c = channels;
    int num_channels;
    while (*c != NULL) {
        num_channels++;
        c++;
    }
    // only poll in the input side of the pipe having data to read
    struct pollfd fds[num_channels];
    for (i=0; i < num_channels; i++) {
        fds[i].fd = channels[i]->pipefd[0];
        fds[i].events = POLLIN;
    }
    rc = poll (fds,num_channels*2,-1);
    for (i=0; i < num_channels; i++) {
        if (fds[i].revents == POLLIN) {
            return i;
        }
    }
    return -1;
}

typedef void *(*START1)(void *a);
typedef void *(*START2)(void *a, void *b);
typedef void *(*START3)(void *a, void *b, void *c);
typedef void *(*START4)(void *a, void *b, void *c, void *d);

void *runner(void *arg) {
    PDes pdes = (PDes)arg;
    switch (pdes->num_params) {
        case 1:
            {
                START1 f = (START1)pdes->process;
                f(pdes->params[0]);
            }
        break;
        case 2:
            {
                START2 f = (START2)pdes->process;
                f(pdes->params[0],pdes->params[1]);
            }
        break;
        case 3:
            {
                START3 f = (START3)pdes->process;
                f(pdes->params[0],pdes->params[1],pdes->params[2]);
            }
        break;
        case 4:
            {
                START4 f = (START4)pdes->process;
                f(pdes->params[0],pdes->params[1],pdes->params[2],pdes->params[3]);
            }
        break;
    }
}

void PRun(PDes pdes) {
    int rc;
    pthread_t handle;
    rc = pthread_create(&handle, NULL, runner, pdes);
}

PDes PSetup(void *stack, void (*proc)(), int stack_size, int num_params, ...) {
    assert (num_params <= MAX_PARAMS);
    PDes pdes = (PDes)malloc (sizeof(struct _PDes));
    int i;
    va_list args;
    va_start (args, num_params);
    for (i=0; i < num_params; i++) {
        pdes->params[i] = va_arg(args,void*);
    }
    va_end (args);
    pdes->num_params = num_params;
    pdes->process = proc;
    return pdes;
}

void PStop(void) {
    while (1) {
        sleep(1);
    }
}
