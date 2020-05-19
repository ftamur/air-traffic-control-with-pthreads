#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pthread_sleep.c"

/* declarations and data types */

/* sleep simulation for t seconds */
#define t 1

/* emergency planes comes every e_t seconds */
#define e_t 40

/* plane struct */
typedef struct{

    /* plane id to add/remove queue */
    int id;

    /* plane status L, D or E(L) */
    char status[5];

    /* arrival time */
    time_t arrival_time;

    /* request time */
    int request_time;

    /* respond time */
    int respond_time;

    /* emergency */
    int emergency;

    /* condition variable */
    pthread_cond_t cond;

    /* mutex lock */
    pthread_mutex_t lock;

} plane;

/* passing multiple arguments to thread via struct */
typedef struct {
   int s;
   plane *plane_;
} thread_plane_data;

typedef struct {
    int s;
} tower_data;

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

/* time_t for simulation start and current time*/

time_t start_time;
time_t current_time;

/* simulation time  */
int s;

/* probability of land/depart */
float p; 

/* simulation log time  */
int n;

/* file output for logs  */
int f;

/* thread for tower and initial planes */
pthread_t tower;

/* creating threads attrs*/
pthread_attr_t attr;

/* mutexes and cond for tower */
pthread_mutex_t tower_mutex;
pthread_cond_t tower_cond;

/* locks for queues */
pthread_mutex_t landing_lock;
pthread_mutex_t departing_lock;
pthread_mutex_t emergency_lock;

/* lock for all planes in the simulation. */
pthread_mutex_t planes_array_lock;

/*********************/
/* function declarations */

/* queue */

/* display queue debugging purposes */
void disqueue_f(queue *queue_, pthread_mutex_t *lock, FILE *fptr);
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

void planes_log_f(plane **planes, int size, int time);
void planes_log(plane *planes[], int size, int time);

/* air traffic control */

void* airport_tower(void *arg);

int starvation();

/* utils */

/* different text colors for terminal */
void red();
void yellow();
void magenta();
void blue();
void green();
void reset ();

/* reads command line arguments -s and -p */
int read_command_line(int argc, char *argv[], float *p, int *s, int *n, int *f);

/* generates a float between 0 and 1 */
float rand_p();

/* formats struct tm */
void print_time(struct tm tm, int simulation);

