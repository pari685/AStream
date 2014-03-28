/*
 * abget - One side available bandwidth estimation tool using normal TCP traffic
 *
 * Authors: Antonis Papadogiannakis, Demetres Antoniades, Manos Athanatos,
 *          Evagelos Markatos and Constantine Dovrolis
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <math.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#ifdef __FreeBSD__
#include <netinet/ip_fw.h>
#endif /* __FreeBSD__ */
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "inet.h"
#include "capture.h"
#include "parseconf.h"

//abget internal default parameters and constants
#define DEFAULT_TARGETPORT	(80)
#define DEFAULT_PACKETSIZE	(1460)
#define DEFAULT_RMAX		(100.0)
#define DEFAULT_RMIN		(0.0)
#define DEFAULT_ESTIMATE_RESOLUTION	(10.0)
#define DEFAULT_VARIATION_RESOLUTION	(2.0)
#define DEFAULT_MAX_TIME	(0.0)
#define STREAM_LENGTH		(51)
#define DEFAULT_GROUP_SIZE	(4)
#define DEFAULT_NUM_OF_STREAMS	(5)
#define PCT_INCR_THRESHOLD	(0.65)
#define PCT_NINCR_THRESHOLD	(0.54)
#define DEFAULT_IDLE_TIME	(10000)

#define MSS 			(1500)
#define interarrival ((double)used_size*8.0/rate)	//in microseconds
#define N (packets_examine -1)
#define GROUPS (N/group_size)

#define NOINCR	1
#define INCR	2
#define GREY	3

#define MIN_PARTITIONED_STREAM_LEN  0
#define MIN_TIME_INTERVAL           interarrival/10.0    /* microsecond */
#define MAX_TIME_INTERVAL           interarrival*10.0    /* microsecond */


//timeouts and retransmissions
#define SYNTIMEOUT		(3)	/* 3 seconds */
#define MAXSYNRETRANSMITS	(3)
#define MAXDATARETRANSMITS	(5)
#define ACKSENDERTIMEOUT        (3)     /* 3 second */
#define MAXACKSENDERRETRANSMITS (3)
#define MAXPSHRETRANSMITS       (3)
#define PSHSINGLETIMEOUT        (2)
#define PSHTIMEOUT              (1)
#define FINTIMEOUT              (2)

#define DUMMY_FILENAME	"/index.html"

FILE *writefp;

char *defaultRequest=NULL;
//char defaultRequestFormat[]="GET %s HTTP/1.0\r\n\r\n";
char defaultRequestFormat[]="GET %s HTTP/1.1\r\n\
Host: %s\r\n\
User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.7.10) Gecko/20050715 Firefox/1.0.6 SUSE/1.0.6-4.2 StumbleUpon/1.9993\r\n\
Accept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5\r\n\
Accept-Language: en-us,en;q=0.5\r\n\
Accept-Encoding: gzip,deflate\r\n\
Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n\
Keep-Alive: 300\r\n\
Connection: keep-alive\r\n\r\n";

char *extendedRequest=NULL;
char extendedRequestFormat[]="                                                                                                    GET %s HTTP/1.1\r\n                                                                                                                                                                                                        Host: %s\r\n                                                                                                                                                                                                        User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.7.10) Gecko/20050715 Firefox/1.0.6 SUSE/1.0.6-4.2 StumbleUpon/1.9993\r\n                                                                                                                                                                                                        Accept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5\r\n                                                                                                                                                                                                        Accept-Language: en-us,en;q=0.5\r\n                                                                                                                                                                                                        Accept-Encoding: gzip,deflate\r\n                                                                                                                                                                                                        Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n                                                                                                                                                                                                        Keep-Alive: 300\r\n                                                                                                                                                                                                                                                                                                                                                                                                                Connection: keep-alive                                                                                                    \r\n\r\n";


char *progName;

extern char *optarg;	/* Variables used for getopt(3) */
extern int optind;
extern int opterr;
extern int optopt;

struct TcpSession session;
int acksReceived = 0;
int dataSent = 0;
int dataSeenSent = 0;
int dataReceived = 0;
int acksSent = 0;
int dataLost = 0;
int acksLost = 0;

int verbose = 0;
int initCapture = 0;
int initFirewall = 0;
int killOldFlows = 0;
int timeOut = SYNTIMEOUT;
int pshIndex = 0;
int packetsReceived=0;

char *filename = NULL;
char *writefilename = NULL;
char *target=NULL;
static int used_window = 0;
static int used_size = 0;
char *interface;
char *conf_file=NULL;
double rate;

unsigned char *synPacket, *getPacket;


typedef double timestamp;
timestamp *times=NULL;
timestamp *interarrivals=NULL;
timestamp *owd=NULL;
timestamp *interarrivals_left=NULL;
//int *psizes;
unsigned char *push_packets=NULL;
int payloadSize=-1;
int downstream=1;
int binary_search=0;

uint16 targetPort = DEFAULT_TARGETPORT;
int packetSize = DEFAULT_PACKETSIZE;
double Rmax=DEFAULT_RMAX;
double Rmin=DEFAULT_RMIN;
double Gmax=0;
double Gmin=0;
double min;
double max;
double estimate_resolution=DEFAULT_ESTIMATE_RESOLUTION;
double variation_resolution=DEFAULT_VARIATION_RESOLUTION;
double max_time=DEFAULT_MAX_TIME;
unsigned int packets_examine=STREAM_LENGTH;
unsigned int group_size=DEFAULT_GROUP_SIZE;
unsigned int num_of_streams=DEFAULT_NUM_OF_STREAMS;
double pct_incr_threshold=PCT_INCR_THRESHOLD;
double pct_nincr_threshold=PCT_NINCR_THRESHOLD;
unsigned long idle_time=DEFAULT_IDLE_TIME; 
int optimization=1;

double *medians;
double *ordered;
unsigned int interarrivals_kept;
unsigned int increasing, non_increasing, grey;
double timeoutTime=0;

	uint32 targetIpAddress;		/* IP address of target host */
	uint32 sourceIpAddress;
	uint16 sourcePort = 0;

struct timeval before, after;
int started=0, completed=0;
int faults=0;
#define MAX_FAULTS 3

void print_results();

int Probing_downstream(struct TcpSession *s);
int Probing_upstream(struct TcpSession *s);

int find_source_ip(char* source)
    {
      int i, found=-1;
      int s = socket (PF_INET, SOCK_STREAM, 0);

      for (i=1;;i++)
        {
          struct ifreq ifr;
          struct sockaddr_in *sin = (struct sockaddr_in *) &ifr.ifr_addr;

          ifr.ifr_ifindex = i;
          if (ioctl (s, SIOCGIFNAME, &ifr) < 0)
            break;

          /* now ifr.ifr_name is set */
          if (ioctl (s, SIOCGIFADDR, &ifr) < 0)
            continue;

	  if ( strcmp(ifr.ifr_name, "lo")==0 ) continue;

	  if (interface==NULL) {
	  	interface=strdup(ifr.ifr_name);
          	strcpy(source, inet_ntoa (sin->sin_addr));
	  	printf("  Using interface %s with ip address %s\n",interface,source);
	  	found=0;
	  	break;
	  }
	  else {
		if ( strcmp(ifr.ifr_name, interface)==0 ) {
			strcpy(source, inet_ntoa (sin->sin_addr));
			printf("  Using interface %s with ip address %s\n",interface,source);
			found=0;
			break;
		}
		else continue;
	  }
        }

      close (s);
      return found;
    }

void var_init()
{
	times=(timestamp*)malloc(sizeof(timestamp)*packets_examine);
	interarrivals=(timestamp*)malloc(sizeof(timestamp)*N);
	owd=(timestamp*)malloc(sizeof(timestamp)*packets_examine);
	interarrivals_left=(timestamp*)malloc(sizeof(timestamp)*N);
	//psizes=(int*)malloc(sizeof(int)*packets_examine);
	push_packets=(unsigned char*)malloc(sizeof(unsigned char)*packets_examine);
}


void parse_conf_file(char* conf_file)
{
	//first look for configuration file in install_dir/etc/abget/abget.conf (e.g. /usr/local/etc/abget/abget.conf)
	//and if not found, in the local directory
	//except if the configuration have been explicity declared from command line options
	
	char* conf_filename;

	if (conf_file!=NULL) conf_filename=conf_file;
	else conf_filename=CONF_FILENAME;
	
	if ( pc_load(conf_filename) )
	{
		targetPort = atoi( pc_get_param(pc_get_category(""), "default_target_port") );
		packetSize = atoi( pc_get_param(pc_get_category(""), "default_packet_size") );
		Rmax = atof( pc_get_param(pc_get_category(""), "Rmax") );
		Rmin = atof( pc_get_param(pc_get_category(""), "Rmin") );
		estimate_resolution = atof( pc_get_param(pc_get_category(""), "default_estimate_resolution") );
		variation_resolution = atof( pc_get_param(pc_get_category(""), "default_variation_resolution") );
		max_time = atof( pc_get_param(pc_get_category(""), "default_max_time") );
		packets_examine = atoi( pc_get_param(pc_get_category(""), "stream_length(packets)") );
		group_size = atoi( pc_get_param(pc_get_category(""), "group_size") );
		num_of_streams = atoi( pc_get_param(pc_get_category(""), "number_of_streams") );
		idle_time = atol( pc_get_param(pc_get_category(""), "idle_time") );		
		pct_incr_threshold = atof( pc_get_param(pc_get_category(""), "pct_incr_threshold") );
		pct_nincr_threshold = atof( pc_get_param(pc_get_category(""), "pct_nincr_threshold") );

		pc_close();		
	}
	else
		fprintf(stderr, "Warning: Cannot load configuration file %s\n\n",conf_filename);

	if (conf_file!=NULL) free(conf_file);
}

#ifndef PLANETLAB

#ifdef __FreeBSD__
struct ip_fw firewallRule;
#endif /* __FreeBSD__ */
#ifndef IPTABLES
struct ip_fwchange firewallRule;
#else
/* For storing the exec string to iptables */
char ipt_add_rule[1024];
char ipt_rem_rule[1024];
#endif

#endif	//PLANETLAB


