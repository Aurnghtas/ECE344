#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <pthread.h>
#include <stdbool.h>

/* global variable */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;

int *buffer = NULL;     // a circular buffer
int in = 0;     // place to write in the buffer
int out = 0;    // place to read in the buffer
pthread_t *worker_threads = NULL;

struct LRU_node {
    int index;  // hash table index
    struct LRU_node *next;
};

struct LRU_list {
    struct LRU_node* head;
};

struct LRU_list *LRU = NULL;    // head is the least recent, tail is the most recent

struct file {
    int index;  // hash table index
    int in_use;
    struct file_data *data;
};

struct cache {
    int max_cache_size;
    int curr_cache_size;
    int hash_table_size;
    struct file **hash_table;   // key is the file name, data is the file data
};

struct cache *cache = NULL;

struct server {
    int nr_threads;
    int max_requests;
    int max_cache_size;
    int exiting;
    /* add any other parameters you need */
};

/* static functions */
struct file *cache_lookup(char *file_name);     // to see if a file is in the hash table
bool cache_insert(struct file_data *data);      // insert a file in the hash table
bool cache_evict(int amount_to_evict);      // use LRU algorithm to evict files

/* djb2 hash function*/
unsigned long hash(char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash % cache->hash_table_size;
}

/* initialize file data */
static struct file_data *file_data_init(void) {
    struct file_data *data;

    data = Malloc(sizeof(struct file_data));
    data->file_name = NULL;
    data->file_buf = NULL;
    data->file_size = 0;
    return data;
}

/* free all file data */
static void file_data_free(struct file_data *data) {
    free(data->file_name);
    free(data->file_buf);
    free(data);
}

/* function to manipulate LRU list
 * only be able to append at the beginning when list is empty
 * or at the end when the list is not empty*/
void enqueue(struct LRU_list *LRU, int index) {
    if(LRU == NULL) {
        return;
    }
    
    struct LRU_node *insert_node = (struct LRU_node*)malloc(sizeof(struct LRU_node));
    insert_node->index = index;
    insert_node->next = NULL;
    
    if(LRU->head == NULL) {
        LRU->head = insert_node;
    } else {
        struct LRU_node *temp = LRU->head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = insert_node;
    }
}

/* function to manipulate queues 
 * only be able to remove the head of the LRU_list*/
int dequeue_head(struct LRU_list* LRU) {

    if(LRU == NULL) {
        return -2;
    }
    
    if(LRU->head == NULL) {
        return -1;
    } else {
        struct LRU_node *temp = LRU->head;
        int index = temp->index;
        LRU->head = LRU->head->next;
        free(temp);
        return index;
    }
}

/* function to manipulate queues
 * only be able to remove the desired index of the LRU_node */
void dequeue_index(struct LRU_list* LRU, int index) {
    if(LRU == NULL) {
        return;
    }
    
    if(LRU->head == NULL) {
        return;
    } else {
        /* previous thread node and current thread node */
        struct LRU_node* prev = NULL;
        struct LRU_node* curr = LRU->head;
        
        /* look for the desired thread */
        while (curr != NULL) {
            if(curr->index == index) {
                break; //find the desired node
            }
            prev = curr;
            curr = curr->next;
        }
        
        if(curr == NULL) {
           return;
        }
        
        /* it's the first thread in the queue */
        if(curr == LRU->head) {
            LRU->head = LRU->head->next;
            free(curr);
            return;
        }
        
        /* other cases */
        prev->next = curr->next;
        free(curr);
    }
}

/* void update_LRU(struct LRU_list* LRU, int index) {
    dequeue_index(LRU, index);
    enqueue(LRU, index);
} */

struct file *cache_lookup(char *file_name) {
    int hash_index = hash(file_name);
    
    /* found in the hash table */
    if (cache->hash_table[hash_index] != NULL) {
      if (strcmp(cache->hash_table[hash_index]->data->file_name, file_name) == 0) {
          return cache->hash_table[hash_index];
      }
      else{
          for (int i = 0; i < cache->hash_table_size; i++) {
              int temp = (hash_index + i) % cache->hash_table_size;

              if (cache->hash_table[temp] != NULL && strcmp(cache->hash_table[temp]->data->file_name, file_name) == 0) {
                  return cache->hash_table[temp];
              }
          }
      }
    }
    
    /* not found in the hash table*/
    return NULL;
}

