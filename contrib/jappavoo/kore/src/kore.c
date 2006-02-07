#if defined (HAVE_CONFIG_H)
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>

#include <readline.h>
#include <history.h>
#include <stdlib.h>
#include <tcl.h>
#include <sys/types.h>
#include <sys/stat.h>

extern HIST_ENTRY **history_list ();
char *temp, *prompt;
int done, retval;
Tcl_Interp *interp;
char buf[1024]; 
char *startupFile;

extern int      optind, opterr;
extern char     *optarg;


#define STARTUPFILENAME "kore.tcl"


void
processArgs(int argc, char *argv[])
{
    int c,err=0;
    static char optstr[] = "f:";

    startupFile=NULL;

    opterr=1;
    while ((c = getopt(argc, argv, optstr)) != EOF)
        switch (c) {
        case 'f' :
            startupFile = optarg;
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


main (int argc, char *argv[])
{
    int startupFileMalloc = 0;
    struct stat statStruct;

    processArgs(argc, argv);

    interp = Tcl_CreateInterp ();
    Tcl_FindExecutable (argv[0]);
    
    interp = Tcl_CreateInterp ();
    
    if (TCL_OK != Tcl_Init (interp)) {
        fprintf (stderr, "Tcl_Init error: %s\n", Tcl_GetStringResult (interp));
        exit (EXIT_FAILURE);
    }
    
    if (startupFile == NULL) {
        char *koreDir=getenv("KORE_LIBDIR");
        
        if (koreDir == NULL) {
            fprintf(stderr, "KORE: **** Error : KORE_LIBDIR must be set\n");
            exit(EXIT_FAILURE);
        } 
        
        startupFile = 
            (char *)malloc(strlen(koreDir)+strlen(STARTUPFILENAME)+2);
        sprintf(startupFile, "%s/tcl/%s", koreDir, STARTUPFILENAME);
        startupFileMalloc=1;
    }

    if (stat(startupFile, &statStruct) == -1) {
        fprintf(stderr, "KORE: **** ERROR: stat of %s failed\n");
        exit(EXIT_FAILURE);
    }
    
    if ((S_IRUSR & statStruct.st_mode) ||
        (S_IRGRP & statStruct.st_mode) ||
        (S_IROTH & statStruct.st_mode)) {
        Tcl_SetVar(interp, "tcl_rcFileName", startupFile, TCL_GLOBAL_ONLY);
        Tcl_SourceRCFile(interp);
    } else {
        fprintf(stderr, "KORE: *** ERROR: cannot read startup file%s\n",
                startupFile);
    }

    if (startupFileMalloc) free(startupFile);
    temp = (char *)NULL;
    prompt = "kore$ ";
    done = 0;
    
    while (!done) {
        temp = readline (prompt);
        
        /* Test for EOF. */
        if (!temp)
            exit (1);
        
        /* If there is anything on the line, print it and remember it. */
        if (*temp)
            add_history (temp);
        
        if (strcmp (temp, "quit") == 0)
            done = 1;
        else{
            retval = Tcl_Eval(interp, temp);
            if(retval != TCL_OK){
                fprintf(stdout, "%s\n", Tcl_GetStringResult(interp));
            }
        }
        free (temp);
    }
    exit (0);
}
