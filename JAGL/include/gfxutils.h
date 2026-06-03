#pragma once
#include "shapes.h"

// Copies in into a new buffer and returns it.
char *CopyTextUtil(const char *in);

// Copies in + args into a new buffer and returns it.
char *CopyTextArgsUtil(const char *in, ...);

// Logs text to fuck.txt in the same folder as the nro, for testing ( ͡° ͜ʖ ͡°)
void Log(char *text);

// Copies in into a new buffer, up to the specified length, and returns it
char *CopyTextLenUtil(const char *in, int len);