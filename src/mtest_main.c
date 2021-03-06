/* mtest_main.c - Used for experimentation and unit testing.
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
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/tree.h>
#include <libxml/xpathInternals.h>
#include "ResourceVisitor.h"
#include "SeqDatesUtil.h"
#include "SeqLoopsUtil.h"
#include "SeqUtil.h"
#include "nodeinfo.h"
#include "getopt.h"
#include "SeqNode.h"
#include "XmlUtils.h"

static char * testDir = NULL;
int MLLServerConnectionFid=0;

/********************************************************************************
 * MAESTRO TEST FILE
 *
 * This file is intended as a place to do unit testing and experimentation
 * during development and bug solving in maestro.
 *
 * This file assumes that the executable is being run from the maestro directory
 * so that paths can be relative to that directory:
 *
 * testDir is the location to look in for whaterver files are being used for
 * these tests.
 *
 * It is encouraged to put all files that need to be accessed in
 * that directory, so that, for example, if one must modify resourceVisitor
 * functions, they can checkout the mtest_main.c file from a previous commit and
 * run the tests periodically to make sure that all the functions still fulfill
 * their contract, and to catch runtime errors at the earliest possible moment.
 *
 * The base of the test file is the main(), the runTests() function and the
 * absolutePath() function, and the header() function.  The rest is the actual
 * tests, which should have a SETUP, and some TESTS where the result of the test
 * is verified, and raiseError() should be called if the result is different
 * from the expected result.
 *
 * Lower level functions should be tested first so that they may be known to
 * work when testing the higher level functions that use them.
 *
********************************************************************************/

/********************************************************************************
 * Creates an absolute path by appending the relative path to testDir, where
 * testDir = ${MAESTRO_REPO_LOCATION}/testDir/
 * This should be used for any paths so that the tests can be portable to
 * different users who keep their maestro stuff in different places.
********************************************************************************/
char * absolutePath(const char * relativePath)
{
   SeqUtil_TRACE(TL_FULL_TRACE, "absolutePath() begin\n");
   char * absPath = (char *) malloc( strlen(testDir) + 1 + strlen(relativePath) + 1);
   sprintf( absPath, "%s%c%s", testDir,'/', relativePath);
   SeqUtil_TRACE(TL_FULL_TRACE, "absolutePath() end, returning %s\n", absPath);
   return absPath;
}

ResourceVisitorPtr createTestResourceVisitor(SeqNodeDataPtr ndp, const char * nodePath, const char * xmlFile, const char * defFile);

void header(const char * test){
   SeqUtil_TRACE(TL_CRITICAL, "\n=================== UNIT TEST FOR %s ===================\n",test);
}

int test_xml_fallback()
{
   header("xml_fallback");
   /* SETUP: Create an empty xml file.  xml_fallback will then write the
    * mandatory tags to it.  If we get non-null document, the we're good
    * Note that if we have a syntax error in the XML, the program will fail.
    * We must only get a non-null document if we start with an empty file.*/
   char cmd[100];
   const char * badFile = "/tmp/bad_file.xml";
   SeqNodeDataPtr ndp = SeqNode_createNode("phil");
   sprintf(cmd,"rm -f %s",badFile);system(cmd);
   sprintf(cmd,"touch %s", badFile); system(cmd);
   ndp->type = Task;

   /* TEST: try to parse the empty xml file, it won't succeed, and therefore we
    * run xml_fallback(), which must produce a parsable xml file */
   xmlDocPtr doc = XmlUtils_getdoc(badFile);
   if ( doc == NULL ){
      SeqUtil_TRACE(TL_FULL_TRACE, "Parsing unsuccessful, running fallback\n");
      doc = xml_fallbackDoc(badFile,ndp->type);
   }
   if( doc == NULL ) raiseError("TEST_FAILED");

   /* CLEANUP */
   xmlFreeDoc(doc);
   SeqNode_freeNode(ndp);
   sprintf(cmd,"rm -f %s", badFile); system(cmd);
   return 0;
}

int test_getIncrementedDatestamp()
{
   header("getIncrementedDatestamp");
   /* SETUP : Create a datestamp and ValidityData object with a non-empty hour
    * attribute */
   const char * baseDatestamp = "20160102030405";
   ValidityData validityData1 = { "", "", "", "", "", "" };
   ValidityDataPtr val = &validityData1;
   val->hour = "03";

   /* TEST : Resulting incremented datestamp must be 20160102060405 */
   const char * newDatestamp = SeqDatesUtil_getIncrementedDatestamp(baseDatestamp,val->hour,val->time_delta);
   if ( strcmp("20160102060405",newDatestamp) ) raiseError("TEST_FAILED");

   /* CLEANUP */
   free( (char *) newDatestamp);
   return 0;
}



