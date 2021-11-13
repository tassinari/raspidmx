

#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "imageLayer.h"
#include "backgroundLayer.h"
#include "loadpng.h"
#include "bcm_host.h"


#define PIPENAME "myPipe"
int fd;
int read_bytes;
char readbuf[80];
pthread_mutex_t lock;
bool cancelInterupt = false;
bool showing = false;
uint16_t background = 0x00;
int32_t xOffset = 0;
int32_t yOffset = 0;
int32_t layer = 1;

int commandParser(char *command, char **path)
{
    if (access(command, F_OK) == 0)
    {
        *path = command;
        return 1;
    }
    return 0;
   
}


void * displayPNG(void *ptr){

    bool isVol = strncmp(ptr, "vol", 3) == 0;
    printf("display received '%s'\n", (char *)ptr);

    uint32_t currentTime = 0;
    pthread_mutex_lock(&lock);
    showing = true;
    pthread_mutex_unlock(&lock);

    // Sleep for 10 milliseconds every run-loop
    const int sleepMilliseconds = 10;
    bool run = true;
    //---------------------------------------------------------------------

    IMAGE_LAYER_T imageLayer;

    const char *imagePath = (char *)ptr;

    // Load image from path
    if (loadPng(&(imageLayer.image), imagePath) == false)
    {
        fprintf(stderr, "unable to load %s\n", imagePath);
        exit(EXIT_FAILURE);
    }
    
    bcm_host_init();

    //---------------------------------------------------------------------

    DISPMANX_DISPLAY_HANDLE_T display
        = vc_dispmanx_display_open(0);
    assert(display != 0);

    //---------------------------------------------------------------------

    DISPMANX_MODEINFO_T info;
    int result = vc_dispmanx_display_get_info(display, &info);
    assert(result == 0);

    //---------------------------------------------------------------------

    BACKGROUND_LAYER_T backgroundLayer;

    if (background > 0)
    {
        initBackgroundLayer(&backgroundLayer, background, 0);
    }

    createResourceImageLayer(&imageLayer, layer);

    //---------------------------------------------------------------------

    DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);
    assert(update != 0);

    if (background > 0)
    {
        addElementBackgroundLayer(&backgroundLayer, display, update);
    }

    if(isVol){
        xOffset = 0;
        yOffset = info.height - imageLayer.image.height;        
    }
    else{
        xOffset = 20;
        yOffset = 20;
    }
    

    addElementImageLayerOffset(&imageLayer,
                               xOffset,
                               yOffset,
                               display,
                               update);

    result = vc_dispmanx_update_submit_sync(update);
    assert(result == 0);

    //---------------------------------------------------------------------
    uint32_t timeout = 1300;
    if (!isVol){
        timeout = 2000;
    }
   
   
    while (run)
    {

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

   
    if (background > 0)
    {
        destroyBackgroundLayer(&backgroundLayer);
    }

    destroyImageLayer(&imageLayer);

    //---------------------------------------------------------------------

    result = vc_dispmanx_display_close(display);
    assert(result == 0);
    return NULL;
}


int main(int argc, char *argv[])
{
    if(access( PIPENAME, F_OK ) == 0 ){
        printf("found and deleting\n");
        unlink(PIPENAME);
    }
    mknod(PIPENAME, S_IFIFO | 0777, 0);
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
        readbuf[strcspn(readbuf, "\n")] = 0;
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
            
            int ret = pthread_create(&thread_id, NULL, &displayPNG, path);
            if (ret == 0)
            {
                printf("Thread created successfully  (%ld).\n",(unsigned long)thread_id);
            }
            
            //printf("\nDONE\n");
        }

        close(fd);
        usleep(sleepMilliseconds * 1000);
    }
    return 0;
}