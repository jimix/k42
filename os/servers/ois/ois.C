/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ois.C,v 1.10 2005/08/30 00:48:54 neamtiu Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: dameon code that awakes occasionally and writes
 *                     out tracing buffers
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include <sys/SystemMiscWrapper.H>
#include <sync/MPMsgMgr.H>
#include <usr/ProgExec.H>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <trace/traceTest.h>
#include <trace/traceControl.h>
#include <trace/trace.H>
#include <trace/traceClustObj.h>
#include <sys/systemAccess.H>
#include <stdlib.h>
#include <cobj/COList.H>
#include <sys/time.h>

#ifndef ULLONG_MAX 
# define ULLONG_MAX (~((unsigned long long)0))
#endif

#define SERVER_PORT 3624
#define MESG_SIZE 1024


uval debugOIS = 1;

typedef int FDType;
typedef int PortType;

void 
handleError(SysStatus rc)
{
    fprintf(stderr, "Got an unxpected rc=%ld\n", rc);
}

class OISCommand {
    enum {MaxArgs=10, BufSize=MESG_SIZE};
    char buffer[BufSize];
    char *cmdToken;
    char *args[MaxArgs];
    uval numArgs;
    uval cmdTerminated;
	uval pemLastIndex;
	uval pemLastBuffer;
    SysStatus getObjectList(COListRef coList, FILE *replyStream);
    SysStatus getDiffObjects(FILE *replyStream);
public:
    OISCommand() { reset(); pemLastIndex = 0; pemLastBuffer = 0;}
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
    SysStatus process(COListRef coList, FILE *replyStream);
    void print();
};

class OISProtocol {
public:
    static SysStatus parseCmd(FDType fd, OISCommand *cmd);
    static SysStatus terminateResponse(FILE *replyStream) {
        if (fprintf(replyStream, "ois-done\n") < 0) {
            fprintf(stderr, "error sending termination\n");
            return -1;
        }
        fflush(replyStream);
        return 0;
    }
};

class Connection {
protected:
    FDType fd;
public:
    virtual uval process() = 0;
    virtual void init(FDType fd) {
        printf("Connection %p: inited for fd=%d\n", this, fd);
        this->fd = fd;
    }
    virtual void close() = 0;

    virtual ~Connection() { }
};

class OISConnection : public Connection {
    FILE *replyStream;
    static COListRef kernCOList;
    OISCommand cmd;
public:
    static SysStatus getTargetData(uval addr, uval len, char *buf) {
        return DREF(kernCOList)->readMem(addr, len, buf);
    }
    virtual void init(FDType fd) {
        Connection::init(fd);
        replyStream=fdopen(fd,"w");        
    }

    virtual void close() {
       fclose(replyStream);
       delete this;
    }

    virtual uval process() {
        SysStatus rc = OISProtocol::parseCmd(fd, &cmd);
        if (_SUCCESS(rc)) {
            rc = cmd.process(kernCOList, replyStream);
            if (_SUCCESS(rc)) {
                rc = OISProtocol::terminateResponse(replyStream);
                if (_SUCCESS(rc)) return 1;
            }
        }
        handleError(rc);
        return 0;
    }

    static void globalInit() {
        SysStatus sysRC = COList::Create(kernCOList, _KERNEL_PID);
        tassertMsg(_SUCCESS(sysRC), "rc=%ld\n", sysRC);
    }
};

COListRef OISConnection::kernCOList;

static const char hexchars[]="0123456789abcdef";



class gdbStub {
    enum {BUFMAX=2048};
    char inBuf[BUFMAX];
    char outBuf[BUFMAX];
    char targetData[BUFMAX];

    uval oidx;
    uval iidx;

    uval debug;

    struct _registers {
        uval64 gpr[32];
        uval64 fpr[32];
        uval64 pc;
        uval64 ps;
        uval32 cr;
        uval64 lr;
        uval64 ctr;
        uval32 xer;
        uval32 fpscr;
    } __attribute__((packed));

    static struct _registers registers;

    static void initRegisters() {
        bzero(&registers, sizeof(struct _registers)); 
    }

    uval8 checksumResponse(sval *cnt) 
        {
            uval8 cs = 0;
            uval i;

            for (i = 1; i<BUFMAX; i++) {
                if (outBuf[i] == '\0') break;
                cs += outBuf[i];
            } 
            *cnt = i;
            return cs;
        }

