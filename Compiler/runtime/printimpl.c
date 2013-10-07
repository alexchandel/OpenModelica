/*
 * This file is part of OpenModelica.
 *
 * Copyright (c) 1998-2010, Linköpings University,
 * Department of Computer and Information Science,
 * SE-58183 Linköping, Sweden.
 *
 * All rights reserved.
 *
 * THIS PROGRAM IS PROVIDED UNDER THE TERMS OF THIS OSMC PUBLIC
 * LICENSE (OSMC-PL). ANY USE, REPRODUCTION OR DISTRIBUTION OF
 * THIS PROGRAM CONSTITUTES RECIPIENT'S ACCEPTANCE OF THE OSMC
 * PUBLIC LICENSE.
 *
 * The OpenModelica software and the Open Source Modelica
 * Consortium (OSMC) Public License (OSMC-PL) are obtained
 * from Linköpings University, either from the above address,
 * from the URL: http://www.ida.liu.se/projects/OpenModelica
 * and in the OpenModelica distribution.
 *
 * This program is distributed  WITHOUT ANY WARRANTY; without
 * even the implied warranty of  MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE, EXCEPT AS EXPRESSLY SET FORTH
 * IN THE BY RECIPIENT SELECTED SUBSIDIARY LICENSE CONDITIONS
 * OF OSMC-PL.
 *
 * See the full OSMC Public License conditions for more details.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "errorext.h"
#include "systemimpl.h"

/* adrpo: "int showErrorMessages" is defined in errorext. (enabled with omc +showErrorMessages) */

#define GROWTH_FACTOR 1.4  /* According to some rumors of buffer growth */
#define INITIAL_BUFSIZE 4000 /* Seems reasonable */
#define MAXSAVEDBUFFERS 10   /* adrpo: added this so it compiles again! MathCore can change it later */

typedef struct print_members_s {
  char *buf;
  char *errorBuf;
  int nfilled;
  int cursize;
  int errorNfilled;
  int errorCursize;
  char** savedBuffers;
  long* savedCurSize;
  long* savedNfilled;
} print_members;

#include <pthread.h>

pthread_once_t printimpl_once_create_key = PTHREAD_ONCE_INIT;
pthread_key_t printimplKey;

static void free_printimpl(void *data)
{
  int i;
  print_members *members = (print_members *) data;
  if (data == NULL) return;
  if (members->savedBuffers) {
    for (i=0; i<MAXSAVEDBUFFERS; i++) {
      if (members->savedBuffers[i]) {
        free(members->savedBuffers[i]);
        members->savedBuffers[i] = NULL;
      }
    }
    free(members->savedBuffers);
  }
  if (members->buf != NULL) free(members->buf);
  if (members->errorBuf != NULL) free(members->errorBuf);
  if (members->savedCurSize != NULL) free(members->savedCurSize);
  if (members->savedNfilled != NULL) free(members->savedNfilled);
  free(members);
}

static void make_key()
{
  pthread_key_create(&printimplKey,free_printimpl);
}

static print_members* getMembers()
{
  pthread_once(&printimpl_once_create_key,make_key);
  print_members *res = (print_members*) pthread_getspecific(printimplKey);
  if (res != NULL) return res;
  res = (print_members*) calloc(1,sizeof(print_members));
  pthread_setspecific(printimplKey,res);
  return res;
}


#define buf members->buf
#define errorBuf members->errorBuf
#define nfilled members->nfilled
#define cursize members->cursize
#define errorNfilled members->errorNfilled
#define errorCursize members->errorCursize
#define savedBuffers members->savedBuffers
#define savedCurSize members->savedCurSize
#define savedNfilled members->savedNfilled

static int increase_buffer(void)
{
  print_members* members = getMembers();
  char *new_buf;
  int new_size;
  if (cursize == 0) {
    new_buf = (char*)malloc(INITIAL_BUFSIZE*sizeof(char));
    if (new_buf == NULL) { return -1; }
    new_buf[0]='\0';
    cursize = INITIAL_BUFSIZE;
  } else {
    //fprintf(stderr,"increasing buffer from %d to %d \n",cursize,((int)(cursize * GROWTH_FACTOR)));
    new_buf = (char*)malloc((new_size =(int) (cursize * GROWTH_FACTOR))*sizeof(char));
    if (new_buf == NULL) { return -1; }
    memcpy(new_buf,buf,cursize);
    cursize = new_size;
  }
  if (buf) {
    free(buf);
  }
  buf = new_buf;
  return 0;
}

