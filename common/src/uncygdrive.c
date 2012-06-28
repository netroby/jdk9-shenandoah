/*
 * Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <Windows.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

/*
 * Test if pos points to /cygdrive/_/ where _ can
 * be any character.
 */
int is_cygdrive_here(int pos, char *in, int len)
{
  // Length of /cygdrive/c/ is 12
  if (pos+12 > len) return 0;
  if (in[pos+11]=='/' &&
      in[pos+9]=='/' &&
      in[pos+8]=='e' &&
      in[pos+7]=='v' &&
      in[pos+6]=='i' &&
      in[pos+5]=='r' &&
      in[pos+4]=='d' &&
      in[pos+3]=='g' &&
      in[pos+2]=='y' &&
      in[pos+1]=='c' &&
      in[pos+0]=='/') {
    return 1;
  }
  return 0;
}

/*
 * Replace /cygdrive/_/ with _:/
 * Works in place since drive letter is always
 * shorter than /cygdrive/
 */
char *replace_cygdrive(char *in)
{
  int len = strlen(in);
  char *out = malloc(len+1);
  int i,j;

  if (len < 12) {
    strcpy(out, in);
    return out;
  }
  for (i = 0, j = 0; i<len;) {
    if (is_cygdrive_here(i, in, len)) {
      out[j++] = in[i+10];
      out[j++] = ':';
      i+=11;
    } else {
      out[j] = in[i];
      i++;
      j++;
    }
  }
  out[j] = 0;
  return out;
}

void append(char **b, size_t *bl, size_t *u, char *add, size_t addlen)
{
  while ( (addlen+*u+1) > *bl) {
    *bl *= 2;
    *b = realloc(*b, *bl);
  }
  memcpy(*b+*u, add, addlen);
  *u += addlen;
}

/*
 * Creates a new string from in where the first occurance of sub is
 * replaced by rep.
 */
char *replace_substring(char *in, char *sub, char *rep)
{
  int in_len = strlen(in);
  int sub_len = strlen(sub);
  int rep_len = strlen(rep);
  char *out = malloc(in_len - sub_len + rep_len + 1);
  char *p;

  if (!(p = strstr(in, sub))) {
    // If sub isn't a substring of in, just return in.
    return in;
  }

  // Copy characters from beginning of in to start of sub.
  strncpy(out, in, p - in);
  out[p - in] = '\0';

  sprintf(out + (p - in), "%s%s", rep, p + sub_len);

  return out;
}

char *files_to_delete[1024];
int num_files_to_delete = 0;

char *fix_at_file(char *in)
{
  char *tmpdir;
  char name[2048];
  char *atname;
  char *buffer;
  size_t buflen=65536;
  size_t used=0;
  size_t len;
  int rc;
  FILE *atout;
  FILE *atin;
  char block[2048];
  size_t blocklen;
  char *fixed;

  atin = fopen(in+1, "r");
  if (atin == NULL) {
    fprintf(stderr, "Could not read at file %s\n", in+1);
    exit(-1);
  }

  tmpdir = getenv("TMP");
  if (tmpdir == NULL) {
    tmpdir = "c:/cygwin/tmp";
  }
  _snprintf(name, sizeof(name), "%s\\atfile_XXXXXX", tmpdir);

  rc = _mktemp_s(name, strlen(name)+1);
  if (rc) {
    fprintf(stderr, "Could not create temporary file name for at file!\n");
    exit(-1);
  }

  atout = fopen(name, "w");
  if (atout == NULL) {
    fprintf(stderr, "Could open temporary file for writing! %s\n", name);
    exit(-1);
  }

  buffer = malloc(buflen);
  while((blocklen = fread(block,1,sizeof(block),atin)) > 0) {
    append(&buffer, &buflen, &used, block, blocklen);
  }
  buffer[used] = 0;
  fixed = replace_cygdrive(buffer);
  fwrite(fixed, strlen(fixed), 1, atout);
  fclose(atin);
  fclose(atout);
  free(fixed);
  free(buffer);
  files_to_delete[num_files_to_delete] = malloc(strlen(name)+1);
  strcpy(files_to_delete[num_files_to_delete], name);
  num_files_to_delete++;
  atname = malloc(strlen(name)+2);
  atname[0] = '@';
  strcpy(atname+1, name);
  return atname;
}

int main(int argc, char **argv)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    unsigned short rc;

    char *new_at_file;
    char *old_at_file;
    char *line;
    int i;
    DWORD exitCode;

    if (argc<2) {
        fprintf(stderr, "Usage: uncygdrive.exe /cygdrive/c/WINDOWS/notepad.exe /cygdrive/c/x/test.txt");
        exit(0);
    }

    line = replace_cygdrive(strstr(GetCommandLine(), argv[1]));

    for (i=1; i<argc; ++i) {
       if (argv[i][0] == '@') {
          // Found at-file! Fix it!
          old_at_file = replace_cygdrive(argv[i]);
          new_at_file = fix_at_file(old_at_file);
          line = replace_substring(line, old_at_file, new_at_file);
       }
    }

    if (getenv("DEBUG_UNCYGDRIVE") != NULL) {
      fprintf(stderr, "uncygdrive >%s<\n", line);
    }

    ZeroMemory(&si,sizeof(si));
    si.cb=sizeof(si);
    ZeroMemory(&pi,sizeof(pi));

    rc = CreateProcess(NULL,
                       line,
                       0,
                       0,
                       TRUE,
                       0,
                       0,
                       0,
                       &si,
                       &pi);
    if(!rc)
    {
      //Could not start process;
      fprintf(stderr, "Could not start process!\n");
      exit(-1);
    }

    WaitForSingleObject(pi.hProcess,INFINITE);
    GetExitCodeProcess(pi.hProcess,&exitCode);

    if (getenv("DEBUG_UNCYGDRIVE") != NULL) {
      for (i=0; i<num_files_to_delete; ++i) {
        fprintf(stderr, "Not deleting temporary uncygdrive file %s\n",
                files_to_delete[i]);
      }
    }
    else {
      for (i=0; i<num_files_to_delete; ++i) {
        remove(files_to_delete[i]);
      }
    }

    exit(exitCode);
}