    void setOutBufPosition(uval i) { oidx=i; }

    void setInBufPosition(uval i) { iidx=i; }

    void  initResponse(void)
        {
            setOutBufPosition(0);
            addCharToResponse('$');
            addCharToResponse('\0');
            setOutBufPosition(1);
        }

    void addCharToResponse(char c) 
        {
            if (oidx < (BUFMAX - 1)) {
                outBuf[oidx] = c;
                setOutBufPosition(oidx+1);
            } else {
                passertMsg(0, "overflowed transmit buffer\n");
            }
        }

    void termAndSendResponse(int fd) 
        {
            sval count,n;
            char ch;

            uval8 cs = checksumResponse(&count);
            setOutBufPosition(count);
            addCharToResponse('#');
            addCharToResponse(hexchars[cs >> 4]);
            addCharToResponse(hexchars[cs % 16]);
            do {
                n=write(fd, outBuf, count + 3);
                passertMsg(n==count+3, 
                           "oops write=%ld", n);
                while (read(fd, &ch, 1) < 1) /* no-op */;
            } while (ch != '+');
            addCharToResponse('\0');
        }

    char getCharFromRequest() {
        char ch = inBuf[iidx];
        if (iidx < BUFMAX) iidx++;
        return ch;
    }
    static sval hex(char ch)
        {
            if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
            if ((ch >= '0') && (ch <= '9')) return (ch-'0');
            if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
            return (-1);
        }

    uval  getPacket(int fd) 
        {
            sval count;
            uval8 cs, pcs;

            for (;;) {
                count = read(fd, inBuf, BUFMAX-2);
                if (count == -1) {
                    printf("read failed: count =%ld\n",count);
                    return 0;
                }
                {
                    inBuf[count]='\0';
                    if (debug) printf("read: %s\n",inBuf);
                }

                if ((count >= 4) &&
                    (inBuf[0] == '$') &&
                    (inBuf[count-3] == '#')) {
                    // Might be a valid packet confirm checksum
                    inBuf[count-3] = '\0';
                    pcs = (hex(inBuf[count-2]) << 4) + hex(inBuf[count-1]);
                    cs = 0;
                    for (sval i=1; i<count-3; i++) cs += inBuf[i];
                    if (cs == pcs) {
                        write(fd, "+", 1);
                        if ((count >= 7) && (inBuf[3] == ':')) {
                            printf("hmmm ':' packet\n");
                            write(fd, &(inBuf[1]), 2);
                            setInBufPosition(4);
                        } else {
                            setInBufPosition(1);
                        }
                        return iidx;
                    } else {
                        printf("failed checksum cs=%d pcs=%d\n", cs, pcs);
                    }
                }
                write(fd, "-", 1);
            }
            return 0;
        }

    sval get_char (char *addr) { return *addr; }

    void memTohexResponse(char * mem, sval count) 
        {
            sval i;
            char ch;

            for(i=0; i<count; i++) {
                ch = get_char(mem++);
                addCharToResponse(hexchars[ch >> 4]);
                addCharToResponse(hexchars[ch % 16]);
            }
            addCharToResponse('\0');
        }

    uval64 memword(uval32* wordPtr)
        {
            uval32 word;
            word = *wordPtr;	
            return word;
        }

    sval hexRequestToInt(sval *value, char *nextChar)
        {
            char ch;
            uval numChars = 0;
            uval noMoreHex = 0;            
            sval hexValue;

            *value = 0;
            do {
                ch=getCharFromRequest();
                hexValue = hex(ch);
                if (ch=='\0' || hexValue == -1) noMoreHex=1;
                else {
                    *value = (*value <<4) | hexValue;
                    numChars++;
                }
            } while (!noMoreHex);
            *nextChar = ch;
            return (numChars);
        }    
    void init() { bzero(&targetData, BUFMAX); }
public:

    static void globalInit() { initRegisters(); }
    
    gdbStub() : oidx(0), iidx(0), debug(0) {init();}

