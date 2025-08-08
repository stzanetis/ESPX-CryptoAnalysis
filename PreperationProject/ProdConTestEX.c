#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <limits.h>

#define QUEUESIZE 10
#define LOOP 20

int q, p;

struct workFunction {
    void *(*work)(void *);
    void *arg;

    struct timeval enqueue_time;
    struct timeval dequeue_time;
    int producer_id;
    int work_id;
};

typedef struct {
    struct workFunction buf[QUEUESIZE];
    long head, tail;
    int full, empty;
    pthread_mutex_t *mut;
    pthread_cond_t *notFull, *notEmpty;
} queue;

struct {
    long total_time;
    long min_time;
    long max_time;
    int count;
    long total_mutex_wait_time;
    long max_mutex_wait_time;
    int mutex_contests;
    int empty_encounters;
    int full_encounters;
    pthread_mutex_t *mut;
} queue_stats;

void initQueueStats() {
    queue_stats.total_time = 0;
    queue_stats.min_time = LONG_MAX;
    queue_stats.max_time = 0;
    queue_stats.count = 0;
    
    queue_stats.total_mutex_wait_time = 0;
    queue_stats.max_mutex_wait_time = 0;
    queue_stats.mutex_contests = 0;
    
    queue_stats.empty_encounters = 0;
    queue_stats.full_encounters = 0;
    
    queue_stats.mut = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(queue_stats.mut, NULL);
}

void updateQueueStats(long time_diff_usec) {
    pthread_mutex_lock(queue_stats.mut);
    
    queue_stats.total_time += time_diff_usec;
    queue_stats.count++;
    
    if (time_diff_usec < queue_stats.min_time) {
        queue_stats.min_time = time_diff_usec;
    }
    if (time_diff_usec > queue_stats.max_time) {
        queue_stats.max_time = time_diff_usec;
    }
    
    pthread_mutex_unlock(queue_stats.mut);
}

void updateMutexWaitStats(long wait_time_usec) {
    pthread_mutex_lock(queue_stats.mut);
    
    queue_stats.total_mutex_wait_time += wait_time_usec;
    queue_stats.mutex_contests++;
    
    if (wait_time_usec > queue_stats.max_mutex_wait_time) {
        queue_stats.max_mutex_wait_time = wait_time_usec;
    }
    
    pthread_mutex_unlock(queue_stats.mut);
}

void updateQueueFullStats() {
    pthread_mutex_lock(queue_stats.mut);
    queue_stats.full_encounters++;
    pthread_mutex_unlock(queue_stats.mut);
}

void updateQueueEmptyStats() {
    pthread_mutex_lock(queue_stats.mut);
    queue_stats.empty_encounters++;
    pthread_mutex_unlock(queue_stats.mut);
}

void printQueueStats() {
    pthread_mutex_lock(queue_stats.mut);
    
    printf("\n===== Queue Wait Time Statistics =====\n");
    printf("Total work items processed: %d\n", queue_stats.count);
    
    if (queue_stats.count > 0) {
        double avg_time = (double)queue_stats.total_time / queue_stats.count;
        printf("Average wait time: %.2f microseconds\n", avg_time);
        printf("Minimum wait time: %ld microseconds\n", queue_stats.min_time);
        printf("Maximum wait time: %ld microseconds\n", queue_stats.max_time);
    } else {
        printf("No work items were processed.\n");
    }
    
    printf("\n===== Mutex Contention Statistics =====\n");
    printf("Total mutex contests: %d\n", queue_stats.mutex_contests);
    
    if (queue_stats.mutex_contests > 0) {
        double avg_mutex_wait = (double)queue_stats.total_mutex_wait_time / queue_stats.mutex_contests;
        printf("Average mutex wait time: %.2f microseconds\n", avg_mutex_wait);
        printf("Maximum mutex wait time: %ld microseconds\n", queue_stats.max_mutex_wait_time);
    }
    
    printf("\n===== Queue State Statistics =====\n");
    printf("Empty queue encounters: %d\n", queue_stats.empty_encounters);
    printf("Full queue encounters: %d\n", queue_stats.full_encounters);
    
    printf("=====================================\n");
    
    pthread_mutex_unlock(queue_stats.mut);
    
    // Clean up
    pthread_mutex_destroy(queue_stats.mut);
    free(queue_stats.mut);
}

void *producer(void *args);
void *consumer(void *args);
queue *queueInit();
void queueDelete(queue *q);
void queueAdd(queue *q, struct workFunction in);
void queueDel(queue *q, struct workFunction *out);

void *calculate_sine(void *arg) {
    double *angles = (double *)arg;
    for (int i = 0; i < 10; i++) {
      double result = sin(angles[i]);
      //printf("Sine of %.1f is %.3f\n", angles[i], result);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // Check command line arguments
    if (argc != 4) {
        printf("Usage: %s <producers> <consumers> <queue_size>\n", argv[0]);
        return 1;
    }
    // Parse arguments
    p = atoi(argv[1]);
    q = atoi(argv[2]);
      
    // Validate if positive
    if (p <= 0 || q <= 0) {
        printf("Number of producers, consumers, and queue size must be positive integers\n");
        return 1;
    }

    queue *fifo;
    pthread_t pro[p], con[q];
   
    fifo = queueInit();
    if (fifo ==  NULL) {
      fprintf(stderr, "main: Queue Init failed.\n");
      exit(1);
    }
    initQueueStats();
    
    // Create producer and consumer threads
    for (int i = 0; i < p; i++) {
        pthread_create(&pro[i], NULL, producer, fifo);
    }
    for (int i = 0; i < q; i++) {
        pthread_create(&con[i], NULL, consumer, fifo);
    }

    // Wait for producer to finish
    for (int i = 0; i < p; i++) {
        pthread_join(pro[i], NULL);
    }

    // Send termination signal to consumers
    for (int i = 0; i < q; i++) {
        struct workFunction termination;
        termination.work = NULL;
        termination.arg = NULL;
        termination.producer_id = -1;
        termination.work_id = -1;
        pthread_mutex_lock(fifo->mut);
        while (fifo->full) {
            printf("main: queue FULL.\n");
            pthread_cond_wait(fifo->notFull, fifo->mut);
        }
        queueAdd(fifo, termination);
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notEmpty);
    }

    // After all consumers have finished
    for (int i = 0; i < q; i++) {
        pthread_join(con[i], NULL);
    }
    
    printQueueStats();
    queueDelete(fifo);
    return 0;
}

