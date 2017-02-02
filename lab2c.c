#include "SortedList.h"
#define _GNU_SOURCE
#include <math.h>
#include <stdint.h>/* for uint64 definition */
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>/* for clock_gettime */
#define BILLION 1000000000L
#include <inttypes.h>

int numthreads=1;
int iterations=1;
int keylength=3;
int lists=1;
int m=0;
int s=0;
int optyield=0;
/* Struct that holds a list with its own synchronization*/
typedef struct{
  SortedList_t *list_per;
  pthread_mutex_t list_mutex;
  int volatile list_lock;
} args;
/* Variable that holds multiple lists*/
args *split_lists;

SortedListElement_t *elements;
/*function to generate key hash*/
int genHash(const char* source)
{
  int length = keylength;
  int hash = 0;
  int i;
  for(i = 0; i < length; i++) {
    char c = source[i];
    int a = c - '0';
    hash += a;// (hash * 10) + a;
  }
  return hash;
}
/*function that creates the random keys*/
void genRandom(SortedListElement_t a[]){
  int n=numthreads*iterations;
  int nchars= 1+'z'-'0';
  int counter=0;
  char *keylist=(char*)malloc((keylength+1)*n*sizeof(char));
  int i;
  for(i=0;i<((keylength+1)*n);i++){
    counter++;
    if(counter%(keylength+1)==0){
      keylist[i]='\0';
    }
    else{
      keylist[i]='0' + rand()%nchars;
    }
  }
  for(i=0;i<n;i++){
    a[i].key=keylist;
    keylist+=(keylength+1);
    a[i].next=NULL;
    a[i].prev=NULL;
  }
}

