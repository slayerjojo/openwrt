#include "sigma_functor.h"
#include "interface_os.h"
#include <stdarg.h>

SigmaFunctor *sigma_functor_init(SigmaFunctor *functor, SigmaCaller caller, void *ctx)
{
    functor->caller = caller;
    functor->ctx = ctx;
    return functor;
}

int sigma_functor_call(SigmaFunctor *functor, ...)
{
    va_list args;
    int ret = 0;
    if (!functor || !functor->caller)
        return ret;

    va_start(args, functor);
    ret = functor->caller(functor->ctx, args);
    va_end(args);
    return ret;
}
