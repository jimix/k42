/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sockTestSrv.C,v 1.11 2005/05/05 15:55:33 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: dameon code that provides an echo and file port
 * **************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/poll.h>

typedef int SysStatus;
typedef unsigned int uval;

extern int      optind, opterr;
extern char     *optarg;

#define MESG_SIZE 1024

typedef int FDType;
typedef int PortType;

int server = 0;
char *echoString = NULL;
char *cmdString = NULL;
char *host = NULL;
int  port = 0;
int  repeatCount = 1;
int  delay = 10;
int  assertPoll = 0;
int  verbose = 0;

enum ClientActions { CMD, ECHO, NONE } clientAction;

void
handleError(int num, SysStatus rc)
{
    fprintf(stderr, "%d: Got an unxpected rc=%d\n", num, rc);
}

int
assertFDState(FDType fd, int state)
{
    struct pollfd pollFDStruct;
    int rc;

    pollFDStruct.fd = fd;
    pollFDStruct.events = state;
    pollFDStruct.revents = 0;

    rc = poll(&pollFDStruct, 1, 0);

    if (rc == -1) {
        fprintf(stderr, "Error: poll on fd=%d failed errno=%d\n", fd, errno);
        return 0;
    }

    if ( pollFDStruct.revents & state ) {
        return 1;
    }
    printf("assertFDState Failed: state = %d != %d: ", state,
           pollFDStruct.revents);
    if (pollFDStruct.revents & POLLNVAL)
        printf(" POLLNVAL: Invalid polling request");
    if (pollFDStruct.revents & POLLHUP)
        printf(" POLLHUP: Hung up");
    if (pollFDStruct.revents & POLLERR)
        printf(" POLLERR: Error condition");
    printf("\n");

    return 0;
}

void
processArgs(int argc, char *argv[])
{
    int c,err=0;
    static char optstr[] = "h:p:e:c:r:d:vPS";

    clientAction = NONE;

    opterr=1;
    while ((c = getopt(argc, argv, optstr)) != EOF)
        switch (c) {
        case 'S' :
            printf("Server Mode...\n");
            server++;
            break;
        case 'e' :
            echoString = optarg;
            clientAction = ECHO;
            break;
        case 'c' :
            cmdString = optarg;
            clientAction = CMD;
            break;
        case 'h' :
            host = optarg;
            break;
        case 'p' :
            port = atoi(optarg);
            break;
        case 'r' :
            repeatCount = atoi(optarg);
            break;
        case 'd' :
            delay = atoi(optarg);
            break;
        case 'P' :
            assertPoll = 1;
            break;
        case 'v':
            verbose = 1;
            break;
        case ':' :
            printf("-%c requires argument\n", optopt);
            break;
        default:
            err=1;
        }

    if (err) {
        printf("usage: %s %s\n", argv[0], optstr);
        exit(1);
    }
}

class Command {
    enum {MaxArgs=10, BufSize=MESG_SIZE};
    char buffer[BufSize];
    char *cmdToken;
    char *args[MaxArgs];
    uval numArgs;
    uval cmdTerminated;
public:
    Command() { reset(); }
    void reset() {numArgs=0; cmdTerminated=0;}
    char *getBuffer() { return buffer; }
    uval getBufSize() { return BufSize; }
    void startCmdToken(uval idx) { cmdToken=&buffer[idx]; }
    void termCmdToken(uval idx) {
        buffer[idx]=0;
        cmdTerminated=1;
    }
    uval cmdCompleted() { return cmdTerminated; }
    uval startArgToken(uval *arg, uval idx ) {
        if (numArgs > MaxArgs) {
            printf("Error unsupported number of args\n");
            return 0;
        }
        args[numArgs]=&buffer[idx];
        *arg=numArgs;
        numArgs++;
        return 1;
    }
    void termArgToken(uval arg, uval idx) { buffer[idx]=0; }
    SysStatus process(FDType fd);
    void print();
};

void
Command::print()
{
    printf("cmd=%s numArgs=%d ", cmdToken, numArgs);
    for (uval i=0;i<numArgs;i++) {
        printf("arg[%d]=%s ",i,args[i]);
    }
    printf("\n");
}