int main(int argc, char *argv[]) {

    f = 0;

    if(!read_command_line(argc, argv, &p, &s, &n, &f))
        return 0;

    /* set seed for debugging purposes. */
    srand(42);

    /* plane threads */
    /* Here, my assumption is there can be max 2*s+2 planes in the simulation
       because each time main thread will sleep 1 seconds.
       Because init_land and init_depart planes are defined separately 
       there will be s plane thread.
     */
    pthread_t planes[2*s+2];

    /* planes array keeps all planes in the simulation */
    plane *planes_array[2*s+2];
    int planes_index = 0;

    /* allocate memory for plane also set id and emergency for all planes */
    for (int i=0; i<2*s+2; i++){
        plane *plane_ = malloc(sizeof(plane));
        plane_->id = i;
        plane_->emergency = 0;
        planes_array[i] = plane_;
    }
    
    /* initialization tower mutex and condition variable */
    pthread_mutex_init(&tower_mutex, NULL);
    pthread_cond_init(&tower_cond, NULL);

    /* initialization thread attr */
    pthread_attr_init(&attr);

    /* initialization queue mutexes */
    pthread_mutex_init(&landing_lock, NULL);
    pthread_mutex_init(&departing_lock, NULL);
    pthread_mutex_init(&emergency_lock, NULL);

    /* initialization planes_array mutexes */
    pthread_mutex_init(&planes_array_lock, NULL);

    /* initialization of landing/departing queues */
    landing = create_queue(s+2, "Air:");
    departing = create_queue(s+2, "Ground:");

    /* There can be max s/e_t emergency planes */
    emergency = create_queue(s/e_t, "Emergency:");

    /* set the timers */
    start_time = time(NULL);
    current_time = time(NULL);

    /* formatted time for start time. */
    struct tm tm_start = *localtime(&start_time);

    yellow();
    printf("-----------%02d:%02d:%02d - ", tm_start.tm_hour, tm_start.tm_min, tm_start.tm_sec);
    printf("Running Simulation... s: %d - p: %f - n: %d-----------\n", s, p, n);
    reset();

    /* passing data for tower thread */
    tower_data *data_tower = malloc(sizeof(data_tower));

    data_tower->s = s;

    /* initialization of tower thread */
    pthread_create(&tower, &attr, airport_tower, (void *) data_tower);

    /* data for each plane */
    thread_plane_data **planes_data = malloc(sizeof(thread_plane_data*) * (2*s+2));

    /* allocate memory and set planes for planes_data */
    /* In my solution, I create planes and set their id and emergency
       condition then I give planes to each plane thread with planes_data.
    */
    for (int i = 0; i<2*s+2; i++) {
        planes_data[i] = malloc(sizeof(thread_plane_data));
        planes_data[i]->s = s;
        planes_data[i]->plane_ = malloc(sizeof(plane));
    }

    /* setting plane_data for init_land plane */
    planes_data[0]->plane_ = planes_array[0];
    planes_data[0]->plane_->request_time = current_time - start_time;
    strcpy(planes_data[0]->plane_->status, "L");
    planes_data[0]->plane_->arrival_time = current_time;
    planes_data[0]->plane_->respond_time = -1;

    /* initialization of init_land plane thread */
    pthread_create(&planes[0], &attr, land, (void *) planes_data[0]);

    /* setting plane_data for init_depart plane */
    planes_data[1]->plane_ = planes_array[1];
    planes_data[1]->plane_->request_time = current_time - start_time;
    strcpy(planes_data[1]->plane_->status, "D");
    planes_data[1]->plane_->arrival_time = current_time;
    planes_data[1]->plane_->respond_time = -1;

    /* initialization of init_depart plane thread */
    pthread_create(&planes[1], &attr, depart, (void *) planes_data[1]);
    
    /* planes_index is index for thread array and planes_data array */
    planes_index = 2;
    
    int simulation_time;

    FILE *fptr;
    fptr = fopen("log-min.txt", "w");
    
    while (current_time < start_time + s) {
        simulation_time = ((int) current_time -  (int) start_time);

        /* emergency plane of landing plane with prob p */
        if (simulation_time % e_t == 0 && simulation_time != 0){

            /* setting plane_data for next plane */
            planes_data[planes_index]->plane_ = planes_array[planes_index];
            planes_data[planes_index]->plane_->request_time = simulation_time;
            planes_data[planes_index]->plane_->respond_time = -1;
            planes_data[planes_index]->plane_->arrival_time = current_time;
            strcpy(planes_data[planes_index]->plane_->status, "E(L)");
            planes_data[planes_index]->plane_->emergency = 1;

            pthread_create(&planes[planes_index], &attr, land, (void *) planes_data[planes_index]);

            planes_index++;

        }else if (rand_p() <= p){

            /* setting plane_data for next plane */
            planes_data[planes_index]->plane_ = planes_array[planes_index];
            planes_data[planes_index]->plane_->request_time = simulation_time;
            planes_data[planes_index]->plane_->respond_time = -1;
            planes_data[planes_index]->plane_->arrival_time = current_time;
            strcpy(planes_data[planes_index]->plane_->status, "L");

            pthread_create(&planes[planes_index], &attr, land, (void *) planes_data[planes_index]);

            planes_index++;
        }
            
        /* departing plane with prob 1 - p */
        if (rand_p() <= 1 - p){

            /* setting plane_data for next plane */
            planes_data[planes_index]->plane_ = planes_array[planes_index];
            planes_data[planes_index]->plane_->request_time = simulation_time;
            planes_data[planes_index]->plane_->respond_time = -1;
            planes_data[planes_index]->plane_->arrival_time = current_time;
            strcpy(planes_data[planes_index]->plane_->status, "D");
            
            pthread_create(&planes[planes_index], &attr, depart, (void *) planes_data[planes_index]);
            
            planes_index++;
        }

        red();
        if (f == 1)
            fprintf(fptr, "--------------------------------------Time:%d-------------------------------------\n", ((int) current_time -  (int) start_time));
        else
            printf("--------------------------------------Time:%d-------------------------------------\n", ((int) current_time -  (int) start_time));
        
        reset();

        pthread_sleep(t);
        current_time = time(NULL);
        simulation_time = ((int) current_time -  (int) start_time);

        /* display each air and ground queues */
        if (f){
            disqueue_f(landing, &landing_lock, fptr);
            disqueue_f(departing, &departing_lock, fptr);
        }else{
            disqueue(landing, &landing_lock);
            disqueue(departing, &departing_lock);
        }

        /* every nth second display log for all planes in the simulation. */
        if (simulation_time % n == 0 && simulation_time != 0){
            if (f)
                planes_log_f(planes_array, planes_index, (current_time - start_time));
            else
                planes_log(planes_array, planes_index, (current_time - start_time));
        }
   }

    /* wait for tower thread */
    pthread_join(tower, NULL);
    pthread_attr_destroy(&attr);

    /* kill all planes threads */
    for (int i=0; i<2*s+2; i++)
        pthread_cancel(planes[i]);

    /* free allocated memory for planes_array */
    for (int i=0; i<2*s+2; i++)
        free(planes_array[i]);

    /* free allocated memory for planes_data */
    for (int i=0; i<2*s+2; i++) 
        free(planes_data[i]);

    fclose(fptr);
    free(planes_data);
    free(data_tower);
    return 0;
}

