#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/errno.h>
#include<sys/ioctl.h>

#define BUFSIZE 100

#define MAGIC 'R'
#define GETMSG  _IOR(MAGIC, 1, char *)
#define SETMSG  _IOW(MAGIC, 2, char *)


int main(int argc, char* argv[])
{
        int fd;
        char  rbuf[BUFSIZE];
 	const char * wbuf = "Setting Kernel Buffer of simple driver via ioctl";

        // open file descriptor for read and write.

	if ((fd = open ("/dev/simple_drv", O_RDWR)) == -1){
       		 perror("open"); 
		 printf("errno: %d \n", errno);
         }
	printf ("\nfile descriptor: %d is returned for read: simple_drv device node", fd);

// Read from kernel
	if(ioctl(fd, GETMSG, rbuf) < 0){
	   perror("GETMSG");
	   printf("errno: %d \n", errno);
	   exit(EXIT_FAILURE); 
         }

	printf(" \nRead from kernel: %s\n", rbuf);

// write into kernel
	if(ioctl(fd, SETMSG, wbuf) < 0){ 
	   perror("SETMSG");
	   printf("errno: %d \n", errno);
	   exit(EXIT_FAILURE); 
       }

       printf(" \nwrite into kernel buffer: %s\n", wbuf);

// Read again to confirm kernel buffer has changed
	if(ioctl(fd, GETMSG, rbuf) < 0){
           perror("GETMSG");
           printf("errno: %d \n", errno);
	   exit(EXIT_FAILURE); 
         }

	printf(" \n Read from kernel again: %s\n", rbuf);


close(fd);
exit(EXIT_SUCCESS);
}