/* make a clean exit on interrupts */
void Cleanup()
{
#ifndef PLANETLAB
	 /* If a firewall rule has been installed then remove it */
#ifndef IPTABLES
  if (initFirewall > 0) {
#define IP_FW_DEL	(IP_FW_DELETE)
    if (setsockopt(session.socket,IPPROTO_IP, IP_FW_DEL,
		   &firewallRule, sizeof(firewallRule)) != 0) {
      fprintf(stderr, "ERROR: couldn't remove firewall rule\n");
    }
  }
#else
	if (initFirewall>0)
		system(ipt_rem_rule);
#endif

#endif	// PLANETLAB

	if (initCapture > 0) 
	{
		initCapture=0;	
		CaptureEnd();
	}

	if(filename)
		free(filename);
	if (writefilename)
		free(writefilename);
	if (conf_file)
		free(conf_file);
	if (interface)
		free(interface);

	if (times!=NULL) free(times);
	if (interarrivals!=NULL) free(interarrivals);
	if (owd!=NULL) free(owd);
	if (interarrivals_left) free(interarrivals_left);
	//free(psizes);
	if (push_packets!=NULL) free(push_packets);

	fflush(stdin);
	if(writefp)
		fclose(writefp);

	if (started && !completed) {
		printf("\nabget algorithm did not finished! Printing current results...\n");
		gettimeofday(&after , NULL);		
	}
	if (started) print_results();
	exit(0);
}

/*
 * Send three resets to the receiver to cleanup its state.
 */
void SendReset(struct TcpSession *s)
{
	struct TcpPacket p;
	int i;

	for (i=0;i<3;i++) {  /* 3 is totally arbitrary */
		SendSessionPacket(s, &p, 0, 0, TCPFLAGS_RST);
	}
}

void TcpBreakup(struct TcpSession *s)
{
	struct TcpPacket p;

	SendSessionPacket(s, &p, 0, 0, TCPFLAGS_ACK | TCPFLAGS_FIN);

	SendReset(s);
}

void RespondWithReset(struct TcpPacket *p) 
{
	struct TcpSession s;
	
	memset(&s, 0, sizeof(struct TcpSession));
	if(writefp)
		fprintf(writefp, "Resetting connection\n");
	else
		printf("Resetting connection\n");
	s.src = p->ip.ip_dst;
	s.dst = p->ip.ip_src;
	s.dport = ntohs(p->tcp.tcp_sport);
	s.sport = ntohs(p->tcp.tcp_dport);
	s.snd_nxt = ntohl(p->tcp.tcp_ack);
	s.rcv_nxt = ntohl(p->tcp.tcp_seq);
	SendReset(&s);
}

void PrintTimeStamp(struct timeval *ts)
{
	if(writefp)
		(void)fprintf(writefp, "%02d:%02d:%02d.%06u ",
			 (unsigned int)ts->tv_sec / 3600,
		     ((unsigned int)ts->tv_sec % 3600) / 60,
		     (unsigned int)ts->tv_sec % 60, (unsigned int)ts->tv_usec);
	else
		(void)printf("%02d:%02d:%02d.%06u ",
			 (unsigned int)ts->tv_sec / 3600,
		     ((unsigned int)ts->tv_sec % 3600) / 60,
		     (unsigned int)ts->tv_sec % 60, (unsigned int)ts->tv_usec);
}

double GetTime()
{
  struct timeval tv;
  struct timezone tz;
  double postEpochSecs;

  if (gettimeofday(&tv, &tz) < 0) {
    perror("GetTime");
    exit(-1);
  }
  
  postEpochSecs = (double)tv.tv_sec + ((double)tv.tv_usec/(double)1000000.0);
  return postEpochSecs;
}


double GetTime_usec()
{
  struct timeval tv;
  struct timezone tz;

  if (gettimeofday(&tv, &tz) < 0) {
    perror("GetTime");
    exit(-1);
  }
  
  return (double)tv.tv_sec*1000000.0+(double)tv.tv_usec;
}


/*
 * Does a packet belong to a directional flow in our session?
 */
#define INSESSION(p, src, sport, dst, dport)			\
    (((p)->ip.ip_src == (src)) && ((p)->ip.ip_dst == (dst)) &&	\
     ((p)->ip.ip_p == IPPROTOCOL_TCP) &&			\
     ((p)->tcp.tcp_sport == htons(sport)) &&			\
     ((p)->tcp.tcp_dport == htons(dport)))

void Quit()
{
	fflush(stdout);
	fflush(stderr);
	if(writefp)
		fflush(writefp);
	Cleanup();
	exit(-1);
}

int BindTcpPort(int sockfd)
{
	struct sockaddr_in  sockName;
	int port, result;
	int randomOffset;
	
	#define START_PORT (10*1024) 
	#define END_PORT   (0xFFFF)
	
	/* Choose random offset to reduce likelihood of collision with last run */
	randomOffset = (int)(1000.0*drand48());
	
	/* Try to find a free port in the range START_PORT+1..END_PORT */
	port = START_PORT+randomOffset;
	do 
	{
		++port;
		sockName.sin_addr.s_addr = INADDR_ANY;       
		sockName.sin_family = AF_INET;
		sockName.sin_port = htons(port);
		result = bind(sockfd, (struct sockaddr *)&sockName,sizeof(sockName));
	} while ((result < 0) && (port < END_PORT));
	
	if (result < 0) 
	{
		/* No free ports */
		perror("bind");
		port = 0;
	}   
	
	return port;
}


int GetCannonicalInfo(char *string, char name[MAXHOSTNAMELEN], uint32 *address)
{
	struct hostent *hp;
	
	/* Is string in dotted decimal format? */
	if ((*address = inet_addr(string)) == INADDR_NONE) 
	{
		/* No, then lookup IP address */
		if ((hp = gethostbyname(string)) == NULL) 
		{
			/* Can't find IP address */
			fprintf(stderr, "ERROR: couldn't obtain address for %s\n", string);
			return -1;
		}
		else 
		{
			strncpy(name, hp->h_name, MAXHOSTNAMELEN-1);
			memcpy((void *)address, (void *)hp->h_addr, hp->h_length);
		}
	}
	else 
	{
		if ((hp = gethostbyaddr((char *)address, sizeof(*address),AF_INET)) == NULL) 
		{
			/* Can't get cannonical hostname, so just use input string */
#ifdef DEBUG
			if (verbose) {
				fprintf(stderr, "WARNING: couldn't obtain cannonical name for %s\n", string);
			}
#endif
			strncpy(name, string, MAXHOSTNAMELEN - 1);
		}
		else 
		{
			strncpy(name, hp->h_name, MAXHOSTNAMELEN - 1);
		}
	}
	return 0;
}

/*
 * Initialize firewall, socket, etc..
 */
