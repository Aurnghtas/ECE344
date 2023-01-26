#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "wc.h"
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#define HASH_TABLE_SIZE 2200000

//this function determines the index of the hash table
unsigned int hash(char *s) {
    unsigned int h = 0;

    for (; *s; s++) {
        h = *s + h * 31;
    }

    return h % HASH_TABLE_SIZE;
}

struct dataItem {
    char* word_array;
    int num_occur;
};

struct wc {
    /* you can define this struct to have whatever fields you want. */
    struct dataItem *hash_table[HASH_TABLE_SIZE];
};

bool insert(struct dataItem *data, struct wc *wc) {

  if (data == NULL) {
      return false;
  }

  int hash_index = hash(data->word_array);
  
  if(hash_index == 0) {
      return true;
  }

  if (wc->hash_table[hash_index] != NULL) {
    if (strcmp(wc->hash_table[hash_index]->word_array, data->word_array) == 0) {
        wc->hash_table[hash_index]->num_occur++;
        return true;
    }
    else{
        for (int i = 0; i < HASH_TABLE_SIZE; i++) {
            int temp = (hash_index + i) % HASH_TABLE_SIZE;
            
            if (wc->hash_table[temp] != NULL && strcmp(wc->hash_table[temp]->word_array, data->word_array) == 0) {
                wc->hash_table[temp]->num_occur++;
                return true;
            }
            
            if (wc->hash_table[temp] == NULL) {
                wc->hash_table[temp] = data;
                wc->hash_table[temp]->num_occur++;
                return true;
            }
        }
    }
  }

    wc->hash_table[hash_index] = data;
    wc->hash_table[hash_index]->num_occur++;
    return true;

}

struct wc * wc_init(char *word_array, long size) {
    struct wc *wc;

    wc = (struct wc *)malloc(sizeof(struct wc));
    assert(wc);
    
    for(int i=0;i<HASH_TABLE_SIZE; i++){
        wc->hash_table[i] = NULL;
    }

    char temp_word[100] = {0};
    int length = 0;

    for(int i=0; i<size; i++){
        if(isspace(word_array[i]) != 0 /*|| word_array[i] == '\0'*/) {
            strncpy(temp_word, &word_array[i - length], length);
            temp_word[length] = '\0';
            
            struct dataItem *temp_data;
            temp_data = (struct dataItem *)malloc(sizeof(struct dataItem));
            temp_data->word_array = malloc(sizeof(char)*(strlen(temp_word)+1));
            
            strcpy(temp_data->word_array, temp_word);
            if (!insert(temp_data, wc)) {
                fprintf(stderr, "insert failed");
            }
           // temp_data->word_array = NULL;
            //free(temp_data->word_array);
            temp_data = NULL;
            free(temp_data);
            
            memset(temp_word,0,100);
            length = 0;
        } else {
//            temp_word[length] = word_array[i];
            length++;
        }
    }
    
    

    return wc;
}

void wc_output(struct wc *wc) {
    for(int i=0; i<HASH_TABLE_SIZE; i++) {
        if (wc->hash_table[i] != NULL) {
            printf("%s:%d\n", wc->hash_table[i]->word_array, wc->hash_table[i]->num_occur);
        }
    }
}

void wc_destroy(struct wc *wc) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        if (wc->hash_table[i] != NULL) {
            free(wc->hash_table[i]);
        }
    }
    free(wc);
}



/*void wc_output(struct wc *wc)
{
  for (int i = 0; i < HASHSIZE; i++) {
    if (wc[i] != NULL) {
      printf("%s:%d\n", wc[i]->word_array, wc[i]->num_occur);
    }
  }
}

void wc_destroy(struct wc *wc)
{
  for (int i = 0; i < HASHSIZE; i++) {
    if (wc[i] != NULL) {
      free(wc[i]);
    }
  }
  free(wc);
}*/
/*int main(int argc, char const *argv[]) {
  char word_array[50] = "hardware hardware head on.";
  struct wc *wc= wc_init(word_array, 20);
  wc_output(wc);
  return 0;
}*/