int test_checkValidityData()
{
   header("checkValidityData()");
   /* SETUP : Create a nodeDataPtr with a datestamp and an extension, and create
    * an xmlNodePtr with various characteristics */
   SeqUtil_setTraceFlag(TRACE_LEVEL,TL_CRITICAL);
   ValidityData validityData1 = { "", "", "", "", "", "" };
   ValidityDataPtr val = &validityData1;

   SeqNodeDataPtr ndp = SeqNode_createNode("Phil");
   free(ndp->extension);
   free(ndp->datestamp);
   ndp->extension = "";
   ndp->datestamp = strdup("20160102030405");

   SeqUtil_TRACE(TL_CRITICAL,"valid_hour =============\n");
   /* TEST 1 : With valid_hour == 3, the validity data should match the node's
    * datestamp hour */
   val->valid_hour = "03";
   printValidityData(&validityData1);
   if( checkValidity(ndp, val) != 1 ) raiseError("TEST FAILED\n");

   /* TEST 2 : With valid_hour == 4, the valid_hour should not match the node's
    * datesamp hour */
   val->valid_hour = "04";
   val->hour = "01";
   if( checkValidity(ndp, val) != 1 ) raiseError("TEST FAILED\n");

   SeqUtil_TRACE(TL_CRITICAL,"valid_dow =============\n");
   /* TEST 3 : With 2016-01-02 being a Saturday, the valid_dow of "6" should
    * match the node's datestamp dow. */
   val->valid_dow = "6"; /* Since 2016-01-02 is a saturday */
   if( checkValidity(ndp, val) != 1 ) raiseError("TEST FAILED\n");

   /* TEST 4 : With 2016-01-02 being a Saturday, teh valid_dow of "1" should not
    * match the node's datestamp dow. */
   val->valid_dow = "1"; /* Since 2016-01-02 is not a monday */
   if( checkValidity(ndp, val) != 0 ) raiseError("TEST FAILED\n");
   val->valid_dow = "";

   SeqUtil_TRACE(TL_CRITICAL,"local_index =============\n");
   /* TEST 5 : Having local_index = "loop1=1,loop2=2" translates to the
    * extension "+1+2" which should match the nodeDataPtr's extension */
   validityData1.local_index = "loop1=1,loop2=2";
   ndp->extension = "+1+2";
   if( checkValidity(ndp, val) != 1 ) raiseError("TEST FAILED\n");

   /* TEST 6 : We should have a mismatch of the node's extension with the
    * local_index */
   validityData1.local_index = "loop1=1,loop2=2";
   ndp->extension = "+1+3";
   if( checkValidity(ndp, val) != 0 ) raiseError("TEST FAILED\n");

out_free:
   ndp->extension = NULL;
   SeqNode_freeNode(ndp);
   return 0;
}
int test_Resource_createContext()
{
   header("Resource_createContext");

   /* SETUP : We need nodeDataPtr, an XML filename, and a defFile */
   SeqUtil_setTraceFlag(TRACE_LEVEL,TL_FULL_TRACE);
   SeqNodeDataPtr ndp = SeqNode_createNode("Phil");

   const char * expHome = "/home/ops/afsi/phc/Documents/Experiences/sample";
   const char * nodePath = NULL;
   const char * xmlFile = NULL;
   const char * defFile = SeqUtil_resourceDefFilename(expHome);
   const char * expected = NULL;

   /* TEST 1 : With the node having type Loop */
   ndp->type = Loop;
   nodePath = "/sample_module/Loop_Examples/outerloop";
   xmlFile = xmlResourceFilename( expHome, nodePath, Loop);
   expected = "/home/ops/afsi/phc/Documents/Experiences/sample/resources/sample_module/Loop_Examples/outerloop/container.xml";
   if ( strcmp( xmlFile, expected) != 0 ){
      SeqUtil_TRACE(TL_FULL_TRACE, " xmlFile=%s\nexpected=%s\n", xmlFile, expected);
      raiseError("TEST FAILED");
   }
   free(xmlFile);

   /* TEST 2 : With the node being of type Task */
   ndp->type = Task;
   nodePath = "/sample_module/Loop_Examples/outerloop/outerloopTask";
   xmlFile = xmlResourceFilename(expHome, nodePath, Task);
   expected = "/home/ops/afsi/phc/Documents/Experiences/sample/resources/resources.def";
   if ( strcmp( defFile, expected) != 0 ){
      SeqUtil_TRACE(TL_FULL_TRACE, " defFile=%s\nexpected=%s\n", defFile, expected);
      raiseError("TEST FAILED");
   }

out_free:
   SeqNode_freeNode(ndp);
   SeqUtil_TRACE(TL_FULL_TRACE,"HERE");
   free((char*) xmlFile);
   free((char*) defFile);
   return 0;
}

