
#include <stdio.h>
#include "threadpool.h"
#include <stdlib.h>

void freeData( pthread_t * , threadpool * );
void pthread_condMutex_destroy( threadpool * );

//Macro for the pool
//get the work list head node
#define list_head(p) p -> qhead
//get the  work list tail node
#define list_tail(p) p -> qtail
//get the size of the work list
#define list_size(p) p -> qsize
//get the dont_accept value
#define pool_dont_accept(p) p -> dont_accept
//get the shutdown value
#define pool_shutdown(p) p -> shutdown
//get the num_threads value
#define pool_num_threads(p) p -> num_threads
//get the threads  array
#define pool_threads(p) p -> threads
//get the lock value
#define pool_lock(p) p -> qlock
//get the empty value
#define pool_empty(p) p -> q_empty
//get the not_empty value
#define pool_not_empty(p) p -> q_not_empty

//Macro for the node
//get the next node
#define node_next(n) n -> next
//get the arg " parameter "
#define node_arg(n) n -> arg
//get the routine job/function
#define node_routine(n) n -> routine

//for pool_shutdown  and  pool_dont_accept value
#define STOP 0
#define START 1

threadpool* create_threadpool(int num_threads_in_pool) { //init the pool
    if ( num_threads_in_pool < 1 || num_threads_in_pool > MAXT_IN_POOL ) { //input sanity/legacy check
        printf( "\nInput\n" );
        return NULL;
    }
    threadpool * pool;
    if ( ( pool = (threadpool * ) malloc( sizeof( threadpool ) ) ) == NULL ) {
        printf( "\nMalloc\n " );
        return NULL;
    }
    // after we start the pool we enter the value
    pool_num_threads( pool ) = num_threads_in_pool; // the requested  thread number
    list_size(pool) = 0; // size of the list
    if ( ( pool_threads( pool ) = ( pthread_t * ) malloc( pool_num_threads(pool) * sizeof( pthread_t ) ) ) == NULL ) {
        printf("\nMalloc\n ");
        return NULL;
    }
    list_head( pool ) = list_tail( pool ) = NULL; // the head and the tail equal to null
    if ( pthread_mutex_init( & pool_lock( pool ), NULL) != 0 ) { //mutex
        printf("\nMutex\n ");
        return NULL;
    }
    pthread_cond_init( & pool_empty( pool ), NULL); //Conditional Variables pool_empty
    pthread_cond_init( & pool_not_empty( pool ), NULL); //Conditional Variables pool_not_empty
    pool_shutdown( pool ) = pool_dont_accept( pool ) = STOP; // shutdown and dont_accept processes on stop mode

    for ( int i = 0; i < num_threads_in_pool; i++ ) { //start the pool_threads
        if ( pthread_create(  & pool_threads(pool)[ i ], NULL, do_work, pool ) < -1 ) {
            printf( "\ncreate\n " );
            pthread_condMutex_destroy( pool ); // free locks
            freeData( pool_threads( pool ), pool ); // free data
            return NULL;
        }
    }

    return pool; // return the pool
}

void dispatch(threadpool * from_me, dispatch_fn dispatch_to_here, void * arg) {// add work/job
    if ( arg == NULL || from_me == NULL || dispatch_to_here == NULL ) { //input sanity/legacy check
        return;
    }
    // if the dont_accept process start
    if ( pool_dont_accept(from_me) == START ) { //dont accept
        return;
    }
    // queue new work to add
    work_t * work ;
    if ( ( work = ( work_t * ) malloc( sizeof( work_t ) ) ) == NULL ) {
        printf( "\nMalloc\n" );
        return;
    }
    // start the work struct to prepare it to function do_work push it to the queue
    node_routine( work ) = dispatch_to_here; // link function
    node_arg( work ) = arg; // link arg
    pthread_mutex_lock( & pool_lock( from_me ) );//in order to push we have to lock

    if (list_tail( from_me ) == NULL) { // empty queue
        //the head and the tail point to the new work node
        list_head(from_me) = work;
        list_tail(from_me) = work;
    } else { // add at the last place "tail" because do_work take from the first place "head"
        node_next( list_tail( from_me ) ) = work;
        list_tail( from_me ) = work;
    }
    list_size( from_me ) ++; // queue is bigger now

    pthread_cond_signal( & pool_not_empty( from_me ) ); // call thread to do the work
    pthread_mutex_unlock( & pool_lock( from_me ) ); // we finish adding  unlock
}

void* do_work(void* p) {
    threadpool * pool = ( threadpool * ) p; // get the pool
    work_t * work = NULL; // work to take from the queue
    /*
     *
     * thread spend his life here in
     * endless while search for job to do
     *
     */
    while (1) {
        pthread_mutex_lock( & pool_lock( pool ) ); // we're going to work on the pool so lock

        if (pool_shutdown( pool ) == START) { // destruction start
            pthread_mutex_unlock( & pool_lock( pool ) ); // return the lock
            return NULL; // exit the thread
        }

        if ( list_size( pool ) == 0 ) { // if the queue is empty
            pthread_cond_wait(  & pool_not_empty( pool ), & pool_lock( pool ) ); //wait
        }

        if ( pool_shutdown( pool ) == START ) { // check again destruction flag
            pthread_mutex_unlock( & pool_lock(pool)); // return the lock
            return NULL; // exit the thread
        }
        if ( list_size( pool ) != 0 ) {
            work = list_head(pool);
            list_head(pool) = node_next(list_head(pool));
            list_size(pool)--;
            if (list_size(pool) == 0) {
                list_head(pool) = list_tail(pool) = NULL;
            }
        }
        if ( pool_dont_accept( pool ) == START && list_size( pool ) == 0 ) { //now we can start the destructor process
            pthread_cond_signal( & pool_empty( pool ) );
        }
        pthread_mutex_unlock( & pool_lock( pool ) ); // return the lock
        if (work != NULL) {
            node_routine( work )( node_arg( work ) ); // start the function
            free( work );
        }
    }
}

void destroy_threadpool(threadpool* destroyme) {
    pthread_mutex_lock( & pool_lock( destroyme ) ); // we're going to work on the pool so protect
    pool_dont_accept( destroyme ) = START; // don't accept any job
    if (list_size( destroyme ) > 0) { // there is job not finish yet
        pthread_cond_wait( & pool_empty( destroyme ), & pool_lock( destroyme ) ); // wait
    }
    pool_shutdown( destroyme ) = START; // start the destruction

    pthread_cond_broadcast( & pool_not_empty( destroyme ) ); // send signal to all the thread to die
    pthread_mutex_unlock( & pool_lock( destroyme ) ); // we finish sir

    for ( int i = 0; i < pool_num_threads(destroyme); i++ ) { // main join the threads
        if ( pthread_join(pool_threads( destroyme )[ i ], NULL ) != 0) {
            printf("\njoin\n");
            return;
        }
    }
    pthread_condMutex_destroy(destroyme);
    freeData(pool_threads(destroyme), destroyme);
}

//free the data
void freeData(pthread_t * threads, threadpool * pool) {
    if (threads != NULL) {
        free(threads);
    }
    if (pool != NULL) {
        free(pool);
    }

}
//free the locks
void pthread_condMutex_destroy(threadpool * pool) {
    if (pool != NULL) {
        pthread_cond_destroy( & pool_not_empty(pool));
        pthread_mutex_destroy( & pool_lock(pool));
        pthread_cond_destroy( & pool_empty(pool));
    }
}