    uval process(int fd) 
        {
            char gdbCmdChar;
            initResponse();
            if (getPacket(fd)) {
                gdbCmdChar = getCharFromRequest();
                switch (gdbCmdChar) {
                case '?':
                {
                    // hardcoded to return "software generated" signal
                    sval sigval = 7;
                    addCharToResponse('S');
                    addCharToResponse(hexchars[sigval >> 4]);
                    addCharToResponse(hexchars[sigval % 16]);
                    addCharToResponse('\0');
                }
                break;
                case 'g':
                {
                    memTohexResponse((char*) &registers, 
                                      sizeof(struct _registers));
                    break;
                }
                case 'm': 
                {
                    sval addr,length;
                    uval error = 0;
                    
                    char ch;
                    if (hexRequestToInt(&addr,&ch)) {
                        if (ch == ',') {
                            if (hexRequestToInt(&length,&ch)) {
                                if (length > (sval) sizeof(targetData)) {
                                    addCharToResponse('E');
                                    addCharToResponse('0');
                                    addCharToResponse('3');
                                    error = 2;
                                } else {
                                    OISConnection::getTargetData(addr, length,
                                                                 targetData);
                                    memTohexResponse(targetData, length);
                                }
                            } else  error =  1;
                        } else error = 1;
                    } else error = 1;
                    if (error==1) {
                        addCharToResponse('E');
                        addCharToResponse('0');
                        addCharToResponse('1');
                    }
                    addCharToResponse('\0');
                }
                break;
                case 'k':
                {
                    return 0;
                }
                break;
                default:
                    if (debug) {
                        printf("gdbInspect::process: Unsupported gdb"
                               " command %c\n", gdbCmdChar);
                    }
                    addCharToResponse('\0');
                    break;
                }
                termAndSendResponse(fd);
                if (debug) printf("sent response : %s\n", outBuf);
                return 1;
            }
            printf("getPacket Failed\n");
            return 0;
        }
};

struct gdbStub::_registers gdbStub::registers;


class InspectConnection : public Connection {
    gdbStub gdbInspect;
    static PortType port;
public:
    virtual uval process() {
//        printf("InspectConnection::process: START fd=%d\n", fd);
        return gdbInspect.process(fd);
    }

    virtual void close() {
        delete this;
    }

    static void globalInit(PortType port) { 
        InspectConnection::port = port;
        gdbStub::globalInit(); 
    }

    static PortType getPort() { return port; }
    
};

PortType InspectConnection::port=0;

void 
OISCommand::print()
{
    printf("cmd=%s numArgs=%ld ", cmdToken, numArgs);
    for (uval i=0;i<numArgs;i++) {
        printf("arg[%ld]=%s ",i,args[i]);
    }
    printf("\n");
}

SysStatus 
OISCommand::process(COListRef coList, FILE * replyStream)
{
    SysStatus rc;

    if (debugOIS) {
	   printf("Command::Process : BEGIN :");
       print();
	}

    if (strcmp(cmdToken, "getObjectList") == 0) rc=getObjectList(coList, 
                                                                 replyStream);
	else if (strcmp(cmdToken, "getDiffObjects") == 0) rc=getDiffObjects( 
                                                                 replyStream);
    else if (strcmp(cmdToken, "hotSwapInstance") == 0) {
        if (numArgs != 2) {
            printf("ERROR : %s requires 2 arguments :  factory ref and target"
                   " root\n",cmdToken);
            return -1;
        }
        uval facRef = strtoull(args[0],NULL,0);
        uval tarRoot = strtoull(args[1],NULL,0);
        if (facRef == ULLONG_MAX || tarRoot == ULLONG_MAX) {
            printf("error in converting arguments facRef=%s or tarRef=%s\n",
                   args[0], args[1]);
            return -1;
        }
        printf("facRef = 0x%lx tarRoot=0x%lx\n", facRef, tarRoot);
        rc=DREF(coList)->hotSwapInstance((FactoryRef)facRef, 
                                         (CObjRoot *)tarRoot);
        passertMsg(_SUCCESS(rc), "rc=%ld on call to hotSwapInstance :"
                   "facRef=0x%lx tarRoot=0x%lx\n", rc, facRef, tarRoot);
    }
    else if (strcmp(cmdToken, "takeOverFromFac") == 0) {
        if (numArgs != 2) {
            printf("ERROR : %s requires 2 arguments :  new factory ref "
                   "and old factory ref\n",cmdToken);
            return -1;
        }
        uval newRef = strtoull(args[0],NULL,0);
        uval oldRef = strtoull(args[1],NULL,0);
        if (newRef == ULLONG_MAX || oldRef == ULLONG_MAX) {
            printf("error in converting arguments newRef=%s or oldRef=%s\n",
                   args[0], args[1]);
            return -1;
        }
        printf("newRef = 0x%lx oldRef=0x%lx\n", newRef, oldRef);
        rc=DREF(coList)->takeOverFromFac((FactoryRef)newRef, 
                                         (FactoryRef)oldRef);
        passertMsg(_SUCCESS(rc), "rc=%ld on call to takeOverFromFac :"
                   "newRef=0x%lx oldRef=0x%lx\n", rc, newRef, oldRef);
    }
    else if (strcmp(cmdToken, "printInstances") == 0) {
        if (numArgs != 1) {
            printf("ERROR : %s requires 1 argument :  factory ref\n", cmdToken);
            return -1;
        }
        uval fac = strtoull(args[0],NULL,0);
        if (fac == ULLONG_MAX) {
            printf("error in converting arguments fac=%s\n",
                   args[0]);
            return -1;
        }
        printf("fac = 0x%lx\n", fac);
        rc=DREF(coList)->printInstances((FactoryRef)fac);
        passertMsg(_SUCCESS(rc), "rc=%ld on call to printInstances :"
                   "fac=0x%lx\n", rc, fac);
    }
    else if (strcmp(cmdToken, "getInspectPort") == 0) {
        printf("getInspectPort port = %d\n", InspectConnection::getPort());
        fprintf(replyStream, "%d\n", InspectConnection::getPort());
    }
    else if (strcmp(cmdToken, "testNoResponse") == 0) {
    }
    else if (strcmp(cmdToken, "testResponse") == 0) {
        fprintf(replyStream, "testResponse\n");
    }
	else if (strcmp(cmdToken, "debugOIS") == 0) {
	    debugOIS = 1-debugOIS;
		printf("debugOIS set to %ld\n", debugOIS);
	}
    else {
        printf("ERROR: unknown command %s\n", cmdToken);
        return -1;
    }
    return 1;
}