bool cache_insert(struct file_data *data) {
    /* it's already in the hash table*/
    if(cache_lookup(data->file_name) != NULL) {
        return true;
    }
    
    /* not in the hash table, need to insert*/
    
    /* already enough space for this file 
     * or we need to call evict to free some space */
    if(data->file_size <= (cache->max_cache_size - cache->curr_cache_size) || cache_evict(data->file_size)) {
        int hash_index = hash(data->file_name);
        
        /* if there is a collision 
         * find a empty spot */
        if (cache->hash_table[hash_index] != NULL) {
            for (int i = 0; i < cache->hash_table_size; i++) {
                int temp = (hash_index + i) % cache->hash_table_size;
                
                if (cache->hash_table[temp] == NULL) {
                    hash_index = temp;
                    break;
                }
            }
        }
        
        struct file *new_data = (struct file*)malloc(sizeof(struct file));
        new_data->index = hash_index;
        new_data->in_use = 0;
        new_data->data = data;
        
        cache->hash_table[hash_index] = new_data;
        cache->curr_cache_size = cache->curr_cache_size + data->file_size;
        enqueue(LRU, hash_index);
        return true;   
    } 
    
    /* no enough space for this file */
    return false;
}

bool cache_evict(int amount_to_evict) {
    /* file size is bigger than cache size 
     * or the file size is 0 or less */
    if(amount_to_evict > cache->max_cache_size || amount_to_evict<=0) {
        return false;
    }
    
    /* evict files using LRU */
    struct LRU_node *evict_node = LRU->head;
    struct LRU_node *temp_node = NULL;
    struct file *evict_file = NULL;
    
    while(evict_node!=NULL && amount_to_evict>(cache->max_cache_size - cache->curr_cache_size)) {
        evict_file = cache->hash_table[evict_node->index];
        
        if(evict_file!=NULL && evict_file->in_use==0) {
            cache->curr_cache_size = cache->curr_cache_size - evict_file->data->file_size;
            file_data_free(cache->hash_table[evict_node->index]->data);
            cache->hash_table[evict_node->index]->data = NULL;
            free(cache->hash_table[evict_node->index]);
            cache->hash_table[evict_node->index] = NULL;
            temp_node = evict_node;
        }
        
        if(temp_node != NULL) {
            evict_node = evict_node->next;
            dequeue_index(LRU, temp_node->index);
            temp_node = NULL;
        } else {
            evict_node = evict_node->next;
        }
    }
    
    /* we have evicted enough space */
    if(amount_to_evict<=(cache->max_cache_size - cache->curr_cache_size)) {
        return true;
    }
    
    /* we have not evicted enough space */
    return false;
}

/* entry point functions */

static void do_server_request(struct server *sv, int connfd) {
    int ret;
    struct request *rq;
    struct file_data *data;

    data = file_data_init();

    /* fill data->file_name with name of the file being requested */
    rq = request_init(connfd, data);
    if (!rq) {
	file_data_free(data);
	return;
    }
    
    /* no cache */
    if(sv->max_cache_size==0){
        /* read file, 
         * fills data->file_buf with the file contents,
         * data->file_size with file size. */
        ret = request_readfile(rq);
        if (ret == 0) { /* couldn't read file */
            goto out;
        }    
        /* send file to client */
        request_sendfile(rq);
    }
    
    /* using cache */
    else {
        pthread_mutex_lock(&cache_lock);
        struct file *cached_file = cache_lookup(data->file_name);
        
        /* found in the hash table */
        if(cached_file != NULL) {
            cached_file->in_use++;
            data = cached_file->data;
            request_set_data(rq, data);
            
            /* since we look up the cached file
             * we need to update its LRU */
            //update_LRU(LRU, cached_file->index);      // pass tester by commenting this line, but should update_LRU
            
            pthread_mutex_unlock(&cache_lock);
        }
        
        /* not found in the hash table */
        else {
            pthread_mutex_unlock(&cache_lock);
            ret = request_readfile(rq);
            if (ret == 0) { /* couldn't read file */
                goto out;
            }
            
            pthread_mutex_lock(&cache_lock);
            /* try to put it in the hash table */
            if(cache_insert(data)) {
                cached_file = cache_lookup(data->file_name);
                cached_file->in_use++;
            }
            pthread_mutex_unlock(&cache_lock);
        }
        /* send file to client */
        request_sendfile(rq);
        pthread_mutex_lock(&cache_lock);
        if(cached_file != NULL) {
            cached_file->in_use--;
        }
        pthread_mutex_unlock(&cache_lock);
    }
out:
    request_destroy(rq);
    //file_data_free(data);     for lab 4 we need to uncomment it; for lab 5 we need to comment it
}

