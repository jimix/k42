/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mkserv.c,v 1.33 2004/08/20 17:30:48 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: mkserv - shared tool for taking a set of files
 *                     and packing them together putting a header at the
 *                     beginning indicating where each starts
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#if defined(PLATFORM_OS_Linux) || defined(PLATFORM_OS_Darwin) || defined(__linux)
#include <stdint.h>
#include <string.h>
#include <gelf.h>
#else /* #if defined(PLATFORM_OS_Linux) || defined(PLATFORM_OS_Darwin) */
#include <strings.h>
#endif /* #if defined(PLATFORM_OS_Linux) || defined(PLATFORM_OS_Darwin) */
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

typedef int sval;
#include "bootServers.H"

#ifndef O_BINARY
#define O_BINARY 0 /* O_BINARY is only defined on Windows, apparently. */
#endif /* #ifndef O_BINARY */


/* ENDIAN STUFF */


#define CONVE(v)	((((v) >> 56) & 0x00000000000000ffULL) | \
			 (((v) >> 40) & 0x000000000000ff00ULL) | \
			 (((v) >> 24) & 0x0000000000ff0000ULL) | \
                         (((v) >>  8) & 0x00000000ff000000ULL) | \
                         (((v) <<  8) & 0x000000ff00000000ULL) | \
                         (((v) << 24) & 0x0000ff0000000000ULL) | \
                         (((v) << 40) & 0x00ff000000000000ULL) | \
                         (((v) << 56) & 0xff00000000000000ULL) )

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
# ifdef __BYTE_ORDER
#  if __BYTE_ORDER == __LITTLE_ENDIAN
#    define __LITTLE_ENDIAN__
#  else /* #if __BYTE_ORDER == __LITTLE_ENDIAN */
#    define __BIG_ENDIAN__
#  endif /* #if __BYTE_ORDER == __LITTLE_ENDIAN */
# else /* #ifdef __BYTE_ORDER */
#  define __BIG_ENDIAN__   // defined BIG_ENDIAN by default
# endif /* #ifdef __BYTE_ORDER */
#endif /* #if !defined(__BIG_ENDIAN__) && ... */


#if defined(__BIG_ENDIAN__)
#define BE(v) (v)
#define LE(v) (CONVE(v))
#elif defined(__LITTLE_ENDIAN__)
#define BE(v) (CONVE(v))
#define LE(v) (v)
#else /* #if defined(__BIG_ENDIAN__) */
#error "Need to define either __BIG_ENDIAN__ or __LITTLE_ENDIAN__"
#endif /* #if defined(__BIG_ENDIAN__) */

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* global variables */

static char *progname;

FILE *fout = stdout;  /* -o: output file */

int verbose = 0;     /* -v: verbose output */

/* must implement these */
static void (*C_func)(FILE *, const char *, int) = 0;
static void (*D_func)(FILE *, uint64_t, uint64_t ) = 0;
static void (*Z_func)(FILE *, int) = 0;


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

void *xmalloc(int size, char *why)
{
  void *p;

  p = malloc(size);
  if (p == NULL) {

    fprintf(stderr, "*** no malloc room for %s (%d)\n", why, size);
    exit(-2);
  }
  return (p);
}

