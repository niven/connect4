#ifndef __BASE_H__
#define __BASE_H__

#define true 1
#define false 0


// alternative names for static to be less confused and to be able to find e.g. global_variable later
#define internal static
#define local_persist static
#define global_variable static

typedef unsigned char bool;

// enjoy this ugly hack:
// macro to auto insert function name in printf function
// but this introduces extra " marks around the format string
// so we dirtily strip those out.

#define print( format, ... ) do {\
char buf[200];\
int pos=0;\
sprintf(buf, "%s(): " #format, __func__, __VA_ARGS__ ); \
	for(int i=0; i<200; i++) {\
		if( buf[i] == '"' ) {\
				continue;\
		} \
		buf[pos++] = buf[i];\
	}\
		buf[pos++] = '\0';\
	puts(buf);\
} while (0)

#endif
