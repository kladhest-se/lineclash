/* Socket-level regressions for fragmented input and output backpressure. */
#include "lineclash.h"
#include <assert.h>
#include <signal.h>

static void pair(sock_t s[2]) {
#ifdef PLATFORM_WINDOWS
    sock_t listener=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in addr; socklen_t len=sizeof(addr);
    memset(&addr,0,sizeof(addr)); addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(bind(listener,(struct sockaddr *)&addr,sizeof(addr))==0);
    assert(getsockname(listener,(struct sockaddr *)&addr,&len)==0);
    assert(listen(listener,1)==0);
    s[0]=socket(AF_INET,SOCK_STREAM,0);
    assert(connect(s[0],(struct sockaddr *)&addr,sizeof(addr))==0);
    s[1]=accept(listener,NULL,NULL); assert(s[1]!=INVALID_SOCK); sock_close(listener);
#else
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, s) == 0);
#endif
    net_set_nonblocking(s[0]); net_set_nonblocking(s[1]);
}
static int receive_line(sock_t s,char *buf,int *len,char *line,int maxline) {
    int r;
    for(int i=0;i<2000;i++) {
        r=net_recv(s,buf,len,line,maxline);
        if(r) return r;
        SLEEP_MS(1);
    }
    return 0;
}
int main(void) {
    sock_t s[2]; char buf[RECV_BUF_SIZE] = "", line[MAX_MSG_LEN]; int len = 0;
    assert(net_init());
#ifndef PLATFORM_WINDOWS
    signal(SIGPIPE, SIG_IGN);
#endif
    pair(s);
    assert(send(s[0], "one\ntwo\npar", 11, 0) == 11);
    assert(receive_line(s[1], buf, &len, line, sizeof(line)) == 1 && !strcmp(line,"one"));
    assert(receive_line(s[1], buf, &len, line, sizeof(line)) == 1 && !strcmp(line,"two"));
    assert(net_recv(s[1], buf, &len, line, sizeof(line)) == 0);
    assert(send(s[0], "tial\n", 5, 0) == 5);
    assert(receive_line(s[1], buf, &len, line, sizeof(line)) == 1 && !strcmp(line,"partial"));
    char big[MAX_MSG_LEN + 1]; memset(big,'x',sizeof(big));
    assert(send(s[0],big,sizeof(big),0) == sizeof(big));
    assert(receive_line(s[1],buf,&len,line,sizeof(line)) == -1);
    sock_close(s[0]); sock_close(s[1]);

    pair(s); len = 0;
    /* Many coalesced lines exceed the receive buffer without losing bytes. */
    for (int i=0; i<2000; i++) assert(net_send(s[0],"abcdefghij"));
    for (int i=0; i<2000;) {
        int r=net_recv(s[1],buf,&len,line,sizeof(line)); assert(r>=0);
        if(r) { assert(!strcmp(line,"abcdefghij")); i++; }
    }
    sock_close(s[0]); sock_close(s[1]);

    pair(s);
    int small = 1024;
    setsockopt(s[0],SOL_SOCKET,SO_SNDBUF,(const char *)&small,sizeof(small));
    char message[401], expected[40100], received[40100];
    size_t want=0, got=0;
    for(int i=0;i<100;i++) {
        memset(message,'a'+i%26,400); message[400]=0;
        memcpy(expected+want,message,400); expected[want+400]='\n'; want+=401;
        assert(net_send(s[0],message));
    }
    for(int i=0;i<10000 && got<want;i++) {
        int n=(int)recv(s[1],received+got,want-got,0);
        if(n>0) got+=n;
        net_flush_all();
        if(n<0) SLEEP_MS(1);
    }
    assert(got==want && !memcmp(expected,received,want));
    /* Pending bytes must not survive close / descriptor reuse. */
    for(int i=0;i<20;i++) assert(net_send(s[0],message));
    sock_close(s[0]); sock_close(s[1]);
    pair(s); assert(net_send(s[0],"fresh"));
    len=0; assert(receive_line(s[1],buf,&len,line,sizeof(line))==1 && !strcmp(line,"fresh"));
    sock_close(s[0]); sock_close(s[1]);
    pair(s);
    setsockopt(s[0],SOL_SOCKET,SO_SNDBUF,(const char *)&small,sizeof(small));
    int accepted=0;
    while(accepted < 10000 && net_send(s[0],message)) accepted++;
    assert(accepted < 10000); /* slow peers hit the bounded queue limit */
    sock_close(s[0]); sock_close(s[1]);
    NetReconnect retry;
    net_reconnect_begin(&retry,1000,3);
    assert(retry.active && retry.deadline==4000);
    assert(net_reconnect_poll(&retry,"127.0.0.1",1,1100)==INVALID_SOCK && retry.attempts==0);
    assert(net_reconnect_poll(&retry,"127.0.0.1",1,4000)==INVALID_SOCK && !retry.active);
    net_reconnect_begin(&retry,1000,0); assert(!retry.active);
    net_reconnect_begin(&retry,1000,3); net_reconnect_stop(&retry); assert(!retry.active);
    net_cleanup();
    puts("Networking and reconnect scheduler regressions passed");
    return 0;
}