ResourceVisitorPtr createTestResourceVisitor(SeqNodeDataPtr ndp, const char * nodePath, const char * xmlFile, const char * defFile)
{
   SeqUtil_TRACE(TL_FULL_TRACE, "createTestResourceVisitor() begin\n");
   ResourceVisitorPtr rv = (ResourceVisitorPtr) malloc ( sizeof (ResourceVisitor) );

   rv->nodePath = ( nodePath != NULL ? strdup(nodePath) : strdup("") );

   rv->defFile = (defFile != NULL ? strdup(defFile) : NULL );
   rv->xmlFile = (xmlFile != NULL ? strdup(xmlFile) : NULL );

   rv->context = Resource_createContext(ndp, xmlFile, defFile,ndp->type );
   rv->context->node = rv->context->doc->children;

   rv->loopResourcesFound = 0;
   rv->forEachResourcesFound = 0;
   rv->batchResourcesFound = 0;
   rv->abortActionFound = 0;
   rv->workerPathFound = 0;

   rv->_stackSize = 0;

   SeqUtil_TRACE(TL_FULL_TRACE, "createTestResourceVisitor() end\n");
   return rv;
}
#define NODE_RES_XML_ROOT "NODE_RESOURCES"
int test_nodeStackFunctions()
{
   header("Resource_visitor nodeStack functions ");
   /* SETUP: We need a resource visitor, so we need a nodeDataPtr, an xmlFile,
    * and a defFile. */
   SeqNodeDataPtr ndp = SeqNode_createNode("phil");
   const char * xmlFile = absolutePath("loop_container.xml");
   const char * defFile = absolutePath("resources.def");

   ResourceVisitorPtr rv = createTestResourceVisitor(ndp,NULL,xmlFile,defFile);

   /* TEST1 : Make sure that the current node at creation is the NODE_RESOURCES
    * node */
   SeqUtil_TRACE(TL_FULL_TRACE, "Current node:%s\n", rv->context->node->name);
   if( strcmp( rv->context->node->name, NODE_RES_XML_ROOT) != 0) raiseError("TEST_FAILED");

   /* TEST 2 : Resource_setNode(): push a node and check that the new node is
    * indeed the current node of the context, check that the stack size is one,
    * and check that the previous current node is indeed on the stack.*/
   xmlXPathObjectPtr result = XmlUtils_getnodeset( "child::VALIDITY", rv->context);
   if( result == NULL ) raiseError("Failure, the test xml file should contain at least one VALIDITY node");
   SeqUtil_TRACE(TL_FULL_TRACE, "First result node:%s\n", result->nodesetval->nodeTab[0]->name);
   Resource_setNode(rv,result->nodesetval->nodeTab[0]);
   if( strcmp(rv->context->node->name,"VALIDITY") != 0) raiseError("TEST_FAILED");
   if( rv->_stackSize != 1 ) raiseError("TEST_FAILED");
   if( strcmp(rv->_nodeStack[0]->name, NODE_RES_XML_ROOT) != 0) raiseError("TEST_FAILED");
   xmlXPathFreeObject(result);

   /* TEST 3 : Resource_unsetNode(): pop a node from the stack, check that it is
    * now the current node and that the stack size is 0.*/
   Resource_unsetNode(rv);
   if( strcmp( rv->context->node->name, NODE_RES_XML_ROOT) != 0 ) raiseError("TEST_FAILED");
   if( rv->_stackSize != 0 ) raiseError("TEST_FAILED");

out_free:
   SeqNode_freeNode(ndp);
   free((char *)xmlFile);
   free((char *)defFile);
   deleteResourceVisitor(rv);
   return 0;
}

