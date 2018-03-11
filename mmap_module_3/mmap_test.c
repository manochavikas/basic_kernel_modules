#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define BUFSIZE 8*1024
char *my_string = "Hello My test string!";

int main(int argc, char *argv[])
{
        int i, fd, len, wlen ;
        char * mptr;
        size_t size = BUFSIZE;
        char buffer[BUFSIZE];

	printf("opening file %s\n", argv[1]);
        fd = open(argv[1], O_RDWR | O_SYNC);
        if( fd == -1) {
                printf("open error...\n");
                return -1;
        }
	
	/** 
	  * Requesting mmaping  at offset 0 on device memory, last argument. 
	  * This is used by driver to map device memory. First argument is the 
	  * virtual memory location in user address space where application 
	  * wants to setup the mmaping. Normally, it is set to 0  to allow 
	  * kernel to pick the best location in user address space. Please 
	  * review man pages: mmap(2)
	  */

        mptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if(mptr == MAP_FAILED) {
                printf("mmap() failed\n");
                return -1;
        }

        /**
	  * Now mmap memory region can be access as user memory. No syscall 
          * overhead! 
	  */

	/* read from mmap memory */
        printf("mptr is %p\n", mptr);
        memset(buffer, 0, size);       		/* Clear the buffer */
        memcpy(buffer, mptr, size-1);  		/* Reading from kernel */
        printf("mmap:  '%s'\n", buffer);
        printf("mptr is %p\n", mptr);

        /* write to mmap memory */
        //memcpy(mptr, my_string, strlen(my_string));
        strcpy(mptr, my_string);
        memset(buffer, 0, size);
        memcpy(buffer, mptr, size-1);
        printf("mmap:  '%s'\n", buffer);

        munmap(mptr, size);
        close(fd);
}
