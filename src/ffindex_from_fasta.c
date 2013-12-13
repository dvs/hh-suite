/*
 * FFindex
 * written by Andy Hauser <hauser@genzentrum.lmu.de>.
 * Please add your name here if you distribute modified versions.
 * 
 * FFindex is provided under the Create Commons license "Attribution-ShareAlike
 * 3.0", which basically captures the spirit of the Gnu Public License (GPL).
 * 
 * See:
 * http://creativecommons.org/licenses/by-sa/3.0/
*/

#define _GNU_SOURCE 1
#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS 64

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>


#include "ffindex.h"
#include "ffutil.h"

#define MAX_FILENAME_LIST_FILES 4096
#define MAX_ENTRY_LENGTH 100000


void usage(char *program_name)
{
    fprintf(stderr, "USAGE: %s -v | [-s] data_header_filename index_header_filename data_sequence_filename index_sequence_filename fasta_filename\n"
                    "\t-s\tsort index file\n"
                    "\nBases on a Design and Implementation of Andreas W. Hauser <hauser@genzentrum.lmu.de>.\n", program_name);
}

int main(int argn, char **argv)
{
  int sort = 0, version = 0;
  int opt, err = EXIT_SUCCESS;
  while ((opt = getopt(argn, argv, "sv")) != -1)
  {
    switch (opt)
    {
      case 's':
        sort = 1;
        break;
      case 'v':
        version = 1;
        break;
      default:
        usage(argv[0]);
        return EXIT_FAILURE;
    }
  }

  if(version == 1)
  {
    /* Don't you dare running it on a platform where byte != 8 bits */
    printf("%s version %.2f, off_t = %zd bits\n", argv[0], FFINDEX_VERSION, sizeof(off_t) * 8);
    return EXIT_SUCCESS;
  }

  if(argn - optind < 3)
  {
    usage(argv[0]);
    return EXIT_FAILURE;
  }


  char *data_filename  = argv[optind++];
  char *index_filename = argv[optind++];

  char *fasta_filename = argv[optind++];

  printf("data file: %s\n", data_filename);
  printf("index file: %s\n", index_filename);
  printf("fasta file: %s\n", fasta_filename);


  FILE *data_file, *index_file, *fasta_file;
  size_t offset = 0;

  /* open ffindex */
  err = ffindex_index_open(data_filename, index_filename, "w", &data_file, &index_file, &offset);
  if(err != EXIT_SUCCESS)
    return err;

  fasta_file = fopen(fasta_filename, "r");
  if(fasta_file == NULL) { perror(fasta_filename); return EXIT_FAILURE; }

  size_t fasta_size;
  char *fasta_data = ffindex_mmap_data(fasta_file, &fasta_size);
//  size_t from_length = 0;

  char name[FFINDEX_MAX_ENTRY_NAME_LENTH];
  int seq_id = 1;
  size_t seq_id_length = 0;
  size_t count_ws = 0;

  char entry[MAX_ENTRY_LENGTH];
  entry[0] = '>';
  size_t entry_length = 0;

  for(size_t fasta_offset = 1; fasta_offset < fasta_size; fasta_offset++) // position after first ">"
  {
    seq_id_length = 0;
    count_ws = 0;

    entry_length = 1;

    while(fasta_offset < fasta_size && !(*(fasta_data + fasta_offset) == '>' && *(fasta_data + fasta_offset - 1) == '\n'))
    {
      char input = *(fasta_data + fasta_offset);

      //get fasta name
      if(isspace(input))
      {
        count_ws++;
        name[seq_id_length] = '\0';
      }
      else if(count_ws == 0)
      {
        name[seq_id_length++] = *(fasta_data + fasta_offset);
      }

      entry[entry_length++] = input;

      fasta_offset++;
    }

    if(seq_id_length == 0) {
      sprintf(name, "%d", seq_id);
    }
    seq_id++;

    ffindex_insert_memory(data_file, index_file, &offset, entry, entry_length, name);
  }
  fclose(data_file);

  /* Sort the index entries and write back */
  if(sort)
  {
    rewind(index_file);
    ffindex_index_t* index = ffindex_index_parse(index_file, 0);
    if(index == NULL)
    {
      perror("ffindex_index_parse failed");
      exit(EXIT_FAILURE);
    }
    fclose(index_file);
    ffindex_sort_index_file(index);
    index_file = fopen(index_filename, "w");
    if(index_file == NULL) { perror(index_filename); return EXIT_FAILURE; }
    err += ffindex_write(index, index_file);
  }

  return err;
}

/* vim: ts=2 sw=2 et: */
