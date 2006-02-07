/*
 * LinuxEvents.h
 *
 * Copyright (C) 2000, 2001, 2002 Karim Yaghmour (karym@opersys.com).
 *
 * This header is distributed under GPL.
 *
 * Linux events being traced.
 *
 * History : 
 *    K.Y., 31/05/1999, Initial typing.
 *
 */

#ifndef __TRACE_TOOLKIT_LINUX_HEADER__
#define __TRACE_TOOLKIT_LINUX_HEADER__

#include <LTTTypes.h>

/* Traced events */
#define TRACE_START           0    /* This is to mark the trace's start */
#define TRACE_SYSCALL_ENTRY   1    /* Entry in a given system call */
#define TRACE_SYSCALL_EXIT    2    /* Exit from a given system call */
#define TRACE_TRAP_ENTRY      3    /* Entry in a trap */
#define TRACE_TRAP_EXIT       4    /* Exit from a trap */
#define TRACE_IRQ_ENTRY       5    /* Entry in an irq */
#define TRACE_IRQ_EXIT        6    /* Exit from an irq */
#define TRACE_SCHEDCHANGE     7    /* Scheduling change */
#define TRACE_KERNEL_TIMER    8    /* The kernel timer routine has been called */
#define TRACE_SOFT_IRQ        9    /* Hit key part of soft-irq management */
#define TRACE_PROCESS        10    /* Hit key part of process management */
#define TRACE_FILE_SYSTEM    11    /* Hit key part of file system */
#define TRACE_TIMER          12    /* Hit key part of timer management */
#define TRACE_MEMORY         13    /* Hit key part of memory management */
#define TRACE_SOCKET         14    /* Hit key part of socket communication */
#define TRACE_IPC            15    /* Hit key part of inter-process communication */
#define TRACE_NETWORK        16    /* Hit key part of network communication */

#define TRACE_BUFFER_START   17    /* Mark the begining of a trace buffer */
#define TRACE_BUFFER_END     18    /* Mark the ending of a trace buffer */
#define TRACE_NEW_EVENT      19    /* New event type */
#define TRACE_CUSTOM         20    /* Custom event */

#define TRACE_CHANGE_MASK    21    /* Change in event mask */
#define TRACE_HEARTBEAT      22    /* Heartbeat event */

/* Number of traced events */
#define TRACE_MAX            TRACE_HEARTBEAT

/* 64-bit word logged at the beginning of every trace event */
typedef union _trace_event_header {
	struct {
		uint32_t timestamp;     /* time stamp */
		uint8_t event_id;       /* event ID */
		uint8_t event_sub_id;   /* sub ID (for events which use them) */
		uint16_t size;          /* size (in bytes), including header */
	} LTT_PACKED_STRUCT x;
	uint64_t raw;
} trace_event_header;

/* Architecture types */
#define TRACE_ARCH_TYPE_I386                1   /* i386 system */
#define TRACE_ARCH_TYPE_PPC                 2   /* PPC system */
#define TRACE_ARCH_TYPE_SH                  3   /* SH system */
#define TRACE_ARCH_TYPE_S390                4   /* S/390 system */
#define TRACE_ARCH_TYPE_MIPS                5   /* MIPS system */
#define TRACE_ARCH_TYPE_ARM                 6   /* ARM system */
#define TRACE_ARCH_TYPE_PPC64               7   /* PPC64 system */

/* Standard definitions for variants */
#define TRACE_ARCH_VARIANT_NONE             0   /* Main architecture implementation */

/* PowerPC variants */
#define TRACE_ARCH_VARIANT_PPC_4xx          1   /* 4xx systems (IBM embedded series) */
#define TRACE_ARCH_VARIANT_PPC_6xx          2   /* 6xx/7xx/74xx/8260/POWER3 systems (desktop flavor) */
#define TRACE_ARCH_VARIANT_PPC_8xx          3   /* 8xx system (Motoral embedded series) */
#define TRACE_ARCH_VARIANT_PPC_ISERIES      4   /* 8xx system (iSeries) */
#define TRACE_ARCH_VARIANT_PPC_PSERIES      5

