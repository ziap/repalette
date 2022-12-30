#ifndef REPALETTE_H
#define REPALETTE_H

typedef __INT8_TYPE__ i8;
typedef __INT16_TYPE__ i16;
typedef __INT32_TYPE__ i32;
typedef __INT64_TYPE__ i64;

typedef __UINT8_TYPE__ u8;
typedef __UINT16_TYPE__ u16;
typedef __UINT32_TYPE__ u32;
typedef __UINT64_TYPE__ u64;

typedef __SIZE_TYPE__ usize;

typedef struct {
  i16 red;
  i16 green;
  i16 blue;
} Color16;

extern void recolor(u8 *, usize, usize, Color16 *, usize, usize);

#endif
