#ifndef INTERFACE_H
#define INTERFACE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <stdbool.h> 

    int OpusCodecStart(void **inst, int sampleRate);
    int OpusCodecEnd(void **inst);
    int OpusCodecEncode(void *inst, const char *input, int inputLen, char **output, int *outputLen, bool last);
    int OpusCodecDecode(void *inst, const char *input, int inputLen, char **output, int *outputLen, bool last);

#ifdef __cplusplus
}
#endif

#endif // INTERFACE_H