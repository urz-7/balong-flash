#include <stdio.h>
#include <stdint.h>
#ifndef WIN32
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>
#else
#include <windows.h>
#include "getopt.h"
#include "printf.h"
#include "buildno.h"
#endif

#include "hdlcio.h"
#include "ptable.h"
#include "flasher.h"
#include "util.h"
#include "signver.h"
#include "zlib.h"

// file structure error flag
unsigned int errflag=0;

// digital signature flag
int gflag=0;
// firmware type flag
int dflag=0;

// type of firmware from the file header
int dload_id=-1;

//***********************************************
//* Partition table
//***********************************************
struct ptb_t ptable[120];
int npart=0; // number of partitions in the table


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

int main(int argc, char* argv[]) {

unsigned int opt;
int res;
FILE* in;
char devname[50] = "";
unsigned int  mflag=0,eflag=0,rflag=0,sflag=0,nflag=0,kflag=0,fflag=0;
unsigned char fdir[40];   // directory for multifile firmware

// parse command line
while ((opt = getopt(argc, argv, "d:hp:mersng:kf")) != -1) {
  switch (opt) {
   case 'h': 
     
printf("\n The utility is designed for flashing modems on the Balong V7 chipset\n\n\
%s [keys] <the name of the file to upload or the name of the directory with the files>\n\n\
 The following keys are allowed:\n\n"
#ifndef WIN32
"-p <tty> - Serial port for communicating with the loader (by default / dev / ttyUSB0)\n"
#else
"-p #     - Serial port number for communicating with the loader (example, -p8)\n"
"         (if -p is not specified, autodetection of the port)\n"
#endif
"-n       - multifile firmware mode from the specified directory\n\
-g#      - setting digital signature mode\n\
  -gl    - description of parameters\n\
  -gd    - prohibition of autodetection of a signature\n\
-m       - Display the firmware file and exit\n\
-e       - disassemble the firmware file into sections without headers\n\
-s       - disassemble the firmware file into sections with headers\n\
-k       - Do not restart the modem at the end of the firmware\n\
-r       - force restart the modem without flashing the partitions\n\
-f       - flash even if there are CRC errors in the source file\n\
-d#      - installation of firmware type (DLOAD_ID, 0..7), -dl - list of types\n\
\n",argv[0]);
    return 0;

   case 'p':
    strcpy(devname,optarg);
    break;

   case 'm':
     mflag=1;
     break;
     
   case 'n':
     nflag=1;
     break;
     
   case 'f':
     fflag=1;
     break;
     
   case 'r':
     rflag=1;
     break;
     
   case 'k':
     kflag=1;
     break;
     
   case 'e':
     eflag=1;
     break;

   case 's':
     sflag=1;
     break;

   case 'g':
     gparm(optarg);
     break;
     
   case 'd':
     dparm(optarg);
     break;
     
   case '?':
   case ':':  
     return -1;
  }
}  
printf("--------------------------------------------------------------------------------------------------");
//printf("\n Program for flashing devices on the Balong-chipset, V3.0.%i, (c) forth32, 2015, GNU GPLv3",BUILDNO);
#ifdef WIN32
printf("\n              Flasher by (c) urz7, 2018");
#endif
printf("\n--------------------------------------------------------------------------------------------------\n");

if (eflag&sflag) {
  printf("\n The -s and -e keys are incompatible\n");
  return -1;
}  

if (kflag&rflag) {
  printf("\n The -k and -r keys are incompatible\n");
  return -1;
}  

if (nflag&(eflag|sflag|mflag)) {
  printf("\n The -n switch is incompatible with the -s, -m and -e keys\n");
  return -1;
}  
  

// ------  reboot without specifying a file
//--------------------------------------------
if ((optind>=argc)&rflag) goto sio; 


// Open the input file
//--------------------------------------------
if (optind>=argc) {
  if (nflag)
    printf("\n - Not specified directory with files\n");
  else 
    printf("\n - The file name is not specified for the download, use the -h switch for the tooltip\n");
  return -1;
}  

if (nflag) 
  // for -n - just copy the prefix
  strncpy(fdir,argv[optind],39);
else {
  // for single-file operations
in=fopen(argv[optind],"rb");
if (in == 0) {
  printf("\n Error opening %s",argv[optind]);
  return -1;
}
}


// Search for partitions within a file
if (!nflag) {
  findparts(in);
  show_fw_info();
}  

// Search for firmware files in the specified directory
else findfiles(fdir);
  
//------ The output mode of the firmware file card
if (mflag) show_file_map();

// error output CRC
if (!fflag && errflag) {
    printf("\n\n! The input file contains errors - we terminate the work\n");
    return -1; 
}

//------- Cutting mode of the firmware file
if (eflag|sflag) {
  fwsplit(sflag);
  printf("\n");
  return 0;
}

sio:
//--------- Main mode - write firmware
//--------------------------------------------

// SIO Configuration
open_port(devname);

// Determine the port mode and the version of the dload protocol

res=dloadversion();
if (res == -1) return -2;
if (res == 0) {
  printf("\n The modem is already in HDLC mode");
  goto hdlc;
}

// If necessary, we send a digital signature command
if (gflag != -1) send_signver();

// Enter the HDLC mode

usleep(100000);
enter_hdlc();

// Have entered into HDLC
//------------------------------
hdlc:

// get the protocol version and the device ID
protocol_version();
dev_ident();


printf("\n----------------------------------------------------\n");

if ((optind>=argc)&rflag) {
  // reboot without specifying a file
  restart_modem();
  exit(0);
}  

// Write down the entire USB flash drive
flash_all();
printf("\n");

port_timeout(1);

// Exit the HDLC mode and reboot
if (rflag || !kflag) restart_modem();
// Exit HDLC without rebooting
else leave_hdlc();
} 