int test_getValidityData()
{
   header("getValidityData()");
   /* SETUP : We need a nodeDataPtr, and the xmlNodePtr of a validity node */
   SeqNodeDataPtr ndp = SeqNode_createNode("Phil");

   const char * xmlFile = absolutePath("validityXml.xml");
   SeqUtil_TRACE(TL_FULL_TRACE, "HERE, xmlFile = %s\n", xmlFile);
   xmlDocPtr doc = XmlUtils_getdoc(xmlFile);
   xmlXPathContextPtr rc = xmlXPathNewContext(doc);
   xmlXPathObjectPtr result = XmlUtils_getnodeset("(/NODE_RESOURCES/VALIDITY)", rc);
   xmlNodePtr valNode = result->nodesetval->nodeTab[1];

   /* TEST 1 : We get the VALIDITY node's info and compare it to the actual
    * values read with our eyes. */
   ValidityDataPtr valDat = getValidityData(valNode);
   printValidityData(valDat);
   if( strcmp(valDat->dow, "0")
         || strcmp(valDat->hour, "00")
         || strcmp(valDat->local_index,"loop=0"))
      raiseError("TEST FAILED");

   header("isValid");
   /* INPUT: _nodeDataPtr and xmlNodePtr of a validityNode. */
out_free:
   SeqNode_freeNode(ndp);
   free((char *)xmlFile);
   xmlXPathFreeContext(rc);
   xmlXPathFreeObject(result);
   xmlFreeDoc(doc);
   deleteValidityData(valDat);
   return 0;
}

int test_isValid()
{

   SeqNodeDataPtr ndp = SeqNode_createNode("Phil");
   const char * xmlFile = absolutePath("validityXml.xml");
   xmlDocPtr doc = XmlUtils_getdoc(xmlFile);
   xmlXPathContextPtr rc = xmlXPathNewContext(doc);
   xmlXPathObjectPtr result = XmlUtils_getnodeset("(/NODE_RESOURCES/VALIDITY)", rc);
   xmlNodePtr valNode = result->nodesetval->nodeTab[1];
   ValidityDataPtr valDat = getValidityData(valNode);
   printValidityData(valDat);

   /* TEST : With the datestamp and the extension of 0, the VALIDITY node should
    * be considered valid. */
   free(ndp->datestamp);
   ndp->datestamp = strdup("20160102030405");
   free(ndp->extension);
   ndp->extension = strdup("+0");
   printValidityData(valDat);
   if ( !isValid(ndp,valNode) ) raiseError("TEST_FAILED");
   deleteValidityData(valDat);

out_free:
   SeqNode_freeNode(ndp);
   SeqUtil_TRACE(TL_FULL_TRACE, "HERE NOW");
   xmlXPathFreeObject(result);
   xmlXPathFreeContext(rc);
   xmlFreeDoc(doc);
   free((char*)xmlFile);
   return 0;
}



int test_Resource_getLoopAttributes()
{
   header("getLoopAttributes");
   /* SETUP: We need a resource visitor, so an xml file and defFile, and a node
    * data pointer of type loop. */
   SeqNodeDataPtr ndp = SeqNode_createNode("Phil");
   const char * xmlFile = absolutePath("validityXml.xml");
   ResourceVisitorPtr rv = createTestResourceVisitor(ndp,NULL,xmlFile,NULL);

   /* TEST1 : Look for the first validity node. Get loop resources from that,
    * confirm that the expression found matches the one in the xml file */
   xmlXPathObjectPtr result = XmlUtils_getnodeset("(/NODE_RESOURCES/VALIDITY)", rv->context);
   xmlNodePtr valNode = result->nodesetval->nodeTab[1];
   ndp->type = Loop;
   rv->context->node = valNode;
   rv->loopResourcesFound = 0;
   SeqUtil_setTraceFlag(TRACE_LEVEL,TL_FULL_TRACE);
   Resource_getLoopAttributes(rv,ndp);
   SeqNameValues_printList(ndp->data);
   char * expression = SeqNameValues_getValue( ndp->data , "EXPRESSION");
   if( expression == NULL || strcmp(expression,"0:24:3:6") != 0) raiseError("TEST_FAILED");
   free(expression);expression = NULL;

   /* TEST 2 : Do the same with the root node and validate the expression found.
    * */
   xmlNodePtr root_node = rv->context->doc->children;
   rv->context->node = root_node;
   rv->loopResourcesFound = 0;
   SeqNameValues_deleteWholeList(&(ndp->data));
   SeqUtil_TRACE(TL_FULL_TRACE,"root_node->name=%s\n", root_node->name);
   Resource_getLoopAttributes(rv,ndp);
   SeqNameValues_printList(ndp->data);
   expression = SeqNameValues_getValue( ndp->data , "EXPRESSION");
   SeqUtil_TRACE(TL_FULL_TRACE,"expression=%s\n",expression);
   if( expression == NULL || strcmp(expression,"0:54:3:6") != 0) raiseError("TEST_FAILED");

out_free:
   SeqNode_freeNode(ndp);
   free(expression);
   free((char*) xmlFile);
   xmlXPathFreeObject(result);
   deleteResourceVisitor(rv);
   return 0;
}



