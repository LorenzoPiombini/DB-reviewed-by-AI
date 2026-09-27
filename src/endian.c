#include "endian.h"

ui32 htonf(float f)
{
    real_number_interpreter_f rnf;
    rnf.f = f;
    return swap32((ui32)rnf.i);
}

float ntohf(ui32 u_i)
{
    real_number_interpreter_f rnf;
    rnf.i = swap32(u_i);
    return rnf.f;
}


unsigned char ntohb(ui16 ui)
{
    one_byte_interpreter ubi;
    ubi.i = swap16(ui);
    return ubi.c;
}

ui64 htond(double d)
{
    real_number_interpreter_d rnd;
    rnd.d = d;
    return swap64(rnd.i);
}

double ntohd(ui64 u_i)
{
    real_number_interpreter_d rnd;
    rnd.i = swap64(u_i);
    return rnd.d;
}

ui64 swap64(ui64 n)
{
        if(n == 0) return 0;
        ui64 b0 = (n & 0x00000000000000ff);
        ui64 b1 = (n & 0x000000000000ff00);
        ui64 b2 = (n & 0x0000000000ff0000);
        ui64 b3 = (n & 0x00000000ff000000);
        ui64 b4 = (n & 0x000000ff00000000);
        ui64 b5 = (n & 0x0000ff0000000000);
        ui64 b6 = (n & 0x00ff000000000000);
        ui64 b7 = (n & 0xff00000000000000);
 
        ui64 n_n =  (b0 << 56) |
                        (b1 << 40) |
                        (b2 << 24) |
                        (b3 << 8)  |
                        (b4 >> 8)  |
                        (b5 >> 24) |
                        (b6 >> 40) |
                        (b7 >> 56);
 
        return n_n;
}

ui32 swap32(ui32 n)
{

	if(n == 0) return 0;
	ui32 b0 = (n & 0x000000ff);
	ui32 b1 = (n & 0x0000ff00);
	ui32 b2 = (n & 0x00ff0000);
	ui32 b3 = (n & 0xff000000);

	ui32 n_n = (b0 << 24) | (b1 << 8) | (b2 >> 8) | (b3 >> 24);

	return n_n;
}

ui16 swap16(ui16 n)
{

	if(n == 0) return 0;
	ui16 b0 = (n & 0x00ff);
	ui16 b1 = (n & 0xff00);

	ui16 n_n = (b0 << 8) | (b1 >> 8) ;

	return n_n;
}


