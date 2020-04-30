#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pthread_sleep.c"

/* declarations and data types */

#define t 1
#define e_t 40

typedef struct {
    int s;
} tower_data;

/* plane struct */
typedef struct{

    /* plane id to add/remove queue */
    int id;

    /* plane status */
    char status[5];

    /* arrival time */
    time_t arrival_time;

    /* request time */
    int request_time;

    /* respond time */
    int respond_time;

    /* condition variable */
    pthread_cond_t cond;

    /* mutex lock */
    pthread_mutex_t lock;

} plane;


/* passing multiple arguments to thread  */
typedef struct {
   int s;
   int emergency;
   plane *plane_;
   pthread_mutex_t lock;
   pthread_cond_t cond;
} thread_plane_data;

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

/* time_t */

time_t start_time;
time_t current_time;

/* simulation time  */
int s;

/* probability of land/depart */
float p; 

/* log time  */
int n;

/* thread for tower */
pthread_t tower;
pthread_t plane_init_land;
pthread_t plane_init_depart;

/* creating threads attrs*/
pthread_attr_t attr;

/* mutexes and cond */
pthread_mutex_t tower_mutex;
pthread_cond_t tower_cond;

/* locks for queues */
pthread_mutex_t landing_lock;
pthread_mutex_t departing_lock;

/* planes data */
thread_plane_data *planes_data;
thread_plane_data *planes_data2;
pthread_mutex_t planes_array_lock;

/*********************/
/* function declarations */

/* queue */

/* display queue debugging purposes */
void disqueue(queue *queue_, pthread_mutex_t *lock);

/* remove item from queue */
void dequeue(queue *queue_, pthread_mutex_t *lock);

/* front of the queue */
plane *front(queue *queue_, pthread_mutex_t *lock);

/* add item to queue */
void enqueue(queue *queue_, plane *plane_, pthread_mutex_t *lock);

/* add item to front for emergency */
void enquenue_front(queue *queue_, plane *plane_, pthread_mutex_t *lock);

/* creating queue and initialization */
queue * create_queue(int size, char *name);

/* plane */

void* land(void *arg);

void* depart(void *arg);

void planes_log(plane **planes, int size, int time);

/* air traffic control */

void* airport_tower(void *arg);

/* utils */

void red();
void yellow();
void magenta();
void reset ();

/* reads command line arguments -s and -p */
int read_command_line(int argc, char *argv[], float *p, int *s, int *n);

/* generates a float between 0 and 1 */
float rand_p();

/* formats struct tm */
void print_time(struct tm tm, int simulation);