int
sendfile(FDType fd, char *filename)
{
    char *data;
    int rc, fileLen, count, lenStrLen;
    FDType fileFD;
    struct stat statStruct;
    const int OutBufSize = 1024;
    char outBuf[OutBufSize];

    // open file
    fileFD = open(filename, O_RDONLY, 0);

    if (fileFD == -1) {
        fprintf(stderr, "open(): %s: %s\n", filename, strerror(errno));
        return -1;
    }

    // stat file for length
    rc = fstat(fileFD, &statStruct);
    if (rc == -1) {
        fprintf(stderr, "fstat(): %s: %s\n", filename, strerror(errno));
        return -1;
    }
    fileLen = statStruct.st_size;

    data = (char *)mmap(0, fileLen, PROT_READ, MAP_SHARED, fileFD, 0);

    if (data == MAP_FAILED) {
        fprintf(stderr, "mmap(): %s: %s\n", filename, strerror(errno));
        return -1;
    }

    // send length
    snprintf(outBuf, OutBufSize, "%d,", fileLen);
    lenStrLen = strlen(outBuf);
    count = write(fd, outBuf, lenStrLen);
    if (count != lenStrLen) {
        fprintf(stderr, "write(): failed to send : %s (%d)\n", outBuf, fileLen);
       return -1;
    }

    // send file data
    count = write(fd, data, fileLen);
    if (count != fileLen) {
        fprintf(stderr, "write(): failed to send file data count=%d != fileLen"
                "=%d\n", count, fileLen);
        return -1;
    }
//    printf("sent %d bytes\n", count);

    munmap(data, fileLen);
    close(fileFD);
    return 1;
}

SysStatus
Command::process(FDType fd)
{
//    printf("Command::Process : BEGIN :");
//    print();
    if (strcmp(cmdToken, "getFile") == 0) {
        if (numArgs != 1) {
            printf("ERROR : %s requires 1 arguments : file\n",
                   cmdToken);
            return -1;
        }
        if (sendfile(fd, args[0])==-1) {
            return -1;
        }
    } else {
        printf("ERROR: unknown command %s\n", cmdToken);
        return 0;
    }

    return 1;
}

class CmdProtocol {
public:
    static SysStatus parseCmd(FDType fd, Command *cmd);
    static SysStatus terminateResponse(FDType fd) {
        return 0;
    }
};

/* static */ SysStatus
CmdProtocol::parseCmd(FDType fd,Command *cmd)
{
    char *buf = cmd->getBuffer();
    uval bufSize = cmd->getBufSize();
    uval termFound = 0;
    uval arg = 0;
    char c;

    uval bread = read(fd, buf, bufSize);

    if (bread < 0) {
        fprintf(stderr, "Error: server read failed (%d)\n", errno);
        return -1;
    } else if (bread == 0) {
        return -1;
    } else {
        cmd->reset();
        cmd->startCmdToken(0);
        for (uval i=0; i<bread; i++) {
            if (termFound) {
                cmd->startArgToken(&arg, i);
                termFound = 0;
            }
            c=buf[i];
            if (c == ',' || c == '\n') {
                if (!cmd->cmdCompleted()) {
                    cmd->termCmdToken(i);
                } else {
                    cmd->termArgToken(arg, i);
                }
                if (c == '\n') {
                    return 0;
                }
                termFound=1;
            }
        }
    }
    return 0;
}



class Connection {
protected:
    FDType fd;
public:
    virtual uval process() = 0;
    virtual void init(FDType fd) {
//        printf("Connection %p: inited for fd=%d\n", this, fd);
        this->fd = fd;
    }
    virtual void close() = 0;
    virtual ~Connection() { }
};

class CmdConnection : public Connection {
    Command cmd;
public:
    virtual void init(FDType fd) {
        Connection::init(fd);
    }

    virtual void close() {
       delete this;
    }

    virtual uval process() {
        SysStatus rc = CmdProtocol::parseCmd(fd, &cmd);
        if (rc>=0) {
            rc = cmd.process(fd);
            if (rc == 1) {
                rc = CmdProtocol::terminateResponse(fd);
                if (rc>=0) return 1;
            }
            if (rc == 0) return 0;
        }
        handleError(0, rc);
        return 0;
    }

    static void globalInit() {
    }
};

class EchoConnection : public Connection {
    enum { InBufSize = 1024 };
    char inBuf[InBufSize];
public:
    virtual uval process() {
        int count, strLen;

        // receive
        count = read(fd, inBuf, InBufSize);
        if (count == -1) {
            printf("read failed: count=%d\n",count);
            return 0;
        }
        inBuf[count]='\0';
        strLen = strlen(inBuf);
        if (verbose==1)	printf("EchoConnection::process:got: %s\n", inBuf);
        // send
        count = write(fd, inBuf, count);
        if (count != strLen ) {
            printf("write failed: count = %d != strLen = %d\n",
                   count, strLen);
            return 0;
        }
        if (verbose==1)	printf("EchoConnection::process:sent: %s\n", inBuf);
        return 0;
    }

