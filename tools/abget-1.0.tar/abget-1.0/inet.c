#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <string.h>
#include <stdio.h>
#include "inet.h"
/*
 * Deal with struct in_addr type agreement once and for all
 */
char *InetAddress(uint32 addr)
{
  struct in_addr s;
  s.s_addr = addr;
  return (inet_ntoa(s));
}

/*
 * Really slow implementation of ip checksum
 * ripped off from rfc1071
 */
uint16 InetChecksum(uint16 *addr, uint16 len){
  uint32 sum = 0;
  uint32 count = len;

  while( count > 1 )  {
    sum += *addr++;
    count -= 2;
  }
  if( count > 0 ) {
    sum += * (uint8 *) addr;
  }

  while (sum>>16) {
    sum = (sum & 0xffff) + (sum >> 16);
  }
  return(~sum);
}


void WriteTcpPacket(struct TcpPacket *p,
		    uint32 src, uint32 dst, uint16 sport, uint16 dport,
		    uint32 seq, uint32 ack, uint8 flags, uint16 win,
		    uint16 urp, uint16 datalen, uint16 optlen) 
{
  struct TcpHeader *tcp = &p->tcp;
  struct IpHeader *ip = &p->ip;
  //  printf("datalen: %d, optlen: %d\n", datalen, optlen);
  bzero((char *)p, sizeof (struct IpHeader)); 
  ip->ip_src = src;
  ip->ip_dst = dst;
  ip->ip_p = IPPROTOCOL_TCP;
  ip->ip_xsum =
    htons((uint16)(sizeof(struct TcpHeader)+datalen+optlen)); /* pseudo hdr */
  tcp->tcp_sport = htons(sport);
  tcp->tcp_dport = htons(dport);
  tcp->tcp_seq = htonl(seq);
  tcp->tcp_ack = htonl(ack);
  tcp->tcp_hl = (sizeof (struct TcpHeader) + optlen) << 2;
  tcp->tcp_flags = flags;
  tcp->tcp_win = htons(win);
  tcp->tcp_urp = htons(urp);
  tcp->tcp_xsum = 0;
  tcp->tcp_xsum = InetChecksum((uint16 *)ip,
			       (uint16)(sizeof(struct TcpHeader) +
					sizeof(struct IpHeader)+
					datalen+optlen));


  /* Fill in real ip header */
  ip->ip_ttl = 60;
  ip->ip_tos = 0;
  ip->ip_vhl = 0x45;
  ip->ip_p = IPPROTOCOL_TCP;
#ifdef __FreeBSD__
  ip->ip_off = IP_DF;
  ip->ip_len = (uint16)(sizeof (struct TcpPacket) + datalen + optlen);
#else /* __FreeBSD__ */
  ip->ip_off = htons(IP_DF);
  ip->ip_len = htons((uint16)((sizeof (struct TcpPacket) + datalen + optlen)));
#endif /* __FreeBSD__ */

}


void ReadTcpPacket(struct TcpPacket *p,
		    uint32 *src, uint32 *dst, uint16 *sport, uint16 *dport,
		    uint32 *seq, uint32 *ack, uint8 *flags, uint16 *win,
		    uint16 *urp, uint16 *datalen, uint16 *optlen) 
{
  struct TcpHeader *tcp = &p->tcp;
  struct IpHeader *ip = &p->ip;

  uint16 ip_len;
  uint16 ip_hl;
  uint16 tcp_hl;

  /* XXX do checksum check? */
  if (ip->ip_p != IPPROTOCOL_TCP) {
    fprintf(stderr, "Error: not a TCP packet\n");
    return;
  }

  *src = ip->ip_src;
  *dst = ip->ip_dst;
  *sport = ntohs(tcp->tcp_sport);
  *dport = ntohs(tcp->tcp_dport);
  *seq = ntohl(tcp->tcp_seq);
  *ack = ntohl(tcp->tcp_ack);
  *flags = tcp->tcp_flags;
  *win = ntohs(tcp->tcp_win);
  *urp = ntohs(tcp->tcp_urp);

  tcp_hl = tcp->tcp_hl >> 2;
  ip_len = ip->ip_len;
  ip_hl = (ip->ip_vhl & 0x0f) << 2;
  *datalen = (ip_len - ip_hl) - tcp_hl;
  *optlen = tcp_hl - sizeof(struct TcpHeader);
}

extern FILE *writefp;

