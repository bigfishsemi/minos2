#include "stdio_impl.h"
#include <sys/ioctl.h>

size_t __stdout_write(FILE *f, const unsigned char *buf, size_t len)
{
	f->write = __stdio_write;

	return __stdio_write(f, buf, len);
}