static int increase_buffer_fixed(int increase)
{
  print_members* members = getMembers();
  char * new_buf;
  int new_size;

  if (cursize == 0) {
    new_buf = (char*)malloc(increase*sizeof(char));
    if (new_buf == NULL) { return -1; }
    new_buf[0]='\0';
    cursize = increase;
  } else {
  new_size = (int)(cursize+increase);
    //fprintf(stderr,"increasing buffer_FIXED_ from %d to %d \n",cursize,new_size);
    new_buf = (char*)malloc(new_size*sizeof(char));
    if (new_buf == NULL) { return -1; }
    //memcpy(new_buf,buf,cursize);
    cursize = new_size;
  }
  //printf("new buff size set to: %d\n",cursize);
  if (buf) {
    free(buf);
  }
  buf = new_buf;
  return 0;
}

static int error_increase_buffer(void)
{
  print_members* members = getMembers();
  char * new_buf;
  int new_size;

  if (errorCursize == 0) {
    new_buf = (char*)malloc(INITIAL_BUFSIZE*sizeof(char));
    if (new_buf == NULL) { return -1; }
    new_buf[0]='\0';
    errorCursize = INITIAL_BUFSIZE;
  } else {
    new_buf = (char*)malloc((new_size =(int) (errorCursize * GROWTH_FACTOR))*sizeof(char));
    if (new_buf == NULL) { return -1; }
    memcpy(new_buf,errorBuf,errorCursize);
    errorCursize = new_size;
  }
  if (errorBuf) {
    free(errorBuf);
  }
  errorBuf = new_buf;
  return 0;
}

static int print_error_buf_impl(const char *str)
{
  print_members* members = getMembers();
  /*  printf("cursize: %d, nfilled %d, strlen: %d\n",cursize,nfilled,strlen(str));*/

  if (str == NULL) {
    return -1;
  }
  while (errorNfilled + strlen(str)+1 > errorCursize) {
    if (error_increase_buffer() != 0) {
      return -1;
    }
    /* printf("increased -- cursize: %d, nfilled %d\n",cursize,nfilled);*/
  }

  sprintf((char*)(errorBuf+strlen(errorBuf)),"%s",str);
  errorNfilled=strlen(errorBuf);
  return 0;
}

static void PrintImpl__setBufSize(long newSize)
{
  print_members* members = getMembers();
  if (newSize > 0) {
    printf(" setting init_size to: %ld\n",newSize);
    increase_buffer_fixed(newSize);
  }
}

static void PrintImpl__unSetBufSize(void)
{
  increase_buffer_fixed(INITIAL_BUFSIZE);
}

/* Returns 0 on success; 1 on failure */
static int PrintImpl__printErrorBuf(const char* str)
{
  if (showErrorMessages) /* adrpo: should we show error messages while they happen? */
  {
    fprintf(stderr, "%s", str);
    fflush(stderr);
  }

  if (print_error_buf_impl(str) != 0) {
    return 1;
  }

  return 0;
}

static void PrintImpl__clearErrorBuf(void)
{
  print_members* members = getMembers();
  errorNfilled=0;
  if (errorBuf != 0) {
    /* adrpo 2008-12-15 free the error buffer as it might have got quite big meantime */
    free(errorBuf);
    errorBuf = NULL;
    errorCursize=0;
  }
}

/* returns NULL on failure */
static const char* PrintImpl__getErrorString(void)
{
  print_members* members = getMembers();
  if (errorBuf == 0) {
    if(error_increase_buffer() != 0) {
      return NULL;
    }
  }
  return errorBuf;
}

/* returns 0 on success */
static int PrintImpl__printBuf(const char* str)
{
  print_members* members = getMembers();
  long len = strlen(str);
  /* printf("cursize: %d, nfilled %d, strlen: %d\n",cursize,nfilled,strlen(str)); */

  while (nfilled + len + 1 > cursize) {
    if(increase_buffer()!= 0) {
      return 1;
    }
    /* printf("increased -- cursize: %d, nfilled %d\n",cursize,nfilled); */
  }

  /*
   * nfilled += sprintf((char*)(buf+nfilled),"%s",str);
   * replaced by (below), courtesy of Pavol Privitzer
   */
  memcpy(buf+nfilled, str, len);
  nfilled += len;
  buf[nfilled] = '\0';

  return 0;
}

static void PrintImpl__clearBuf(void)
{
  print_members* members = getMembers();
  nfilled=0;
  if (buf != 0) {
    /* adrpo 2008-12-15 free the print buffer as it might have got quite big meantime */
    free(buf);
    buf = NULL;
    cursize = 0;
  }
}

/* returns NULL on failure */
static const char* PrintImpl__getString(void)
{
  print_members* members = getMembers();
  if(buf == NULL || buf[0]=='\0' || cursize==0){
    return "";
  }
  if (buf == 0) {
    if (increase_buffer() != 0) {
      return NULL;
    }
  }
  return buf;
}

