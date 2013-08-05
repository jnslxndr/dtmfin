//
//  main.cpp
//  dtmfin
//
//  Created by jens on 12.04.13.
//  Copyright (c) 2013 jens alexander ewald. All rights reserved.
//

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 

int sockfd;
struct sockaddr_in servaddr;

pid_t pid = getpid();

static bool running = true;

void catch_int(int sig) {
  signal(SIGINT, catch_int);
  fflush(stdout);
  running = false;
}


#define SAMPLING_RATE 44100

extern "C" {
  #include "dtmf.h"
}

#include "portaudio.h"


static char lastCode = NO_CODE;
static PaTime lastDetection = -1;
static PaTime timeOut = 0.300;

static int audioCallback( const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData )
{
  
  if (lastDetection == -1) lastDetection = timeInfo->currentTime;
  
  (void) outputBuffer;
  
  char code = NO_CODE;
  
  DTMFDecode(inputBuffer,framesPerBuffer,&code);
  
  if (code!=NO_CODE) {
    if (code!=lastCode || timeInfo->currentTime-lastDetection > timeOut) {
      lastCode = code;
      lastDetection = timeInfo->currentTime;
      std::cout << code;
      std::flush(std::cout);
      
      sendto(sockfd,&code,1,0,
             (struct sockaddr *)&servaddr,sizeof(servaddr));
    }
  }
  
  return 0;
}


const PaStreamInfo* info;
const PaDeviceInfo* deviceInfo;

int main(int argc, const char * argv[])
{
  
  if (argc != 3)
  {
    printf("usage:  dtmfin <IP address> <port>\n");
    exit(1);
  }
  
  sockfd=socket(AF_INET,SOCK_DGRAM,0);
  
  bzero(&servaddr,sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  const char *hostname;
  hostname = argv[1];
  struct hostent *server;
  server = gethostbyname(hostname);
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host as %s\n", hostname);
    exit(0);
  }
  bzero((char *) &servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
        (char *)&servaddr.sin_addr.s_addr, server->h_length);
  
  servaddr.sin_port=htons(atoi(argv[2]));
  
  // attach signal handlers
  signal(SIGINT, catch_int);
  
  PaStream *stream;
  PaError err;
  
  // try to open audio:
  err = Pa_Initialize();
  if( err != paNoError ) goto error;
  
  DTMFSetup(SAMPLING_RATE, BUFFER_SIZE);
  
  deviceInfo = Pa_GetDeviceInfo(Pa_GetDefaultInputDevice());
  
  std::clog
  << "Default Samplerate: " << deviceInfo->defaultSampleRate
  << "\t" << "Max input channels: " << deviceInfo->maxInputChannels
  << "\t" << "Name: " << deviceInfo->name
  << std::endl;
  
  
  /* Open an audio I/O stream. */
  err = Pa_OpenDefaultStream( &stream, 1, 0,
                             paInt16,
                             SAMPLING_RATE,
                             BUFFER_SIZE,
                             audioCallback,
                             NULL );
  
  if( err != paNoError ) goto error;
  
  err = Pa_StartStream( stream );
  if( err != paNoError ) goto error;
  
  info = Pa_GetStreamInfo(stream);
  
  std::clog
  << "In Latency: " << info->inputLatency << "\t"
  << "Out Latency: " << info->outputLatency << "\t"
  << "Samplerate: " << info->sampleRate << "\t"
  << "Buffer size " << BUFFER_SIZE
  << std::endl;
  
  
  while (running){sleep(2);};
  
  err = Pa_StopStream( stream );
  if( err != paNoError ) goto error;
  
  err = Pa_CloseStream( stream );
  if( err != paNoError ) goto error;
  
  // Close Audio:
  err = Pa_Terminate();
  
  if( err != paNoError ) {
  error:
    std::cerr <<  "PortAudio error: " <<  Pa_GetErrorText( err ) << std::endl;
  }
  
  /* Deallocate the socket */
  close(sockfd);
  
  return 0;
}

