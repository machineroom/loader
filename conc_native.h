
#ifndef __CONC_NATIVE_H__
#define __CONC_NATIVE_H__

#include <stdint.h>
#include <unistd.h>

typedef struct {
    int pipefd[2];
} Channel;

Channel	*ChanAlloc(void);
int	ChanInInt(Channel *);
void ChanIn(Channel *,void *,int);
void ChanOutInt(Channel *,int);
void ChanOut(Channel *,void *,int);
int	ProcAltList(Channel **);

#define MAX_PARAMS 4

typedef void (*PP)();

typedef	struct _PDes {
    PP process;
    void *params[MAX_PARAMS];
    int num_params;
} *PDes;

void	PRun(PDes);
PDes	PSetup(void *stack, PP process, int stack_size, int num_params, ...);
void	PStop(void);
#endif /*__CONC_NATIVE_H__*/