/* functions */

/* generates a float between 0 and 1 */
float rand_p() {
    return (float) rand() / (float) RAND_MAX;
}

/* plane functions */

void* land(void *arg) { 

    thread_plane_data *data_ = (thread_plane_data *) arg;
    plane *landing_plane = data_->plane_;

    pthread_mutex_init(&(landing_plane->lock), NULL); 
    pthread_cond_init(&(landing_plane->cond), NULL);

    if (landing_plane->emergency)
        enqueue(emergency, landing_plane, &emergency_lock);
    else
        enqueue(landing, landing_plane, &landing_lock);

    /* first plane signals to tower. */
    if (landing_plane->id == 0)
        pthread_cond_signal(&tower_cond);

    pthread_cond_wait(&(landing_plane->cond), &(landing_plane->lock));

    /* get the tower lock */
    pthread_mutex_lock(&tower_mutex);

    pthread_sleep(2*t);
    
    if (landing_plane->emergency)
        dequeue(emergency, &emergency_lock);
    else
        dequeue(landing, &landing_lock);
        
    landing_plane->respond_time = ((int) time(NULL) - (int) start_time);

    /* release the tower lock */
    pthread_mutex_unlock(&tower_mutex);

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

    pthread_mutex_lock(&(departing_plane->lock));
 
    enqueue(departing, departing_plane, &departing_lock);

    pthread_cond_wait(&(departing_plane->cond), &(departing_plane->lock));

    pthread_mutex_lock(&tower_mutex);

    pthread_sleep(2*t);
    dequeue(departing, &departing_lock);
    departing_plane->respond_time = ((int) time(NULL) - (int) start_time);

    pthread_mutex_unlock(&tower_mutex);
    
    pthread_mutex_unlock(&(departing_plane->lock));
    pthread_mutex_destroy(&(departing_plane->lock));
    pthread_cond_destroy(&(departing_plane->cond));
    pthread_exit(NULL);

    return NULL;
}

void *airport_tower(void *arg) {

    tower_data *data_ = (tower_data *) arg;

    pthread_cond_wait(&tower_cond, &tower_mutex);
    pthread_mutex_unlock(&tower_mutex);

    time_t start_t = time(NULL);
    time_t current_t = time(NULL);
   
    while (current_t < (start_t + data_->s)) {

        /* check for emergency. */
        if (front(emergency, &emergency_lock) != NULL){
            plane *landing_plane = front(emergency, &emergency_lock);

            /* singal to waiting plane to continue and finish itself. */
            pthread_cond_signal(&(landing_plane->cond));
        }else{

            /* no emergency then if depart->capacity > 5 and no starvation */
            if (departing->capacity > 5 && !starvation()){
                plane *departing_plane = front(departing, &departing_lock);

                /* singal to waiting plane to continue and finish itself. */
                pthread_cond_signal(&(departing_plane->cond));

            }else if (landing->capacity > 0){
                plane *landing_plane = front(landing, &landing_lock);
                        
                /* singal to waiting plane to continue and finish itself. */
                pthread_cond_signal(&(landing_plane->cond));
            }
        
        }

        current_t = time(NULL);
        
    }

    pthread_exit(NULL);
    return NULL;

}

