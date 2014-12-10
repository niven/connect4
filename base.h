#ifndef __BASE_H__
#define __BASE_H__

#define true 1
#define false 0

// alternative names for static to be less confused and to be able to find e.g. global_variable later
#define internal static
#define local_persist static
#define global_variable static

typedef unsigned char bool;

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

// enjoy this ugly hack:
// macro to auto insert function name in printf function
// but this introduces extra " marks around the format string
// so we dirtily strip those out.


#ifdef VERBOSE

#define prints( string ) print( "%s", string )

#define print( format, ... ) do {\
char _verbose_buf[200];\
int _verbose_pos=0;\
sprintf(_verbose_buf, "%s(): " #format, __func__, __VA_ARGS__ ); \
	for(int _verbose_i=0; _verbose_i<200; _verbose_i++) {\
		if( _verbose_buf[_verbose_i] == '"' ) {\
				continue;\
		} \
		_verbose_buf[_verbose_pos++] = _verbose_buf[_verbose_i];\
	}\
		_verbose_buf[_verbose_pos++] = '\0';\
	puts(_verbose_buf);\
} while (0)

#else
	
#define print( format, ... ) // DEBUG OFF
	
#define prints( string ) // DEBUG OFF

#endif

#endif