    virtual void close() {
        delete this;
    }

    static void globalInit() {
    }

};

uval
setupListenSock(FDType *fd, PortType *port)
{
    struct sockaddr_in name;
    int one = 1;
    FDType fd_listen;
    socklen_t addrLen = sizeof(struct sockaddr);
    int rc;

    fd_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd_listen < 0) {
        fprintf(stderr, "Error: server socket create failed (%d)\n", errno);
        exit(1);
    }

    rc = setsockopt(fd_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (rc != 0) {
        fprintf(stderr, "setsockopt(SO_REUSEADDR): %s\n", strerror(errno));
        exit(1);
    }

    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (*port) name.sin_port = htons(*port);
    else name.sin_port = 0;

    rc = bind(fd_listen, (struct sockaddr *)&name, addrLen);
    if (rc < 0) {
        fprintf(stderr, "Error: server bind failed (%d)\n", errno);
        close (fd_listen);
        exit(1);
    }

    if (*port == 0) {
        rc = getsockname(fd_listen, (struct sockaddr *)&name, &addrLen);
        if (rc < 0) {
            fprintf(stderr, "Error server getsocketname failed (%d)\n", errno);
            close(fd_listen);
            exit(1);
        }
        *port = ntohs(name.sin_port);
    }
    *fd = fd_listen;
    return 1;
}


class Connections {
    enum {MAX_CON=1000};
    static Connection *cons[MAX_CON];
    static uval num;
    static FDType firstConFD;

    static uval fdToConIdx(FDType fd) {
        if (fd < firstConFD) {
            fprintf(stderr, "oops fd out of range for a connection\n");
            exit(1);
        }
        if ((fd - firstConFD) >= MAX_CON) {
            fprintf(stderr, "oops more connections than I can handle\n");
            return 0;
        }
        return fd - firstConFD;
    }

public:
    static void init(FDType lastListenFD) {
        num = 0;
        firstConFD = lastListenFD + 1;
        for (uval i=0; i<MAX_CON; i++) cons[i]=0;
    }

    static uval openConnectionForFD(Connection * con, FDType fd) {
        uval newConIdx = fdToConIdx(fd);

        if (cons[newConIdx] != NULL) {
            fprintf(stderr, "oops a connection object exists already"
                    "for conIdx=%d fd=%d\n", newConIdx, fd);
            exit(1);
        }
        con->init(fd);
        cons[newConIdx] = con;
        num++;
        return 1;
    }

    static void closeConnectionForFD(FDType fd) {
        uval conIdx = fdToConIdx(fd);
        if (cons[conIdx] != NULL) {
            cons[conIdx]->close();
            cons[conIdx] = NULL;
            num--;
        }
    }

    static uval processConnectionForFD(FDType fd) {
        uval conIdx = fdToConIdx(fd);
        Connection *con = cons[conIdx];
        if (con == NULL) {
            fprintf(stderr, "oops no open connection for fd=%d\n", fd);
            exit(1);
        }
        if (con->process() == 0) {
            closeConnectionForFD(fd);
            return 0;
        }
        return 1;
    }
};

FDType Connections::firstConFD;
uval Connections::num;
Connection *Connections::cons[Connections::MAX_CON];

void
acceptConn(FDType fd_listen, int *max_fd, fd_set *orig_fds, Connection *con)
{
    FDType new_fd;
    new_fd = accept(fd_listen, NULL, NULL);
    if (new_fd < 0) {
        fprintf(stderr, "Error: server accept failed (%d)\n", errno);
    } else {
        if (Connections::openConnectionForFD(con, new_fd)) {
            if (new_fd > *max_fd) *max_fd = new_fd;
            FD_SET(new_fd, orig_fds);
        }
    }
}