int test_parseNodeDFS()
{
   header("parseNodeDFS");

   /* SETUP : Artificially create a resourceVisitor with the xml file, and a
    * nodeDataPtr with a datestamp and an extension for validity checking */
   SeqNodeDataPtr ndp = SeqNode_createNode("phil");
   free(ndp->datestamp);
   ndp->datestamp = strdup("20160102030405");
   const char * xmlFile = absolutePath("loop_container.xml");
   ResourceVisitorPtr rv = createTestResourceVisitor(ndp,NULL,xmlFile,NULL);

   Resource_parseNodeDFS(rv, ndp, Resource_getLoopAttributes);
   char * expression = SeqNameValues_getValue( ndp->data, "EXPRESSION" );
   if( expression != NULL && strcmp(expression, "5:6:7:8") != 0) raiseError("TEST_FAILED"); 
   SeqNameValues_deleteWholeList(&(ndp->data));
   free(expression);

   rv->loopResourcesFound = 0;
   SeqUtil_TRACE(TL_FULL_TRACE,"============================ test with datestamp hour = 12\n");
   free(ndp->datestamp);
   ndp->datestamp = strdup("20160102120000");
   Resource_parseNodeDFS(rv, ndp, Resource_getLoopAttributes);
   expression = SeqNameValues_getValue( ndp->data, "EXPRESSION" );
   if( expression == NULL || strcmp(expression, "9:10:11:12") != 0) raiseError("TEST_FAILED"); 
   SeqNameValues_deleteWholeList(&(ndp->data));
   free(expression);


   rv->loopResourcesFound = 0;
   free(ndp->extension);
   ndp->extension = strdup("+1");
   Resource_parseNodeDFS(rv, ndp, Resource_getLoopAttributes);
   expression = SeqNameValues_getValue( ndp->data, "EXPRESSION" );
   if( expression == NULL || strcmp(expression, "13:14:15:16") != 0) raiseError("TEST_FAILED"); 
   SeqNameValues_deleteWholeList(&(ndp->data));
   free(expression);

   SeqNode_freeNode(ndp);
   free((char*)xmlFile);
   deleteResourceVisitor(rv);
   return 0;
}

int test_Resource_parseWorkerPath()
{
   header("parseWorkerPath");
   SeqNodeDataPtr ndp = SeqNode_createNode("phil");
   free(ndp->datestamp);
   ndp->datestamp = strdup("20160102120000");
   free(ndp->extension);
   ndp->extension = strdup("+1");

   const char * xmlFile = absolutePath("loop_container.xml");
   ResourceVisitorPtr rv = createTestResourceVisitor(ndp,NULL,xmlFile,NULL);

   Resource_parseNodeDFS(rv,ndp,Resource_getWorkerPath);

   if( strcmp(ndp->workerPath, "this/is/the/end" ) != 0 ) raiseError("TEST FAILED");

   free(ndp->datestamp);
   ndp->datestamp = strdup("20160101030405");
   free(ndp->workerPath);
   ndp->workerPath = strdup("HELLO");
   rv->workerPathFound = RESOURCE_FALSE;

   Resource_parseNodeDFS(rv,ndp,Resource_getWorkerPath);

   SeqUtil_TRACE(TL_FULL_TRACE, "ndp->workerPath: %s\n", ndp->workerPath);
   if( strcmp(ndp->workerPath, "hello/my/name/is/inigo/montoya/you/killed/my/father/prepare/to/die") != 0 )
      raiseError("TEST FAILED");
   
   SeqNode_freeNode(ndp);
   free((char*)xmlFile);
   deleteResourceVisitor(rv);
   return 0;
}


