/* nodeinfo_main.c - Command-line API to use the node contruction mechanism in the Maestro sequencer software package.
 * Copyright (C) 2011-2015  Operations division of the Canadian Meteorological Centre
 *                          Environment Canada
 *
 * Maestro is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation,
 * version 2.1 of the License.
 *
 * Maestro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "SeqUtil.h"
#include "nodeinfo.h"
#include "SeqLoopsUtil.h"
#include "SeqNameValues.h"
#include "getopt.h"

int MLLServerConnectionFid=0;

static void printUsage()
{
   char *seq_exp_home = NULL;
   char * usage = "\
DESCRIPTION: Nodeinfo\n\
\n\
USAGE\n\
\n\
    nodeinfo -n node [-f filters -l loopargs -d datestamp -e expHome -v]\n\
\n\
OPTIONS\n\
\n\
    -n, --node\n\
        Specify the full path of task or family node (mandatory (except -f root))\n\
\n\
    -l, --loop-args\n\
        Specify the loop arguments as a comma seperated value loop index: inner_Loop=1,outer_Loop=3\n\
\n\
    -f, --filters\n\
        Speciry a comma separated list of filters (optional):\n\
            all (default)\n\
            task (node task path only)\n\
            cfg (node config path only)\n\
            res (batch resource only)\n\
            res_path (batch resource path only)\n\
            type (node type only)\n\
            node (node name and extention if applicable)\n\
            root (root node name)\n\
            var  (variables exported in wrapper)\n\
\n\
    -d, --datestamp\n\
        Specify the 14 character date of the experiment ex: 20080530000000\n\
        (anything shorter will be padded with 0s until 14 characters) Default\n\
        value is the date of the experiment.\n\
\n\
    -e, --exp \n\
        Experiment path.  If it is not supplied, the environment variable \n\
        SEQ_EXP_HOME will be used.\n\
\n\
    -v, --verbose\n\
        Turn on full tracing\n\
\n\
    -h, --help\n\
        Show this help screen\n\
\n\
EXAMPLES:\n\
\n\
    nodeinfo -n regional/assimilation/00/task_0\n";
puts(usage);
}

int main ( int argc, char * argv[] )
{
   char * short_opts = "n:f:l:o:d:e:v";
   extern char *optarg;
   
   extern char *optarg;
   extern int   optind;
   struct       option long_opts[] =
   { /*  NAME        ,    has_arg       , flag  val(ID) */
      {"exp"         , required_argument,   0,     'e'},
      {"node"        , required_argument,   0,     'n'},
      {"loop-args"   , required_argument,   0,     'l'},
      {"datestamp"   , required_argument,   0,     'd'},
      {"outputfile"  , required_argument,   0,     'o'},
      {"filters"     , required_argument,   0,     'f'},
      {"verbose"     , no_argument      ,   0,     'v'},
      {NULL,0,0,0} /* End indicator */
   };
   int opt_index, c = 0;



   char * loops = NULL; 

   SeqNodeDataPtr  nodeDataPtr = NULL;
   SeqNameValuesPtr loopsArgs = NULL;
   char *node = NULL, *seq_exp_home = NULL, *outputFile=NULL, *datestamp=NULL, *tmpDate=NULL ;
   char filters[256];
   int errflg = 0, nodeFound = 0;
   int gotLoops=0, showRootOnly = 0;
   if ( argc == 1 || argc == 2) {
      printUsage();
      exit(1);
   }
   strcpy(filters,"all");
   while ((c = getopt_long(argc, argv, short_opts, long_opts, &opt_index )) != -1) {
         switch(c) {
         case 'n':
	    node = malloc( strlen( optarg ) + 1 );
            strcpy(node,optarg);
            nodeFound = 1;
            break;
         case 'd':
	    datestamp = malloc( strlen( optarg ) + 1 );
            strcpy(datestamp,optarg);
            break;
         case 'f':
            strcpy(filters,optarg);
            break;
         case 'v':
				SeqUtil_setTraceFlag( TRACE_LEVEL , TL_FULL_TRACE );
				SeqUtil_setTraceFlag( TF_TIMESTAMP , TF_ON );
            break;
	      case 'o':
	         outputFile=malloc( strlen( optarg ) + 1 );
	         strcpy(outputFile, optarg);
	         break;
         case 'l':
            /* loops argument */
            gotLoops=1;
            loops = malloc( strlen( optarg ) + 1 );
            strcpy(loops,optarg);
	         if( SeqLoops_parseArgs( &loopsArgs, loops ) == -1 ) {
               fprintf( stderr, "ERROR: Invalid loop arguments: %s\n", loops );
               exit(1);
            }
            break;
         case 'e':
            seq_exp_home = strdup( optarg );
            break;
         case '?':
            printUsage();
            exit(1);
         }
   }

   if ( seq_exp_home == NULL) {
      if ((seq_exp_home = getenv("SEQ_EXP_HOME")) == NULL){
         fprintf(stderr , "nodelogger_main.c : SEQ_EXP_HOME must be set either with '-e' option or through the environment variable SEQ_EXP_HOME \n");
         printUsage();
         exit(1);
      }
   }

   if ( nodeFound == 0 && strstr( filters, "root" ) == NULL ) {
      printUsage();
      exit(1);
   }

   if  (( datestamp == NULL ) && ( (tmpDate = getenv("SEQ_DATE")) != NULL ))  {
      datestamp = malloc( strlen(tmpDate) + 1 );
      strcpy(datestamp,tmpDate);
   }

   nodeDataPtr = nodeinfo( node, filters, loopsArgs, seq_exp_home, NULL, datestamp );

   if (gotLoops){
      SeqLoops_validateLoopArgs( nodeDataPtr, loopsArgs );
   }

   SeqNode_printNode( nodeDataPtr, filters, outputFile );
   SeqNode_freeNode( nodeDataPtr );
   free( node );
   free(outputFile);
   free(datestamp);
   exit(0);
}
