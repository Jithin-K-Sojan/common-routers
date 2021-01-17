// Jithin Kallukalam Sojan 2017A7PS0163P
// Samina Shiraj Mulani 2018A7PS0314P

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>

#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>

#include <sys/types.h>
#include <sys/select.h>

#define BUFFERSIZE 100
#define MAXDOMAINS 40
#define MAX 50

#define MAX_TTL 30

#define BUFSIZE 1500

struct rec{
    u_short rec_seq;
    u_short rec_ttl;
};

char recvbuf[BUFSIZE];
char sendbuf[BUFSIZE];

int datalen;
char* host;
pid_t pid;

int ttl,max_ttl;

typedef struct pathNode{
    int family;
    unsigned long address;
}pathNode;

pathNode* path[MAX_TTL];

int datalen = sizeof(struct rec);
int max_ttl = MAX_TTL;
u_short sport;
u_short dport = 32768+666;
int sendfd = -1;
int recvfd = -1;

int maxLen = MAX_TTL;

int icmpRecv[MAXDOMAINS][MAX_TTL];

void sigIntHandler(int signo){
    printf("Terminating process.\n");

    if(sendfd!=-1){
        close(sendfd);
    }
    if(recvfd!=-1){
        close(recvfd);
    }
    exit(0);
}

int findIndex(struct addrinfo* addrInfoList[],int currListSize, in_addr_t addrVal){

    for(int i = 0;i<currListSize;i++){
        if(((struct sockaddr_in*)(addrInfoList[i]->ai_addr))->sin_addr.s_addr==addrVal){
            return i;
        }
    }   
    return -1;
}

