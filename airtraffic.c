#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pthread_sleep.c"

/* declarations and data types */

typedef struct {
    time_t start_time;
    int s;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} tower_data;


/* passing multiple arguments to thread  */
typedef struct {
   int s;
   int emergency;
   time_t arrival_time;
   pthread_mutex_t lock;
   pthread_cond_t cond;
} plane_data;

/* plane struct */
typedef struct{

    /* plane id to add/remove queue */
    int id;

    /* plane thread id to work with threads. */
    pthread_t tid;

    /* arrival time */
    struct tm arrival;

    /* condition variable */
    pthread_cond_t cond;

    /* mutex lock */
    pthread_mutex_t lock;

} plane;


/* queue struct */
typedef struct {
    
    int capacity;
    int size;
    int front;
    int rear;
    plane **items;

} queue;

/* function declarations */

/* queue */

/* display queue debugging purposes */
void disqueue(queue *queue_);

/* remove item from queue */
void dequeue(queue *queue_);

/* front of the queue */
plane * front(queue *queue_);

/* add item to queue */
void enqueue(queue *queue_, plane *plane_);

/* creating queue and initialization */
queue * create_queue(int size);

/* plane */

void* land(void *data);

void* depart(void *data);

/* air traffic control */

void* airport_tower(void *data);

/* utils */

/* reads command line arguments -s and -p */
int read_command_line(int argc, char *argv[], float *p, int *s);

/* generates a float between 0 and 1 */
float rand_p();

/* data declarations */

queue *landing;
queue *departing;
queue *emergency;

/* time_t */

time_t start_time;
time_t current_time;

/* simulation time  */
int s;

/* probability of land/depart */
float p; 

/* thread for tower */
pthread_t tower;
pthread_t plane_init_land;
pthread_t plane_init_depart;

/* creating threads attrs*/
pthread_attr_t attr;

/* mutexes and cond */
pthread_mutex_t lock;
pthread_cond_t cond;

int main(int argc, char *argv[]) {

    if(!read_command_line(argc, argv, &p, &s))
        return 0;

    /* set seed to 42 debugging purposes. */
    srand(42);

    /* use to assign arrival time to each plane. */
    // time_t t = time(NULL);
    pthread_t planes[s];

    /* initialization of mutex and cond */
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);

    pthread_attr_init(&attr);

    /* initialization of landing/departing queues */
    queue *landing = create_queue(s+1);
    queue *departing = create_queue(s+1);
    queue *emergency = create_queue(s+1);

    /* set the timers */
    start_time = time(NULL);
    current_time = time(NULL);

    // struct tm tm = *localtime(&t);

    tower_data *data_tower = NULL;

    data_tower->cond = cond;
    data_tower->lock = lock;
    data_tower->s = s;
    data_tower->start_time = current_time;

    pthread_create(&tower, &attr, airport_tower, (void *) data_tower);

    plane_data *data_plane = NULL;

    data_plane->lock = lock;
    data_plane->cond = cond;
    data_plane->s = s;
    data_plane->emergency = 0;
    data_plane->arrival_time = current_time;

    pthread_create(&plane_init_land, &attr, land, (void *) data_plane);
    pthread_create(&plane_init_depart, &attr, depart, (void *) data_plane);
    
    int plane = 0;

    while (current_time < start_time + s) {
        if (rand_p() <= p)
            pthread_create(&planes[plane], &attr, land, (void *) data_plane);
        else
            pthread_create(&planes[plane], &attr, depart, (void *) data_plane);

        plane++;
        pthread_sleep(1);
        current_time = time(NULL);
        printf("%ld\n", current_time % s);

    }

    // printf("%d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    
    return 0;
}

/* functions */

/* generates a float between 0 and 1 */
float rand_p() {
    return (float) rand() / (float) RAND_MAX;
}

/* plane */

void* land(void *data) { 

    plane *landing_plane = NULL;
    plane_data *data_ = (plane_data *) data;

    landing_plane->id =  data_->arrival_time % data_->s;
    landing_plane->arrival = *localtime(&data_->arrival_time);

    landing_plane->lock = data_->lock;
    landing_plane->cond = data_->cond;

    // pthread_mutex_init(&(landing_plane->lock), NULL);
    // pthread_cond_init(&(landing_plane->cond), NULL);

    pthread_mutex_lock(&(landing_plane->lock));

    if (data_->emergency)
        enqueue(emergency, landing_plane);
    else
        enqueue(landing, landing_plane);

    if (front(landing) == landing_plane)
        pthread_cond_signal(&(landing_plane->cond));

    pthread_mutex_unlock(&(landing_plane->lock));
    pthread_exit(NULL);

    return NULL;

}

void *depart(void *data) {

    plane *departing_plane = NULL;
    plane_data *data_ = (plane_data *) data;

    departing_plane->id = data_->arrival_time % data_->s;
    departing_plane->arrival = *localtime(&data_->arrival_time);

    departing_plane->lock = data_->lock;
    departing_plane->cond = data_->cond;

    // pthread_mutex_init(&(departing_plane->lock), NULL);
    // pthread_cond_init(&(departing_plane->cond), NULL);

    pthread_mutex_lock(&(departing_plane->lock));

    if (front(departing) == departing_plane)
        pthread_cond_signal(&(departing_plane->cond));

    pthread_cond_wait(&(departing_plane->cond), &(departing_plane->lock));

    pthread_mutex_unlock(&(departing_plane->lock));
    pthread_exit(NULL);

    return NULL;

}

void *airport_tower(void *data) {

    tower_data *data_ = (tower_data *) data;

    pthread_mutex_lock(&(data_->lock));
    pthread_cond_wait(&(data_->cond), &(data_->lock));

    time_t current_t = time(NULL);

    while (current_t < data_->start_time + data_->s) {

        if (emergency->capacity > 0){
            dequeue(emergency); 
            pthread_sleep(1);          
        }else {

            if (departing->capacity > 0){
                dequeue(departing);
                pthread_sleep(1);      
            }else{
                dequeue(landing);
                pthread_sleep(1);
            }
        }
            
        current_t = time(NULL);
    }

    pthread_exit(NULL);

    return NULL;

}


/* reads command line arguments -s and -p */
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

/* queue functions */

/* creating queue and initialization */
queue * create_queue(int size) {

    queue *queue_;

    queue_ = malloc(sizeof(queue));

    queue_->capacity = size;
    queue_->size = size;
    queue_->front = -1;
    queue_->rear = -1;
    queue_->items = malloc(sizeof(plane) * size);

    return queue_;

}

/* add item to queue */
void enqueue(queue *queue_, plane *plane_) {

    if (queue_->rear == queue_->size - 1)
        return;

    if (queue_->front == -1)
        queue_->front = 0;
    
    queue_->rear++;
    queue_->items[queue_->rear] = plane_;
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

/* front of the queue */
plane * front(queue *queue_) {

    if (queue_->front == -1)
        return NULL;

    return queue_->items[queue_->front];

}

/* display queue for debugging purposes */
void disqueue(queue *queue_) {

    if (queue_->rear == -1){
        puts("Empty queue!");
        return;
    }

    for (int i = queue_->front; i <= queue_->rear; i++)
        printf("%d ", queue_->items[i]->id);

    puts("");

}