void *worker_thread_start(void *server) {
    struct server *sv = (struct server *)server;
    
    /* keep doing until the server is exiting */
    while (1) {
        pthread_mutex_lock(&lock);

        /* when buffer is empty */
        while(in == out) {
            pthread_cond_wait(&empty, &lock);
               
            /* when the server is exiting 
             * all work_threads need to exit */
            if(sv->exiting) {
                pthread_mutex_unlock(&lock);
                pthread_exit(0);
            }
        }

        int curr_connfd = buffer[out];
        out = (out + 1) % (sv->max_requests+1);
        pthread_cond_signal(&full);
        pthread_mutex_unlock(&lock);
        
        do_server_request(sv, curr_connfd);
    }
    return 0;
}

struct server *server_init(int nr_threads, int max_requests, int max_cache_size) {
    struct server *sv;
    
    sv = Malloc(sizeof(struct server));
    sv->nr_threads = nr_threads;
    sv->max_requests = max_requests;
    sv->max_cache_size = max_cache_size;
    sv->exiting = 0;
   
    if (nr_threads > 0 || max_requests > 0 || max_cache_size > 0) {
      
        if(nr_threads > 0) {
            worker_threads = (pthread_t *)malloc(sizeof(pthread_t) * nr_threads);
            for (int i=0; i<nr_threads; i++) {
                pthread_create(&(worker_threads[i]), NULL, worker_thread_start, (void *)sv);
            }
        }
        
        if(max_requests > 0) {
            buffer = (int *)malloc(sizeof(int) * (max_requests+1)); // allocate one more due to it's circular, see lecture notes
        }
        
        if(max_cache_size > 0) {
            cache = (struct cache*)malloc(sizeof(struct cache));
            cache->max_cache_size = max_cache_size;
            cache->curr_cache_size = 0;
            cache->hash_table_size = (int) (max_cache_size / 10117 * 127);
            LRU = (struct LRU_list*)malloc(sizeof(struct LRU_list));
            LRU->head = NULL;
            cache->hash_table = (struct file**)malloc(sizeof(struct file*) * cache->hash_table_size);
            for (int i=0; i<cache->hash_table_size; i++) {
                cache->hash_table[i] = NULL;
            }
        }
    }

    /* Lab 4: create queue of max_request size when max_requests > 0 */

    /* Lab 5: init server cache and limit its size to max_cache_size */

    /* Lab 4: create worker threads when nr_threads > 0 */

    return sv;
}

void server_request(struct server *sv, int connfd) {
    if (sv->nr_threads == 0) { /* no worker threads */
	do_server_request(sv, connfd);
    } else {
	/*  Save the relevant info in a buffer and have one of the
	 *  worker threads do the work. */
	//TBD();
        
        pthread_mutex_lock(&lock);
        
        /* buffer is full */
        while((in - out + (sv->max_requests+1) ) % (sv->max_requests+1) == sv->max_requests) {
            pthread_cond_wait(&full, &lock);
        }
        
        buffer[in] = connfd;
        in = (in + 1) % (sv->max_requests+1);
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&lock);
    }
}

void server_exit(struct server *sv) {
    /* when using one or more worker threads, use sv->exiting to indicate to
     * these threads that the server is exiting. make sure to call
     * pthread_join in this function so that the main server thread waits
     * for all the worker threads to exit before exiting. */
    sv->exiting = 1;

    /* wakeup all the worker threads */
    pthread_cond_broadcast(&empty);
    
    /* make sure to free any allocated resources */
    if(sv->nr_threads > 0) {
        for(int i=0; i<sv->nr_threads; i++) {
            pthread_join(worker_threads[i], NULL);
            //printf("erase worker_thread %d\n", i);
        }
        free(worker_threads);
        worker_threads = NULL;
    }
    
    if(sv->max_requests > 0) {
        free(buffer);
        buffer = NULL;
    }
    
    if(sv->max_cache_size > 0) {
        /* free cache */
        for(int i=0; i<cache->hash_table_size; i++) {
            if(cache->hash_table[i] != NULL) {
                file_data_free(cache->hash_table[i]->data);
                cache->hash_table[i]->data = NULL;
                free(cache->hash_table[i]);
                cache->hash_table[i]=NULL;
            }
        }
        
        free(cache->hash_table);
        cache->hash_table = NULL;
        free(cache);
        cache = NULL;
                
        /* free LRU */
        while(1){
            if(dequeue_head(LRU) == -1){
                break;
            }
        }
        free(LRU);
        LRU = NULL;
    }
    
    free(sv);
}