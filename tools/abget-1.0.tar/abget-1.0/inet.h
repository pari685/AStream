
#ifndef _LOCAL_INET_H_
#define _LOCAL_INET_H_

/* XXX These are machine/compiler dependent */
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

struct IpHeader {
  uint8		ip_vhl;	/* version (4bits) & header length (4 bits) */
  uint8		ip_tos;	/* type of service */
  uint16	ip_len; /* length of IP datagram */
  uint16	ip_id;	/* identification (for frags) */ 
  uint16	ip_off; /* offset (within a fragment) and flags (3 bits) */
  uint8		ip_ttl; /* time to live */
  uint8		ip_p;	/* protocol number */
  uint16	ip_xsum; /* checksum */
  uint32	ip_src; /* source address */
  uint32	ip_dst; /* destination address */
};

#define IPPROTOCOL_ICMP		1
#define IPPROTOCOL_IGMP		2
#define IPPROTOCOL_TCP		6
#define IPPROTOCOL_UDP		17

#define IP_DF 0x4000

/* Pseudo header for doing TCP checksum calculation */
struct PseudoIpHeader {
  uint32	filler[2];
  uint8		zero;
  uint8		ip_p;
  uint16	ip_len;
  uint32	ip_src;
  uint32	ip_dst;
};

struct TcpHeader {
  uint16	tcp_sport;	/* source port */
  uint16	tcp_dport;	/* destination port */
  uint32	tcp_seq;	/* sequence number */
  uint32	tcp_ack;	/* acknoledgement number */
  uint8		tcp_hl;		/* header length (4 bits) */
  uint8		tcp_flags;	/* flags */
  uint16	tcp_win;	/* advertized window size */
  uint16	tcp_xsum;	/* checksum */
  uint16	tcp_urp;	/* urgent pointer */
};

/* TCP Flags */
#define TCPFLAGS_FIN	0x01
#define TCPFLAGS_SYN	0x02
#define TCPFLAGS_RST	0x04
#define TCPFLAGS_PSH	0x08
#define TCPFLAGS_ACK	0x10
#define TCPFLAGS_URG	0x20

struct TcpPacket {
  struct IpHeader ip;
  struct TcpHeader tcp;
};


struct TcpSession {
  int socket;		/* raw socket we use to send on */

/* connection endpoint identifiers */
  uint32 src;
  uint16 sport;
  uint32 dst;
  uint16 dport;

/* sender info, from RFC 793 */
  uint32 iss;			    /* sender sequence number */
  uint32 snd_una;		    /* oldest unacknowledged sequence nr. */
  uint32 snd_nxt;		    /* next sequence number to be sent */
  uint16 snd_wnd;		    /* min(cwnd, rwnd) */

/* Receiver info */
  uint32 rcv_wnd;
  uint32 rcv_nxt;    /* sequence nr. expected on next incoming segment */
  uint32 irs;			    /* receiver sequence number */

  uint8 ttl;

/* timing */
  double rtt;
};

void WriteTcpPacket(struct TcpPacket *p,
		    uint32 src, uint32 dst, uint16 sport, uint16 dport,
		    uint32 seq, uint32 ack, uint8 flags, uint16 win,
		    uint16 urp, uint16 datalen, uint16 optlen); 
void PrintTcpPacket(struct TcpPacket *p);
void SendSessionPacket(struct TcpSession *session, struct TcpPacket *packet,
		       uint16 ip_len, uint16 optlen, uint8 tcp_flags);

char *InetAddress(uint32 addr);
#endif /* _LOCAL_INET_H_ */




