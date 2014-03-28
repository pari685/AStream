
#ifndef _CAPTURE_H_
#define _CAPTURE_H_

#include <sys/time.h>
#include "inet.h"


struct PacketInfo {
  struct timeval ts;
  unsigned int caplen;
  unsigned int len;
};

void CaptureInit(uint32 sourceIP, uint16 sourcePort,
		 uint32 targetIP, uint16 targetPort);
char *CaptureGetPacket(struct PacketInfo *);
void CaptureSkipNPackets(int, struct PacketInfo *);
void CaptureEnd();

#endif /* _CAPTURE_H_ */
