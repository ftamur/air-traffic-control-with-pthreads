#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pthread_sleep.c"

/* declarations and data types */

/* plane struct */
typedef struct{

    /* plane id to add/remove queue */
    int id;

    /* plane thread id to work with threads. */
    pthread_t tid;

    /* arrival time */
    struct tm arrival;



} plane;


/* queue struct */
typedef struct {
    
    int capacity;
    int size;
    int front;
    int rear;
    int *items;

} queue;

/* functions */

/* queue functions */

/* creating queue and initialization */
queue * create_queue(int size) {

    queue *queue_;

    queue_ = malloc(sizeof(queue));

    queue_->capacity = size;
    queue_->size = size;
    queue_->front = -1;
    queue_->rear = -1;
    queue_->items = malloc(sizeof(int) * size);

    return queue_;

}

/* add item to queue */
void enqueue(queue *queue_, int item) {

    if (queue_->rear == queue_->size - 1)
        return;

    if (queue_->front == -1)
        queue_->front = 0;
    
    queue_->rear++;
    queue_->items[queue_->rear] = item;
    queue_->capacity++;

}

/* remove item from queue */
void dequeue(queue *queue_) {

    if (queue_->front == -1)
        return;

    queue_->front++;

    if (queue_->front > queue_->rear)
        queue_->front = queue_->rear = -1;

    queue_->capacity--;
        
}

/* display queue debugging purposes */
void disqueue(queue *queue_) {

    if (queue_->rear == -1){
        puts("Empty queue!");
        return;
    }

    for (int i = queue_->front; i <= queue_->rear; i++)
        printf("%d ", queue_->items[i]);

    puts("");

}


/* reads command line arguments -s and -p */
int read_command_line(int argc, char *argv[], float *p, int *s);

/* generates a float between 0 and 1 */
float rand_p() {
    return (float) rand() / (float) RAND_MAX;
}

int main(int argc, char *argv[]) {

    
    /* simulation time  */
    // int s;

    /* probability of land/depart */
    // float p;   

    // if(!read_command_line(argc, argv, &p, &s))
    //     return 0;

    /* use to assign arrival time to each plane. */
    // time_t t = time(NULL);

    /* set seed to 42 debugging purposes. */
    // srand(42);

    /* initialization of landing/departing queues */
    // queue *landing = create_queue(s+1);
    // queue *departing = create_queue(s+1);

    return 0;
}


int read_command_line(int argc, char *argv[], float *p, int *s) {

    if (argc != 5){
        puts("Inappropriate argument count!!!");
        return 0;
    }

    if (strcmp(argv[1], "-s")) {
        puts("Specify -s as first argument for the simulation!!!");
        return 0;
    }

    if (strcmp(argv[3], "-p")) {
        puts("Specify -s as second argument for the simulation!!!");
        return 0;
    }

    *s = atoi(argv[2]);
    *p = atof(argv[4]);

    if (*s == 0 || *p == 0) {
        puts("Specify integer values for the -s or -p values!!!");
        return 0;
    }

    return 1;

}