int main(int argc, char *argv[]){

    if(argc!=2){
        printf("Invalid Input\n");
        printf("./a.out <input-file>\n");
        exit(0);
    }

    for(int i = 0;i<MAX_TTL;i++){
        path[i] = NULL;
    }

    for(int i = 0;i<MAXDOMAINS;i++){
        for(int j = 0;j<MAX_TTL;j++){
            icmpRecv[i][j] = 0;
        }
    }

    signal(SIGINT,sigIntHandler);

    FILE* fp = fopen(argv[1],"r");
    if(fp==NULL){
        printf("Input file does not exist.\n");
        exit(0);
    }

    char buffer[BUFFERSIZE];
    struct addrinfo* addrInfoList[MAXDOMAINS];

    int currListSize = 0;
    char psuedoServ[10] = "http";
    
    ssize_t size;
    while(1){
        if(fgets(buffer,BUFFERSIZE,fp)==NULL){
            break;
        }

        if(buffer[0]=='\0' || buffer[0]=='\t' || buffer[0]==' ' || buffer[0]=='\n'){
                continue;
        }

        buffer[strlen(buffer)-1] = '\0';

        char* host = buffer;
        char* serv = NULL;
        char* check = strstr(buffer,"://");
        if(check!=NULL){
            host = check+3;
            *check = '\0';
            serv = buffer;
        }
        else{
            serv = psuedoServ;
        }
        
        check = strstr(host,"/");
        if(check!=NULL){
            *check='\0';
        }

        if(strlen(host)==0)continue;

        printf("%s\n",host);

        struct addrinfo hints,*res,*ressave;
        bzero(&hints,sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;

        int n;
        if((n=getaddrinfo(host,serv,&hints,&res))!=0){
            printf("Error on domain name %s lookup: %s.\n",buffer,gai_strerror(n));
            continue;
        }
        else{
            printf("Given domain exists.\n");
        }

        ressave = res;

        struct addrinfo* prev;
        while(res!=NULL && res->ai_family!=AF_INET){
            prev = res;
            res = res->ai_next;
            prev->ai_next = NULL;
            freeaddrinfo(prev);
        }
        if(res==NULL){
            printf("One of the addresses has only IPV6 IP adresses. Not supported.\n");
            exit(0);
        }
        printf("IP: %s\n",inet_ntoa(((struct sockaddr_in*)(res->ai_addr))->sin_addr));
        addrInfoList[currListSize] = res;
        res = res->ai_next;
        if(res!=NULL){
            freeaddrinfo(res);
        }

        currListSize++;
    }

    if(currListSize==0){
        printf("No domain to checks.\n");
        exit(0);
    }

    pid = getpid();

    int seq,code,done;
    struct rec* rec;

    sendfd = socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in bindAddr;
    bzero(&bindAddr,sizeof(bindAddr));
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddr.sin_port = htons((getpid() & 0xffff)|0x8000);
    if(bind(sendfd,(struct sockaddr*)&bindAddr,sizeof(struct sockaddr_in))<0){
        perror("bind error");
        exit(0);
    }
    
    recvfd = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
    setuid(getuid());

    struct sockaddr_in destAddr;
    bzero(&destAddr,sizeof(destAddr));
    destAddr.sin_family = AF_INET;

    seq = 0;

    fd_set readfds,writefds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    int prevI = 0;
    int i = 1;
    int j = 0;
    int once = 0;

    int maxfd;

    struct timeval tv;
    struct timeval* pTv = NULL;

    while(1){

        if(prevI!=i){
            setsockopt(sendfd,IPPROTO_IP,IP_TTL,&i,sizeof(int));
        }
        prevI = i;

        FD_SET(recvfd,&readfds);
        maxfd = recvfd;
        
        if(once!=2){
            FD_SET(sendfd,&writefds);
            if(sendfd>maxfd){
                maxfd = sendfd;
            } 
        }

        int n = select(maxfd+1,&readfds,&writefds,NULL,pTv);
        if(n<0){
            perror("select");
        }

        if(n==0){            
            break;
        }

        if(FD_ISSET(recvfd,&readfds)){

            struct sockaddr_in src;

            struct ip *ip, *hip;
            struct icmp *icmp;
            struct udphdr *udp;
            int hlen1,hlen2,hlen3,icmplen,ret;

            bzero(&src,sizeof(src));
            struct sockaddr_in recvAddr;
            int len = sizeof(recvAddr);

            int n = recvfrom(recvfd,recvbuf,sizeof(recvbuf),0,(struct sockaddr*)&recvAddr,&len);
            if(n<0){
                printf("Error in recvfrom, returned.\n");
                continue;
            }
            
            ip = (struct ip*)recvbuf;
            hlen1 = ip->ip_hl<<2;
            icmp = (struct icmp*)(recvbuf+hlen1);
            if((icmplen = n-hlen1)<8){
                continue;
            }

            if(icmp->icmp_type==ICMP_TIMXCEED && icmp->icmp_code==ICMP_TIMXCEED_INTRANS){
                if(icmplen< 8+sizeof(struct ip)){
                    continue;
                }

                hip = (struct ip*)(recvbuf + hlen1 + 8);
                hlen2 = hip->ip_hl<<2;
                if(icmplen< 8 + hlen2 +4){
                    continue;
                }

                udp = (struct udphdr*)(recvbuf + hlen1 + 8 + hlen2);
                if(hip->ip_p == IPPROTO_UDP && udp->uh_sport == htons((getpid() & 0xffff)|0x8000)){

                    // Intermediate router.
                    int destport = ntohs(udp->uh_dport);
                    int seq = destport-dport;

                    int ttl = seq;
                    int ind = findIndex(addrInfoList,currListSize,hip->ip_dst.s_addr);

                    if(ind==-1){
                        printf("Could not find corresponding IP.\n");
                        continue;
                    }

                    // printf("IP: %s TTL: %d\n",inet_ntoa(ip->ip_src),ttl);
                    // printf("SRCIP: %s\n",inet_ntoa(hip->ip_dst));

                    if(ttl>maxLen){
                        continue;
                    }

                    if(icmpRecv[ind][ttl-1]==1){
                        continue;
                    }

                    icmpRecv[ind][ttl-1] = 1;

                    if(path[ttl-1]==NULL){
                        path[ttl-1] = (pathNode*)malloc(sizeof(pathNode));
                        path[ttl-1]->family = AF_INET;
                        path[ttl-1]->address = ip->ip_src.s_addr;

                    }
                    else{
                        if(path[ttl-1]->address!=ip->ip_src.s_addr){
                            maxLen = ttl-1;
                        }
                    }

                }
            }
            else if(icmp->icmp_type==ICMP_UNREACH){

                if(icmplen< 8 + sizeof(struct ip)){
                    continue;
                }

                hip = (struct ip*)(recvbuf + hlen1 + 8);
                hlen2 = hip->ip_hl<<2;
                if(icmplen< 8 + hlen2 + 4){
                    continue;
                }
                udp = (struct udphdr*)(recvbuf + hlen1 + 8 + hlen2);
                if(hip->ip_p == IPPROTO_UDP && udp->uh_sport == htons((getpid() & 0xffff)|0x8000)){
                    if(icmp->icmp_code == ICMP_UNREACH_PORT){

                        // Reached destination.
                        int destport = ntohs(udp->uh_dport);
                        int seq = destport-dport;

                        int ttl = seq;
                        int ind = findIndex(addrInfoList,currListSize,hip->ip_dst.s_addr);
                        if(ind==-1){
                            printf("Could not find corresponding IP.\n");
                            continue;
                        }

                        // printf("IP: %s TTL: %d\n",inet_ntoa(ip->ip_src),ttl);
                        // printf("SRCIP: %s\n",inet_ntoa(hip->ip_dst));

                        if(ttl>maxLen){
                            continue;
                        }

                        if(icmpRecv[ind][ttl-1]==1){
                            continue;
                        }

                        icmpRecv[ind][ttl-1] = 1;

                        if(path[ttl-1]==NULL){
                            path[ttl-1] = (pathNode*)malloc(sizeof(pathNode));
                            path[ttl-1]->family = AF_INET;
                            path[ttl-1]->address = ip->ip_src.s_addr;

                            maxLen = ttl;
                        }
                        else{
                            if(path[ttl-1]->address!=ip->ip_src.s_addr){
                                maxLen = ttl-1;
                            }
                        }

                    }
                    else{
                        printf("Some other ICMP Error has been faced, retuned.\n");
                        continue;
                    }
                }
            }
        }


        if(FD_ISSET(sendfd,&writefds)){

            if(!icmpRecv[j][i]){
                rec = (struct rec*)sendbuf;
                rec->rec_ttl = i;

                seq = i;
                rec->rec_seq = seq;
                ((struct sockaddr_in*)(addrInfoList[j]->ai_addr))->sin_port = htons(dport+seq);

                for(int k = 0;k<2;k++){
                    if(sendto(sendfd,sendbuf,datalen,0,addrInfoList[j]->ai_addr,addrInfoList[j]->ai_addrlen)<0){
                        if(errno==EINTR){
                            printf("Interrupted. Retransmit.\n");
                            k = k-1;
                        }
                        else{
                            perror("sendto");
                        }
                    }
                }
            }
            
            j++;
            if(j==currListSize){
                i++;
                j = 0;
            }

            if(i>maxLen){
                if(once==0){
                    printf("\n");
                    printf("Retransmitting packets for which replies havent arrived.\n");
                    once = 1;
                    j = 0;
                    i  = 1;
                }
                else if(once==1){
                    printf("\n");
                    printf("All packets sent, waiting for 10 seconds.\n");
                    once = 2;
                    j = 0;
                    i = 1;

                    tv.tv_sec = 10;
                    tv.tv_usec = 0;
                    pTv = &tv;
                }
            }
        }

        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

    }

    printf("\n");
    printf("Longest Common Path:\n");
    int tag = 0;
    for(int i = 1;i<maxLen+1;i++){
        if(i==2)continue;
        if(path[i-1]!=NULL){
            tag = 1;
            struct in_addr comPathAddr;
            comPathAddr.s_addr = path[i-1]->address;
            printf("%s\n",inet_ntoa(comPathAddr));
        }
        else{
            break;
        }
    }

    if(tag==0){
        printf("No node is common in the paths of the domains given.\n");
    }

    fclose(fp);
}