void PrintTcpPacket(struct TcpPacket *p)
{
  struct TcpHeader *tcp = &p->tcp;
  struct IpHeader *ip = &p->ip;

  if(writefp) {
  	  fprintf(writefp, "%s.%u > ",InetAddress(ip->ip_src), ntohs(tcp->tcp_sport));
	  fprintf(writefp, "%s.%u ", InetAddress(ip->ip_dst), ntohs(tcp->tcp_dport));
	  if (tcp->tcp_flags & TCPFLAGS_FIN) {
		  fprintf(writefp, "F");
	  }
	  if (tcp->tcp_flags & TCPFLAGS_SYN) {
		  fprintf(writefp, "S");
	  }
	  if (tcp->tcp_flags & TCPFLAGS_RST) {
		  fprintf(writefp, "R");
	  }
	  if (tcp->tcp_flags & TCPFLAGS_PSH) {
		  fprintf(writefp, "P");
	  }
	  if (tcp->tcp_flags & TCPFLAGS_ACK) {
		  fprintf(writefp, "A");
	  }
	  if (tcp->tcp_flags & TCPFLAGS_URG) {
		  fprintf(writefp, "U");
	  }
	  fprintf(writefp, " seq: %u, ack: %u",
			  (uint32)ntohl(tcp->tcp_seq),
			  (uint32)ntohl(tcp->tcp_ack));
	  fprintf(writefp, " win: %u, urg: %u",ntohs(tcp->tcp_win), ntohs(tcp->tcp_urp));
	  fprintf(writefp, " datalen: %u, optlen: %u\n",
			  ntohs(ip->ip_len) - ((ip->ip_vhl &0x0f) << 2) - (tcp->tcp_hl >> 2),
			  (tcp->tcp_hl >> 2) - (unsigned int)sizeof (struct TcpHeader));
  }
  else {
	  printf("%s.%u > ",InetAddress(ip->ip_src), ntohs(tcp->tcp_sport));
	  printf("%s.%u ", InetAddress(ip->ip_dst), ntohs(tcp->tcp_dport));
	  if (tcp->tcp_flags & TCPFLAGS_FIN) {
		  printf("F");
	  }
	  if (tcp->tcp_flags & TCPFLAGS_SYN) {
		  printf("S");
	  }
	  if (tcp->tcp_flags & TCPFLAGS_RST) {
		  printf("R");
	  }
	  if (tcp->tcp_flags & TCPFLAGS_PSH) {
		  printf("P");
	  }
	  if (tcp->tcp_flags & TCPFLAGS_ACK) {
		  printf("A");
	  }
	  if (tcp->tcp_flags & TCPFLAGS_URG) {
		  printf("U");
	  }
	  printf(" seq: %u, ack: %u",
			  (uint32)ntohl(tcp->tcp_seq),
			  (uint32)ntohl(tcp->tcp_ack));
	  printf(" win: %u, urg: %u",ntohs(tcp->tcp_win), ntohs(tcp->tcp_urp));
	  printf(" datalen: %u, optlen: %u\n",
			  ntohs(ip->ip_len) - ((ip->ip_vhl &0x0f) << 2) - (tcp->tcp_hl >> 2),
			  (tcp->tcp_hl >> 2) - (unsigned int)sizeof (struct TcpHeader));
  }
}


void SendSessionPacket(struct TcpSession *s, struct TcpPacket *p,
		       uint16 optlen, uint16 datalen, uint8 tcp_flags)
{
  int iplen = sizeof(struct TcpPacket) + datalen + optlen;
  int nbytes;
  struct sockaddr_in sockAddr;   

  (void) WriteTcpPacket(p,
			s->src,
			s->dst,
			s->sport,
			s->dport,
			s->snd_nxt, 
			s->rcv_nxt, 
			tcp_flags,
			s->snd_wnd,
			0, datalen, optlen);

  sockAddr.sin_family  = AF_INET;
  sockAddr.sin_addr.s_addr = s->dst;

#ifdef DEBUG
	printf("==================================== Sending Packet =================================\n");
	PrintTcpPacket(p);
	//print_pkt(p,1,iplen);
	printf("\n\n");
	printf("=====================================================================================\n");
#endif

  if ((nbytes = sendto(s->socket, (char *)p, iplen, 0,
		       (struct sockaddr *)&sockAddr,
		       sizeof(sockAddr))) < iplen) {
    printf("WARNING: only sent %d of %d bytes\n", nbytes, iplen);
  }
}

