//
//  main.cpp
//  dtmfin
//
//  Created by jens on 12.04.13.
//  Copyright (c) 2013 jens alexander ewald. All rights reserved.
//

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/wait.h>
#include <iostream>


#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"

#define SAMPLING_RATE 44100

extern "C" {
  #include "dtmf.h"
}

#include "portaudio.h"

const char *default_path = "/dtmf";
char * osc_path = (char*)default_path;

struct OSCSender {
  UdpTransmitSocket *socket;
  osc::OutboundPacketStream stream;
  OSCSender(UdpTransmitSocket *_socket,osc::OutboundPacketStream _stream) :
  socket(_socket), stream(_stream){}
};

pid_t pid = getpid();

static bool running = true;

void catch_int(int sig) {
  std::cout << sig << std::endl;
  fflush(stdout);
  running = false;
  signal(SIGINT, catch_int);
}


static PaTime timeOut = .5; // default to 200 millisecond timeout

static int audioCallback( const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData )
{
  
  static PaTime lastDetection = timeInfo->currentTime;
  static char lastCode = NO_CODE;
  
  (void) outputBuffer;
  
  static char code;
  
  DTMFDecode(inputBuffer,framesPerBuffer,&code);
  
  if(code != NO_CODE && (code!=lastCode || (code == lastCode && timeInfo->currentTime-lastDetection > timeOut)))
  {
    lastDetection = timeInfo->currentTime;
    lastCode = code;
    
    std::clog << code;
    
    if (userData != NULL) {
      OSCSender *sender = (OSCSender*) userData;
      sender->stream.Clear();
      sender->stream << osc::BeginMessage(osc_path);
      if(code >= 0x30 && code <= 0x39) {
        sender->stream << atoi(&code);
      } else {
        sender->stream << (&code);
      }
      sender->stream << osc::EndMessage;
      sender->socket->Send(sender->stream.Data(), sender->stream.Size());
    }
  }
  
  
  return 0;
}


const PaStreamInfo* info;
const PaDeviceInfo* deviceInfo;

void usage() {
  printf("\
usage:\n\
    dtmfin -h <IP address> -p <port> -t <time out> [-d <device id>]\n\
    dtmfin -l (to list devices)\n"
  );
}

void listDevices() {
  PaError err;
  err = Pa_Initialize();
  if(err != paNoError) goto err;
  std::cout
    << "Available audio devices:" << std::endl
    << "Index\tName"
    << std::endl
    << "-----\t------------------"
    << std::endl;
  for (int id = 0; id<Pa_GetDeviceCount(); id++) {
    const PaDeviceInfo *info = Pa_GetDeviceInfo(id);
    if (info->maxInputChannels > 0) {
      std::cout << id << "\t" << info->name << std::endl;
    }
  }
err:
  Pa_Terminate();
}


int main(int argc, char * const argv[])
{
  
  char *host = (char*)"127.0.0.1";
  int port = 3001;
  int device = -1; // for default device
  int c;
  
  opterr = 0;
  
  /* options descriptor */
  static struct option longopts[] = {
    { "help",      no_argument      ,      NULL,           '?' },
    { "list",      no_argument      ,      NULL,           'l' },
    { "host",      required_argument,      NULL,           'h' },
    { "port",      required_argument,      &port,          'p' },
    { "device",    required_argument,      &device,        'd' },
    { "time-out",  required_argument,      NULL,           't' },
    { "osc-path",  required_argument,      NULL,           'o' },
    { NULL,        0,                      NULL,            0  }
  };
  
  while ((c = getopt_long (argc, argv, ":h:p:t:d:l",longopts,NULL)) != -1)
    switch (c)
  {
    case 'h':
      host = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 'd':
      device = atoi(optarg);
      break;
    case 't':
      timeOut = atoi(optarg)/1000.f;
      break;
    case 'o':
      osc_path = optarg;
      break;
    case 'l':
      listDevices();
      return 0;
    case '?':
      usage();
      return 0;
    case ':':
      switch (optopt) {
        case 'h':
        case 'p':
          fprintf (stderr, "Please specify an ip and port to send to.\n");
          break;
          
        case 't':
          fprintf (stderr, "Please specify a time out value in milliseconds.\n");
          break;
          
        case 'd':
          fprintf (stderr, "-d takes a device index. Use -l to list all available devices.\n");
          break;
          
        default:
          if (isprint (optopt))
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
          else
            fprintf (stderr,
                   "Unknown option character `\\x%x'.\n",
                   optopt);
          break;
      }
      return 1;
    default:
      usage();
      return 1;
  }
  
  // attach signal handlers
  signal(SIGINT, catch_int);

  
  // Set up OSC
  const int osc_buffer_size = 512;
  char buffer[osc_buffer_size];
  UdpTransmitSocket _sock(IpEndpointName(host,port));
  osc::OutboundPacketStream _stream(buffer,osc_buffer_size);
  OSCSender osc_sender(&_sock,_stream);
  _sock.SetEnableBroadcast(true);
  
  std::clog << "Sending OSC to " << host << " on port " << port << " with path '" << osc_path << "'" << std::endl;
  
  std::clog << "Time for repeated detection is set to " << ((int)(timeOut*1000)) << " milliseconds." << std::endl;
  std::clog << "Samplerate is set to " << SAMPLING_RATE << "Hz" << std::endl;
  
  
  // try to open audio:
  PaError err;
  err = Pa_Initialize();
  if( err != paNoError ) goto error;
  
  DTMFSetup(SAMPLING_RATE, BUFFER_SIZE);
  
  deviceInfo = Pa_GetDeviceInfo((device == -1) ? Pa_GetDefaultInputDevice() : device);
  
  
  /* Open an audio I/O stream. */
  PaStream *stream;
  err = Pa_OpenDefaultStream( &stream, 1, 0,
                             paInt16,
                             SAMPLING_RATE,
                             BUFFER_SIZE,
                             audioCallback,
                             &osc_sender ); // send a pointer to the OSCSender with it
  
  if( err != paNoError ) goto error;
  
  err = Pa_StartStream( stream );
  if( err != paNoError ) goto error;
  
  info = Pa_GetStreamInfo(stream);
  
  std::clog
  << "Audio initialized with:" << std::endl
  << "  Device       " << deviceInfo->name << std::endl
  << "  Latency      " << info->inputLatency << "ms" << std::endl
  << "  Samplerate   " << info->sampleRate << "kHz" << std::endl
  << "  Buffer Size  " << BUFFER_SIZE << "samples" << std::endl;
  
  std::clog << "Read for detection!" << std::endl;
  
  // pause and let the audio thread to the work
  while(running) pause();
  
  // end program
  err = Pa_StopStream( stream );
  if( err != paNoError ) goto error;
  
  err = Pa_CloseStream( stream );
  if( err != paNoError ) goto error;
  
  // Close Audio:
  err = Pa_Terminate();
  
  if( err != paNoError ) {
  error:
    std::cerr <<  "PortAudio error: " <<  Pa_GetErrorText( err ) << std::endl;
    return -1;
  }
  
  return 0;
}

