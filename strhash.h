
#ifndef MKB_STRHASH_H
#define MKB_STRHASH_H


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define HT_SIZE 512

// MKB: simple chained hash table, can optimise this later
typedef struct strhash_entry {
  struct strhash_entry *next; 
  void  *value; 
  unsigned char key[8];
  unsigned int keylen;
  unsigned int flags;
} strhash_entry; 


static strhash_entry* strhash_entry_alloc()
{
  strhash_entry *she = (strhash_entry*)malloc(sizeof(strhash_entry));
  memset(she, 0, sizeof(strhash_entry)); 
  return she; 
}


/*
 * this algorithm (k=33) was first reported by dan bernstein many years ago 
 * in comp.lang.c. another version of this algorithm (now favored by bernstein) 
 * uses XOR: hash(i) = hash(i - 1) * 33 ^ str[i]; 
 * the magic of number 33 (why it works better than many other constants, 
 * prime or not) has never been adequately explained. 
 */
static uint16_t strhash_entry_hash(const char *restrict str)  
{
  int c;
  unsigned long hash = 5381;
  c = *str++;
  while ((c = *str++)) 
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash % HT_SIZE;
}



static bool strhash_table_store(strhash_entry *restrict hash_table[],
                                void* value, 
                                void* key, 
                                size_t keylen)
{
  uint16_t hash_idx   = strhash_entry_hash(key);    
  strhash_entry *slot = hash_table[hash_idx]; 

  if (!slot) {
    slot = strhash_entry_alloc(); 
    hash_table[hash_idx] = slot; 
  }
  else { 
    strhash_entry *prev;
    while (slot) {
      // key already in the hash_table
      if (slot->keylen == keylen && 
          memcmp(slot->key, key, keylen) == 0) 
      {
        slot->value = value; 
        return false; 
      }
      prev = slot;
      slot = slot->next; 
    }

    prev->next = (strhash_entry*)malloc(sizeof(strhash_entry));
    slot = prev; 
  }
  
  memcpy(slot->key, key, keylen);
  slot->keylen = keylen;
  slot->value = value; 
  slot->next  = (strhash_entry*)0; 
  return true;
}


static strhash_entry* strhash_table_load(strhash_entry *restrict hash_table[], const char *key, size_t keylen)
{
  uint16_t hash_idx = strhash_entry_hash(key); 
  strhash_entry *slot = hash_table[hash_idx]; 
  while (slot) {
    if (slot->keylen == keylen && 
        memcmp(key, slot->key, keylen) == 0) 
    {
      return slot; 
    }
    slot = slot->next; 
  }
  
  return (void*)NULL; 
}


static void strhash_table_clear(strhash_entry *restrict hash_table[])
{
  strhash_entry *slot; 
  strhash_entry *prev; 

  for (unsigned int i = 0; i < HT_SIZE; i++) {
    slot = hash_table[i]; 
    prev = NULL; 
    while (slot) {
      prev = slot; 
      slot = slot->next;
      free(prev); 
    }
    hash_table[i] = NULL;
  }
}


static void strhash_table_free(strhash_entry *restrict hash_table[])
{
  strhash_table_clear(hash_table); 
  free((void*)hash_table); 
}


#endif