void *thr_func(void *arg) {
  intptr_t iter = (intptr_t) arg;
  int length=0;
  int i;
  if(m==1){
     for(i=iter; i<(iter+iterations);i++){
      int hash = genHash(elements[i].key)%lists;
      pthread_mutex_lock(&split_lists[hash].list_mutex);
      SortedList_insert(split_lists[hash].list_per,&elements[i]);
      pthread_mutex_unlock(&split_lists[hash].list_mutex);
    }
    for(i=0;i<lists;i++){
      pthread_mutex_lock(&split_lists[i].list_mutex);
      length += SortedList_length(split_lists[i].list_per);
      pthread_mutex_unlock(&split_lists[i].list_mutex);
    }
    for(i=iter;i<iter+iterations;i++){
      int hash=genHash(elements[i].key)%lists;
      pthread_mutex_lock(&split_lists[hash].list_mutex);
      SortedListElement_t *del = SortedList_lookup(split_lists[hash].list_per,elements[i].key);
      SortedList_delete(del);
      pthread_mutex_unlock(&split_lists[hash].list_mutex);
    }
  }
  else if(s==1){
    //while (__sync_lock_test_and_set(&lock, 1));
    for(i=iter; i<(iter+iterations);i++){
      int hash = genHash(elements[i].key)%lists;
      while (__sync_lock_test_and_set(&split_lists[hash].list_lock, 1));
      SortedList_insert(split_lists[hash].list_per,&elements[i]);
      __sync_lock_release(&split_lists[hash].list_lock);
    }
    for(i=0;i<lists;i++){
      while (__sync_lock_test_and_set(&split_lists[i].list_lock, 1));
      length += SortedList_length(split_lists[i].list_per);
      __sync_lock_release(&split_lists[i].list_lock);
      
    }
    for(i=iter;i<iter+iterations;i++){
      int hash=genHash(elements[i].key)%lists;
      while (__sync_lock_test_and_set(&split_lists[hash].list_lock, 1));
      SortedListElement_t *del = SortedList_lookup(split_lists[hash].list_per,elements[i].key);
      SortedList_delete(del);
      __sync_lock_release(&split_lists[hash].list_lock);
    }
    //__sync_lock_release(&lock);
  }
  else{
    for(i=iter; i<(iter+iterations);i++){
      int hash = genHash(elements[i].key)%lists;
      SortedList_insert(split_lists[hash].list_per,&elements[i]);
    }
    for(i=0;i<lists;i++)
      length += SortedList_length(split_lists[i].list_per);
    for(i=iter;i<iter+iterations;i++){
      int hash=genHash(elements[i].key)%lists;
      SortedListElement_t *del = SortedList_lookup(split_lists[hash].list_per,elements[i].key);
      SortedList_delete(del);
    }
  }
  pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    int opt=0;
    int longIndex=0;
    char* syncopt="";
    char* yieldopts="";
    static const char *optString = "tilsy";
    static struct option long_options[] = {
      {"threads", required_argument, NULL, 't'},
      {"iterations", required_argument, NULL,'i'},
      {"lists", required_argument, NULL,'l'},
      {"sync", required_argument, NULL,'s'},
      {"yield", required_argument, NULL,'y'},
      {NULL, no_argument, NULL, 0}
    };
    opt = getopt_long( argc, argv, optString, long_options, &longIndex );
    while( opt!= -1) {
      switch( opt ) {
      case 't':
	numthreads=atoi(optarg);
	break;
      case 'i':
	iterations=atoi(optarg);
	break;
      case 's':
	syncopt=optarg;
	break;
      case 'y':
	yieldopts=optarg;
	optyield=1;
	break;
      case 'l':
	lists=atoi(optarg);
	break;
      default:
	/* You won't actually get here. */
	exit(4);
	break;
      }
      opt = getopt_long( argc, argv, optString, long_options, &longIndex );
    }
    split_lists=(args*)malloc(lists*sizeof(args));
    if(strcmp(syncopt,"")!=0){
      if(strcmp(syncopt,"m")==0){
	m=1;
      }
      else if(strcmp(syncopt,"s")==0){
	s=1;
      }
    }
    for(int i =0;i<lists;i++){
      split_lists[i].list_per=(SortedList_t*)malloc(sizeof(SortedList_t));
      if(m==1){
	  if (pthread_mutex_init(&split_lists[i].list_mutex, NULL) != 0)
	    {
	      printf("\n mutex init failed\n");
	      return 1;
	    }
      }
      else if(s==1){
	split_lists[i].list_lock=0;
      }
    }
    if(optyield==1){
      if(strstr(yieldopts,"i")!=NULL){
	opt_yield |= INSERT_YIELD;
      }
      if(strstr(yieldopts,"d")!=NULL){
	opt_yield |= DELETE_YIELD;
      }
      if(strstr(yieldopts,"s")!=NULL){
	opt_yield |= SEARCH_YIELD;
      }
    }
    //  list=(SortedList_t*)malloc(sizeof(SortedList_t));
    int k =1;
    // while(lists<100){
    elements = (SortedListElement_t*)malloc(numthreads*iterations*sizeof(SortedListElement_t));
    genRandom(elements);
    uint64_t diff;
    struct timespec start, end;
    pthread_t *thr=malloc(numthreads*sizeof(pthread_t));
    pthread_attr_t attr;
    void *status;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE);
    int i, rc;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (i = 0; i < numthreads; ++i) {
      intptr_t level=iterations*i;
      if ((rc = pthread_create(&thr[i], &attr, &thr_func, (void*)level))) {
	//if ((rc = pthread_create(&thr[i], NULL, &thr_func, (void *)&args1))) {
	fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
	exit(EXIT_FAILURE);
      }
    }
    for (i = 0; i < numthreads; ++i) {
      pthread_join(thr[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);/* mark the end time */
    diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
    int operations= 2*iterations*numthreads;
    int avgop=diff/operations;
    int length=0;
    for(int i =0;i<lists;i++)
      length+=SortedList_length(split_lists[i].list_per);
    
    //if(length!=0){
    //printf("ERROR /n");
    //}
    //printf("%d/%d \t %d \n",numthreads,lists,avgop);
    //numthreads+=2;
    //lists+=1;
    //k++;
    //numthreads = k * lists;
    //    }
    if(length!=0){
      fprintf(stderr,"%d threads x %d iterations x (insert + lookup/delete) = %d operations \n",numthreads,iterations,operations);
      fprintf(stderr,"ERROR: final count = %d\n",length);
      fprintf(stderr,"elapsed time: %lluns\n",(long long unsigned int) diff);
      fprintf(stderr,"per operation: %d\n",(int) avgop);
      exit(1);
    }
    printf("%d threads x %d iterations x (insert + lookup/delete) = %d operations \n",numthreads,iterations,operations);
    printf("elapsed time: %lluns\n",(long long unsigned int) diff);
    printf("per operation: %dns\n",(int) avgop);
    exit(0);
}
