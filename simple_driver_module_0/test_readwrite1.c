#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/errno.h>

// techbus.safaribooksonline.com/book/programming/linux/9781449341527/2dot-file-i-o/reading_via_read_open_parenthesis_html
// compile: gcc user-prog.c -o user-prog
// Test open, read and write file operation methods of driver simple  
// insmod simple.ko. This will create a device node simple_drv 
#define BUFSIZE	100

int main(void)
{
 int fd;
 ssize_t ret;
 char  rbuf[BUFSIZE];
 const char * wbuf = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

 // open file descriptor for read and write.

 if ((fd = open ("/dev/simple_drv", O_RDWR)) == -1)
  	perror("open");
  printf ("\nfile descriptor: %d is returned for read: simple_drv device node", fd);

 if ((ret = read(fd, rbuf, 20)) == -1) 
 	 perror("read");
  printf(" \nRead 20 bytes from buffer: %s\n", rbuf);
   memset(rbuf,0,BUFSIZE);

 if ((ret = write(fd, wbuf, strlen(wbuf))) == -1) 
 	 perror("write");
  printf("\nwrote %lu bytes in buffer: %s\n", strlen(wbuf), wbuf);

 printf("\nReading the new data that was written into the kernel buffer using write system call\n");
 if ((ret = read(fd, rbuf, strlen(wbuf))) == -1)  
  	perror("read");
 printf(" \nRead %lu bytes from buffer: %s\n", strlen(wbuf), rbuf);

close(fd);
exit(EXIT_SUCCESS);
}