/* returns 0 on success */
static int PrintImpl__writeBuf(const char* filename)
{
  print_members* members = getMembers();
#if defined(__MINGW32__) || defined(_MSC_VER)
  const char *fileOpenMode = "wt"; /* on Windows do translation so that \n becomes \r\n */
#else
  const char *fileOpenMode = "wb";  /* on Unixes don't bother, do it binary mode */
#endif
  FILE * file = NULL;
  /* check if we have something to write */
  /* open the file */
  /* adrpo: 2010-09-22 open the file in BINARY mode as otherwise \r\n becomes \r\r\n! */
  file = fopen(filename,fileOpenMode);
  if (file == NULL) {
    const char *c_tokens[1]={filename};
    c_add_message(21, /* WRITING_FILE_ERROR */
      ErrorType_scripting,
      ErrorLevel_error,
      gettext("Error writing to file %s."),
      c_tokens,
      1);
    return 1;
  }

  if (buf == NULL || buf[0]=='\0') {
    /* nothing to write to file, just close it and return ! */
    fclose(file);
    return 1;
  }

  /*  write 1 element of size nfilled to file and check for errors */
  if (1 != fwrite(buf, nfilled, 1,  file))
  {
    const char *c_tokens[1]={filename};
    c_add_message(21, /* WRITING_FILE_ERROR */
      ErrorType_scripting,
      ErrorLevel_error,
      gettext("Error writing to file %s."),
      c_tokens,
      1);
    fprintf(stderr, "Print.writeBuf: error writing to file: %s!\n", filename);
    fclose(file);
    return 1;
  }
  if (fflush(file) != 0)
  {
    fprintf(stderr, "Print.writeBuf: error flushing file: %s!\n", filename);
  }
  fclose(file);
  return 0;
}

#include <regex.h>

/* returns 0 on success */
static int PrintImpl__writeBufConvertLines(const char *filename)
{
  print_members* members = getMembers();
#if defined(__MINGW32__) || defined(_MSC_VER)
  const char *fileOpenMode = "wt"; /* on Windows do translation so that \n becomes \r\n */
#else
  const char *fileOpenMode = "wb";  /* on Unixes don't bother, do it binary mode */
#endif
  char *str = buf, *next;
  FILE * file = NULL;
  regex_t re_begin,re_end;
  regmatch_t matches[3];
  int i;
  long nlines=3 /* We start at 3 because we write 2 lines before the first line */, modelicaLine;
  /* What we try to match: */
  /*#modelicaLine [/path/to/a.mo:4:3-4:12]*/
  /*#endModelicaLine*/
  const char *re_str[2] = {"^ */[*]#modelicaLine .([^:]*):([0-9]*):[0-9]*-[0-9]*:[0-9]*.[*]/$", "^ */[*]#endModelicaLine[*]/$"};
  const char *modelicaFileName = NULL;
  str[nfilled] = '\0';

  /* First, compile the regular expressions */
  if (regcomp(&re_begin, re_str[0], REG_EXTENDED) || regcomp(&re_end, re_str[1], 0)) {
    const char *c_tokens[1]={filename};
    c_add_message(21, /* WRITING_FILE_ERROR */
      ErrorType_scripting,
      ErrorLevel_error,
      gettext("Error compiling regular expression: %s or %s."),
      re_str,
      2);
    return 1;
  }

  /* check if we have something to write */
  /* open the file */
  /* adrpo: 2010-09-22 open the file in BINARY mode as otherwise \r\n becomes \r\r\n! */
  file = fopen(filename,fileOpenMode);
  if (file == NULL) {
    const char *c_tokens[1]={filename};
    c_add_message(21, /* WRITING_FILE_ERROR */
      ErrorType_scripting,
      ErrorLevel_error,
      gettext("Error writing to file %s."),
      c_tokens,
      1);
    regfree(&re_begin);
    regfree(&re_end);
    return 1;
  }
  if (str == NULL || str[0]=='\0') {
    /* nothing to write to file, just close it and return ! */
    regfree(&re_begin);
    regfree(&re_end);
    fclose(file);
    return 1;
  }
#define ABC __FILE__
  fprintf(file,"#define OMC_FILE __FILE__\n#line %ld OMC_FILE\n", nlines++);
  do {
    next = strchr(str,'\n');
    if (!next) {
      fprintf(file,"%s",str);
      break;
    }
    *next++ = '\0';
    if (0==regexec(&re_begin, str, 3, matches, 0)) {
      str[matches[1].rm_eo] = '\0';
      str[matches[2].rm_eo] = '\0';
      modelicaFileName = str + matches[1].rm_so;
      modelicaLine = strtol(str + matches[2].rm_so, NULL, 10);
    } else if (0==regexec(&re_end, str, 3, matches, 0)) {
      if (modelicaFileName) { /* There is sometimes #endModlicaLine without a matching #modelicaLine */
        modelicaFileName = NULL;
        fprintf(file,"#line %ld OMC_FILE\n", nlines++);
      }
    } else if (modelicaFileName) {
      fprintf(file,"#line %ld \"%s\"\n", modelicaLine, modelicaFileName);
      fprintf(file,"%s\n", str);
      nlines+=2;
    } else {
      fprintf(file,"%s\n", str);
      nlines++;
    }
    str = next;
  } while (1);
  /* We do destructive updates on the print buffer; hide our tracks */
  *buf = 0;
  nfilled = 0;
  regfree(&re_begin);
  regfree(&re_end);
  fclose(file);
  return 0;
}

