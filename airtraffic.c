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

/*********************/
/* data declarations */

queue *landing = NULL;
queue *departing = NULL;
queue *emergency = NULL;

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

/* planes data */
plane_data *planes_data;

/*********************/
/* function declarations */

/* queue */

/* display queue debugging purposes */
void disqueue(queue *queue_);

/* remove item from queue */
void dequeue(queue *queue_);

/* front of the queue */
plane *front(queue *queue_);

/* add item to queue */
void enqueue(queue *queue_, plane *plane_);

/* creating queue and initialization */
queue * create_queue(int size);

/* plane */

void* land(void *arg);

void* depart(void *arg);

/* air traffic control */

void* airport_tower(void *arg);

/* utils */

/* reads command line arguments -s and -p */
int read_command_line(int argc, char *argv[], float *p, int *s);

/* generates a float between 0 and 1 */
float rand_p();

int main(int argc, char *argv[]) {

    // if(!read_command_line(argc, argv, &p, &s))
    //     return 0;

    s = 5;
    p = 0.5;

    /* set seed to 42 debugging purposes. */
    srand(42);

    /* plane threads */
    pthread_t planes[s];

    /* data for each plane */
    planes_data = malloc(sizeof(plane_data) * (s+2));

    /* initialization of mutex and cond */
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);

    pthread_attr_init(&attr);

    /* initialization of landing/departing queues */
    landing = create_queue(s+1);
    departing = create_queue(s+1);
    emergency = create_queue(s+1);

    /* set the timers */
    start_time = time(NULL);
    current_time = time(NULL);

    struct tm tm_start = *localtime(&start_time);
    struct tm tm_current = *localtime(&current_time);

    printf("Time Started: %02d:%02d:%02d\n", tm_start.tm_hour, tm_start.tm_min, tm_start.tm_sec);

    tower_data *data_tower = malloc(sizeof(data_tower));

    data_tower->cond = cond;
    data_tower->lock = lock;
    data_tower->s = s;
    data_tower->start_time = start_time;

    pthread_create(&tower, &attr, airport_tower, (void *) data_tower);

    planes_data[0].lock = lock;
    planes_data[0].cond = cond;
    planes_data[0].s = s;
    planes_data[0].emergency = 0;
    planes_data[0].arrival_time = current_time;
    
    planes_data[1].lock = lock;
    planes_data[1].cond = cond;
    planes_data[1].s = s;
    planes_data[1].emergency = 0;
    planes_data[1].arrival_time = current_time;
 
    pthread_create(&plane_init_land, &attr, land, (void *) &planes_data[0]);

    pthread_create(&plane_init_depart, &attr, depart, (void *) &planes_data[1]);
    
    // int plane = 0;

    // while (current_time < start_time + s) {
    //     if (rand_p() <= p)
    //         pthread_create(&planes[plane], &attr, land, (void *) &planes_data[plane]);
    //     else
    //         pthread_create(&planes[plane], &attr, depart, (void *) &planes_data[plane]);

    //     plane++;
    //     pthread_sleep(1);
    //     current_time = time(NULL);
    //     printf("%ld\n", current_time % s);

    // }

    // printf("%d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    
    pthread_join(tower, NULL);
    pthread_join(plane_init_land, NULL);
    pthread_join(plane_init_depart, NULL);

    // pthread_join(NULL);
    pthread_attr_destroy(&attr);
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);
    pthread_exit(NULL);
    return 0;
}

/* functions */

/* generates a float between 0 and 1 */
float rand_p() {
    return (float) rand() / (float) RAND_MAX;
}

/* plane */

void* land(void *arg) { 

    plane *landing_plane = malloc(sizeof(plane));
    plane_data *data_ = (plane_data *) arg;

    landing_plane->id =  data_->arrival_time % data_->s;
    landing_plane->arrival = *localtime(&data_->arrival_time);

    landing_plane->lock = data_->lock;
    landing_plane->cond = data_->cond;

    puts("Waiting for landing...");
    pthread_mutex_lock(&(landing_plane->lock));
    puts("Landing got the lock.");

    if (data_->emergency)
        enqueue(emergency, landing_plane);
    else
        enqueue(landing, landing_plane);

    if (front(landing) == landing_plane)
        pthread_cond_signal(&(landing_plane->cond));

    printf("Plane %d is landed.\n", landing_plane->id);
    pthread_mutex_unlock(&(landing_plane->lock));
    pthread_exit(NULL);

    return NULL;

}

void *depart(void *arg) {

    plane *departing_plane = malloc(sizeof(plane));
    plane_data *data_ = (plane_data *) arg;

    departing_plane->id = data_->arrival_time % data_->s;
    departing_plane->arrival = *localtime(&data_->arrival_time);

    departing_plane->lock = data_->lock;
    departing_plane->cond = data_->cond;

    puts("Waiting for departing...");
    pthread_mutex_lock(&(departing_plane->lock));
    puts("Departing got the lock.");

    if (front(departing) == departing_plane)
        pthread_cond_signal(&(departing_plane->cond));

    pthread_cond_wait(&(departing_plane->cond), &(departing_plane->lock));

    printf("Plane %d is departed.", departing_plane->id);
    pthread_mutex_unlock(&(departing_plane->lock));
    pthread_exit(NULL);

    return NULL;

}

void *airport_tower(void *arg) {

    tower_data *data_ = (tower_data *) arg;

    puts("Tower waiting for first signal...");
    pthread_cond_wait(&(data_->cond), &(data_->lock));

    time_t start_t = time(NULL);
    time_t current_t = time(NULL);

    struct tm tm_current = *localtime(&current_t);
    struct tm tm_start = *localtime(&start_t);

    printf("Time Started: %02d:%02d:%02d\n", tm_start.tm_hour, tm_start.tm_min, tm_start.tm_sec);

    while (current_t < (start_t + data_->s)) {

        printf("%02d:%02d:%02d\n", tm_current.tm_hour, tm_current.tm_min, tm_current.tm_sec);

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
        tm_current = *localtime(&current_t);
    }

    puts("Simulation is done.");
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


