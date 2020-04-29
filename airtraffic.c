#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pthread_sleep.c"

/* declarations and data types */

#define t 1

typedef struct {
    int s;
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
    char *name;
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
pthread_mutex_t tower_mutex;
pthread_cond_t tower_cond;

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
queue * create_queue(int size, char *name);

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

/* formats struct tm */
void print_time(struct tm tm, int simulation);

int main(int argc, char *argv[]) {

    if(!read_command_line(argc, argv, &p, &s))
        return 0;

    // s = 5;
    // p = 0.5;
    printf("s: %d and p: %f - Simulation starts in 3 seconds.\n", s, p);
    pthread_sleep(3);

    /* set seed to 42 debugging purposes. */
    srand(42);

    /* plane threads */
    pthread_t planes[s];

    /* initial tower mutex and condition variable */
    pthread_mutex_init(&tower_mutex, NULL);
    pthread_cond_init(&tower_cond, NULL);

    pthread_attr_init(&attr);

    /* initialization of landing/departing queues */
    landing = create_queue(s+2, "landing");
    departing = create_queue(s+2, "departing");
    emergency = create_queue(s+2, "emergency");

    /* set the timers */
    start_time = time(NULL);
    current_time = time(NULL);

    struct tm tm_start = *localtime(&start_time);
    struct tm tm_current = *localtime(&current_time);

    printf("Time Started: %02d:%02d:%02d\n", tm_start.tm_hour, tm_start.tm_min, tm_start.tm_sec);

    /* data for tower */
    tower_data *data_tower = malloc(sizeof(data_tower));

    data_tower->s = s;

    /* data for each plane */
    planes_data = malloc(sizeof(plane_data));

    planes_data->s = s;
    planes_data->emergency = 0;

    pthread_create(&tower, &attr, airport_tower, (void *) data_tower); 
    pthread_create(&plane_init_land, &attr, land, (void *) planes_data);
    pthread_create(&plane_init_depart, &attr, depart, (void *) planes_data);
    
    int plane = 0;

    while (current_time < start_time + s) {
        if (rand_p() <= p)
            pthread_create(&planes[plane], &attr, land, (void *) planes_data);
        else
            pthread_create(&planes[plane], &attr, depart, (void *) planes_data);

        plane++;
        pthread_sleep(t);
        current_time = time(NULL);
    }

    pthread_join(tower, NULL);
    pthread_attr_destroy(&attr);
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
   
    time_t arrival_time = time(NULL);

    landing_plane->id =  arrival_time % data_->s + data_->s;
    landing_plane->arrival = *localtime(&arrival_time);

    pthread_mutex_init(&(landing_plane->lock), NULL); 
    pthread_cond_init(&(landing_plane->cond), NULL);

    pthread_mutex_lock(&tower_mutex);

    if (data_->emergency)
        enqueue(emergency, landing_plane);
    else
        enqueue(landing, landing_plane);

    if (front(landing) == landing_plane)
        pthread_cond_signal(&tower_cond);


    pthread_cond_wait(&(landing_plane->cond), &(landing_plane->lock));

    printf("Landed -> %d.\n", landing_plane->id);

    pthread_mutex_unlock(&tower_mutex);
    pthread_mutex_destroy(&(landing_plane->lock));
    pthread_cond_destroy(&(landing_plane->cond));
    pthread_exit(NULL);

    return NULL;

}

void *depart(void *arg) {

    plane *departing_plane = malloc(sizeof(plane));
    plane_data *data_ = (plane_data *) arg;

    time_t arrival_time = time(NULL);

    departing_plane->id = arrival_time % data_->s;
    departing_plane->arrival = *localtime(&arrival_time);

    pthread_mutex_init(&(departing_plane->lock), NULL);
    pthread_cond_init(&(departing_plane->cond), NULL);

    pthread_mutex_lock(&tower_mutex);
    
    enqueue(departing, departing_plane);

    if (front(departing) == departing_plane)
        pthread_cond_signal(&tower_cond);

    pthread_cond_wait(&(departing_plane->cond), &(departing_plane->lock));
    printf("Departed -> %d.\n", departing_plane->id);
    
    pthread_mutex_unlock(&tower_mutex);
    pthread_mutex_unlock(&(departing_plane->lock));
    pthread_mutex_destroy(&(departing_plane->lock));
    pthread_cond_destroy(&(departing_plane->cond));
    pthread_exit(NULL);

    return NULL;

}

void *airport_tower(void *arg) {

    tower_data *data_ = (tower_data *) arg;

    time_t start_t = time(NULL);
    time_t current_t = time(NULL);

    struct tm tm_current = *localtime(&current_t);
    struct tm tm_start = *localtime(&start_t);

    while (current_t < (start_t + data_->s)) {

        print_time(tm_current, (start_t + data_->s) - current_t);

        if (landing->capacity > 0){
            disqueue(landing);
            plane *landing_plane = front(landing);
                       
            dequeue(landing);
            pthread_sleep(2*t);

            pthread_cond_signal(&(landing_plane->cond));
    
        }else if (departing->capacity > 0){
            disqueue(departing);
            plane *departing_plane = front(departing);

            dequeue(departing);
            pthread_sleep(2*t);
        
            pthread_cond_signal(&(departing_plane->cond));
        }
            
        current_t = time(NULL);
        tm_current = *localtime(&current_t);
        
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
queue * create_queue(int size, char *name) {

    queue *queue_;

    queue_ = malloc(sizeof(queue));

    queue_->capacity = 0;
    queue_->size = size;
    queue_->front = -1;
    queue_->rear = -1;
    queue_->name = name;
    queue_->items = malloc(sizeof(plane) * size);

    // puts("queue initialized.");
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

    // printf("queue added - capacity: %d\n", queue_->capacity);

}

/* remove item from queue */
void dequeue(queue *queue_) {

    if (queue_->front == -1)
        return;

    queue_->front++;

    if (queue_->front > queue_->rear)
        queue_->front = -1;
        queue_->rear = -1;

    queue_->capacity--;

    // printf("queue removed - capacity: %d\n", queue_->capacity);  
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

    printf("%s -> ", queue_->name);
    for (int i = queue_->front; i <= queue_->rear; i++)
        printf("%d ", queue_->items[i]->id);

    puts("");

}

void print_time(struct tm tm, int simulation) {
    printf("Time: %02d:%02d:%02d - %d seconds in simulation\n", tm.tm_hour, tm.tm_min, tm.tm_sec, simulation);
}