static long PrintImpl__getBufLength(void)
{
  print_members* members = getMembers();
  return nfilled;
}

/* returns 0 on success */
static int PrintImpl__printBufSpace(long nSpaces)
{
  print_members* members = getMembers();
  if (nSpaces > 0) {
   while (nfilled + nSpaces + 1 > cursize) {
     if(increase_buffer()!= 0) {
       return 1;
     }
   }
   memset(buf+nfilled,' ',(size_t)nSpaces);
   nfilled += nSpaces;
   buf[nfilled] = '\0';
  }
  return 0;
}

/* returns 0 on success */
static int PrintImpl__printBufNewLine(void)
{
  print_members* members = getMembers();
  while (nfilled + 1+1 > cursize) {
    if(increase_buffer()!= 0) {
      return 1;
    }
  }
  buf[nfilled++] = '\n';
  buf[nfilled] = '\0';

  return 0;
}

static int PrintImpl__hasBufNewLineAtEnd(void)
{
  print_members* members = getMembers();
  return (nfilled > 0 && buf[nfilled-1] == '\n') ? 1 : 0;
}

static int PrintImpl__restoreBuf(long handle)
{
  print_members* members = getMembers();
  if (handle < 0 || handle > MAXSAVEDBUFFERS-1) {
    fprintf(stderr,"Internal error, handle %ld out of range. Should be in [%d,%d]\n",handle,0,MAXSAVEDBUFFERS-1);
    return 1;
  } else {
    if (buf) { free(buf);}
    buf = savedBuffers[handle];
    cursize = savedCurSize[handle];
    nfilled = savedNfilled[handle];
    savedBuffers[handle] = 0;
    savedCurSize[handle] = 0;
    savedNfilled[handle] = 0;
    if (buf == 0) {
      fprintf(stderr,"Internal error, handle %ld does not contain a valid buffer pointer\n",handle);
      return 1;
    }
    return 0;
  }
}

static long PrintImpl__saveAndClearBuf()
{
  print_members* members = getMembers();
  long freeHandle,foundHandle=0;
  if (!buf) {
    increase_buffer();
  }
  if (! savedBuffers) {
    savedBuffers = (char**)calloc(MAXSAVEDBUFFERS,sizeof(char*));
    if (!savedBuffers) {
      fprintf(stderr, "Internal error allocating savedBuffers in Print.saveAndClearBuf\n");
      return -1;
    }
  }
  if (! savedCurSize) {
    savedCurSize = (long*)calloc(MAXSAVEDBUFFERS,sizeof(long*));
    if (!savedCurSize) {
      fprintf(stderr, "Internal error allocating savedCurSize in Print.saveAndClearBuf\n");
      return -1;
    }
  }
  if (! savedNfilled) {
    savedNfilled = (long*)calloc(MAXSAVEDBUFFERS,sizeof(long*));
    if (!savedNfilled) {
      fprintf(stderr, "Internal error allocating savedNfilled in Print.saveAndClearBuf\n");
      return -1;
    }
  }
  for (freeHandle=0; freeHandle< MAXSAVEDBUFFERS; freeHandle++) {
    if (savedBuffers[freeHandle]==0)
    {
      foundHandle = 1;
      break;
    }
  }
  if (!foundHandle) {
    fprintf(stderr,"Internal error, can not save more than %d buffers, increase MAXSAVEDBUFFERS in printimpl.c\n",MAXSAVEDBUFFERS);
    return -1;
  }
  if (!buf) {
    increase_buffer(); /* Initialize it; the saved buffers cannot handle NULL */
  }
  savedBuffers[freeHandle] = buf;
  savedCurSize[freeHandle] = cursize;
  savedNfilled[freeHandle] = nfilled;
  buf = (char*)malloc(INITIAL_BUFSIZE*sizeof(char));
  *buf = 0;
  nfilled=0;
  cursize=INITIAL_BUFSIZE;
  return freeHandle;
}
