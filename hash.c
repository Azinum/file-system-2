// hash.c

#include "hash.h"

// djb2
unsigned long hash(const char *input, unsigned long size) {
   unsigned long hash_number = 5381;
   int c;

   for (unsigned long i = 0; i < size; i++) {
      c = input[i];
      hash_number = ((hash_number << 5) + hash_number) + c;
   }

   return hash_number;
}