#include <stdint.h>
#include <stdbool.h>

#ifndef __FB_H__
#define __FB_H__

extern uint8_t *FB_Init(int *width, int *height, int *bpp, bool quiet);

#endif  /* __FB_H__ */
