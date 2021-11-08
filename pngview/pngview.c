//-------------------------------------------------------------------------
//
// The MIT License (MIT)
//
// Copyright (c) 2013 Andrew Duncan
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//-------------------------------------------------------------------------

#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "backgroundLayer.h"
#include "imageLayer.h"
#include "key.h"
#include "loadpng.h"

#include "bcm_host.h"


#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

//-------------------------------------------------------------------------

#define NDEBUG

//-------------------------------------------------------------------------

const char *program = NULL;

//-------------------------------------------------------------------------

volatile bool run = true;

//-------------------------------------------------------------------------

static void
signalHandler(
    int signalNumber)
{
    switch (signalNumber)
    {
    case SIGINT:
    case SIGTERM:

        run = false;
        break;
    };
}

//-------------------------------------------------------------------------

void usage(void)
{
    fprintf(stderr, "Usage: %s ", program);
    fprintf(stderr, "[-b <RGBA>] [-d <number>] [-l <layer>] ");
    fprintf(stderr, "[-x <offset>] [-y <offset>] <file.png>\n");
    fprintf(stderr, "    -b - set background colour 16 bit RGBA\n");
    fprintf(stderr, "         e.g. 0x000F is opaque black\n");
    fprintf(stderr, "    -d - Raspberry Pi display number\n");
    fprintf(stderr, "    -l - DispmanX layer number\n");
    fprintf(stderr, "    -x - offset (pixels from the left)\n");
    fprintf(stderr, "    -y - offset (pixels from the top)\n");
    fprintf(stderr, "    -t - timeout in ms\n");
    fprintf(stderr, "    -n - non-interactive mode\n");

    exit(EXIT_FAILURE);
}


int fd;
char readbuf[80];
char end[10];
int to_end;
int read_bytes;
char * path = ""; 



void * pipeSetup(void * ptr){
   mknod(path, S_IFIFO|0640, 0);
   strcpy(end, "end");
    const int sleepMilliseconds = 1000;
   while(1) {
      fd = open(path, O_RDONLY);
      read_bytes = read(fd, readbuf, sizeof(readbuf));
      readbuf[read_bytes] = '\0';
      printf("Received string: \"%s\" and length is %d\n", readbuf, (int)strlen(readbuf));
      to_end = strcmp(readbuf, end);
      if (to_end == 0) {
         close(fd);
         break;
      }
       usleep(sleepMilliseconds * 1000);
   }
}

void showFile( char * path){

    uint16_t background = 0x000F;
    int32_t layer = 1;
    uint32_t displayNumber = 0;
    int32_t xOffset = 0;
    int32_t yOffset = 0;
    uint32_t timeout = 0;
    bool xOffsetSet = false;
    bool yOffsetSet = false;
    bool interactive = true;
   

    IMAGE_LAYER_T imageLayer;

    const char *imagePath = path;

    if(strcmp(imagePath, "-") == 0)
    {
        // Use stdin
        if (loadPngFile(&(imageLayer.image), stdin) == false)
        {
            fprintf(stderr, "unable to load %s\n", imagePath);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        // Load image from path
        if (loadPng(&(imageLayer.image), imagePath) == false)
        {
            fprintf(stderr, "unable to load %s\n", imagePath);
            exit(EXIT_FAILURE);
        }
    }

    //---------------------------------------------------------------------

    if (signal(SIGINT, signalHandler) == SIG_ERR)
    {
        perror("installing SIGINT signal handler");
        exit(EXIT_FAILURE);
    }

    //---------------------------------------------------------------------

    if (signal(SIGTERM, signalHandler) == SIG_ERR)
    {
        perror("installing SIGTERM signal handler");
        exit(EXIT_FAILURE);
    }

    //---------------------------------------------------------------------

    bcm_host_init();

    //---------------------------------------------------------------------

    DISPMANX_DISPLAY_HANDLE_T display
        = vc_dispmanx_display_open(displayNumber);
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

    if (xOffsetSet == false)
    {
        xOffset = (info.width - imageLayer.image.width) / 2;
    }

    if (yOffsetSet == false)
    {
        yOffset = (info.height - imageLayer.image.height) / 2;
    }

    addElementImageLayerOffset(&imageLayer,
                               xOffset,
                               yOffset,
                               display,
                               update);

    result = vc_dispmanx_update_submit_sync(update);
    assert(result == 0);

    //---------------------------------------------------------------------

    int32_t step = 1;
    uint32_t currentTime = 0;

    // Sleep for 10 milliseconds every run-loop
    const int sleepMilliseconds = 10;

    while (run)
    {
    
        //---------------------------------------------------------------------

        usleep(sleepMilliseconds * 1000);

        currentTime += sleepMilliseconds;
        if (timeout != 0 && currentTime >= timeout) {
            run = false;
        }
    }

    if (background > 0)
    {
        destroyBackgroundLayer(&backgroundLayer);
    }

    destroyImageLayer(&imageLayer);

    //---------------------------------------------------------------------

    result = vc_dispmanx_display_close(display);
    assert(result == 0);

}


//-------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    
    program = basename(argv[0]);
    //---------------------------------------------------------------------

    int opt = 0;

    while ((opt = getopt(argc, argv, "p")) != -1)
    {
        switch(opt)
        {

        case 'p':

            path = optarg;
            break;

        default:
            usage();
            break;
        }
    }

    //---------------------------------------------------------------------

    if (optind >= argc)
    {
        usage();
    }

    //---------------------------------------------------------------------

    /*creating thread id*/
    pthread_t id;
    int ret;
    
    /*creating thread*/
    ret=pthread_create(&id,NULL,&pipeSetup,NULL);
    if(ret==0){
        printf("Thread created successfully.\n");
    }
    
  
    const int sleepMilliseconds = 1000;

    while (1){
        printf("main loop.\n");
         usleep(sleepMilliseconds * 1000);
    }

   

    return 0;
}

