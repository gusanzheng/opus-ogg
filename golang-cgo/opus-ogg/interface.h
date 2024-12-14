#ifndef INTERFACE_H
#define INTERFACE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <stdbool.h> // Include this to support bool in C

    int OpusOggCodecStart(void **inst, int sampleRate);
    int OpusOggCodecEnd(void **inst);
    int OpusOggCodecEncode(void *inst, const char *input, int inputLen, char **output, int *outputLen, bool last);
    int OpusOggCodecDecode(void *inst, const char *input, int inputLen, char **output, int *outputLen, bool last);

#ifdef __cplusplus
}
#endif

#endif // INTERFACE_H