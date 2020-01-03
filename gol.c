// Given a file with initialization, this program implements Conway's game of life
// in user specified format (in terminal or in visi program).
// Updated and changed to implement threads in user specified format
/*
 */
#include <pthreadGridVisi.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "colors.h"

/****************** Definitions **********************/
/* Three possible modes in which the GOL simulation can run */
#define OUTPUT_NONE   0   // with no animation
#define OUTPUT_ASCII  1   // with ascii animation
#define OUTPUT_VISI   2   // with ParaVis animation

/* Used to slow down animation run modes: usleep(SLEEP_USECS);
 */
//#define SLEEP_USECS  1000000
#define SLEEP_USECS    100000

static int total_live = 0;
static pthread_barrier_t barrier;

struct gol_data {

   int rows;  // the row dimension
   int cols;  // the column dimension
   int iters; // number of iterations to run the gol simulation
   int output_mode; // set to:  OUTPUT_NONE, OUTPUT_ASCII, or OUTPUT_VISI

   int round_num; //keeps track of which iteration of loop in
   int *arr; //data board of alive and dead cells
   int *arr_2;
   int *temp_arr;
   int nthreads;
   int part_type;
   int thread_id;
   int print_type;
   //info for partitioning function
   int distribute;
   int col_start;
   int col_end;
   int r_start;
   int r_end;
   /* fields used by ParaVis library (when run in OUTPUT_VISI mode). *
   visi_handle handle;
   color3 *image_buff;
};

/****************** Function Prototypes **********************/
void *play_gol(void *args);
void *play_step(void *args);
/* init gol data from the input file and run mode cmdline args */
int init_game_data_from_args(struct gol_data *data, char *argv[]);

void print_board(struct gol_data *data, int round);

// Determines if cell is alive or dead by counting neighbors
int is_alive(int row, int col, struct gol_data *data);
//Updates the grid for the visi library, taken from lab
//Converts 2d array index into a grid visi can understand
void update_grid(struct gol_data *data);
/************ Definitions for using ParVisi library ***********/
/* register animation with Paravisi library */
int connect_animation(void (*applfunc)(struct gol_data *data),
    struct gol_data* data);
void * partition(void * args);
/* loop through play_gol round and implements visi functions */
void *  visi_loop(void *args);
/* name for visi (you may change the string value if you'd like) */
static char visi_name[] = "GOL!";
pthread_mutex_t mutex;
/**********************************************************/
int main(int argc, char *argv[]) {

  int ret;
  struct gol_data data;


  double secs;

  struct timeval start_time, end_time;
  // init mutex
  pthread_mutex_init(&mutex, NULL);

  /* check number of command line arguments */
  if (argc < 3) {
    printf("usage: ./gol <infile> <0|1|2>\n");
    printf("(0: with no visi, 1: with ascii visi, 2: with ParaVis visi)\n");
    exit(1);
  }

  /* Initialize game state (all fields in data) from information
   * read from input file */
  ret = init_game_data_from_args(&data, argv);
  if(ret != 0) {
    printf("Error init'ivoid update_grid(struct gol_data *data)ng with file %s, mode %s\n", argv[1], argv[2]);
    exit(1);
  }

  ret = gettimeofday(&start_time, NULL);
  if (ret == -1) {
       printf("ERROR: time call failed from gettimeofday.");
       exit(1);

  }
  if(pthread_barrier_init(&barrier, NULL, data.nthreads)){
   perror("pthread_barrier_init");
   exit(1);
  }
  // initialize visi info for all threads
  if (data.output_mode == OUTPUT_VISI){
      data.handle = init_pthread_animation(data.nthreads, data.rows, data.cols, visi_name);
      if(data.handle == NULL) {
        printf("ERROR init_pthread_animation\n");
        exit(1);
      }
      // get the animation buffer
      data.image_buff = get_animation_buffer(data.handle);
      if(data.image_buff == NULL) {
        printf("ERROR get_animation_buffer returned NULL\n");
        exit(1);
      }
    }
  pthread_t* thread_array = malloc(data.nthreads * sizeof(pthread_t));
  struct gol_data * arg_array = malloc(data.nthreads * sizeof(struct gol_data));
  for (int i = 0; i < data.nthreads; i++){
    arg_array[i] = data;
    arg_array[i].thread_id = i;
  }
  if(data.output_mode == OUTPUT_NONE || data.output_mode == OUTPUT_ASCII){
    for (int i = 0; i < data.nthreads; i++){
      pthread_create(&thread_array[i], NULL, play_gol, &arg_array[i]);

    }
    for (int i = 0; i < data.nthreads; i++){
      pthread_join(thread_array[i], NULL);
    }
  }

  else {  // OUTPUT_VISI: run with ParaVisi animation
    // connect ParaVisi animation to main application loop function

    for (int i = 0; i < data.nthreads; i++){
      if( pthread_create(&thread_array[i], NULL, visi_loop, &arg_array[i])) {
        printf("pthread_created failed\n");
        exit(1);
      }
    }
    run_animation(data.handle, data.iters);

    // start ParaVisi animation
    for (int i = 0; i < data.nthreads; i++){
      pthread_join(thread_array[i], NULL);
    }



  }

  free(thread_array);
  free(arg_array);
  pthread_barrier_destroy(&barrier);

  // Compute the total runtime in seconds, including fractional seconds
  //       (e.g., 10.5 seconds should NOT be truncated to 10).
  if (data.output_mode != OUTPUT_VISI) {

    ret = gettimeofday(&end_time, NULL);
    if (ret == -1) {
       printf("ERROR: time call failed from gettimeofday.");
       exit(1);
    }

    //secs = the amount of time for this section of the program to run. Calculated
    //by taking the end time and subtracting the start time. Accessed the time
    //values accessing time struct values.
    secs = ((end_time.tv_sec * 1000000) + end_time.tv_usec)
		  - ((start_time.tv_sec * 1000000) + start_time.tv_usec);

    /* Print the total runtime, in seconds. */
    //secs = gettimeofday(&start_time, NULL);
    //using 10000 gives us the correct time. Should use 1000000 because write up?
    fprintf(stdout, "Total time: %0.3f seconds\n", secs/1000000);
    fprintf(stdout, "Number of live cells after %d rounds: %d\n\n",
        data.iters, total_live);
    fflush(stdout);
  }

  free(data.arr);
  free(data.arr_2);

  return 0;
}
/**********************************************************/

/* initialize the gol game state from command line arguments
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode value
 * data: pointer to gol_data struct to initialize
 * argv: command line args
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode
 * returns: 0 on success, 1 on error
 */
int is_alive(int row, int col, struct gol_data *data){
  int count = 0;
  for (int i = -1; i < 2; i++){
    for (int j = -1; j < 2; j++){
       //right boundary is not crossed
       if( (col + j) < data->cols ){
         //left boundary is not crossed
         if ( (col + j) >= 0 ){
            //top boundary is not crossed
            if ( (row+i) >= 0 ){
                //bottom boundary is not crossed
                if ( (row+i) < data->rows ){
                  if (data->arr[(row+i)*data->cols+(col+j)] == 1){
                         count ++;
                  }
                }
                //bottom is crossed, top, left, right not crossed
                else{
                  if (data->arr[0*data->cols+(col+j)] == 1){
                       count ++;
                  }
                }
            }
            // top boundary is crossed, bottom, right, left not crossed
           else{
              if (data->arr[(data->rows-1)*data->cols+(col+j)] == 1){
                     count ++;
              }
           }
         }
         //left boundary is crossed
         else{
           //top boundary is not crossed
           if ( (row+i) >= 0 ){
                // bottom boundary is not crossed, left is
                if ( (row+i) < data->rows ){
                  if (data->arr[(row+i)*data->cols+(data->cols-1)] == 1){
                         count ++;
                  }
                }
                //bottom boundary is crossed, so is left
                else{
                  if (data->arr[0*data->cols+(data->cols-1)] == 1){
                       count ++;
                  }
                }
            }
           //top boundary is crossed, so is left
           else{
              if (data->arr[(data->rows-1)*data->cols+(data->cols-1)] == 1){
                     count ++;
              }
           }
         }
       }
       else{ //right boundary ivoid visi_loop(struct gol_data *data)s crossed
          //top boundary is not crossed
            if ( (row+i) >= 0 ){
                //bottom boundary is not crossed
                if ( (row+i) < data->rows ){
                  if (data->arr[(row+i)*data->cols+0] == 1){
                         count ++;
                  }
                }
                //bottom boundary is crossed
                else{
                  if (data->arr[0*data->cols+0] == 1){
                       count ++;
                  }
                }
            }
            //top boundary is crossed
           else{
              if (data->arr[(data->rows-1)*data->cols+0] == 1){
                     count ++;
              }
           }
       }
    }
  }
  int cell = data->arr[row*data->cols+col];

  //returns a value that signifies whether the cell is alive or dead
  //uses Conway's logic
  count-= cell;
  if (cell){
    if ( (count<2) | (count>3)){
      return 0;
      total_live --;
    }
    return 1;
  }
  else{
    if(count == 3){
      return 1;
      total_live ++;
    }
    return 0;
  }
}

int init_game_data_from_args(struct gol_data *data, char *argv[]) {

  // DONE: Reads in values from user given argument, initializes the board, and
  // specifies game mode. Prints error if user data is incomplete, poorly
  // formatted, or there an error reading or opening the file.
  FILE *infile;
  int ret;

  data->output_mode = atoi(argv[2]);
  infile = fopen(argv[1],"r");
  if(infile == NULL){
    printf("Error: file open %s\n", argv[1]);
    exit(1);
  }
  int count = 0;
  data->nthreads = atoi(argv[3]);
  data->part_type = atoi(argv[4]);
  data->print_type = atoi(argv[5]);
  ret = fscanf(infile, "%d\n %d\n %d\n %d\n", &data->rows, &data->cols, &data->iters, &count);
  if (ret != 4){
        printf("Error: Incorrect number of fields");
        exit(1);
  }
  total_live = count;
  data->arr = malloc(sizeof(int)*data->rows*data->cols);
  for (int i = 0; i < data->rows*data->cols; i++){
    data->arr[i] = 0; //Trying to set entire 2D array to 0
  }
  data->arr_2 = malloc(sizeof(int)*data->rows*data->cols);
  for (int i = 0; i < data->rows*data->cols; i++){
    data->arr_2[i] = 0; //Trying to set entire 2D array to 0
  }
  for (int i = 0; i < count; i ++){ //Sets individual coordinates to 1
    int r, c;
    ret = fscanf(infile, "%d %d\n", &r,&c);
    if (ret != 2){
          printf("Error: Incorrect number of entries");
          exit(1);
    }

    data->arr[r*data->cols + c] = 1; //Sets coordinates to 1
    //data->arr_2[r*data->cols + c] = 1; // not sure if we have to do Sets coordinates to 1 for 2nd board
  }
  return 0;
}
/**********************************************************/
/* the gol application main loop function:
 *  runs round of GOL,
 *    * updates program state for next round (world and total_live)
 *    * performs any animation step for modes 0 or 1
 *
 *   data: pointer to a struct gol_data  initialized with
 *         all GOL game playing state
 */
 // original argument = struct gol_data *data
void *play_step(void *args) {

   struct gol_data *data;
   data = (struct gol_data *)args;
   int colsize, ret;
  // rowsize = data->rows;
  colsize = data->cols;
   for (int i = data->r_start; i < data->r_end+1; i++){
        for (int j = data->col_start;j < data->col_end+1; j++){
                data->arr_2[i*colsize+j] = is_alive(i,j,data);
                if (is_alive(i,j,data) == 1){

                }
        }
   }

   ret = pthread_barrier_wait(&barrier);

    if(ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
      perror("pthread_barrier_wait");
      exit(1);
    }

    data->temp_arr = data->arr;
    data->arr = data->arr_2;
    data->arr_2 = data->temp_arr;


    if(data->thread_id == 0 && data->output_mode == 1){
        if(system("clear")){
           perror("clear"); exit(1);
         }
        print_board(data, data->round_num);
        fflush(stdout);
    }


   //if ascii clears the terminal, prints board, and pauses to show screen



  return NULL;
}
// main functionality for modes 0 and 1
// loops through rounds of gol and performs desired output
void *play_gol(void *args){
  struct gol_data *data;
   data = (struct gol_data *)args;
   partition(data);
   if(data->print_type){
    printf("tid %d: rows: %d:%d (%d) cols: %d:%d (%d)\n",data->thread_id, data->r_start, data->r_end,
      (data->r_end - data->r_start+1), data->col_start, data->col_end, (data->col_end - data->col_start+1));
      fflush(stdout);
  }


  /* Invoke play_gol in different ways based on the run mode */
  if(data->output_mode == OUTPUT_NONE) {  // run with no animation
    for (data->round_num = 1; data->round_num < (data->iters+1); data->round_num++) {
       play_step(data);

    }

  }
  else if (data->output_mode == OUTPUT_ASCII) { // run with ascii animation
    if(data->thread_id == 0){
        if(system("clear"));
        print_board(data , 0);

  }
    usleep(SLEEP_USECS);
    for (data->round_num = 1; data->round_num < (data->iters+1); data->round_num++) {
        play_step(data);
        usleep(SLEEP_USECS);

   }
 }


  return NULL;
}
/**********************************************************/
/* Print the board to the terminal.
 *   data: gol game specific data
 *   round: the current round number
 */

//loops through the visi functions
// performs main functionality for game mode 2
// uses threads to display animation
void *  visi_loop(void *args){
  struct gol_data *data;
   data = (struct gol_data *)args;
   partition(data);
   if(data->print_type){
    printf("tid %d: rows: %d:%d (%d) cols: %d:%d (%d)\n",data->thread_id, data->r_start, data->r_end,
      (data->r_end - data->r_start+1), data->col_start, data->col_end, (data->col_end - data->col_start+1));
      fflush(stdout);
  }

  for (data->round_num = 1; data->round_num < (data->iters+1); data->round_num++){
    play_step(data);
    update_grid(data);
    draw_ready(data->handle);
    usleep(SLEEP_USECS);
  }

  return NULL;
}

//prints the ascii board to terminal
void print_board(struct gol_data *data, int round) {

    int i, j;

    /* Print the round number. */


    fprintf(stderr, "Round: %d\n", round);

    for (i = 0; i < data->rows; ++i) {
        for (j = 0; j < data->cols; ++j) {
            if(data->arr[i*data->cols+j]){
                fprintf(stderr, " @");

            }
            else{
                fprintf(stderr, " .");
            }
        }
        fprintf(stderr, "\n");
    }


    /* Print the total number of live cells. */
    fprintf(stderr, "Live cells: %d\n\n", total_live);

}
//Taken from weeklylab09 and altered
//has main visi visualization functionality
void update_grid(struct gol_data *data){


  int i, j, r, c, index, buff_i;
  color3 *buff;

  buff = data->image_buff;
  r = data->rows;
  c = data->cols;
  //sets entire grid to same color (nice purple)
  for(i= data->r_start; i < data->r_end+1; i++) {
    for(j= data-> col_start; j < data->col_end+1; j++) {
         buff_i = (r - (i+1))*c + j;
         buff[buff_i].r = 80;
         buff[buff_i].g = 0;
         buff[buff_i].b = 80;
    }
  }

  //updates cell color values if location in grid is alive
  for(i= data->r_start; i < data->r_end+1; i++) {
    for(j= data-> col_start; j < data->col_end+1; j++) {
        index = i*c + j;
        buff_i = (r - (i+1))*c + j;
        if (data->arr[index] == 1){
           buff[buff_i].r = 240;
           buff[buff_i].g = 0;
           buff[buff_i].b = 0;
           }
      }
  }
  int ret = pthread_barrier_wait(&barrier);

   if(ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
     perror("pthread_barrier_wait");
     exit(1);
   }
}

// Partitions the board into specific threads
// Each thread gets about equal partitions, they will not vary by more than 1
// assigns thread specific data struct information
// chunk = rounded division of rows or columns
// remainder is distruted among threads
void * partition(void * args){
  struct gol_data* data = (struct gol_data*) args;
  int length;
  int id = data->thread_id;
  int nthreads = data->nthreads;
  int remainder;
  int start, end;
  int rowst, rowe, colst, cole;
  if (data->part_type){
    length = data->cols;
  }
  else{
    length = data->rows;
  }
  int chunk = length/nthreads;
  remainder = length % nthreads;
  if (!remainder){
    start = id * chunk;
    end = start + chunk;
  }
  else{
    if (id < remainder){
// partition logic
      start = id * chunk + id;
      end = start + chunk + 1;
    }
    else{
      start = id * chunk + remainder;
      end = start + chunk;
    }
   }
  if (data->part_type){
    rowst = 0;
    rowe = data->rows-1;
    colst = start;
    cole = end-1;
  }
  else {
    rowst = start;
    rowe = end-1;
    colst = 0;
    cole = data->cols-1;
  }
  data->col_start = colst;
  data->col_end = cole;
  data->r_start = rowst;
  data->r_end = rowe;

  return NULL;
}