SysStatus
OISCommand::getObjectList(COListRef coList, FILE *replyStream)
{
    CODesc *coDescs;
    uval num;
    SysStatus rc = DREF(coList)->getAndLockCODescArray(&coDescs, num);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    if (debugOIS) printf("Found %ld objects ", num);
    for (uval i=0; i<num; i++) {
       fprintf(replyStream, "%p %p %p\n", 
               (void *)coDescs[i].getRef(),
               (void *)coDescs[i].getRoot(),
               (void *)coDescs[i].getTypeToken());
    }
    DREF(coList)->unlockCODescArray();
    if (debugOIS) printf("... and sent\n");
    return rc;
}

SysStatus
OISCommand::getDiffObjects(FILE *replyStream)
{

    SysStatusUval rc = 0;
    uval myBuffer, buffersProduced;
    uval currentIndex, currentBuffer;
    uval index, size = 0, buffersReady;

	printf("diff objects called\n");

    volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
    TraceControl *const trcCtrl = trcInfo->traceControl;
    uval64 *const trcArray = trcInfo->traceArray;

    myBuffer = pemLastBuffer % trcInfo->numberOfBuffers;
    buffersReady = trcCtrl->buffersProduced - pemLastBuffer;
	if (buffersReady >= trcInfo->numberOfBuffers) {
	    printf("WARNING: Infrequent polling, events have been lost.\n");
		(void) SwapVolatile(&pemLastBuffer,
				trcCtrl->buffersProduced - 1);
		myBuffer = pemLastBuffer % trcInfo->numberOfBuffers;
	}

	while (!(trcCtrl->writeOkay)) {
	    //printf("blocking on write not okay\n");
	    Scheduler::DelayMicrosecs(1000000); // 1 second
	}

	currentIndex = trcCtrl->index & trcInfo->indexMask;
	currentBuffer = TRACE_BUFFER_NUMBER_GET(currentIndex);

	// Now write out all buffers between pemLastBuffer:pemLastIndex to currentBuffer:currentIndex
	printf ("%ld %ld %ld %ld %lld\n", trcCtrl->buffersProduced, 
					pemLastBuffer, pemLastIndex, currentBuffer, TRACE_BUFFER_OFFSET_GET(currentIndex));
	
	while (myBuffer != currentBuffer) {
		size = (TRACE_BUFFER_SIZE - pemLastIndex) * sizeof(uval64);
		index = myBuffer * TRACE_BUFFER_SIZE + pemLastIndex;

		buffersProduced = trcCtrl->buffersProduced;
		// Second check, just before sending
		if (buffersProduced - pemLastBuffer < trcInfo->numberOfBuffers) { 
			// First send # of bytes we're going to send
			fwrite(&size, sizeof(size), 1, replyStream);
			// Then the actual data itself
			rc = fwrite(&trcArray[index], size, 1, replyStream);
			if (_SGETUVAL(rc) <= 0) printf("stream write failed\n");
			printf("Sent %ld bytes\n", size);
		} else {
			printf("Trace has overlapped ois, skipped a buffer\n");
		}
		pemLastIndex = 0;
		pemLastBuffer++;
		myBuffer = (myBuffer + 1) % trcInfo->numberOfBuffers;
		currentIndex = trcCtrl->index & trcInfo->indexMask;
		currentBuffer = TRACE_BUFFER_NUMBER_GET(currentIndex);
	}

	if (TRACE_BUFFER_OFFSET_GET(currentIndex) > pemLastIndex) {
		size = (TRACE_BUFFER_OFFSET_GET(currentIndex) - pemLastIndex) * sizeof(uval64);
		index = myBuffer * TRACE_BUFFER_SIZE + pemLastIndex;
		// First send # of bytes we're going to send
		fwrite(&size, sizeof(size), 1, replyStream);
		// Then the actual data itself
		rc = fwrite(&trcArray[index], size, 1, replyStream);
		if (_SGETUVAL(rc) <= 0) printf("stream write failed\n");
		printf("Sent %ld bytes\n", size);
	}
	pemLastIndex = TRACE_BUFFER_OFFSET_GET(currentIndex);	
	printf ("%ld %ld %ld %ld %lld\n", trcCtrl->buffersProduced, 
					pemLastBuffer, pemLastIndex, currentBuffer, TRACE_BUFFER_OFFSET_GET(currentIndex));

	// Lastly, send a 9 to indicate ois-done: completion
	size = 9;
	fwrite(&size, sizeof(size), 1, replyStream);

	return rc;
}

