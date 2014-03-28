#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>
#include <sys/time.h>
#include <unistd.h>
#include "inet.h"
#include "capture.h"

#define DEFAULT_SNAPLEN 100 //1514

pcap_t *pc;		/* pcap device */
int datalinkOffset;	/* offset of ip packet from datalink packet */

int captureDebug = 0;
fd_set fd_wait;
struct timeval st;

extern int verbose;
extern char *interface;

void CaptureInit(uint32 sourceIP, uint16 sourcePort,
		 uint32 targetIP, uint16 targetPort)
{
  char *device;
  char errbuf[PCAP_ERRBUF_SIZE];
  int snaplen = DEFAULT_SNAPLEN;
  int promisc = 1;
  int timeout = 10;  /* timeout in 10 ms */
  char filtercmds[255];
  bpf_u_int32 netmask;
  struct bpf_program filter;
  char source[18];
  char target[18];
  int i;

  if (interface != NULL) {
    device = interface;
  }
  else {
    device = pcap_lookupdev(errbuf);
  }
  
  if (device == NULL) {
    fprintf(stderr, "Can't find capture device: %s\n", errbuf);
    exit(-1);
  } 
  if (captureDebug) {
    printf("Device name is %s\n", device);
  }
  pc = pcap_open_live(device, snaplen, promisc, timeout, errbuf);
  if (pc == NULL) {
    fprintf(stderr,"Can't open capture device %s: %s\n",device, errbuf);
    exit(-1);
  } 

 /* if (pcap_setnonblock(pc, 1, errbuf) == -1) {
  	fprintf(stderr,"Could not set device %s to non-blocking: %s\n",device,errbuf);
	exit(1);
  }*/

  /* XXX why do we need to do this? */
  i = pcap_snapshot(pc);
  if (snaplen < i) {
    fprintf(stderr, "Warning: snaplen raised to %d from %d",
	    snaplen, i);
  }

  if ((i = pcap_datalink(pc)) < 0) {
    fprintf(stderr,"Unable to determine datalink type for %s: %s\n",
	    device, errbuf);
    exit(-1);
  }
  switch(i) {
    case DLT_EN10MB: datalinkOffset = 14; break;
    case DLT_IEEE802: datalinkOffset = 22; break;
    case DLT_NULL: datalinkOffset = 4; break;
    case DLT_SLIP: 
    case DLT_PPP: datalinkOffset = 24; break;
    case DLT_RAW: datalinkOffset = 0; break;
    default: 
       fprintf(stderr,"Unknown datalink type %d\n",i);
       exit(-1);
       break;
  }

  if ( InetAddress(sourceIP) ==NULL ) {
    fprintf(stderr, "Invalid source IP address (%d)\n", sourceIP);
    exit(-1);
  }

  strncpy(source, InetAddress(sourceIP), sizeof(source)-1);
  strncpy(target, InetAddress(targetIP), sizeof(target)-1);


  /* Setup initial filter */
  sprintf(filtercmds,
	  "dst host %s and src host %s and dst port %d and src port %d\n",
	  source, target, sourcePort, targetPort);

  /*sprintf(filtercmds,
	  "host %s and port %d\n",
	  target, targetPort);
  */
  if (captureDebug) {
    printf("datalinkOffset = %d\n", datalinkOffset);
    printf("filter = %s\n", filtercmds);
  }
  if (pcap_compile(pc, &filter, filtercmds, 1, netmask) < 0) {
    printf("Error: %s", pcap_geterr(pc));
    exit(-1);
  }

  if (pcap_setfilter(pc, &filter) < 0) {
    fprintf(stderr, "Can't set filter: %s",pcap_geterr(pc));
    exit(-1);
  }
  
  if (captureDebug) {
    printf("Listening on %s...\n", device);
  }

}

char *CaptureGetPacket(struct PacketInfo *pi)
{
  const u_char *p;
  int t;

  FD_ZERO(&fd_wait);
  FD_SET(pcap_get_selectable_fd(pc), &fd_wait);
  st.tv_sec=1;
  st.tv_usec=0;

  t=select(pcap_get_selectable_fd(pc)+1, &fd_wait, NULL, NULL, &st);

  if (t==-1) {
	fprintf(stderr,"Error in select()\n");
	return NULL;
  }
  else if (t==0) return NULL;
  else {

	p = pcap_next(pc, (struct pcap_pkthdr *)pi);
	if (p!= NULL) {
		p += datalinkOffset;
	}
	//pi->ts.tv_sec = (pi->ts.tv_sec + thisTimeZone) % 86400;

#ifdef DEBUG
	if(p!=NULL)
	{
		printf("==================================== CaptureGetPacket =================================\n");
		PrintTcpPacket((struct TcpPacket*)p);
		//print_pkt(p,1,pi->caplen);
		printf("\n\n");
		printf("=======================================================================================\n");
	}
#endif

	return (char *)p;
  }

  return NULL;
}

void CaptureSkipNPackets(int n, struct PacketInfo *pi) {
  int i;
  
  for(i = 0; i < n; i += 1) pcap_next(pc, (struct pcap_pkthdr *)pi);
}

void CaptureEnd()
{
  struct pcap_stat stat;

  if (pcap_stats(pc, &stat) < 0) {
    (void)fprintf(stderr, "pcap_stats: %s\n", pcap_geterr(pc));
  }
  else {
    if (verbose) fprintf(stderr, "%d packets received by filter\n", stat.ps_recv); 
    if (verbose) fprintf(stderr, "%d packets dropped by kernel\n", stat.ps_drop);
  }

  pcap_close(pc);
}