int main(int argc, char *argv[]) {

    if(!read_command_line(argc, argv, &p, &s, &n))
        return 0;

    // p = 0;
    // s = 10;
    // n = 2;

    /* set seed to 42 debugging purposes. */
    srand(1588004448);

    /* plane threads */
    pthread_t planes[s];

    /* planes array */
    plane *planes_array[s+2];
    int planes_index = 0;

    for (int i=0; i<s+2; i++){
        plane *plane_ = malloc(sizeof(plane));
        plane_->id = i;
        planes_array[i] = plane_;
    }
    
    /* initial tower mutex and condition variable */
    pthread_mutex_init(&tower_mutex, NULL);
    pthread_cond_init(&tower_cond, NULL);

    pthread_attr_init(&attr);

    /* initial queue mutexes */
    pthread_mutex_init(&landing_lock, NULL);
    pthread_mutex_init(&departing_lock, NULL);

    /* initialization planes_array mutexes */
    pthread_mutex_init(&planes_array_lock, NULL);

    /* initialization of landing/departing queues */
    landing = create_queue(s+2, "Air:");
    departing = create_queue(s+2, "Ground:");

    /* set the timers */
    start_time = time(NULL);
    current_time = time(NULL);

    struct tm tm_start = *localtime(&start_time);

    yellow();
    printf("-----------%02d:%02d:%02d - ", tm_start.tm_hour, tm_start.tm_min, tm_start.tm_sec);
    printf("Running Simulation... s: %d - p: %f - n: %d-----------\n", s, p, n);
    reset();

    /* data for tower */
    tower_data *data_tower = malloc(sizeof(data_tower));

    data_tower->s = s;

    pthread_create(&tower, &attr, airport_tower, (void *) data_tower);

    /* data for each plane */
    planes_data = malloc(sizeof(thread_plane_data));
    planes_data2 = malloc(sizeof(thread_plane_data));

    planes_data2->s = s;
    planes_data2->emergency = 0;

    planes_data2->plane_ = planes_array[0];
    planes_data2->plane_->request_time = current_time - start_time;
    strcpy(planes_data2->plane_->status, "L");
    planes_data2->plane_->arrival_time = current_time;
    planes_data2->plane_->respond_time = -1;
    pthread_create(&plane_init_land, &attr, land, (void *) planes_data2);


    planes_data = malloc(sizeof(thread_plane_data));

    planes_data->plane_ = planes_array[1];
    planes_data->plane_->request_time = current_time - start_time;
    strcpy(planes_data->plane_->status, "D");
    planes_data->plane_->arrival_time = current_time;
    planes_data->plane_->respond_time = -1;
    pthread_create(&plane_init_depart, &attr, depart, (void *) planes_data);
    
    int plane = 0;
    planes_index = 2;

    while (current_time < start_time + s) {
        planes_data = malloc(sizeof(thread_plane_data));
        planes_data->plane_ = planes_array[plane+2];
        planes_data->plane_->request_time = ((int) current_time -  (int) start_time);
        planes_data->plane_->respond_time = -1;
        planes_data->plane_->arrival_time = current_time;
        planes_index++;

        if ((current_time - start_time) == e_t){
            strcpy(planes_data->plane_->status, "L(E)");
            planes_data->emergency = 1;
            pthread_create(&planes[plane], &attr, land, (void *) planes_data);

        }else {

            planes_data->emergency = 0;
            if (rand_p() <= p){
                strcpy(planes_data->plane_->status, "L");
                pthread_create(&planes[plane], &attr, land, (void *) planes_data);
            }else{
                strcpy(planes_data->plane_->status, "D");
                pthread_create(&planes[plane], &attr, depart, (void *) planes_data);
            }
        }

        plane++;

        red();
        printf("--------------------------------------Time:%d-------------------------------------\n", ((int) current_time -  (int) start_time));
        reset();

        pthread_sleep(t);
        current_time = time(NULL);

        disqueue(landing, &landing_lock);
        disqueue(departing, &departing_lock);

        if ((current_time - start_time) % n == 0 && (current_time - start_time) != 0)
            planes_log(planes_array, planes_index, (current_time - start_time));


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

    thread_plane_data *data_ = (thread_plane_data *) arg;
    plane *landing_plane = data_->plane_;

    pthread_mutex_init(&(landing_plane->lock), NULL); 
    pthread_cond_init(&(landing_plane->cond), NULL);

    if (data_->emergency)
        enquenue_front(landing, landing_plane, &landing_lock);
    else
        enqueue(landing, landing_plane, &landing_lock);

    pthread_cond_wait(&(landing_plane->cond), &(landing_plane->lock));
    landing_plane->respond_time = ((int) time(NULL) - (int) start_time);

    pthread_mutex_destroy(&(landing_plane->lock));
    pthread_cond_destroy(&(landing_plane->cond));
    pthread_exit(NULL);

    return NULL;

}

void *depart(void *arg) {

    thread_plane_data *data_ = (thread_plane_data *) arg;
    plane *departing_plane = data_->plane_;

    pthread_mutex_init(&(departing_plane->lock), NULL);
    pthread_cond_init(&(departing_plane->cond), NULL);
    
    enqueue(departing, departing_plane, &departing_lock);

    pthread_cond_wait(&(departing_plane->cond), &(departing_plane->lock));
    departing_plane->respond_time = ((int) time(NULL) - (int) start_time);

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

    while (current_t < (start_t + data_->s)) {

        if (landing->capacity > 0 && departing->capacity < 5){
            plane *landing_plane = front(landing, &landing_lock);
                       
            dequeue(landing, &landing_lock);
            pthread_sleep(2*t);

            pthread_cond_signal(&(landing_plane->cond));
    
        }else if (departing->capacity > 0){
            plane *departing_plane = front(departing, &departing_lock);

            dequeue(departing, &departing_lock);
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
int read_command_line(int argc, char *argv[], float *p, int *s, int *n) {

    if (argc != 7){
        puts("Inappropriate argument count!!!");
        return 0;
    }

    if (strcmp(argv[1], "-p")) {
        puts("Specify -p as first argument for the simulation!!!");
        return 0;
    }

    if (strcmp(argv[3], "-s")) {
        puts("Specify -s as second argument for the simulation!!!");
        return 0;
    }

    if (strcmp(argv[5], "-n")) {
        puts("Specify -n as second argument for the simulation!!!");
        return 0;
    }

    *p = atof(argv[2]);
    *s = atoi(argv[4]);
    *n = atoi(argv[6]);

    if (*s <= 0 || *p < 0 || *n < 1) {
        puts("Specify valid values for the -s, -p and -n values!!!");
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

    return queue_;

}

/* add item to queue */
void enqueue(queue *queue_, plane *plane_, pthread_mutex_t *lock) {

    pthread_mutex_lock(lock);

    if (queue_->rear == queue_->size - 1){
        pthread_mutex_unlock(lock);
        return;
    }
    
    if (queue_->front == -1)
        queue_->front = 0;
    
    queue_->rear++;
    queue_->items[queue_->rear] = plane_;
    queue_->capacity++;

    pthread_mutex_unlock(lock);

}

void enquenue_front(queue *queue_, plane *plane_, pthread_mutex_t *lock) {

    pthread_mutex_lock(lock);

    if (queue_->rear == queue_->size - 1){
        pthread_mutex_unlock(lock);
        return;
    }
        
    if (queue_->front == -1)
        queue_->front = 0;

    for (int i=queue_->capacity; i>0; i--)
        queue_->items[i] = queue_->items[i-1];

    queue_->items[queue_->front] = plane_;
    queue_->rear++;
    queue_->capacity++;

    pthread_mutex_unlock(lock);

}

/* remove item from queue */
void dequeue(queue *queue_, pthread_mutex_t *lock) {

    pthread_mutex_lock(lock);

    if (queue_->front == -1){
        pthread_mutex_unlock(lock);
        return;
    }
        

    queue_->front++;

    if (queue_->front > queue_->rear) {
        queue_->front = -1;
        queue_->rear = -1;
    }

    queue_->capacity--;

    pthread_mutex_unlock(lock);
}

/* front of the queue */
plane * front(queue *queue_, pthread_mutex_t *lock) {

    pthread_mutex_lock(lock);

    if (queue_->front == -1){
        pthread_mutex_unlock(lock);
        return NULL;
    }
        

    plane *front_ = queue_->items[queue_->front];

    pthread_mutex_unlock(lock);

    return front_;

}

/* display queue for debugging purposes */
void disqueue(queue *queue_, pthread_mutex_t *lock) {

    pthread_mutex_lock(lock);

    magenta();

    if (queue_->rear == -1){
        printf("%-7s -\n", queue_->name);
        pthread_mutex_unlock(lock);
        return;
    }

    printf("%s ", queue_->name);
    for (int i = queue_->front; i <= queue_->rear; i++)
        printf("%d ", queue_->items[i]->id);

    printf("\n");

    reset();
    pthread_mutex_unlock(lock);

}

void planes_log(plane *planes[], int size, int time) {

    pthread_mutex_lock(&planes_array_lock);

    yellow();

    printf("---------------------------------------------------------------------------------\n");
    printf("--------------------------------------Time:%d-------------------------------------\n", time);
    printf("|%-15s|%-15s|%-15s|%-15s|%-15s|\n", "PlaneID", "Status", "Request-Time",
    "Runway-Time", "Turnaround-Time");
    printf("---------------------------------------------------------------------------------\n");
    red();
    int i;
    for (i=0; i < size; i++){
        if (planes[i]->respond_time != -1)
            printf("|%-15d|%-15s|%-15d|%-15d|%-15d|\n", planes[i]->id, planes[i]->status, planes[i]->request_time, planes[i]->respond_time, planes[i]->respond_time - planes[i]->request_time);
        else
            printf("|%-15d|%-15s|%-15d|%-15s|%-15s|\n", planes[i]->id, planes[i]->status, planes[i]->request_time, "-", "-");
    }
    yellow();
    printf("---------------------------------------------------------------------------------\n");
    reset();

    pthread_mutex_unlock(&planes_array_lock);

}

void print_time(struct tm tm, int simulation) {
    printf("Time: %02d:%02d:%02d - %d seconds in simulation\n", tm.tm_hour, tm.tm_min, tm.tm_sec, simulation);
}

void red () {
  printf("\033[1;31m");
}

void yellow() {
  printf("\033[1;33m");
}

void magenta() {
  printf("\033[0;35m");
}

void reset() {
  printf("\033[0m");
}