void
serverLoop()
{
  fd_set fds, orig_fds;
  FDType fd_echolisten, fd_cmdlisten;
  int max_fd;
  int rc;
  PortType port;

  port = 0;
  setupListenSock(&fd_echolisten, &port);
  rc = listen(fd_echolisten, 8);
  if (rc < 0) {
    fprintf(stderr, "Error: server listen failed (%d)\n", errno);
    exit(1);
  }
  printf("Echo Port = %d\n", port);
  EchoConnection::globalInit();

  port = 0;
  setupListenSock(&fd_cmdlisten, &port);
  rc = listen(fd_cmdlisten, 8);
  if (rc < 0) {
    fprintf(stderr, "Error: server listen failed (%d)\n", errno);
    exit(1);
  }
  printf("Cmd Port = %d\n", port);
  CmdConnection::globalInit();

  FD_ZERO(&orig_fds);
  FD_SET(fd_echolisten, &orig_fds);
  FD_SET(fd_cmdlisten, &orig_fds);

  max_fd = fd_cmdlisten;

  Connections::init(max_fd);

  while (1) {

    fds = orig_fds;

    rc = select(max_fd+1, &fds, NULL, NULL, NULL);

    if (rc < 0) {
      fprintf(stderr, "Error: server select failed (%d)\n", errno);
      exit(1);
    }

    if (rc == 0) {
      fprintf(stderr, "What Select timed out\n");
      exit(1);
    } else {
        if (FD_ISSET(fd_echolisten, &fds)) {
            // got a new echo connection
//            printf("got an echo connection on %d\n", fd_echolisten);
            acceptConn(fd_echolisten, &max_fd, &orig_fds, new EchoConnection);
            rc--;
            FD_CLR(fd_echolisten, &fds);
        }
        if (FD_ISSET(fd_cmdlisten, &fds)) {
            // got a new cmd connection
            printf("got an command connection on %d\n", fd_cmdlisten);
            acceptConn(fd_cmdlisten, &max_fd, &orig_fds,
                       new CmdConnection);
            rc--;
            FD_CLR(fd_cmdlisten, &fds);
        }
      FDType i = 0;
      while (rc > 0 && i <= max_fd) {
          if (FD_ISSET(i, &fds)) {
//              printf("Found activity on fd=%d\n", i);
              if (Connections::processConnectionForFD(i)==0) {
                  FD_CLR(i, &orig_fds);
                  close(i);
              }
          }
          i++;
      }

    }
  }
}

FDType
clientConnect(char *host, int port)
{
    FDType fd;
    int rc;
    struct sockaddr_in localAddr, servAddr;
    struct hostent *h;

    h = gethostbyname(host);
    if (h == NULL) {
        fprintf(stderr, "unknown host '%s'\n", host);
        return -1;
    }

    servAddr.sin_family = h->h_addrtype;
    memcpy((char *) &servAddr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
    servAddr.sin_port = htons(port);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd<0) {
        fprintf(stderr, "unable to create socket");
        perror("oops ");
        return -1;
    }

    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(0);

    rc = bind(fd, (struct sockaddr *)&localAddr, sizeof(localAddr));
    if (rc<0) {
        fprintf(stderr, "cannot bind port TCP %u\n", port);
        perror("oops ");
        return -1;
    }

    rc = connect(fd, (struct sockaddr *) &servAddr, sizeof(servAddr));
    if (rc<0) {
        perror("cannot connect ");
        return -1;
    }
    return fd;
}

void
doDelay()
{
    int i;
    volatile int j=0;

    for (i=0; i<delay; i++) {
        j++;
    }
}

int
echoClient(FDType fd)
{
    int count;
    int strLen = strlen(echoString);
    const int InBufSize = 1048;
    char inBuf[InBufSize];
//    printf("echo string = %s len = %d\n", echoString, strLen);

    // send
    if (assertPoll && !assertFDState(fd, POLLOUT)) {
        printf("echoClient: %d not ready for write\n", fd);
    }

    count = write(fd, echoString, strLen);
    if (count != strLen ) {
        printf("write failed: count = %d != strLen = %d\n",
               count, strLen);
        return 0;
    }

    // receive
    count = read(fd, inBuf, InBufSize);
    if (count == -1) {
        printf("read failed: count=%d\n",count);
        return 0;
    }
    inBuf[count]='\0';

    if (strcmp(inBuf, echoString) != 0) {
        printf("error in echo :  %s  !=  %s\n", echoString, inBuf);
        return 0;
    }
//    printf("got: %s\n", inBuf);
    return 1;
}