/* reads command line arguments -s and -p */
int read_command_line(int argc, char *argv[], float *p, int *s, int *n, int *f) {

    if (argc < 7){
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

    if (argc > 7){
        if (!strcmp(argv[7], "-f")) {
            *f = 1;
        }
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
    queue_->items = malloc(sizeof(plane*) * size);

    for (int i=0; i<size; i++)
        queue_->items[i] = malloc(sizeof(plane));

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

/* display queue for debugging purposes to a file */
void disqueue(queue *queue_, pthread_mutex_t *lock) {

    pthread_mutex_lock(lock);

    magenta();

    if (queue_->rear == -1){
        printf("%-7s -\n", queue_->name);
        pthread_mutex_unlock(lock);
        return;
    }

    printf("%-7s ", queue_->name);
    for (int i = queue_->front; i <= queue_->rear; i++)
        printf("%d ", queue_->items[i]->id);

    printf("\n");

    reset();
    pthread_mutex_unlock(lock);

}

/* display queue for debugging purposes to a file */
void disqueue_f(queue *queue_, pthread_mutex_t *lock, FILE *fptr) {

    pthread_mutex_lock(lock);

    magenta();

    if (queue_->rear == -1){
        fprintf(fptr, "%-7s -\n", queue_->name);
        pthread_mutex_unlock(lock);
        return;
    }

    fprintf(fptr, "%-7s ", queue_->name);
    for (int i = queue_->front; i <= queue_->rear; i++)
        fprintf(fptr, "%d ", queue_->items[i]->id);

    fprintf(fptr, "\n");

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
        if (planes[i]->respond_time != -1){
            if (!strcmp(planes[i]->status, "L"))
                green();
            else if (!strcmp(planes[i]->status, "D"))
                blue();
            else
                red();
            printf("|%-15d|%-15s|%-15d|%-15d|%-15d|\n", planes[i]->id, planes[i]->status, planes[i]->request_time, planes[i]->respond_time, planes[i]->respond_time - planes[i]->request_time);
            reset();
        }else{
            magenta();
            printf("|%-15d|%-15s|%-15d|%-15s|%-15s|\n", planes[i]->id, planes[i]->status, planes[i]->request_time, "-", "-");
            reset();
        }
    }
    yellow();
    printf("---------------------------------------------------------------------------------\n\n");
    reset();

    pthread_mutex_unlock(&planes_array_lock);

}

void planes_log_f(plane *planes[], int size, int time) {

    pthread_mutex_lock(&planes_array_lock);

    FILE *fptr;
    fptr = fopen("planes_log.txt", "a");

    yellow();

    fprintf(fptr, "---------------------------------------------------------------------------------\n");
    fprintf(fptr ,"--------------------------------------Time:%d-------------------------------------\n", time);
    fprintf(fptr, "|%-15s|%-15s|%-15s|%-15s|%-15s|\n", "PlaneID", "Status", "Request-Time",
    "Runway-Time", "Turnaround-Time");
    fprintf(fptr, "---------------------------------------------------------------------------------\n");
    red();
    int i;
    for (i=0; i < size; i++){
        if (planes[i]->respond_time != -1){
            if (!strcmp(planes[i]->status, "L"))
                green();
            else if (!strcmp(planes[i]->status, "D"))
                blue();
            else
                red();
            fprintf(fptr, "|%-15d|%-15s|%-15d|%-15d|%-15d|\n", planes[i]->id, planes[i]->status, planes[i]->request_time, planes[i]->respond_time, planes[i]->respond_time - planes[i]->request_time);
            reset();
        }else{
            magenta();
            fprintf(fptr, "|%-15d|%-15s|%-15d|%-15s|%-15s|\n", planes[i]->id, planes[i]->status, planes[i]->request_time, "-", "-");
            reset();
        }
    }
    yellow();
    fprintf(fptr, "---------------------------------------------------------------------------------\n\n");
    reset();

    fclose(fptr);
    pthread_mutex_unlock(&planes_array_lock);

}

/* checks whether starvation in the landing queue */
int starvation() {

    pthread_mutex_lock(&landing_lock);

    if (landing->capacity == 0){
        pthread_mutex_unlock(&landing_lock);
        return 0;
    }

    pthread_mutex_unlock(&landing_lock);

    int current_time = (int) time(NULL) - (int) start_time;

    plane *front_land = front(landing, &landing_lock);
    plane *front_depart = front(departing, &departing_lock);

    if (front_land->request_time - front_depart->request_time >= 4)
        return 0;

    /*  give proirity to landing turnaround < 6 for landing planes
        cause starvation for the departing planes.
    if (current_time - front_land->request_time >= 6)
        return 1;
     */

    return 1;

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

void blue() {
  printf("\033[0;34m");
}

void green() {
  printf("\033[0;32m");
}

void reset() {
  printf("\033[0m");
}