void *producer(void *q) {
    queue *fifo;
    fifo = (queue *)q;
    int i;
    // Get producer thread ID for tracking
    int producer_id = 0;
    pthread_mutex_lock(fifo->mut);
    static int next_producer_id = 0;
    producer_id = next_producer_id++;
    pthread_mutex_unlock(fifo->mut);
  
    for (i = 0; i < LOOP; i++) {
        // Produce work
        double *angles = malloc(10 * sizeof(double));
        for (int j = 0; j < 10; j++) {
            angles[j] = (i * 10 + j) * 0.1;
        }
        struct workFunction work;
        work.work = calculate_sine;
        work.arg = angles;
        work.producer_id = producer_id;
        work.work_id = i;

        // Measure mutex acquisition time
        struct timeval mutex_start, mutex_end;
        gettimeofday(&mutex_start, NULL);
        pthread_mutex_lock(fifo->mut);
        gettimeofday(&mutex_end, NULL);
        long mutex_wait_usec = (mutex_end.tv_sec - mutex_start.tv_sec) * 1000000 + (mutex_end.tv_usec - mutex_start.tv_usec);
        updateMutexWaitStats(mutex_wait_usec);
        if (fifo->full) {
            updateQueueFullStats();
        }
        
        while (fifo->full) {
            //printf("producer %d: queue FULL.\n", producer_id);
            pthread_cond_wait(fifo->notFull, fifo->mut);
        }

        // Record time just before adding to queue
        gettimeofday(&work.enqueue_time, NULL);
        
        // Add to the queue
        queueAdd(fifo, work);
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notEmpty);
    }
    return (NULL);
}

void *consumer(void *q) {
    queue *fifo;
    fifo = (queue *)q;
    struct workFunction d;
    
    // Get consumer thread ID for tracking
    int consumer_id = 0;
    pthread_mutex_lock(fifo->mut);
    static int next_consumer_id = 0;
    consumer_id = next_consumer_id++;
    pthread_mutex_unlock(fifo->mut);
  
    while (1) {
        // Measure mutex acquisition time
        struct timeval mutex_start, mutex_end;
        gettimeofday(&mutex_start, NULL);
        pthread_mutex_lock(fifo->mut);
        gettimeofday(&mutex_end, NULL);
        long mutex_wait_usec = (mutex_end.tv_sec - mutex_start.tv_sec) * 1000000 + (mutex_end.tv_usec - mutex_start.tv_usec);
        updateMutexWaitStats(mutex_wait_usec);
        if (fifo->empty) {
            updateQueueEmptyStats();
        }
        
        // Check if the queue is empty
        while (fifo->empty) {
            //printf("consumer %d: queue EMPTY.\n", consumer_id);
            pthread_cond_wait(fifo->notEmpty, fifo->mut);
        }

        // Get from the queue
        queueDel(fifo, &d);
        
        // Record time immediately after removing from queue
        gettimeofday(&d.dequeue_time, NULL);
        
        pthread_mutex_unlock(fifo->mut);
        pthread_cond_signal(fifo->notFull);

        if (d.work != NULL) {
            // Update statistics
            long time_diff_usec = (d.dequeue_time.tv_sec - d.enqueue_time.tv_sec) * 1000000 + (d.dequeue_time.tv_usec - d.enqueue_time.tv_usec);
            updateQueueStats(time_diff_usec);

            // Execute the work function
            d.work(d.arg);
            free(d.arg);
        } else {
            printf("consumer %d: received termination signal.\n", consumer_id);
            break;
        }
    }
    return (NULL);
}

queue *queueInit(void) {
    queue *q;
   
    q = (queue *)malloc(sizeof(queue));
    if (q == NULL) return (NULL);
    
    q->empty = 1;
    q->full = 0;
    q->head = 0;
    q->tail = 0;
    q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
    pthread_mutex_init (q->mut, NULL);
    q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
    pthread_cond_init (q->notFull, NULL);
    q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
    pthread_cond_init (q->notEmpty, NULL);
     
    return (q);
}
   
void queueDelete(queue *q) {
    pthread_mutex_destroy (q->mut);
    free (q->mut);	
    pthread_cond_destroy (q->notFull);
    free (q->notFull);
    pthread_cond_destroy (q->notEmpty);
    free (q->notEmpty);
    free (q);
}
   
void queueAdd(queue *q, struct workFunction in) {
    q->buf[q->tail] = in;
    q->tail++;
   
    if (q->tail == QUEUESIZE) {
        q->tail = 0;
    }
    if (q->tail == q->head) {
        q->full = 1;
    }
    q->empty = 0;
   
    return;
}
   
void queueDel(queue *q, struct workFunction *out) {
    *out = q->buf[q->head];
   
    q->head++;
   
    if (q->head == QUEUESIZE) {
        q->head = 0;
    }
    if (q->head == q->tail) {
        q->empty = 1;
    }
    q->full = 0;
   
    return;
}