/* System types */
#define TRACE_SYS_TYPE_VANILLA_LINUX        1   /* Vanilla linux kernel  */
#define TRACE_SYS_TYPE_RTAI_LINUX           2   /* RTAI patched linux kernel */

/* The information logged when the tracing is started */
#define TRACER_MAGIC_NUMBER        0x00D6B7ED     /* That day marks an important historical event ... */
#define TRACER_SUP_VERSION_MAJOR            2     /* Major version number */
#define TRACER_SUP_VERSION_MINOR            3     /* Minor version number */
typedef struct _trace_start
{
  uint32_t           MagicNumber;      /* Magic number to identify a trace */
  uint32_t           ArchType;         /* Type of architecture */
  uint32_t           ArchVariant;      /* Variant of the given type of architecture */
  uint32_t           SystemType;       /* Operating system type */
  uint32_t           BufferSize;       /* Size of buffers */
  uint8_t            MajorVersion;     /* Major version of trace */
  uint8_t            MinorVersion;     /* Minor version of trace */
  uint8_t            UseTSC;	       /* Are we using TSCs or time deltas */
  uint8_t            CPUID;            /* The CPU ID for this trace */
  uint8_t            FlightRecorder;   /* Is this a flight recorder trace? */

  trace_event_mask   EventMask LTT_ALIGNED_FIELD; /* The event mask */
  trace_event_mask   DetailsMask LTT_ALIGNED_FIELD; /* Are the event details logged */
} LTT_PACKED_STRUCT trace_start;
#define START_EVENT(X) ((trace_start*)X)

/*  TRACE_SYSCALL_ENTRY */
typedef struct _trace_syscall_entry
{
  uint8_t        syscall_id;   /* Syscall entry number in entry.S */
  ltt_hostptr_t  address LTT_ALIGNED_FIELD; /* Address from which call was made */
} LTT_PACKED_STRUCT trace_syscall_entry;
#define SYSCALL_EVENT(X) ((trace_syscall_entry*)X)

/*  TRACE_TRAP_ENTRY */
typedef struct _trace_trap_entry
{
  uint16_t       trap_id;     /* Trap number */
  ltt_hostptr_t  address LTT_ALIGNED_FIELD; /* Address where trap occured */
} LTT_PACKED_STRUCT trace_trap_entry;
typedef struct _trace_trap_entry_s390
{
  uint64_t       trap_id;     /* Trap number */
  ltt_hostptr_t  address LTT_ALIGNED_FIELD; /* Address where trap occured */
} LTT_PACKED_STRUCT trace_trap_entry_s390;
#define TRAP_EVENT(X) ((trace_trap_entry*)X)
#define TRAP_EVENT_S390(X) ((trace_trap_entry_s390*)X)

/*  TRACE_IRQ_ENTRY */
typedef struct _trace_irq_entry
{
  uint8_t  irq_id;      /* IRQ number */
  uint8_t  kernel;      /* Are we executing kernel code */
} LTT_PACKED_STRUCT trace_irq_entry;
#define IRQ_EVENT(X) ((trace_irq_entry*)X)

/*  TRACE_SCHEDCHANGE */ 
typedef struct _trace_schedchange
{
  uint32_t  out;         /* Outgoing process */
  uint32_t  in;          /* Incoming process */
  uint32_t  out_state;   /* Outgoing process' state */
} LTT_PACKED_STRUCT trace_schedchange;
#define SCHED_EVENT(X) ((trace_schedchange*)X)

/*  TRACE_SOFT_IRQ */
#define TRACE_SOFT_IRQ_BOTTOM_HALF        1  /* Conventional bottom-half */
#define TRACE_SOFT_IRQ_SOFT_IRQ           2  /* Real soft-irq */
#define TRACE_SOFT_IRQ_TASKLET_ACTION     3  /* Tasklet action */
#define TRACE_SOFT_IRQ_TASKLET_HI_ACTION  4  /* Tasklet hi-action */
typedef struct _trace_soft_irq
{
  ltt_hostptr_t  event_data;       /* Data associated with event */
} LTT_PACKED_STRUCT trace_soft_irq;
#define SOFT_IRQ_EVENT(X) ((trace_soft_irq*)X)

