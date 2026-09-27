#ifndef ENDIAN_H
#define ENDIAN_H

#include "db_types.h"
/*assign a value to the double to get the corrisponding value int*/
typedef union
{
    ui32 i;
    float f;
} real_number_interpreter_f;

/*assign a value to the double to get the corrisponding value int*/
typedef union
{
    ui64 i;
    double d;
} real_number_interpreter_d;

typedef union
{
    ui16 i;
    unsigned char c;
} one_byte_interpreter;

ui16 htonb(unsigned char c);
ui32 htonf(float f);
float ntohf(ui32 u_i);
ui64 htond(double d);
double ntohd(ui64 u_i);
ui64 swap64(ui64 n);
ui32 swap32(ui32 n);
ui16 swap16(ui16 n);

#endif