char *remember(char *p)
{
  long len;
  char *cp;

  if (p == NULL)
    return (NULL);

  len = strlen(p);
  if (len == 0)
    return (NULL);

  /* allocate space for and copy string to save it */
  cp = (char *) xmalloc(len + 1, "remember");
  (void) strncpy(cp, p, len + 1);
  return (cp);
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* The program is driven by a list of file names.  Files
   can be defined on the command line or in a file (-F option).
   However the files are specified, the output of scanning
   the arguments is a list of file names */

/* file name list element */
struct fnle
{
  struct fnle *next;
  char *filename;
};

/* we keep the file name list as a queue: FIFO */
struct fnle *List = NULL;
struct fnle *List_end = NULL;

void add_file_name_to_list(char *filename)
{
  struct fnle *f;

  if (verbose) fprintf(stderr, "\t%s\n", filename);
  f = (struct fnle *)xmalloc(sizeof(struct fnle), "file name element");
  f->filename = remember(filename);

  /* add new element to end of list */
  if (List_end == NULL) {
    List = f;
  }
  else {
    List_end->next = f;
  }
  f->next = NULL;
  List_end = f;
}

int emptyFileList()
{
  return (List == NULL);
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */


//FIXME these should be retrieved from standard include locations
#undef PAGE_SIZE
#define LOG_PAGE_SIZE 12
#define PAGE_SIZE (1 << LOG_PAGE_SIZE)
#define PAGE_MASK (~(PAGE_SIZE-1))
#define PAGE_ROUND_UP(x) (((x)+(PAGE_SIZE-1)) & PAGE_MASK)



/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/*
   Output is generated for different platforms by the following
   routines.  Each platform needs 3 functions:

   *_C -- to print code
   *_D -- to print data
   *_Z -- to reserve space for all-zero data

   A set of pointer functions (C_func, D_func, Z_func) is set
   to point to one of the following sets.
*/


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

static void
powerpc_C(FILE *fp, const char *name, int log_align)
{
  fprintf(fp,
	  ".csect .data[RW], %u\n"
	  "\t.globl\t%s\n"
	  "%s:\n",
	  log_align, name, name);
}

static void
powerpc_D(FILE *fp, uint64_t val0, uint64_t val1)
{
  fprintf(fp, ".llong 0x%llx\t, 0x%llx\n", BE(val0), BE(val1));
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

static void
mips64_C(FILE *fp, const char *name, int log_align)
{
  fprintf(fp,
	  ".data\n"
	  "\t.align %u\n"
	  "\t.globl\t%s\n"
	  "%s:\n",
	  log_align, name, name);
}

static void
mips64_D(FILE *fp, uint64_t val0, uint64_t val1)
{
  fprintf(fp, ".dword 0x%llx\t;\t.dword 0x%llx\n", BE(val0), BE(val1));
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

static void
gas_C(FILE *fp, const char *name, int log_align)
{
  fprintf(fp,
	  ".data\n"
	  "\t.p2align %u\n"  /* use .p2align uses log (2^x) */
	  "\t.globl\t%s\n"
	  "%s:\n",
	  log_align, name, name);
}

static void
gas_DLE(FILE *fp, uint64_t val0, uint64_t val1)
{
  fprintf(fp, "\t.quad 0x%016llx\n", LE(val0));
  fprintf(fp, "\t.quad 0x%016llx\n", LE(val1));
}

static void
gas_DBE(FILE *fp, uint64_t val0, uint64_t val1)
{
  fprintf(fp, "\t.quad 0x%016llx\n", BE(val0));
  fprintf(fp, "\t.quad 0x%016llx\n", BE(val1));
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */


static void
all_Z(FILE *fp, int n)
{
  fprintf(fp, "\t.space 0x%x\n", n * 16);
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

void
usage(void)
{
  fprintf(stderr,
	  "Usage: %s -t <arch> [-o <ofile>] "
	  "<[-F <lfile> [-F <lfile>] ...] | [file [file] ...]>\n"
	  "  -t <arch>         generate assembler for target <arch>.\n"
	  "                      currently: powerpc | mips64 | amd64.\n"
	  "  -g                generate assembler for gas.\n"
	  "  -F <lfile>        read list of input files from <lfile>.\n"
	  "  -o <ofile>        output to <ofile> (`-' is stdout).\n"
	  "\n", progname);
  exit(-1);
}


/* the files that we want to process may be given as
   a list of file names, one per line, in a file.  If
   so, step thru this file and add each file name to
   the list of files that we will process later. */

void step_thru_file_list(char *filelist)
{
  /* read thru the list of file names in the file list and
     put them on the list of file names to process */
  char fpath[1024];
  FILE *fp;

  /* open the file list */
  if (verbose) fprintf(stderr, "    get files from %s\n", filelist);
  fp = fopen(filelist, "r");
  if (fp == NULL) {
    fprintf(stderr, "*** %s: %s: %s\n",
	    progname, filelist , strerror(errno));
    usage();
  }

  /* for each line of the file list, read the file name */
  while (fgets(fpath, sizeof(fpath), fp) != NULL) {
    size_t sz = strlen(fpath);
    if (fpath[sz - 1] == '\n') {
      /* strip new line */
      fpath[sz - 1] = '\0';
    }

    /* and add it to the list of files */
    add_file_name_to_list(fpath);
  }

  /* open the file list */
  fclose(fp);
}


void scanargs(int argc, char **argv)
{
  const char *optstring = "gvo:F:t:";
  extern int optind;
  int c;
  int i;

  /* get program name, for error messages */
  progname = argv[0];

  /* step thru all the arguments */

  while ((c = getopt(argc, argv, optstring)) != EOF) {
    switch (c) {

    case 'o':
      /* define output file name */
      if (!(optarg[0] == '-' && optarg[1] == '\0')) {
	fout = fopen(optarg, "w+");
	if (fout == NULL) {
	  fprintf(stderr, "*** %s: %s: %s\n",
		  progname, optarg, strerror(errno));
	  usage();
	}
      }
      break;

    case 'F':
      /* define file name of list of files */
      step_thru_file_list(optarg);
      break;

    case 't':
      /* define target architecture (REQUIRED) */
      if (strcasecmp(optarg, "mips64") == 0) {
	C_func = mips64_C;
	D_func = mips64_D;
      } else if (strcasecmp(optarg, "powerpc") == 0) {
	C_func = powerpc_C;
	D_func = powerpc_D;
      } else if (strcasecmp(optarg, "gasBE") == 0) {
	C_func = gas_C;
	D_func = gas_DBE;
      } else if (strcasecmp(optarg, "gasLE") == 0) {
	C_func = gas_C;
	D_func = gas_DLE;
      } else {
	fprintf(stderr, "*** %s: Invalid target: %s\n",
		progname, optarg);
	usage();
      }
      Z_func = all_Z;
      break;

    case 'g':
      /* output in "gas" assembler format */
      fprintf(stderr, "*** %s: Generate for gas not supported yet.\n",
	      progname);
      usage();
      break;

    case 'v':
      /* verbose output */
      verbose = 1;     /* -v: verbose output */
      break;

    case '?':
    default:
      /* anything else */
      usage();
    }
  }


  /* now if we still have not defined a target -- quit */
  if (C_func == NULL || D_func == NULL || Z_func == NULL) {
    fprintf(stderr, "*** %s: A target [-t] must be specified!\n",
	    progname);
    usage();
  }

  /* for all the actual file names at the end of the options,
     add them to the list of files to process */
  if (optind < argc) {
    if (verbose) fprintf(stderr, "Add files from command line\n");
    for (i=optind; i<argc; i++) {
      add_file_name_to_list(argv[i]);
    }
  }

  /* or if we still have not defined any files -- quit */
  if (emptyFileList()) {
    fprintf(stderr, "*** %s: Warning: no files specified\n", progname);
  }

}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* get the size of a file, by name */

size_t
get_size(const char *serv_filename)
{
  struct stat sbuf;
  if (stat(serv_filename, &sbuf) != 0) {
    fprintf(stderr, "*** %s: %s: %s\n",
	    progname, serv_filename, strerror(errno));
    return (0);
  }
  return (sbuf.st_size);
}

/* read the file (given by name), into memory (at given address) */
size_t
add_file(const char *serv_filename, void *serv_file)
{
  int fd;
  struct stat serv_info;
  int check_size;

  stat(serv_filename, &serv_info);

  /* open the file */
  fd = open(serv_filename,  O_RDONLY | O_BINARY);
  if (fd == -1) {
    fprintf(stderr, "*** %s: %s: %s\n",
	    progname, serv_filename, strerror(errno));
    exit(-3);
  }

  /* read the file */
  check_size = read(fd, serv_file, serv_info.st_size);

  if (check_size != serv_info.st_size) {
    fprintf(stderr, "*** %s: error in reading in %s\n",
	    progname, serv_filename);
    close(fd);
    exit(-4);
  }

  /* close the file */
  close(fd);

  return (check_size);
}



/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

int
main(int argc, char **argv)
{
  struct fnle *p;

  int i;
  char serv_filename[128];
  int check_size;
  int out_size;
  size_t big_size;
  char *serv_file;
  char *out_file;
  bootServerHeader *hdr;
  int offset_val;
  int zcount;
  char *name;
  char *slash;


  scanargs(argc,argv);

  out_size = PAGE_SIZE; /* size of header information */
  big_size = 0;

  /* go thru and first get the sizes of all files */
  for (p = List; p != NULL; p=p->next) {
    size_t sz;
    sz = get_size(p->filename);
    if (sz == 0) {
      usage();
    } else if (sz > big_size) {
      big_size = sz;
    }

    out_size += PAGE_ROUND_UP(sz);
  }

#ifdef DEBUG
  if (verbose) fprintf(stderr, "Maximum file size is %d bytes\n", big_size);
  if (verbose) fprintf(stderr, "Output file size is %d bytes\n", out_size);
#endif /* #ifdef DEBUG */

  /* make enough room for largest file */
  serv_file = (char *)xmalloc(big_size, "serv_file");
  out_file = (char *)xmalloc(out_size, "out_file");
  bzero(out_file, out_size);

  hdr = (bootServerHeader *)out_file;
  offset_val = PAGE_SIZE;  /* start writing files right after header */


  /* now go through updating header and copying files to out_file */
  for (p = List; p != NULL; p=p->next) {
    strcpy(serv_filename, p->filename);
    check_size =  add_file(serv_filename, serv_file);

    if (check_size == 0) {
      usage();
    }

    bcopy(serv_file,
	  out_file + offset_val,
	  check_size);

    /* use last component of filename as the image name */
    name = ((slash = strrchr(serv_filename, '/')) == NULL) ?
      serv_filename : slash + 1;

    if (verbose)
      fprintf(stderr, "    put %s (size %d) at offset %6d (%s) \n",
	      name, check_size, offset_val, p->filename);

    hdr->name(name);
    hdr->offset(offset_val);
    hdr->size(PAGE_ROUND_UP(check_size));
    hdr++;
    offset_val += PAGE_ROUND_UP(check_size);
  }


  hdr->offset(0);	/* mark the end of the list */
  hdr->size(0);


  /* ******************************************************** */
  /*								*/
  /*								*/
  /* ******************************************************** */

  C_func(fout, "bootServers", LOG_PAGE_SIZE);
  zcount = 0;
  for (i = 0; i < out_size; i += 16) {
    uint64_t *val = (uint64_t *) &out_file[i];
    if (val[0] == 0 && val[1] == 0) {
      zcount++;
    } else {
      if (zcount == 1) {
	D_func(fout, 0, 0);
	zcount = 0;
      }
      else if (zcount > 1) {
	Z_func(fout, zcount);
	zcount = 0;
      }
      D_func(fout, val[0], val[1]);
    }
  }
  if (zcount > 0) {
    Z_func(fout, zcount);
    zcount = 0;
  }
}
