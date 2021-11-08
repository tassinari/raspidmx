
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>


#define PIPENAME "myPipe"
int fd;
int read_bytes;
char readbuf[80];
uint32_t timeout = 5000;
pthread_mutex_t lock;
bool cancelInterupt = false;
bool showing = false;

int commandParser(char * command, char ** path){

    *path = command;
    return 1;
}
 
void * display(void * ptr){
    printf("display received '%s'\n", (char *)ptr);

    uint32_t currentTime = 0;
    pthread_mutex_lock(&lock);
    showing = true;
    pthread_mutex_unlock(&lock);

    // Sleep for 10 milliseconds every run-loop
    const int sleepMilliseconds = 10;
    bool run = true;
    
    while (run)
    {
    
        //---------------------------------------------------------------------

        usleep(sleepMilliseconds * 1000);
        pthread_mutex_lock(&lock);
        currentTime += sleepMilliseconds;
        if (currentTime >= timeout || cancelInterupt) {
            run = false;
            cancelInterupt = false;
            showing = false;
        }
        pthread_mutex_unlock(&lock);
    }
    printf("leaving display\n");
    return NULL;
}

int main(int argc, char *argv[])
{
    if(access( PIPENAME, F_OK ) == 0 ){
        printf("found and deleting\n");
        unlink(PIPENAME);
    }
    mknod(PIPENAME, S_IFIFO | 0640, 0);
    const int sleepMilliseconds = 100;
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }
    while (1)
    {
        pthread_t thread_id;
        fd = open(PIPENAME, O_RDONLY);
        read_bytes = read(fd, readbuf, sizeof(readbuf));
        printf("read bytes = %d\n",read_bytes);
        readbuf[read_bytes] = '\0';
        //printf("Received string: \"%s\" and length is %d\n", readbuf, (int)strlen(readbuf));
        char *path = "";
        if (commandParser(readbuf, &path))
        {
            bool iscancelling = false;
            pthread_mutex_lock(&lock);
            if (showing){
                cancelInterupt = true;  
                iscancelling = true;
            }
            pthread_mutex_unlock(&lock);
            if(iscancelling){
                //the cancel flag was set, now wait for the thread to finish before starting the new command thread
                pthread_join(thread_id,NULL);
            }
            
            int ret = pthread_create(&thread_id, NULL, &display, path);
            if (ret == 0)
            {
                printf("Thread created successfully  (%ld).\n",(unsigned long)thread_id);
            }
            
            //printf("\nDONE\n");
        }

        close(fd);
       // usleep(sleepMilliseconds * 1000);
    }
    return 0;
}