/*  TRACE_PROCESS */
#define TRACE_PROCESS_KTHREAD     1  /* Creation of a kernel thread */
#define TRACE_PROCESS_FORK        2  /* A fork or clone occured */
#define TRACE_PROCESS_EXIT        3  /* An exit occured */
#define TRACE_PROCESS_WAIT        4  /* A wait occured */
#define TRACE_PROCESS_SIGNAL      5  /* A signal has been sent */
#define TRACE_PROCESS_WAKEUP      6  /* Wake up a process */
typedef struct _trace_process
{
  uint32_t       event_data1;     /* Data associated with event */
  ltt_hostptr_t  event_data2 LTT_ALIGNED_FIELD;    
} LTT_PACKED_STRUCT trace_process;
#define PROC_EVENT(X) ((trace_process*)X)

/*  TRACE_FILE_SYSTEM */
#define TRACE_FILE_SYSTEM_BUF_WAIT_START  1  /* Starting to wait for a data buffer */
#define TRACE_FILE_SYSTEM_BUF_WAIT_END    2  /* End to wait for a data buffer */
#define TRACE_FILE_SYSTEM_EXEC            3  /* An exec occured */
#define TRACE_FILE_SYSTEM_OPEN            4  /* An open occured */
#define TRACE_FILE_SYSTEM_CLOSE           5  /* A close occured */
#define TRACE_FILE_SYSTEM_READ            6  /* A read occured */
#define TRACE_FILE_SYSTEM_WRITE           7  /* A write occured */
#define TRACE_FILE_SYSTEM_SEEK            8  /* A seek occured */
#define TRACE_FILE_SYSTEM_IOCTL           9  /* An ioctl occured */
#define TRACE_FILE_SYSTEM_SELECT         10  /* A select occured */
#define TRACE_FILE_SYSTEM_POLL           11  /* A poll occured */
typedef struct _trace_file_system
{
  uint32_t  event_data1;    /* Event data */
  uint32_t  event_data2;    /* Event data 2 */
} LTT_PACKED_STRUCT trace_file_system;
#define FS_EVENT(X) ((trace_file_system*)X)
#define FS_EVENT_FILENAME(X) ((char*) ((X) + sizeof(trace_file_system)))

/*  TRACE_TIMER */
#define TRACE_TIMER_EXPIRED      1  /* Timer expired */
#define TRACE_TIMER_SETITIMER    2  /* Setting itimer occurred */
#define TRACE_TIMER_SETTIMEOUT   3  /* Setting sched timeout occurred */
typedef struct _trace_timer
{
  uint8_t   event_sdata;     /* Short data */
  uint32_t  event_data1;     /* Data associated with event */
  uint32_t  event_data2;     
} LTT_PACKED_STRUCT trace_timer;
#define TIMER_EVENT(X) ((trace_timer*)X)

/*  TRACE_MEMORY */
#define TRACE_MEMORY_PAGE_ALLOC        1  /* Allocating pages */
#define TRACE_MEMORY_PAGE_FREE         2  /* Freing pages */
#define TRACE_MEMORY_SWAP_IN           3  /* Swaping pages in */
#define TRACE_MEMORY_SWAP_OUT          4  /* Swaping pages out */
#define TRACE_MEMORY_PAGE_WAIT_START   5  /* Start to wait for page */
#define TRACE_MEMORY_PAGE_WAIT_END     6  /* End to wait for page */
typedef struct _trace_memory
{
  ltt_hostptr_t  event_data;      /* Data associated with event */
} LTT_PACKED_STRUCT trace_memory;
#define MEM_EVENT(X) ((trace_memory*)X)

/*  TRACE_SOCKET */
#define TRACE_SOCKET_CALL     1  /* A socket call occured */
#define TRACE_SOCKET_CREATE   2  /* A socket has been created */
#define TRACE_SOCKET_SEND     3  /* Data was sent to a socket */
#define TRACE_SOCKET_RECEIVE  4  /* Data was read from a socket */
typedef struct _trace_socket
{
  uint32_t       event_data1;     /* Data associated with event */
  ltt_hostptr_t  event_data2 LTT_ALIGNED_FIELD; /* Data associated with event */
} LTT_PACKED_STRUCT trace_socket;
#define SOCKET_EVENT(X) ((trace_socket*)X)