/* static */ SysStatus 
OISProtocol::parseCmd(FDType fd, OISCommand *cmd) 
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
        *port = name.sin_port;
    }    
    *fd = fd_listen;
    return 1;
}


class Connections {
    enum {MAX_CON=40};
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
                    "for conIdx=%d fd=%d\n", (int)newConIdx, fd);
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
  FDType fd_oislisten, fd_inspectlisten;
  int max_fd;
  int rc;
  PortType port;

  port = SERVER_PORT;
  setupListenSock(&fd_oislisten, &port);
  rc = listen(fd_oislisten, 8);
  if (rc < 0) {
    fprintf(stderr, "Error: server listen failed (%d)\n", errno);
    exit(1);
  }
  OISConnection::globalInit();

  port = 0;
  setupListenSock(&fd_inspectlisten, &port);  
  rc = listen(fd_inspectlisten, 8);
  if (rc < 0) {
    fprintf(stderr, "Error: server listen failed (%d)\n", errno);
    exit(1);
  }
  printf("Inspect Port = %d\n", port);
  InspectConnection::globalInit(port);

  FD_ZERO(&orig_fds);
  FD_SET(fd_oislisten, &orig_fds);
  FD_SET(fd_inspectlisten, &orig_fds);

  max_fd = fd_inspectlisten;

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
        if (FD_ISSET(fd_oislisten, &fds)) {
            // got a new ois connection
            printf("got an ois connection on %d\n", fd_oislisten);
            acceptConn(fd_oislisten, &max_fd, &orig_fds, new OISConnection);
            rc--;
            FD_CLR(fd_oislisten, &fds);
        }
        if (FD_ISSET(fd_inspectlisten, &fds)) {
            // got a new inspect connection
            printf("got an ispect connection on %d\n", fd_inspectlisten);
            acceptConn(fd_inspectlisten, &max_fd, &orig_fds,
                       new InspectConnection);
            rc--;
            FD_CLR(fd_inspectlisten, &fds);
        }
      FDType i = 0;
      while (rc > 0 && i <= max_fd) {
          if (FD_ISSET(i, &fds)) {
              if (debugOIS) printf("Found activity on fd=%d\n", i);
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


int
main(int argc, char *argv[])
{
    NativeProcess();

    serverLoop();
    
    fprintf(stderr, "oops server loop terminated\n");

    return 0; 
}



