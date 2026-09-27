#ifndef __TYPES_H_
#define __TYPES_H_ 1


typedef 	char			i8;
typedef 	unsigned char	ui8;
typedef 	unsigned short	ui16;
typedef 	short			i16;
typedef 	unsigned int	ui32;
typedef 	int				i32;
typedef 	unsigned long	ui64;
typedef 	long			i64;
typedef 	unsigned long 	size_t;
typedef		long int		file_offset;
typedef 	long long		process_id;

#ifndef _LIBC_LIMITS_H_
/*signed short max and min*/
#  define SHRT_MIN	(-32768)
#  define SHRT_MAX	32767

/* unsigned short int max.  (Minimum is 0.)  */
#  define USHRT_MAX	65535

/* signed int min and max  */
#  define INT_MIN	(-INT_MAX - 1)
#  define INT_MAX	2147483647

/* unsigned char max (minimum is 0*/
#  define UCHAR_MAX 	255	

/* unsigned int' max  (Minimum is 0.)  */
#  define UINT_MAX	4294967295

/* signed long int min and max */
#  if __WORDSIZE == 64
#   define LONG_MAX	9223372036854775807L
#else
#   define LONG_MAX	2147483647L
#endif

#  define LONG_MIN	(-LONG_MAX - 1L)

/* unsigned long max  (Minimum is 0.)  */
#  if __WORDSIZE == 64
#   define ULONG_MAX	18446744073709551615UL
#  else
#   define ULONG_MAX	4294967295UL
#  endif


#endif 

#endif /*types.h*/
