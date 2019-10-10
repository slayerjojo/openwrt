#ifndef __SIGMA_FUNCTOR_H__
#define __SIGMA_FUNCTOR_H__

#include "env.h"

typedef int (*SigmaCaller)(void *ctx, ...);

typedef struct
{
    SigmaCaller caller;
    void *ctx;
}SigmaFunctor;

SigmaFunctor *sigma_functor_init(SigmaFunctor *functor, SigmaCaller caller, void *ctx);

int sigma_functor_call(SigmaFunctor *functor, ...);

#endif
