#ifndef _GETOPT_H_
#define _GETOPT_H_

extern int     opterr = 1,             /* if error message should be printed */
optind = 1,             /* index into parent argv vector */
optopt,                 /* character checked for validity */
optreset;               /* reset getopt */
extern char    *optarg;                /* argument associated with option */

int
getopt(int nargc, char * const nargv[], const char *ostr);

#endif  // _GETOPT_H_