/*  TRACE_IPC */
#define TRACE_IPC_CALL            1  /* A System V IPC call occured */
#define TRACE_IPC_MSG_CREATE      2  /* A message queue has been created */
#define TRACE_IPC_SEM_CREATE      3  /* A semaphore was created */
#define TRACE_IPC_SHM_CREATE      4  /* A shared memory segment has been created */
typedef struct _trace_ipc
{
  uint32_t       event_data1;     /* Data associated with event */
  ltt_hostptr_t  event_data2 LTT_ALIGNED_FIELD; /* Data associated with event */
} LTT_PACKED_STRUCT trace_ipc;
#define IPC_EVENT(X) ((trace_ipc*)X)

/*  TRACE_NETWORK */
#define TRACE_NETWORK_PACKET_IN   1  /* A packet came in */
#define TRACE_NETWORK_PACKET_OUT  2  /* A packet was sent */
typedef struct _trace_network
{
  uint32_t event_data;     /* Event data */
} LTT_PACKED_STRUCT trace_network;
#define NET_EVENT(X) ((trace_network*)X)

typedef struct _trace_timeval_32
{ 
  uint32_t           tv_sec;
  uint32_t           tv_usec;
} LTT_PACKED_STRUCT trace_timeval_32;

/* Start of trace buffer information */
typedef struct _trace_buffer_start
{
  trace_timeval_32   Time;    /* Time stamp of this buffer */
  uint32_t           TSC;     /* TSC of this buffer, if applicable */
  uint32_t           ID;      /* Unique buffer ID */
} LTT_PACKED_STRUCT trace_buffer_start;

/* End of trace buffer information */
typedef struct _trace_buffer_end
{
  trace_timeval_32   Time;    /* Time stamp of this buffer */
  uint32_t           TSC;     /* TSC of this buffer, if applicable */
} LTT_PACKED_STRUCT trace_buffer_end;

/* Maximal size a custom event can have */
#define CUSTOM_EVENT_MAX_SIZE        8192

/* String length limits for custom events creation */
#define CUSTOM_EVENT_TYPE_STR_LEN      20
#define CUSTOM_EVENT_DESC_STR_LEN     100
#define CUSTOM_EVENT_FORM_STR_LEN     256

/* Type of custom event formats */
#define CUSTOM_EVENT_FORMAT_TYPE_NONE   0
#define CUSTOM_EVENT_FORMAT_TYPE_STR    1
#define CUSTOM_EVENT_FORMAT_TYPE_HEX    2
#define CUSTOM_EVENT_FORMAT_TYPE_XML    3
#define CUSTOM_EVENT_FORMAT_TYPE_IBM    4

typedef struct _trace_new_event
{
  /* Basics */
  uint32_t         id;                                /* Custom event ID */
  char             type[CUSTOM_EVENT_TYPE_STR_LEN];   /* Event type description */
  char             desc[CUSTOM_EVENT_DESC_STR_LEN];   /* Detailed event description */

  /* Custom formatting */
  uint32_t         format_type;                       /* Type of formatting */
  char             form[CUSTOM_EVENT_FORM_STR_LEN];   /* Data specific to format */
} LTT_PACKED_STRUCT trace_new_event;
#define NEW_EVENT(X) ((trace_new_event*) X)

typedef struct _trace_custom
{
  uint32_t           id;          /* Event ID */
  uint32_t           data_size;   /* Size of data recorded by event */
} LTT_PACKED_STRUCT trace_custom;
#define CUSTOM_EVENT(X) ((trace_custom*) X)

/* TRACE_CHANGE_MASK */
typedef struct _trace_change_mask
{
  trace_event_mask          mask;       /* Event mask */
} LTT_PACKED_STRUCT trace_change_mask;
#define CHMASK_EVENT(X) ((trace_change_mask*) X)

#endif /* __TRACE_TOOLKIT_LINUX_HEADER__ */