int cmdClient(FDType fd)
{
    int strLen = strlen(cmdString);
    int i,count,responseLen,offset;
    const int InBufSize = 1048;
    char inBuf[InBufSize];
    char *response = NULL;
    int infinite = 0, bytesToRead, bytesRead, requests = 0;
    char cmd[MESG_SIZE];

    memcpy(cmd, cmdString, strLen);
    cmd[strLen]='\n';
    cmd[strLen+1]='\0';

    printf("cmd to issue = %s", cmd);
    printf("repeat count = %d\n", repeatCount);
    printf("delay        = %d\n", delay);


    if (repeatCount == 0) { infinite = 1; requests = 0; }

    while (1) {
        if (!infinite && repeatCount==0) break;
        // send
        count = write(fd, cmd, strLen+1);
        if (count != strLen+1 ) {
            printf("write failed: count = %d != strLen+1 = %d\n",
                   count, strLen+1);
            return 0;
        }

        // recieve len of response and any data that will fit in inBuf
        count = read(fd,inBuf,InBufSize);
        if (count <= 0) {
            printf("read failed: count=%d\n",count);
            return 0;
        }
        i=0;
        while (inBuf[i]!=',') i++;
        inBuf[i]='\0';
        responseLen = atoi(inBuf);
        bytesToRead = responseLen;
        i++;

//        printf("count=%d responseLen=%d\n", count, responseLen);

        // prepare memory for entire response
        if (response == NULL) response = (char *)malloc(responseLen+1);
        response[responseLen]='\0';
        // copy and part of the response that ended up in inBuf
        memcpy(response, &inBuf[i], count-i);
        offset = count-i;
        responseLen -= offset;
        bytesRead = offset;

//        printf("responseLen=%d offset=%d i=%d\n", responseLen, offset, i);

        // get any remaining data
        while (responseLen) {
            count = read(fd,(char *)(response + offset),responseLen);
//            printf("after read count = %d reponseLen = %d offset=%d\n",
//                   count, responseLen, offset);
            if (count == -1) {
                printf("read failed: count=%d\n", count);
                return 0;
            }
            offset += count;
            responseLen -= count;
            bytesRead += count;
        }

        if (bytesToRead != bytesRead) {
            printf("read of data failed bytesToRead = %d != bytesRead = %d\n",
                   bytesToRead, bytesRead);
        }

//        printf("response %d: %s\n", responseLen, response);
        if (!infinite) {
            printf("bytesRead = %d\n", bytesRead);
            repeatCount--;
            if (repeatCount) {
                doDelay();
            }
        } else {
            requests++;
            if (requests == 1000) {
                requests = 0;
                write(1,"*",1);
            }
            doDelay();
        }
    }
    if (response) free(response);

    return 0;
}

int usage()
{
  fprintf(stderr, "`sockTestSrv' is a socket stress test\n"
	  "\n"
	  "Usage: sockTestSrv [OPTION [ARG]] ...\n"
	  " -v    Be verbose\n"
	  " -S    Run in server mode\n"
	  " -e M  Run in client echo mode, sending string M\n"
	  " -c C  Run in client command mode, sending command C\n"
	  " -h H  Client connect to host H\n"
          " -p P  Client connect to port P\n"
	  " -r R  Client issue R transcations (0 for infinity)\n"
	  "\n"
	  "Examples:\n"
	  " sockTestSrv -S\n"
	  " sockTestSrv -h 127.0.0.1 -p 32768 -e Hello -r 1024\n"
	  "\n");

  return 0;
}

int
main(int argc, char *argv[])
{
    int rc, infinite = 0, count=0;
    FDType serverFD;

    processArgs(argc, argv);

    if (server) {
        serverLoop();
        fprintf(stderr, "oops server loop terminated\n");
    }

    if (!host) {
	usage();
        fprintf(stderr, "sockTestSrv: FAIL: please use -h to specify host\n");
        return 1;
    }
    printf("host = %s\n", host);

    if (!port) {
        fprintf(stderr, "must specify server port with -p\n");
        return 0;
    }
    printf("port = %d\n", port);


    rc = 1;
    if (clientAction == CMD) {
        serverFD = clientConnect(host, port);
        if (serverFD == -1) return 0;
        rc = cmdClient(serverFD);
        close(serverFD);
    } else if (clientAction == ECHO) {
        if (repeatCount == 0) { infinite = 1; count = 0; }
        while (1) {
            if (!infinite && repeatCount==0) {
                break;
            }
            serverFD = clientConnect(host, port);
            if (serverFD == -1) return 0;
            rc = echoClient(serverFD);
            close(serverFD);
            if (!infinite) {
                repeatCount--;
                write(1,".",1);
                if (repeatCount) {
                    doDelay();
                }
            } else {
                count++;
                if (count == 1000) {  write(1,".",1); count=0; }
                doDelay();
            }
        }
    } else {
        fprintf(stderr, "unknown client action\n");
        rc = 0;
    }
    return rc;
}