void Init(uint32 sourceAddress, uint16 sourcePort,uint32 targetAddress, uint16 targetPort,struct TcpSession *s)
{
	int rawSocket;
	int flag=1;
	
	/* Initialize session structure */
	memset(s, 0, sizeof(struct TcpSession));
	s->src = sourceAddress;
	s->sport = sourcePort;
	s->dst = targetAddress;
	s->dport = targetPort; 
	s->snd_wnd = 0;
	s->rcv_nxt = 0;
	s->irs = 0;
	
	/* Now open a raw socket for sending our "fake" TCP segments */
	if ((rawSocket = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) 
	{
		fprintf(stderr, "ERROR: couldn't open socket\n");
		Quit();
	}
	
	if (setsockopt(rawSocket, IPPROTO_IP, IP_HDRINCL,(char *)&flag,sizeof(flag)) < 0) 
	{
		fprintf(stderr, "ERROR: couldn't set raw socket options\n");
		Quit();
	}
	
	s->socket = rawSocket;
	
	/* 
	 * Block kernel TCP from seeing any of the target's responses.
	 * AFAIK its irrelvant that this is the same raw socket that we're
	 * sending on.
	 *
	 * I wish that everyone would get their act together and support
	 * ONE firewall API... imagine that.
	 */
#ifndef PLANETLAB

#ifndef IPTABLES
  memset(&firewallRule, 0, sizeof(firewallRule));
#endif
#ifdef __FreeBSD__
  firewallRule.fw_flg |= IP_FW_F_DENY | IP_FW_F_IN; 
  firewallRule.fw_prot = IPPROTO_TCP;
  firewallRule.fw_src.s_addr = s->dst;
  firewallRule.fw_smsk.s_addr = htonl(~0);
  firewallRule.fw_uar.fw_pts[0] = s->dport;
  IP_FW_SETNSRCP(&firewallRule, 1);
  firewallRule.fw_dst.s_addr = s->src;
  firewallRule.fw_uar.fw_pts[1] = s->sport;
  IP_FW_SETNDSTP(&firewallRule, 1);
  firewallRule.fw_dmsk.s_addr = htonl(~0);
  firewallRule.fw_number = 999;
  if (setsockopt(s->socket, IPPROTO_IP, IP_FW_ADD,
		 &firewallRule, sizeof(firewallRule)) != 0) {
    fprintf(stderr, "ERROR: couldn't block kernel TCP for %s:%d\n",
	   InetAddress(s->dst), s->dport);
  }
#endif /* __FreeBSD__ */
#ifndef IPTABLES
	memcpy(firewallRule.fwc_label, IP_FW_LABEL_INPUT,
	 sizeof(firewallRule.fwc_label));
  memcpy(firewallRule.fwc_rule.label, IP_FW_LABEL_BLOCK,
	 sizeof(firewallRule.fwc_label)); 
  firewallRule.fwc_rule.ipfw.fw_proto = IPPROTO_TCP;

  firewallRule.fwc_rule.ipfw.fw_src.s_addr = s->dst;
  firewallRule.fwc_rule.ipfw.fw_smsk.s_addr = htonl(~0); 
  firewallRule.fwc_rule.ipfw.fw_spts[0] = s->dport;
  firewallRule.fwc_rule.ipfw.fw_spts[1] = s->dport;
  firewallRule.fwc_rule.ipfw.fw_dst.s_addr = s->src;
  firewallRule.fwc_rule.ipfw.fw_dmsk.s_addr = htonl(~0); 
  firewallRule.fwc_rule.ipfw.fw_dpts[0] = s->sport;
  firewallRule.fwc_rule.ipfw.fw_dpts[1] = s->sport;

  if (setsockopt(s->socket,IPPROTO_IP,IP_FW_APPEND,
		 &firewallRule,sizeof(firewallRule)) != 0) {
    fprintf(stderr, "ERROR: couldn't block kernel TCP for %s:%d\n",
	   InetAddress(s->dst),  s->dport);
		switch(errno) {
			case EBADF: 
				fprintf(stderr,"Bad file descriptor!\n");
			break;
			case ENOTSOCK:
				fprintf(stderr, "Not a socket\n");
				break;
			case ENOPROTOOPT: 
				fprintf(stderr, "Option unknown\n");
				break;
			case EFAULT:
				fprintf(stderr, "Not valid part of address space\n");
				break;
			default: 
				fprintf(stderr, "Errno=%d\n", errno);
				break;
		}
		fflush(stderr);
    Quit();
	}
#else
	if (initFirewall==0)
	{	
		struct in_addr S,D;
		char src[40],dst[40];
		S.s_addr = s->src;
		D.s_addr = s->dst;
		memmove(src,inet_ntoa(S),40);
		memmove(dst,inet_ntoa(D),40);

		snprintf(ipt_add_rule,sizeof(ipt_add_rule) - 1, 
				"%s -I INPUT -p tcp -s %s -d %s --dport %d --sport %d -j DROP\n",
				IPTABLES, dst, src, 
				s->sport, s->dport);
		snprintf(ipt_rem_rule,sizeof(ipt_rem_rule) - 1, 
				"%s -D INPUT -p tcp -s %s -d %s --dport %d --sport %d -j DROP\n",
				IPTABLES, dst, src, 
				s->sport, s->dport);
		if (verbose) {
			if(writefp)
				fprintf(writefp, "applying rule %s\n", ipt_add_rule);
			else 
				printf("applying rule %s\n", ipt_add_rule);
		}

		system(ipt_add_rule);
	}
#endif /* USE_IPTABLES */
  initFirewall=1;

#endif	//PLANETLAB

}

/*
 * This function checks whether the packet has the Reset or FIN bit on, 
 * and if so, prints a message, terminate a connection and return -1.
 * When a packet a received the caller should check whether the packet
 * contains useful information (it's a PSH for example) and after it's done
 * call this function to check RST and FIN in the packet
 */
int CheckResetOrFin(struct TcpPacket *p, char *msg)
{

	/* Is it a RST?  i.e., please stop */
	if (p->tcp.tcp_flags & TCPFLAGS_RST) 
	{
		if (verbose) {
			if(writefp)
				fprintf(writefp, "WARNING: Reset encountered during %s\n", msg);
			else
				fprintf(stderr, "WARNING: Reset encountered during %s\n", msg);
		}
		//Quit();
		return -1;
	}
	/* We can also get FINs, especially if we confused the other stack
	 * in a previous run? This is rare but it can happen */
	else if (p->tcp.tcp_flags & TCPFLAGS_FIN) 
	{ 
		if (verbose) { 
			if(writefp)
				fprintf(writefp, "WARNING: FINs from the other host during %s\n", msg);
			else
				fprintf(stderr, "WARNING: FINs from the other host during %s\n", msg);
		}
		//Quit();
		return -1;
	}
	/* No? then its something weird */
	else 
	{
		if (verbose) {
			if(writefp)
				fprintf(writefp, "WARNING: unexpected packet during %s\n", msg);
			else 
				fprintf(stderr, "WARNING: unexpected packet during %s\n", msg);
		}
		if (verbose) 
			PrintTcpPacket(p);
	}

	return 1;
}

void ConstructSynPacket()
{
	short mss = 1460;
	int optoffset;

	/* Initialize packet to TCP/IP header + MSS option */
	if((synPacket = (unsigned char*)malloc(sizeof(struct TcpPacket) + 8)) == NULL) 
	{
		fprintf(stderr, "ERROR: out of memory\n");
		Quit();
	}

	memset(synPacket, 0, sizeof(struct TcpPacket) +8);

	/* Construct a MSS of 1460 */
	optoffset = sizeof(struct TcpPacket);
	synPacket[optoffset++] = 2;
	synPacket[optoffset++] = 4;
	mss = htons(mss);
	memcpy(&(synPacket[optoffset]), &mss, 2);
	optoffset += 2;
	synPacket[optoffset++] = 1;
	synPacket[optoffset++] = 3;
	synPacket[optoffset++] = 3;
	synPacket[optoffset] = 0;
}



/*
 * This function does the Tcp handshake - SYN, SYN/ACK, ACK.
 * It takes a pointer to the TcpSession, some memory allocated for the packet
 * and the lengths of the options and data fragments in the packet being sent.
 * Here, datalen should be 0 mostly.
 */

int TcpHandshake(struct TcpSession *s, struct TcpPacket *synPacket, int optlen,int datalen)
{
	struct TcpPacket *p;
	struct PacketInfo pi;
	double timeoutTime;
	int synAckReceived = 0;
	int numRetransmits = 0;
	
	/* Send the SYN - first step in the TCP connection */
	SendSessionPacket(s, synPacket, optlen, datalen, TCPFLAGS_SYN); 
	
	/* 
	 * Wait for SYN/ACK and retransmit SYN if appropriate 
	 * Kinda crappy, but it gets the job done 
	 */
	timeoutTime = GetTime() + SYNTIMEOUT;
	while(!synAckReceived && numRetransmits < MAXSYNRETRANSMITS) 
	{
		while(GetTime() < timeoutTime) 
		{
			/* Have we captured any packets? */
			if ((p = (struct TcpPacket *)CaptureGetPacket(&pi)) == NULL) 
			{
				continue;
			}

			/* We check if a packet from them to us */
			if (INSESSION(p, s->dst, s->dport, s->src, s->sport)) 
			{
				/* Is it a SYN/ACK? */
				if ( (p->tcp.tcp_flags == (TCPFLAGS_SYN | TCPFLAGS_ACK)) || (p->tcp.tcp_flags == (TCPFLAGS_ACK)) )
				{
					synAckReceived = 1;
					break;
				}
				/* Is it a RST?  i.e., please stop */
				else {
					if (CheckResetOrFin(p, "in tcp handshake") < 0) return -1;
				}
			}
			else 
			{
#ifdef DEBUG
				if (verbose) { 
					if(writefp)
						fprintf(writefp, "WARNING: unexpected packet during session establishment\n");
					else
						fprintf(stderr, "WARNING: unexpected packet during session establishment\n");
				}
				if (verbose) 
					PrintTcpPacket(p);
#endif
				if (killOldFlows) 
				{
					/* If it was sent to us and wasn't a RST then tell them to stop */
					if (p->ip.ip_dst == s->src && !(p->tcp.tcp_flags & TCPFLAGS_RST)) 
					{
						RespondWithReset(p);
					}
				}
			}
		}
		if (!synAckReceived) 
		{
			if (verbose) {
				if(writefp)
					fprintf(writefp, "SYN timeout.  Retransmitting\n");
				else
					printf("SYN timeout.  Retransmitting\n");
			}
			SendSessionPacket(s, synPacket, optlen, datalen, TCPFLAGS_SYN);
			timeoutTime = GetTime() + SYNTIMEOUT;
			numRetransmits++;	
		}
	} /* while */

	if (numRetransmits >= MAXSYNRETRANSMITS) {
		if(writefp)
			fprintf(writefp, "\nCould not establish connection after %d retries\n",numRetransmits);
		else
			printf("\nCould not establish connection after %d retries\n",numRetransmits);
		Quit();
	}

	/* Update session variables */
	s->irs = ntohl(p->tcp.tcp_seq); /* receiver sequence number */
	s->rcv_nxt = s->irs+1;  /* SYN/ACK takes up a byte of seq space */
	s->snd_nxt = s->iss+1;  /* SYN takes up a byte of seq space */
	s->snd_una = s->iss+1;  
	
	s->iss = s->snd_nxt;
	
	/* Send an ack */
	SendSessionPacket(s, synPacket, optlen, datalen, TCPFLAGS_ACK);

	if (verbose) {
		if(writefp) {
			fprintf(writefp, "TCP Handshake completed successfully\n");
			fprintf(writefp, "src = %s:%d (%u)\n", InetAddress(s->src),s->sport, s->iss);
			fprintf(writefp, "dst = %s:%d (%u)\n",InetAddress(s->dst),s->dport, s->irs);
		}
		else {
			printf("TCP Handshake completed successfully\n");
			printf("src = %s:%d (%u)\n", InetAddress(s->src),s->sport, s->iss);
			printf("dst = %s:%d (%u)\n",InetAddress(s->dst),s->dport, s->irs);
		}
	}
	return 1;
}


/*
 * This function, construct a properly formatted Get request to be sent to the
 * server to which we want to measure bb to.
 */
int ConstructWWWGetRequest(char* target)
{
	int payloadSize=-1;
	char *file_request;

	if (filename[0]!='/') {
		file_request=(char*)malloc(strlen(filename)+2);
		strcpy(file_request,"/");
		strcat(file_request, filename);
	}
	else file_request=filename;

	/* Get some space for the get request */
	defaultRequest = (char*)malloc(strlen(defaultRequestFormat) + strlen(target) + strlen(file_request) );
	if(NULL == defaultRequest) 
	{
		fprintf(stderr, "Error: Out of memory\n");
		exit(-1);
	}

	sprintf(defaultRequest, defaultRequestFormat, file_request, target);

        if((getPacket = (unsigned char*)malloc(sizeof(struct TcpPacket) + strlen(defaultRequest))) == NULL)
        {
                fprintf(stderr, "ERROR: Out of memory: %d\n", (int)getPacket);
                Quit();
        }

        memset(getPacket, 0, sizeof(struct TcpPacket));

	/* append the payload to the end of the packet */
 	memcpy(getPacket+sizeof(struct TcpPacket), defaultRequest, strlen(defaultRequest));
	payloadSize = strlen(defaultRequest);

	free(defaultRequest);
	if (file_request!=filename) free(file_request);
	return payloadSize;
}


void ConstructExtendedRequest(char* target)
{
	extendedRequest = (char*)malloc(strlen(extendedRequestFormat) + strlen(target) + strlen(filename) );
	if(NULL == extendedRequest) 
	{
		fprintf(stderr, "Error: Out of memory\n");
		exit(-1);
	}

	sprintf(extendedRequest, extendedRequestFormat, filename, target);

	if((getPacket = (unsigned char*)malloc(sizeof(struct TcpPacket) + packetSize)) == NULL)
	{
		fprintf(stderr, "ERROR: Out of memory: %d\n", (int)getPacket);
		Quit();
	}

	memset(getPacket, 0, sizeof(struct TcpPacket));
}


int GetRequest(struct TcpSession *s, struct TcpPacket* getPacket, int payloadSize)
{
	struct TcpPacket *p;
	struct PacketInfo pi;
	int ackReceived = 0;
	int numRetransmits = 0;
	int iphdrlen, tcphdrlen, iplen;
	double timeoutTime;

	/* Send out get request. */
	if (verbose) {
		if(writefp)
			fprintf(writefp, "Sending GET request\n");
		else
			printf("Sending GET request\n");
	}
	SendSessionPacket(s, getPacket, 0, payloadSize,TCPFLAGS_ACK | TCPFLAGS_PSH);
	timeoutTime = GetTime() + PSHSINGLETIMEOUT;

	while(0 == ackReceived && numRetransmits < MAXPSHRETRANSMITS) 
	{
		while(GetTime() < timeoutTime) 
		{
			/* Have we captured any packets? */
			if ((p = (struct TcpPacket *)CaptureGetPacket(&pi)) == NULL) 
			{
				continue;
			}

			/* We check if a packet from them to us */
			if (INSESSION(p, s->dst, s->dport, s->src, s->sport)) 
			{
				/* Is it an ACK? */
				if (p->tcp.tcp_flags & TCPFLAGS_ACK) 
				{
					ackReceived = 1;

					/* Compute the lengths of the packet headers */
					iplen = ntohs(p->ip.ip_len);
					iphdrlen = (0x0f & p->ip.ip_vhl) << 2;
					tcphdrlen = p->tcp.tcp_hl >> 2;
					
					/* Check whether any data received in this packet too */
					if((p->tcp.tcp_flags & TCPFLAGS_PSH) ||(iplen > iphdrlen + tcphdrlen)) 
					{
						/* While we're here, we received the PSH, let's reflect that */
						s->irs = ntohl(p->tcp.tcp_seq);
						s->rcv_nxt += iplen - tcphdrlen - iphdrlen;
						s->snd_una = ntohl(p->tcp.tcp_ack); // unack byte is the same as the party's ack 
						s->snd_nxt = s->snd_una;
						used_window = iplen - tcphdrlen - iphdrlen;
						used_size = iplen;
						s->snd_wnd=used_window;
						SendSessionPacket(s, getPacket, 0, 0, TCPFLAGS_ACK);
					}
					break;
				}
				/* Is it a RST?  i.e., please stop */
				else {
					if ( CheckResetOrFin(p, "waiting for GET reply") < 0 ) return -1;
				}
			}
			else 
			{
#ifdef DEBUG
				if (verbose) { 
					if(writefp)
						fprintf(writefp, "WARNING: unexpected weird packet during Head Request\n");
					else
						printf("WARNING: unexpected weird packet during Head Request\n");
				}
				if (verbose)
					PrintTcpPacket(p);
#endif
				continue;
			}
		}
		if(!ackReceived) 
		{
			if (verbose) {
				if(writefp)
					fprintf(writefp, "PSH timeout (Get Request).  Retransmitting\n");
				else
					printf("PSH timeout (Get Request).  Retransmitting\n");
			}
			/* Resend out head request. */
			SendSessionPacket(s, getPacket, 0, payloadSize,TCPFLAGS_ACK | TCPFLAGS_PSH);
			timeoutTime = GetTime() + PSHSINGLETIMEOUT;
			numRetransmits++;
		}
	} /*   while(numRetransmits < MAXPSHRETRANSMITS) */

	if (ackReceived==1) {
		s->irs = ntohl(p->tcp.tcp_seq);
		s->snd_nxt = s->iss + payloadSize;
		s->snd_una = ntohl(p->tcp.tcp_ack); // unack byte is the same as the party's ack 
		//s->snd_nxt = s->snd_una;
		s->iss = s->snd_nxt;
		return 1;
	}
	else return -1;
}


void set_ack_options(struct TcpSession *s)
{
	s->rcv_nxt+=s->snd_wnd;
#ifdef DEBUG	
	if (verbose) { 
		if(writefp)
			fprintf(writefp, "ACK number %u\n",s->rcv_nxt);
		else
			printf("ACK number %u\n",s->rcv_nxt);
	}
#endif

}


void send_fake_acks(struct TcpSession *s, struct TcpPacket* buff, unsigned int num)
{
  unsigned int i;
  double time;
  double prevTime=GetTime_usec();

  for (i=0; i<num;)
    {
	//send ACK's periodically
	if ((time=GetTime_usec())>=prevTime+interarrival)
	{
		set_ack_options(s);
		SendSessionPacket(s, buff, 0, 0, TCPFLAGS_ACK);
#ifdef DEBUG
		if (verbose) { 
			if(writefp)
				fprintf(writefp, "ACK's time %lf\n",time);
			else
				printf("ACK's time %lf\n",time);
		}
#endif

		i++;
		prevTime=time;
	}

    }
}


int Probing_downstream(struct TcpSession *s)
{
	struct TcpPacket *p; 
	struct TcpPacket buff;
	struct PacketInfo pi;
	int iphdrlen, tcphdrlen, iplen;
	double timeoutTime;
	unsigned int pIndex=0;
	uint32 latest_seq=0;
	unsigned int i;

	memset(&buff, 0, sizeof(struct TcpPacket));

	// reading for first packet in order to set the advertize window
	// and also get sure that file exists
	if(used_window == 0)
	{
		timeoutTime = GetTime() + PSHSINGLETIMEOUT;
		while(used_window == 0 && GetTime() < timeoutTime)
		{
			if((p = (struct TcpPacket *)CaptureGetPacket(&pi)) == NULL)
				continue;

			if(INSESSION(p,s->dst, s->dport,s->src,s->sport))
			{
				/* Compute the lengths of the packet headers */
				iplen = ntohs(p->ip.ip_len);
				iphdrlen = (0x0f & p->ip.ip_vhl) << 2;
				tcphdrlen = p->tcp.tcp_hl >> 2;
#ifdef DEBUG
				if (verbose) {
					if(writefp) {
						printf("packet for used win -----\n");
						PrintTcpPacket(p);
						printf("-------------------------\n");
					}
					else {
						printf("packet for used win -----\n");
						PrintTcpPacket(p);
						printf("-------------------------\n");
					}
				}
#endif

				// chech if file exits in the web server
				if ( strncmp(((char*)p)+sizeof(struct IpHeader)+sizeof(struct TcpHeader)+9, "404 Not Found",13)==0 ) {
					printf("\nFile %s does not exist in server %s\n",filename,target);
					started=0;
					Cleanup();	//exiting....
				}

				s->irs = ntohl(p->tcp.tcp_seq);
				s->snd_una = ntohl(p->tcp.tcp_ack); // unack byte is the same as  the party's ack
				s->snd_nxt = s->snd_una;

				used_window = iplen - tcphdrlen - iphdrlen;
				if (used_window>=730) {
					latest_seq=s->irs;
					used_size = iplen;
					s->snd_wnd=used_window;		//Set the advertize window
					//set_ack_options(s);
					//SendSessionPacket(s, &buff, 0, 0, TCPFLAGS_ACK);
				}
				else if (used_window!=0) {
					s->rcv_nxt+=used_window;
					SendSessionPacket(s, &buff, 0, 0, TCPFLAGS_ACK);
					used_window=0;
				}
			}
		}
	}


	if (verbose) {
		if(writefp)
			fprintf(writefp, "used_widow %d\n", used_window);
		else
			printf("used_widow %d\n", used_window);
	}
	if (used_window==0) {
		fprintf(stderr, "Cannot determine the Maximum Segment Size\n");
		return -1;
	}

	if (verbose) { 
		if(writefp)
			fprintf(writefp, "Starting to send fake ACK'S periodically with rate %lf and interarrival %lf\n\n",rate,interarrival);
		else
			printf("Starting to send fake ACK'S periodically with rate %lf and interarrival %lf\n\n",rate,interarrival);
	}

	for (i=0; i<packets_examine; i++)
		push_packets[i]=0;

	pIndex=0;
	packetsReceived=0;

	//usleep(idle_time/2);

	send_fake_acks(s, &buff, packets_examine);

	if (verbose) { 
		if(writefp)
			fprintf(writefp, "ACKs sent, now waiting for %d packets to receive\n",packets_examine);
		else 
			printf("ACKs sent, now waiting for %d packets to receive\n",packets_examine);
	}

	timeoutTime = GetTime() + PSHTIMEOUT*(packets_examine/10);
	while(pIndex < packets_examine-1 && GetTime() < timeoutTime) 
	{
		/* Have we captured any packets? */
		if ((p = (struct TcpPacket *)CaptureGetPacket(&pi)) == NULL) 
		{
			continue;
		}

		/* We check if a packet from them to us */
		if (INSESSION(p, s->dst, s->dport, s->src, s->sport)) 
		{
			/* Compute the lengths of the packet headers */
			iplen = ntohs(p->ip.ip_len);
			iphdrlen = (0x0f & p->ip.ip_vhl) << 2;
			tcphdrlen = p->tcp.tcp_hl >> 2;	  	
			
			/* Is it a PSH */
			if (p->tcp.tcp_flags == (TCPFLAGS_PSH | TCPFLAGS_ACK) ||(p->tcp.tcp_flags == TCPFLAGS_ACK && iplen >tcphdrlen + iphdrlen)) 
			{
				/* While we're here, we received the PSH, let's reflect that */
				s->irs = ntohl(p->tcp.tcp_seq);
				s->snd_una = ntohl(p->tcp.tcp_ack); // unack byte is the same as the party's ack
				s->snd_nxt = s->snd_una;
#ifdef DEBUG
				if (verbose) {
					if(writefp)
						fprintf(writefp, "Received %d packet with seq %u\n",pIndex,s->irs);
					else
						printf("Received %d packet with seq %u\n",pIndex,s->irs);
				}
				//if (verbose) PrintTcpPacket(p);
#endif

				if (iplen - tcphdrlen - iphdrlen != used_window)
				  {
#ifdef DEBUG
					if (verbose) { 
						if(writefp)
							fprintf(writefp, "Invalid window %d vs ours %d\n",iplen - tcphdrlen - iphdrlen,used_window);
						else
							printf("Invalid window %d vs ours %d\n",iplen - tcphdrlen - iphdrlen,used_window);
					}
#endif
					if (iplen - tcphdrlen - iphdrlen > used_window) {
						used_window = iplen - tcphdrlen - iphdrlen;
						used_size = iplen;
						s->snd_wnd=used_window;		//re-set the advertize window

						if (verbose) { 
							if(writefp)
								fprintf(writefp, "Used window updated to %d\n",used_window);
							else
								printf("Used window updated to %d\n",used_window);
						}

					}
					s->rcv_nxt=s->irs+(iplen-tcphdrlen-iphdrlen)-used_window;
					push_packets[pIndex]=1;
#ifdef DEBUG
					if (verbose) { 
						if(writefp)
							fprintf(writefp, "re-send remaing %d acks\n",packets_examine-pIndex-1);
						else
							printf("re-send remaing %d acks\n",packets_examine-pIndex-1);
					}
#endif

					send_fake_acks(s, &buff, packets_examine-pIndex-1);
				  }
/**/
				else if (s->irs!=latest_seq+used_window && latest_seq!=0)
				  {
#ifdef DEBUG
					if (verbose) { 
						if(writefp)
							fprintf(writefp, "Invalid seq number %u while waiting for %u\n",s->irs,latest_seq+used_window);
						else
							printf("Invalid seq number %u while waiting for %u\n",s->irs,latest_seq+used_window);
					}
#endif

					//used_window = iplen - tcphdrlen - iphdrlen;	
					//used_size = iplen;
					//s->snd_wnd=used_window;		//re-set the advertize window
					s->rcv_nxt=s->irs+(iplen-tcphdrlen-iphdrlen)-used_window;
					push_packets[pIndex]=1;
#ifdef DEBUG
					if (verbose) { 
						if(writefp)
							fprintf(writefp, "re-send remaing %d acks\n",packets_examine-pIndex-1);
						else
							printf("re-send remaing %d acks\n",packets_examine-pIndex-1);
					}
#endif

					send_fake_acks(s, &buff, packets_examine-pIndex-1);
				  }
/**/
				latest_seq=s->irs;

				/*s->snd_una = ntohl(p->tcp.tcp_ack); // unack byte is the same as 
				// the party's ack 
				s->snd_nxt = s->snd_una;*/

//				psizes[pIndex] = iplen;

				//keep time to compute delays
				times[pIndex]=(double)pi.ts.tv_sec*1000000.0+(double)pi.ts.tv_usec;

				pIndex++;
				packetsReceived++;
			}
			else if(p->tcp.tcp_flags == (TCPFLAGS_FIN | TCPFLAGS_ACK)) 
			{
#ifdef DEBUG
				if (verbose) {
					if(writefp)
						fprintf(writefp, "FIN or ACK\n");
					else
						printf("FIN or ACK\n");
				}
				if (verbose) 
					PrintTcpPacket(p);
#endif
				/* Update session variables */
				s->irs = ntohl(p->tcp.tcp_seq); /* receiver sequence number */
				//
				//return -1;
			}
			
			/* Is it a RST?  i.e., please stop */
			else {	
#ifdef DEBUG
				if (verbose) { 
					if(writefp)
						fprintf(writefp, "RST or something\n");
					else
						printf("RST or something\n");
				}
				if (verbose) 
					PrintTcpPacket(p);
#endif
				if ( CheckResetOrFin(p, "waiting for HTTP data") < 0 ) return -1;
			} 
		}
		else 
		{
#ifdef DEBUG
			if (verbose) { 
				if(writefp)
					fprintf(writefp, "WARNING: unexpected weird packet during GET Request\n");
				else
					printf("WARNING: unexpected weird packet during GET Request\n");
			}
			if (verbose) 
				PrintTcpPacket(p);
#endif
		}
		timeoutTime = GetTime() + PSHTIMEOUT*(packets_examine/10);
	} /* while */

	return 1;
}


void send_request_segments(struct TcpSession *s, unsigned int num)
{
  unsigned int i;
  double time;
  double prevTime=GetTime_usec();

  //Start sending with the second segment to activate fast retransmit algorithm
  s->snd_nxt +=1;
  s->iss = s->snd_nxt;
  if (faults>MAX_FAULTS) s->rcv_nxt=0;


  for (i=0; i<num;)
    {
	//send data segments periodically
	if ((time=GetTime_usec())>=prevTime+interarrival)
	{
		/* append the payload of the next overlapping segment to the end of the packet */
		memcpy(getPacket+sizeof(struct TcpPacket), extendedRequest+i+1, packetSize);

		SendSessionPacket(s, (struct TcpPacket*)getPacket, 0, packetSize, TCPFLAGS_ACK | TCPFLAGS_PSH);
#ifdef DEBUG
		if (verbose) { 
			if(writefp){
				fprintf(writefp, "Sequence number: %u\n",s->iss);
				fprintf(writefp, "Data segment's time %lf\n",time);
			}
			else {
				printf("Sequence number: %u\n",s->iss);
				printf("Data segment's time %lf\n",time);
			}
		}
#endif
		s->snd_nxt += 1;
		s->iss = s->snd_nxt;
		i++;
		prevTime=time;
		if (faults>MAX_FAULTS) s->rcv_nxt+=1;
	}

    }
}


int Probing_upstream(struct TcpSession *s)
{
	struct TcpPacket *p;
	struct PacketInfo pi;
	double timeoutTime;
	unsigned int pIndex=0;
	uint32 latest_ack=0;

	if (verbose) { 
		if(writefp)
			fprintf(writefp, "Starting to send overlapping request data segments periodically with rate %lf and interarrival %lf\n\n",rate,interarrival);
		else
			printf("Starting to send overlapping request data segments periodically with rate %lf and interarrival %lf\n\n",rate,interarrival);
	}

	pIndex=0;
	packetsReceived=0;

	//usleep(idle_time/2);

	send_request_segments(s, packets_examine);

	if (verbose) { 
		if(writefp)
			fprintf(writefp, "Data segments sent, now waiting for %d ACKs to receive\n",packets_examine);
		else 
			printf("Data segments sent, now waiting for %d ACKs to receive\n",packets_examine);
	}

	timeoutTime = GetTime() + PSHTIMEOUT*(packets_examine/10);
	while(pIndex < packets_examine-1 && GetTime() < timeoutTime) 
	{
		/* Have we captured any packets? */
		if ((p = (struct TcpPacket *)CaptureGetPacket(&pi)) == NULL) 
		{
			continue;
		}

		/* We check if a packet from them to us */
		if (INSESSION(p, s->dst, s->dport, s->src, s->sport)) 
		  {
			if (faults<=MAX_FAULTS) {
				/* Is it a ACK */
				if (p->tcp.tcp_flags & TCPFLAGS_ACK) 
				{
#ifdef DEBUG
					if (verbose) { 
						if(writefp)
							fprintf(writefp, "ACK %d\n",pIndex);
						else
							printf("ACK %d\n",pIndex);
					}
#endif
					s->irs = ntohl(p->tcp.tcp_seq);
					s->snd_una = ntohl(p->tcp.tcp_ack);
					s->snd_nxt = s->snd_una;					
#ifdef DEBUG
						if (verbose) {
							if(writefp)
								fprintf(writefp, "Ack number received: %d\n",s->snd_una);
							else
								printf("Ack number received: %d\n",s->snd_una);
						}
#endif
					latest_ack=s->snd_una;
					//if (verbose) PrintTcpPacket(p);
				
					//keep time to compute delays
					times[pIndex]=(double)pi.ts.tv_sec*1000000.0+(double)pi.ts.tv_usec;

					pIndex++;
					packetsReceived++;
				}
				/* Is it a RST?  i.e., please stop */
				else {
#ifdef DEBUG
					if (verbose) { 
						if(writefp)
							fprintf(writefp, "Not an ACK packet\n");
						else
							printf("Not an ACK packet\n");
					}
					if (verbose) 
						PrintTcpPacket(p);
#endif
					s->irs = ntohl(p->tcp.tcp_seq); /* receiver sequence number */
					if ( CheckResetOrFin(p, "waiting for ACKs for HTTP data segments") < 0 ) return -1;
				}
			}
			else {
				/* Is it a RST */
				if (p->tcp.tcp_flags & TCPFLAGS_RST) 
				{
#ifdef DEBUG
					if (verbose) { 
						if(writefp)
							fprintf(writefp, "RST %d\n",pIndex);
						else
							printf("RST %d\n",pIndex);
					}
#endif
					s->irs = ntohl(p->tcp.tcp_seq);
#ifdef DEBUG
						if (verbose) {
							if(writefp)
								fprintf(writefp, "RST sequence number received: %d\n",s->irs);
							else
								printf("RST sequence number received: %d\n",s->irs);
						}
#endif
					//if (verbose) PrintTcpPacket(p);
				
					//keep time to compute delays
					times[pIndex]=(double)pi.ts.tv_sec*1000000.0+(double)pi.ts.tv_usec;

					pIndex++;
					packetsReceived++;
				}
				/* Is it what ?  i.e., please stop */
				else {
#ifdef DEBUG
					if (verbose) { 
						if(writefp)
							fprintf(writefp, "Not an RST packet\n");
						else
							printf("Not an RST packet\n");
					}
					if (verbose) 
						PrintTcpPacket(p);
#endif
					s->irs = ntohl(p->tcp.tcp_seq); /* receiver sequence number */
				}

			}
		  }
		else 
		{
#ifdef DEBUG
			if (verbose) { 
				if(writefp)
					fprintf(writefp, "WARNING: unexpected weird packet during GET Request segments\n");
				else
					printf("WARNING: unexpected weird packet during GET Request segments\n");
			}
			if (verbose)
				PrintTcpPacket(p);
#endif
			continue;
		}
		timeoutTime = GetTime() + PSHTIMEOUT*(packets_examine/10);
	} /* while */

	return 1;
}


void Usage() 
{
  printf("Usage:\n\t%s",progName);
  printf("\t[-d | --downstream]\n\t\t[-u | --upstream]\n\t\t[-b | --binary]\n\t\t[-l | --linear]\n");
  printf("\t\t[-p <target-port>]\n\t\t[-w <source-port>]\n\t\t[-s <source-ip>]\n\t\t[-i <network-interface>]\n");
  printf("\t\t[-r <estimation-range>] (Mbps)\n\t\t[-x <variation-range>] (Mbps)\n\t\t[-t <max-time>] (seconds)\n");
  printf("\t\t[-L <Rmin>] (Mbps)\n\t\t[-H <Rmax>] (Mbps)\n\t\t[-P <stream-length>] (packets)\n\t\t[-N <number-of-streams>]\n\t\t[-I <idle time>] (microseconds)\n");
  printf("\t\t[-D <writefile>]\n");
  printf("\t\t[-c <configuration file>]\n");  
  printf("\t\t[-v | --verbose]\n\t\t[-h | --help]\n");
  printf("\t\t[-f <filepath>]\n");
  printf("\t\t<host>\n");
  Quit();
}

void print_results()
{
	printf("\n");

	if (min>max) 
		if(writefp)
			fprintf(writefp, "Non-stationary traffic (Available bandwidth estimated: %lf - %lf Mbps)\n",min,max);
		else
			printf("Non-stationary traffic (Available bandwidth estimated: %lf - %lf Mbps)\n",min,max);
	else if (min==max && min==Rmin) 
		if(writefp)
			fprintf(writefp, "Available bandwidth estimated: less than %lf Mbps\n",min);
		else
			printf("Available bandwidth estimated: less than %lf Mbps\n",min);
	else if (min==max && max==Rmax) 
		if(writefp)
			fprintf(writefp, "Available bandwidth estimated: more than %lf Mbps\n",max);
		else
			printf("Available bandwidth estimated: more than %lf Mbps\n",max);
	else
		if(writefp)
			fprintf(writefp, "Available bandwidth estimated: %lf - %lf Mbps\n",min, max);	
		else
			printf("Available bandwidth estimated: %lf - %lf Mbps\n",min, max);	

	if(writefp)
		fprintf(writefp, "Execution Time %lf seconds\n",(double)(after.tv_sec-before.tv_sec)+((double)(after.tv_usec-before.tv_usec)/1000000.0));
	else
		printf("Execution Time %lf seconds\n",(double)(after.tv_sec-before.tv_sec)+((double)(after.tv_usec-before.tv_usec)/1000000.0));

	if(writefp)
		fprintf(writefp, "=========================================\n");
	else
		printf("=========================================\n");
}



/*
    PCT test to detect increasing trend in stream
*/
double pairwise_comparision_test (double array[], unsigned int start , unsigned int end)
{
  unsigned int improvement = 0 ,i ;
  double total ;

  //if ( ( end - start  ) >= MIN_PARTITIONED_STREAM_LEN )
  //{
    for ( i = start ; i < end - 1   ; i++ )
    {
      if ( array[i] < array[i+1] )
        improvement += 1 ;
    }
    total = ( end - start - 1 ) ;
//fprintf(stdout,"pct %d %lf\n",improvement,total);
    return ( (double)improvement/(double)total ) ;
  //}
  //else
  //  return -1 ;
}

/*
    PDT test to detect increasing trend in stream
*/
double pairwise_diff_test(double array[] , unsigned int start , unsigned int end)
{
  double y_abs = 0 ;
  unsigned int i ;
  //if ( ( end - start  ) >= MIN_PARTITIONED_STREAM_LEN )
  //{
    for ( i = start+1 ; i < end    ; i++ )
    {
      y_abs += fabs(array[i] - array[i-1]) ;
    }
    return (array[end-1]-array[start])/y_abs ;
  //}
  //else
  //  return 2.0 ;
}


/* 
  Order an array of doubles using bubblesort 
*/
void order_dbl(double unord_arr[], double ord_arr[], unsigned int start, unsigned int num_elems)
{
  int i,j,k;
  double temp;
  for (i=(int)start,k=0;i<(int)start+(int)num_elems;i++,k++) ord_arr[k]=unord_arr[i];
  for (i=1;i<(int)num_elems;i++) 
  {
    for (j=i-1;j>=0;j--)
      if (ord_arr[j+1] < ord_arr[j]) 
      {
        temp=ord_arr[j]; 
        ord_arr[j]=ord_arr[j+1]; 
        ord_arr[j+1]=temp;
      }
      else break;
  }
}

/*
int remove_interarrivals_after_push(timestamp* old, timestamp* new, int length)
{
	int i,j;

	//remove interarrivals after a push packet
	for (i=0, j=0; i<length; i++)
		if (push_packets[i]==0) new[j++]=old[i];

	return j;
}

int remove_outliers(timestamp* old, timestamp* new, int length)
{
	int i,j;

	for (i=0, j=0; i<length; i++)
		if (old[i]>=MIN_TIME_INTERVAL && old[i]<=MAX_TIME_INTERVAL) new[j++]=old[i];
		//if (old[i]>0) new[j++]=old[i];

	return j;
}
*/

int remove_outliers(timestamp* old, timestamp* new, int length)
{
	int i,j;

	for (i=0, j=0; i<length; i++)
		if (old[i]>=MIN_TIME_INTERVAL && old[i]<=MAX_TIME_INTERVAL && push_packets[i]==0) new[j++]=old[i];
		//if (old[i]>0) new[j++]=old[i];

	return j;
}



void compute_owd(timestamp* interarrivals, timestamp* owd, int interarrivals_num)
{
	int i;
	
	owd[0]=0;

	for (i=1; i<interarrivals_num+1; i++) 
	  {
		owd[i] = owd[i-1] + interarrivals[i-1] - interarrival;
		//if (verbose) printf("owd %d: %lf\n",i, owd[i]);
	  }

}

double compute_fraction(timestamp* interarrivals, int boundary, int total)
{
	double f=0;
	int i;

	for (i=0; i<total; i++)
		if (interarrivals[i]>boundary) f++;

	return f/(double)total;
}


void abget_iteration_downstream()
{
	double pct;
	int i;
	unsigned int stream=0;
	increasing=0;
	non_increasing=0;
	grey=0;

	if(writefp)
		fprintf(writefp, "Rate: %lf Mbits/sec\n",rate);
	else {
		if(verbose) 
			printf("Rate=%lf Mbps: ",rate);
		else {
			printf("Rate=%lf Mbps: ",rate);
			fflush(stdout);
		}
	}

	while (stream++<num_of_streams && (max_time==0 || GetTime()<timeoutTime) )
	  {
		if (verbose) { 
			if(writefp)
				fprintf(writefp, "Stream %d\n",stream);
			else
				printf("Stream %d\n",stream);
		}

		// Init packet capture device and install filter for our flow
		if (initCapture==0)
		{
			CaptureInit(sourceIpAddress, sourcePort, targetIpAddress, targetPort);
			initCapture = 1;
		}

		/* Setup connection to target */
		session.snd_wnd = packetSize*10;//packetSize;
		session.iss=1;
		session.snd_nxt=1;
		session.rcv_nxt=0;
		if ( TcpHandshake(&session, (struct TcpPacket*)synPacket, 8, 0) < 0 ) /* do the handshake */
		  {
		  	if (verbose) printf("Error encoutered in TCP Handshake emulation\n");
			TcpBreakup(&session);
			stream--;
			usleep(idle_time/2);
			if (initCapture==1)  {
				initCapture=0;			
				CaptureEnd();
			}
			continue;
		  }

		if ( GetRequest(&session, (struct TcpPacket*)getPacket, payloadSize) < 0 )
		  {
		  	if (verbose) printf("Error encoutered while waiting for responce in HTTP request\n");
			TcpBreakup(&session);
			stream--;
			usleep(idle_time/2);
			if (initCapture==1)  {
				initCapture=0;			
				CaptureEnd();
			}
			continue;
		  }

		//We got the ACK for http request, start running the algorithm
		session.snd_wnd=used_window;
		if ( Probing_downstream(&session) < 0 )
		  {
		  	if (verbose) printf("Error encoutered while receiving data segments\n");
			TcpBreakup(&session);
			stream--;
			usleep(idle_time/2);
			if (initCapture==1)  {
				initCapture=0;			
				CaptureEnd();
			}
			continue;
		  }

		for (pshIndex=1; pshIndex<packetsReceived; pshIndex++)
		  {
			interarrivals[pshIndex-1]=times[pshIndex]-times[pshIndex-1];
		  }

		interarrivals_kept=remove_outliers(interarrivals, interarrivals_left, packetsReceived-1);

		if (interarrivals_kept<N/3)
		  {
			if (verbose) { 
				if(writefp)
					fprintf(writefp, "Insufficient number of legal interarrivals %d\n",interarrivals_kept);
				else
					printf("Insufficient number of legal interarrivals %d\n",interarrivals_kept);
			}
			TcpBreakup(&session);
			stream--;
			usleep(idle_time/2);
			if (initCapture==1)  {
				initCapture=0;			
				CaptureEnd();
			}
			continue;
		  }

#ifdef DEBUG
		if (verbose) { 
			if(writefp)
				fprintf(writefp, "interarrivals_kept %d\n",interarrivals_kept);
			else
				printf("interarrivals_kept %d\n",interarrivals_kept);
		}
#endif

		compute_owd(interarrivals_left, owd, interarrivals_kept);

		for (i=1; i+group_size<=interarrivals_kept+1; i+=group_size)
		  {
			order_dbl(owd, ordered, i, group_size);
			if (group_size%2==0) medians[i/group_size]=( ordered[(int)(group_size/2)-1] + ordered[(int)(group_size/2)] )/2;
			else medians[i/group_size]=ordered[(int)(group_size/2)];
#ifdef DEBUG
			if (verbose) {
				if(writefp)
					fprintf(writefp, "medians %d %lf\n",i/group_size, medians[i/group_size]);
				else
					printf("medians %d %lf\n",i/group_size, medians[i/group_size]);
			}
#endif
		  }

		/*  INCLUDE ONLY PCT METRIC  */
		pct=pairwise_comparision_test(medians, 0, interarrivals_kept/group_size );
		if (verbose) { 
			if(writefp)
				fprintf(writefp, "pct = %lf\n",pct);
			else
				printf("pct = %lf\n",pct);
		}
		if ( pct>pct_incr_threshold ) {
			increasing++;
				if (verbose) {
					if (writefp) fprintf(writefp, "Trend: Increasing\n");
					else printf("Trend: Increasing\n");
				}
				else {
					if(writefp)
						fprintf(writefp,"I");
					else {
						printf("I");
						fflush(stdout);
					}
				}
		}
		else if ( pct<pct_nincr_threshold ) {
			non_increasing++;
				if (verbose) {
					if (writefp) fprintf(writefp, "Trend: Non-increasing\n");
					else printf("Trend: Non-increasing\n");
				}
				else {
					if(writefp)
						fprintf(writefp,"N");
					else {
						printf("N");
						fflush(stdout);
					}
				}
		}
		else {
			grey++;
				if (verbose) {
					if (writefp) fprintf(writefp, "Trend: Grey\n");
					else printf("Trend: Grey\n");
				}
				else {
					if(writefp)
						fprintf(writefp,"G");
					else {
						printf("G");
						fflush(stdout);
					}
				}
		}

		TcpBreakup(&session);
		if (initCapture==1)  {
			initCapture=0;		
			CaptureEnd();
		}

		usleep(idle_time);
	}
}


void abget_iteration_upstream()
{
	double pct;
	int i;
	unsigned int stream=0;
	increasing=0;
	non_increasing=0;
	grey=0;

	if(writefp)
		fprintf(writefp, "Rate: %lf Mbits/sec\n",rate);
	else {
		if(verbose)
			printf("Rate=%lf Mbps: ",rate);
		else {
			printf("Rate=%lf Mbps: ",rate);
			fflush(stdout);
		}
	}

	while (stream++<num_of_streams && (max_time==0 || GetTime()<timeoutTime) )
	  {
		if (verbose) {
			if(writefp)
				fprintf(writefp, "Stream %d\n",stream);
			else 
				printf("Stream %d\n",stream);
		}

		// Init packet capture device and install filter for our flow
		if (initCapture==0)
		{
			CaptureInit(sourceIpAddress, sourcePort, targetIpAddress, targetPort);
			initCapture = 1;
		}

		/* Setup connection to target */
		session.snd_wnd = packetSize*10;//packetSize;
		session.iss=1;
		session.snd_nxt=1;
		session.rcv_nxt=0;		
		if (faults>MAX_FAULTS) {
			if (faults==MAX_FAULTS+1) {
				printf("\nThe trick with out of sequence segments failed...\nTry to send segments without establish connection, in order to receive RST packets\n");
				faults++;
				if(writefp)
					fprintf(writefp, "Rate: %lf Mbits/sec\n",rate);
				else {
					if(verbose)
						printf("Rate=%lf Mbps: ",rate);
					else {
						printf("Rate=%lf Mbps: ",rate);
						fflush(stdout);
					}
				}
			}
		}
		else {
			if ( TcpHandshake(&session, (struct TcpPacket*)synPacket, 8, 0) < 0 ) /* do the handshake */
			  {
			  	if (verbose) printf("Error encountered in TCP Handshake emulation\n");
				TcpBreakup(&session);
				stream--;
				usleep(idle_time/2);
				if (initCapture==1)  {
					initCapture=0;			
					CaptureEnd();
				}
				faults++;
				continue;
			  }

			session.snd_wnd = 0;		//set advertise window to zero to block server from sending data segments
		}
		if ( Probing_upstream(&session) < 0 )
		  {
		  	if (verbose) printf("Error encountered while sending data segments\n");
			TcpBreakup(&session);
			stream--;
			usleep(idle_time/2);
			if (initCapture==1)  {
				initCapture=0;			
				CaptureEnd();
			}
			faults++;
			continue;
		  }

		for (pshIndex=1; pshIndex<packetsReceived; pshIndex++)
		  {
			interarrivals[pshIndex-1]=times[pshIndex]-times[pshIndex-1];
		  }

		interarrivals_kept=remove_outliers(interarrivals, interarrivals_left, packetsReceived);

		if (interarrivals_kept<N/3)
		  {
			if (verbose) {
				if(writefp)
					fprintf(writefp, "Insufficient number of legal interarrivals %d\n",interarrivals_kept);
				else
					printf("Insufficient number of legal interarrivals %d\n",interarrivals_kept);
			}
			TcpBreakup(&session);
			stream--;
			usleep(idle_time/2);
			if (initCapture==1)  {
				initCapture=0;			
				CaptureEnd();
			}
			faults++;
			continue;
		  }

#ifdef DEBUG
		if (verbose) {
			if(writefp)
				fprintf(writefp, "interarrivals_kept %d\n",interarrivals_kept);
			else
				printf("interarrivals_kept %d\n",interarrivals_kept);
		}
#endif

		compute_owd(interarrivals_left, owd, interarrivals_kept);

		for (i=1; i+group_size<=interarrivals_kept+1; i+=group_size)
		  {
			order_dbl(owd, ordered, i, group_size);
			if (group_size%2==0) medians[i/group_size]=( ordered[(int)(group_size/2)-1] + ordered[(int)(group_size/2)] )/2;
			else medians[i/group_size]=ordered[(int)(group_size/2)];
#ifdef DEBUG
			if (verbose) {
				if(writefp)
					fprintf(writefp, "medians %d %lf\n",i/group_size, medians[i/group_size]);
				else
					printf("medians %d %lf\n",i/group_size, medians[i/group_size]);
			}
#endif
		  }

		/*  INCLUDE ONLY PCT METRIC  */
		pct=pairwise_comparision_test(medians, 0, interarrivals_kept/group_size );
		if (verbose) { 
			if(writefp)
				fprintf(writefp, "pct %lf\n",pct);
			else
				printf("pct %lf\n",pct);
		}
		if ( pct>pct_incr_threshold ) {
			increasing++;
			if (!verbose) {
				if(writefp)
					fprintf(writefp,"I");
				else {
					printf("I");
					fflush(stdout);
				}
			}
		}
		else if ( pct<pct_nincr_threshold ) {
			non_increasing++;
			if (!verbose) {
				if(writefp)
					fprintf(writefp,"N");
				else {
					printf("N");
					fflush(stdout);
				}
			}
		}
		else {
			grey++;
			if (!verbose) {
				if(writefp)
					fprintf(writefp,"G");
				else {
					printf("G");
					fflush(stdout);
				}
			}
		}

		TcpBreakup(&session);
		if (initCapture==1)  {
			initCapture=0;		
			CaptureEnd();
		}

		if (faults<=MAX_FAULTS) faults=-10;

		usleep(idle_time);
	}
}


void abget_algorithm()
{
	double probing_range;
	double adjustment_resolution;
	double estimation_probe_times;
	int continues_increasing;
	unsigned int i;
	
	medians=(double*)malloc(sizeof(double)*GROUPS);
	ordered=(double*)malloc(sizeof(double)*group_size);

	if (downstream==1) 
		if(writefp)
			fprintf(writefp, "--- Downstream path ---\n");
		else
			printf("--- Downstream path ---\n");
	else
		if(writefp)
			fprintf(writefp, "--- Upstream path ---\n");
		else
			printf("--- Upstream path ---\n");

	if (downstream==0) {	//upstream path
		for (i=0; i<packets_examine; i++)
			push_packets[i]=0;
		used_size=packetSize;	//MSS bytes
	}

	if (binary_search==0) {		//linear probing

		if(writefp)
			fprintf(writefp, "--- Linear probing ---\n\n");
		else
			printf("--- Linear probing ---\n\n");

		probing_range = Rmax - Rmin;
		adjustment_resolution = probing_range / 5.0;
		estimation_probe_times = probing_range / estimate_resolution;
		continues_increasing = 0;

		min=Rmin;
		max=Rmax;

		if (max_time>0) timeoutTime = GetTime() + max_time;

		gettimeofday(&before , NULL);
		started=1;

		/*
		 *	We are going to reduce the times of probing only if the overhead we are
		 *	adding for this procedure decreases the total number of probes.
		 */
		if(estimation_probe_times > 10.0 && optimization==1 ) {

			if(writefp)
				fprintf(writefp, "Rate adjustment algorithm\n");
			else
				printf("Rate adjustment algorithm\n");
				
			if (min>0) rate=min;
			else rate=adjustment_resolution;

			while (rate<=Rmax && (max_time==0 || GetTime()<timeoutTime)) {

				if (downstream==1) 
					abget_iteration_downstream();
				else 
					abget_iteration_upstream();

				if(writefp)
					fprintf(writefp,"\n");
				else {
					printf("\n");
					fflush(stdout);
				}
			
				if (verbose) { 
					if(writefp)
						fprintf(writefp, "incr: %d non-incr: %d grey: %d\n",increasing,non_increasing,grey);
					else
						printf("incr: %d non-incr: %d grey: %d\n",increasing,non_increasing,grey);
				}

				if (increasing>num_of_streams/2) {		//Increasing trend
					if (rate<max) max=rate;

					continues_increasing++;
					if(continues_increasing >= 3) break;
				}
				else if (non_increasing>num_of_streams/2) {		//Non-increasing trend
					if (rate>min) min=rate;
				}
				//else  ;			//Grey region

				rate+=adjustment_resolution;
			}

			Rmin=min;
			Rmax=max;

			if(writefp)
				fprintf(writefp, "Rate adjustment finished\n");
			else
				printf("Rate adjustment finished\n");
			/*
			 *	End of rate adjustment algorithm
 			 */
		}

		if(writefp){
			fprintf(writefp, "Probing from %lf to %lf Mbps with resolution of %lf Mbps\n",min, max, estimate_resolution);
			fprintf(writefp, "\n");
		}
		else {
			printf("Probing from %lf to %lf Mbps with resolution of %lf Mbps\n",min, max, estimate_resolution);
			printf("\n");
		}

		min=Rmin;
		max=Rmax;
		continues_increasing = 0;

		if (min>0) rate=min+estimate_resolution;
		else rate=estimate_resolution;

		while (rate<Rmax && (max_time==0 || GetTime()<timeoutTime))
		  {
			if (downstream) 
				abget_iteration_downstream();
			else
				abget_iteration_upstream();

			if(writefp)
				fprintf(writefp,"\n");
			else {
				printf("\n");
				fflush(stdout);
			}

			if (verbose) {
				if(writefp)
					fprintf(writefp, "incr: %d non-incr: %d grey: %d\n",increasing,non_increasing,grey);
				else
					printf("incr: %d non-incr: %d grey: %d\n",increasing,non_increasing,grey);
			}

			if (increasing>num_of_streams/2) {		//Increasing trend
				if (rate<max) max=rate;

				continues_increasing++;

				if(continues_increasing>=3 && optimization==1)
					break;
	    		}
			else if (non_increasing>num_of_streams/2) {		//Non-increasing trend
				if (rate>min) min=rate;
			}
			//else ;			//Grey region

			rate+=estimate_resolution;

		  }

		gettimeofday(&after , NULL);
		completed=1;

	}
		
	else {		//binary search

		if(writefp)
			fprintf(writefp, "--- Binary search probing ---\n\n");
		else
			printf("--- Binary search probing ---\n\n");

		min=Rmin;
		max=Rmax;
		rate=(max+min)/2.0;

		if (max_time>0) timeoutTime = GetTime() + max_time;

		gettimeofday(&before , NULL);
		started=1;

		while ( max-min>estimate_resolution && (Gmin-min>variation_resolution || max-Gmax>variation_resolution) && (max_time==0 || GetTime()<timeoutTime) )
		  {
			if (downstream==1) abget_iteration_downstream();
			else abget_iteration_upstream();

			if(writefp)
				fprintf(writefp,"\n");
			else {
				printf("\n");
				fflush(stdout);
			}
			
			if (verbose) {
				if(writefp)
					fprintf(writefp, "incr: %d non-incr: %d grey: %d\n",increasing,non_increasing,grey);
				else
					printf("incr: %d non-incr: %d grey: %d\n",increasing,non_increasing,grey);
			}

			if (increasing>num_of_streams/2) {	//Increasing trend
				max=rate;
				if (Gmax==0) rate=(max+min)/2;
				else if (Gmax>max) {
					Gmin=0;
					Gmax=0;
					rate=(max+min)/2;
				}
				else rate=(max+Gmax)/2;
    			}

			else if (non_increasing>num_of_streams/2) {	//Non-increasing trend
				min=rate;
				if (Gmin==0) rate=(max+min)/2;
				else if (min>Gmin) {
					Gmin=0;
					Gmax=0;
					rate=(max+min)/2;
				}
				else rate=(min+Gmin)/2;
			}
			else {	//Grey region
				if (verbose) { 
					if(writefp)
						fprintf(writefp, "repeat with the same rate\n");
					else
						printf("repeat with the same rate\n");
				}
				if (Gmin==0 && Gmax==0) 
					Gmin=Gmax=rate;
				if (Gmax<=rate) {
					Gmax=rate;
					if (max-Gmax>variation_resolution) 
						rate=(max+Gmax)/2;
					else 
						rate=(min+Gmin)/2;
				}
				else if (Gmin>rate) {
					Gmin=rate;
					if (Gmin-min>variation_resolution) 
						rate=(min+Gmin)/2;
					else 
						rate=(max+Gmax)/2;
				}
			else rate=(min+max)/2;	//should never come into this else
			}
		  }

		gettimeofday(&after , NULL);
		completed=1;
		
	}

	if (verbose) { 
		if(writefp)
			fprintf(writefp, "Breaking up the connection \n\n\n");
		else
			printf("Breaking up the connection \n\n\n");
	}

	if (medians!=NULL) free(medians);
	if (ordered!=NULL) free(ordered);
}


int main(int argc, char **argv) 
{

	char targetHostName[MAXHOSTNAMELEN];	/* DNS name of target host */
	char sourceHostName[MAXHOSTNAMELEN];
	char source[MAXHOSTNAMELEN];
	char *p;
	struct sockaddr_in saddr;
	int fd;
	int opt;
	int gotsource = 0;	
	
	writefp = NULL;

	interface = NULL;

	parse_conf_file(conf_file);
	var_init();


	if ((p = (char *)strrchr(argv[0], '/')) != NULL) {
		progName = p + 1;
	}
	else {
		progName = argv[0];
	}

	opterr = 0;
	while ((opt = getopt(argc, argv, "hdulbvD:s:p:w:f:c:t:r:x:i:-:L:H:P:N:I:")) != EOF) {
		switch (opt) {
			case 'v':
				verbose = 1;
				break;
			case 'b':
				binary_search = 1;
				break;
			case 'l':
				binary_search = 0;
				break;
			case 'u':
				downstream = 0;
				break;
			case 'd':
				downstream = 1;
				break;
			case 'h':
				Usage();
				break;
			case 'D':
				writefilename = strdup(optarg);
				printf("filename: %s\n", writefilename);
				break;
			case 's':
				gotsource = 1;
				strcpy(source, optarg);
				break;
			case 'p':
				targetPort = atoi(optarg);
				break;
			case 'w':
				sourcePort = atoi(optarg);
				break;
			case 'f':
				filename = strdup(optarg);
				break;
			case 'c':
				conf_file = strdup(optarg);
				break;
			case 't':
				max_time = atoi(optarg);
				break;
			case 'r':
				estimate_resolution = atof(optarg);
				break;
			case 'x':
				variation_resolution = atof(optarg);
				break;
			case 'i':
				interface = strdup(optarg);
				break;
			case '-':
				if ( strcmp(optarg, "binary")==0 ) binary_search=1;
				else if ( strcmp(optarg, "linear")==0 ) binary_search=0;
				else if ( strcmp(optarg, "downstream")==0 ) downstream=1;
				else if ( strcmp(optarg, "upstream")==0 ) downstream=0;
				else if ( strcmp(optarg, "verbose")==0 ) verbose=1;
				else if ( strcmp(optarg, "help")==0 ) Usage();
				else Usage();
				break;
			case 'L':
				Rmin = atoi(optarg);
				break;
			case 'H':
				Rmax = atoi(optarg);
				break;
			case 'P':
				packets_examine = atoi(optarg);
				break;
			case 'N':
				num_of_streams = atoi(optarg);
				break;
			case 'I':
				idle_time = atol(optarg);
				break;
			default:
				Usage();
				break;
		}
	}

	switch (argc - optind) 
	{
		case 1:
			target = argv[optind];
			break;
		default:
			Usage();
	}

	if (conf_file!=NULL) {
		parse_conf_file(conf_file);
	}

	if(writefilename) {
		writefp = fopen(writefilename, "w");
		if(writefp != NULL)
			printf("fileopend\n");
	}

	if (target==NULL) {
		fprintf(stderr, "You must provide a valid host\n");
		exit(1);
	  }

	if (filename==NULL && downstream==1)
	  {
		fprintf(stderr, "You must provide a valid filename\n");
		exit(1);
	  }
	else if (filename==NULL && downstream==0)
	  {
		filename=strdup(DUMMY_FILENAME);
	  }

	/* Setup signal handler for cleaning up */
	signal(SIGTERM, Cleanup);
	signal(SIGINT, Cleanup);
	signal(SIGHUP, Cleanup);

	if(writefp)
		fprintf(writefp, "=============== a b g e t ===============\n");
	else
		printf("=============== a b g e t ===============\n");
	

	/*
	 * Get hostname and IP address of target host
	 */
	if (GetCannonicalInfo(target, targetHostName, &targetIpAddress) < 0) 
	{
		Quit();
	}

	//printf("%s - %s - %.3f\n\n", target, targetHostName, GetTime());

	/*
	 * Get hostname and IP address of source host
	 */

	if(gotsource == 0)
	{
		//if (gethostname(source, MAXHOSTNAMELEN) != 0) 
		//{
		//	fprintf(stderr,"ERROR: can't determine local hostname\n");
		//	Quit();
		//}

		if (find_source_ip(source)<0)
		{
			if (interface==NULL) fprintf(stderr,"ERROR: can't determine local ip address\n");
			else fprintf(stderr,"ERROR: can't determine local ip address for interface %s\n",interface);
			Quit();
		}
	}
	
	if (GetCannonicalInfo(source, sourceHostName, &sourceIpAddress) < 0) 
	{
		Quit();
	}

	if(writefp) {
		fprintf(writefp, "  source = %s [%s]\n", sourceHostName,InetAddress(sourceIpAddress));
		fprintf(writefp, "  target = %s [%s]\n",targetHostName,InetAddress(targetIpAddress));
		fprintf(writefp, "  target port = %d\n", targetPort);	   
	}
	else {
		printf("  source = %s [%s]\n", sourceHostName,InetAddress(sourceIpAddress));
		printf("  target = %s [%s]\n",targetHostName,InetAddress(targetIpAddress));
		printf("  target port = %d\n", targetPort);	   
	}

	/* Initialze random number generator */
	srand48(GetTime());

	if (sourcePort == 0) 
	{
		/* 
		 * Find and allocate a spare TCP port to use
		 */
		saddr.sin_family = AF_INET;
		if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		{
			fprintf(stderr, "ERROR: can't open socket\n");
			return 0;
		}
		if ((sourcePort = BindTcpPort(fd)) == 0) 
		{
			fprintf(stderr, "ERROR: can't bind port\n");
			return 0;
		}
	}

	if(writefp) {
		fprintf(writefp, "  source port = %d\n", sourcePort);
		fprintf(writefp, "\n");
	}
	else {
		printf("  source port = %d\n", sourcePort);
		printf("\n");
	}

	//Init firewall and tcp state
	Init(sourceIpAddress, sourcePort, targetIpAddress, targetPort, &session);
	session.snd_wnd = packetSize*10;//packetSize;
	session.snd_nxt = 1;		//start sender sequence numbers from 1
	//session.snd_nxt = (uint32)mrand48();  // random initial sequence number
	session.iss = session.snd_nxt;

	ConstructSynPacket();
	if (downstream==1) payloadSize=ConstructWWWGetRequest(target);
	else ConstructExtendedRequest(target);

	abget_algorithm();

	close(session.socket);
	free(synPacket);
	free(getPacket);

	Cleanup();

	return (0);
}