int test_SeqLoops_getNodeLoopContainersExtensionsInReverse()
{
   const char * exp = "/home/ops/afsi/phc/Documents/Experiences/sample-loops_bug";
   const char * node = "/sample_1.4.3/BadNames/outer_loop/inner_loop/END/SET";
   SeqNodeDataPtr ndp = nodeinfo (node, NI_SHOW_ALL ,NULL ,exp,
                                 NULL, "20160102030000",NULL );
   SeqNode_printNode(ndp, NI_SHOW_ALL,NULL);
   LISTNODEPTR extensions = SeqLoops_getLoopContainerExtensionsInReverse(ndp,"outer_loop=*,inner_loop=*,END=*");
   SeqListNode_deleteWholeList(&extensions);

   extensions = SeqLoops_getLoopContainerExtensionsInReverse(ndp,"outer_loop=1,inner_loop=2,END=3");
   SeqListNode_deleteWholeList(&extensions);
   extensions = SeqLoops_getLoopContainerExtensionsInReverse(ndp,"");
   SeqListNode_deleteWholeList(&extensions);
   return 0;
}

const char* getVarName(const char *, const char*,const char *);
int test_getVarName()
{
   header("getVarName()");
   char *input = "$((varname))";
   char * output = getVarName(input, "$((", "))");
   fprintf(stderr, "%s -> %s\n",input,output);
   if(strcmp(output,"varname") != 0)
      raiseError("TEST_FAILED:%s()[%s:%d]\n",__func__,__FILE__,__LINE__);

   input = "DominicvarnameRacette";
   output = getVarName(input, "Dominic", "Racette");
   fprintf(stderr, "%s -> %s\n",input,output);
   if(strcmp(output,"varname") != 0)
      raiseError("TEST_FAILED:%s()[%s:%d]\n",__func__,__FILE__,__LINE__);
   return 0;
}

int runTests(const char * seq_exp_home, const char * node, const char * datestamp)
{
   test_xml_fallback();
   test_getIncrementedDatestamp();
   test_checkValidityData();
   test_Resource_createContext();
   test_nodeStackFunctions();
   test_getValidityData();
   test_isValid();
   test_Resource_getLoopAttributes();
   test_parseNodeDFS();
   test_Resource_parseWorkerPath();
   test_SeqLoops_getNodeLoopContainersExtensionsInReverse();


   test_getVarName();

   SeqUtil_TRACE(TL_CRITICAL, "============== ALL TESTS HAVE PASSED =====================\n");
   return 0;
}

int main ( int argc, char * argv[] )
{
   char * short_opts = "n:f:l:o:d:e:v";
   char *node = NULL, *seq_exp_home = NULL, *datestamp=NULL, *tmpDate=NULL ;
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
   int opt_index, c = 0, i;

   while ((c = getopt_long(argc, argv, short_opts, long_opts, &opt_index )) != -1) {
      switch(c) {
         case 'n':
            node = strdup(optarg);
            break;
         case 'e':
            seq_exp_home = strdup(optarg);
            break;
         case 'd':
            datestamp = malloc( PADDED_DATE_LENGTH + 1 );
            strcpy(datestamp,optarg);
            break;
         case '?':
            exit(1);
      }
   }

   SeqUtil_setTraceFlag( TRACE_LEVEL , TL_FULL_TRACE );

   const char * PWD = getenv("PWD");
   /* Check that the path PWD ends with maestro.  It's the best we can do to
    * make sure that mtest is being run from the right place. */
   const char * p = PWD;
   while(*p++ != 0 );
   while(*(p-1) != '/') --p;
   if( strcmp(p,"maestro") != 0 ){
      SeqUtil_TRACE(TL_FULL_TRACE, "\
Main function for doing tests, please run this from the maestro directory so\n\
that the location of the test files may be known.  Eg by doing \n\
   'make install; mtest'\n\
or\n\
   'make; ./src/mtest\n\
from the maestro directory.\n");
      exit(1);
   }

   char * suffix = "/src/testDir";
   testDir = (char *) malloc( sizeof(char) * (strlen(PWD) + strlen(suffix) + 1));
   sprintf( testDir, "%s%s" , PWD, suffix);

   puts ( testDir );

   if  (( datestamp == NULL ) && ( (tmpDate = getenv("SEQ_DATE")) != NULL ))  {
       datestamp = malloc( PADDED_DATE_LENGTH + 1 );
       strcpy(datestamp,tmpDate);
   }

   if ( datestamp != NULL ) {
      i = strlen(datestamp);
      while ( i < PADDED_DATE_LENGTH ){
         datestamp[i++] = '0';
      }
      datestamp[PADDED_DATE_LENGTH] = '\0';
   }

   runTests(seq_exp_home,node,datestamp);

   free(node);
   free(seq_exp_home);
   free(datestamp);
   return 0;
}








