#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

//Global variables 
extern int next_thread_id;
extern int max_squares;           
extern int total_tours;           

pthread_mutex_t mutex_tid = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_max = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_trs = PTHREAD_MUTEX_INITIALIZER;


struct Board{
	int row;
	int col;
	int **board;
	int dim;
	int highest;
	int complete;

	int currx;
	int curry;
	int movenum;

	int thread_id;
};

void function(struct Board b, int currx, int curry, int movenum);


void put(struct Board b, int r, int c, int moveN){
	b.board[r][c] = moveN;
}

void print(struct Board b){
	for(int i = 0; i < b.row; i++){
		for(int j = 0; j < b.col; j++){
			printf("%d", b.board[i][j]);
		}
		printf("\n");
	}
	printf("\n");

	printf("highest:%d\n", b.highest);
	printf("movenum: %d\n", b.movenum);


}

void printmoves(int** b){
	for(int i = 0; i < 8; i++){
		for(int j = 0; j < 2; j++){
			printf("%d", b[i][j]);
		}
		printf("%s","\n");
	}
	printf("%s","\n");
}
int isEmpty(struct Board b, int r, int c){
	if(b.board[r][c] > 0){
		return 0;
	}	
	return 1;
}

void * whattodo(void*arg){
	struct Board b = *(struct Board *) arg;
	// print(b);
	free(arg);
	struct Board * ret_val = malloc(sizeof(struct Board));
	*ret_val = b;
	function(b, b.currx, b.curry, b.movenum);
	pthread_exit(ret_val);
	return NULL;
}


void nextMoves(struct Board b, int positions[8][2] , int startx, int starty, int* moveCount, int** moves){
	int count = 0;
	for(int i = 7; i >= 0; i--){
		if((startx + positions[i][0]) < b.row && (startx + positions[i][0]) >= 0){
			if((starty + positions[i][1]) < b.col && (starty + positions[i][1]) >= 0){
				if(isEmpty(b,(startx + positions[i][0]), (starty + positions[i][1]))){
					moves[count][0] = (startx + positions[i][0]);
					moves[count][1] = (starty + positions[i][1]);
					count++;					
				}
			}
		}
	}

	*moveCount = count;
}



#ifndef NO_PARALLEL
void function(struct Board b, int currx, int curry, int movenum){
	put(b, currx, curry, movenum);
	int moveCount = 0;
	int checks[8][2] = {
        {-2, 1},
        {-1, 2},
        {1, 2},
        {2, 1},
        {2, -1},
        {1, -2},
        {-1, -2},
        {-2, -1},
    };


    int** moves = (int**)calloc(8, sizeof(int*));
	for(int i = 0; i < 8; i++){
		*(moves+ i) = calloc(2, sizeof(int));
	}

	nextMoves(b, checks, currx, curry, &moveCount, moves);
	if(moveCount == 0){
		// print(b);
		if(movenum == (b.dim + 1)){
			if(b.thread_id == 0){
				printf("MAIN: Sonny found a full knight's tour; incremented total_tours\n");

			}else{
				printf("T%d: Sonny found a full knight's tour; incremented total_tours\n", b.thread_id);
			}
			pthread_mutex_lock(&mutex_trs);
			{total_tours += 1;}
			pthread_mutex_unlock(&mutex_trs);
			pthread_mutex_lock(&mutex_max);
			{max_squares = b.dim;}
			pthread_mutex_unlock(&mutex_max);

		}else{
			// print(b);
			if(b.thread_id == 0){
				printf("MAIN: Dead end at move #%d", b.movenum);
			}else{
				printf("T%d: Dead end at move #%d", b.thread_id, b.movenum);
			}
			if(b.highest > max_squares){
				pthread_mutex_lock(&mutex_max);
				{max_squares = b.highest;}
				pthread_mutex_unlock(&mutex_max);
				printf("; updated max_squares");
			}
			printf("\n");
		}
		return;

	}else if(moveCount == 1){
		// print(b);
		b.currx = moves[0][0];
		b.curry = moves[0][1];
		b.highest+= 1;
		b.movenum += 1;
		function(b, moves[0][0], moves[0][1], b.highest);
	}else{
		pthread_t tid[moveCount];
		int array[moveCount];
		if(b.movenum == 1){
			printf("MAIN: %d possible moves after move #%d; creating %d child threads...\n", moveCount, b.movenum, moveCount);
		}else{
			printf("T%d: %d possible moves after move #%d; creating %d child threads...\n",b.thread_id, moveCount, b.movenum, moveCount);
		}
		for(int e = 0; e < moveCount; e++){
			struct Board* ptr = malloc(sizeof(struct Board));
			struct Board a;
			a.col = b.col;
			a.row = b.row;
			// printf("oppoespojsfo\n");
			a.board = (int**)calloc (a.row, sizeof(int*));
			for(int i = 0; i < a.row; i++){
				*(a.board + i) = (int *) calloc(a.col, sizeof(int));
			}
			for(int i = 0; i < a.row; i++){
				for(int j = 0; j < a.col; j++){
					a.board[i][j] = b.board[i][j];
				}
			}
			a.dim = b.dim;
			a.highest = b.highest+1;
			a.complete = b.complete;
			a.currx = b.currx;
			a.curry = b.curry;
			a.movenum = b.movenum;
			a.currx = moves[e][0];
			a.curry = moves[e][1];
			a.movenum += 1;
			a.thread_id = next_thread_id;
			// //create a main thread
			array[e] = a.thread_id;
			*(ptr) = a;
			// print(*ptr);
			int rc = pthread_create(&tid[e], NULL, whattodo, ptr);
			if(rc != 0){
				fprintf(stderr, "pthread_create() failed (%d)\n", rc);
			}			
			pthread_mutex_lock(&mutex_tid);
			{next_thread_id += 1;}
			pthread_mutex_unlock(&mutex_tid);
		}
		for(int i = 0; i < moveCount; i++){
			struct Board * x;
			pthread_join(tid[i], (void**) &x);
			// printf("MAIN: T%d joined\n", array[i]);
			if(b.thread_id == 0){
				printf("MAIN: T%d joined\n",array[i]); 
			}else{
				printf("T%d: T%d joined\n",b.thread_id, array[i]);
			}
		}

	}
}
#endif		


#ifdef NO_PARALLEL
void function(struct Board b, int currx, int curry, int movenum){
	put(b, currx, curry, movenum);
	int moveCount = 0;
	int checks[8][2] = {
        {-2, 1},
        {-1, 2},
        {1, 2},
        {2, 1},
        {2, -1},
        {1, -2},
        {-1, -2},
        {-2, -1},
    };


    int** moves = (int**)calloc(8, sizeof(int*));
	for(int i = 0; i < 8; i++){
		*(moves+ i) = calloc(2, sizeof(int));
	}

	nextMoves(b, checks, currx, curry, &moveCount, moves);
	if(moveCount == 0){
		// print(b);
		if(movenum == (b.dim)){
			if(b.thread_id == 0){
				printf("MAIN: Sonny found a full knight's tour; incremented total_tours\n");

			}else{
				printf("T%d: Sonny found a full knight's tour; incremented total_tours\n", b.thread_id);
			}
			pthread_mutex_lock(&mutex_trs);
			{total_tours += 1;}
			pthread_mutex_unlock(&mutex_trs);

			pthread_mutex_lock(&mutex_max);
			{max_squares = b.dim;}
			pthread_mutex_unlock(&mutex_max);
		}else{
			// print(b);
			if(b.thread_id == 0){
				printf("MAIN: Dead end at move #%d", b.movenum);
			}else{
				printf("T%d: Dead end at move #%d", b.thread_id, b.movenum);
			}
			if(b.highest > max_squares){
				pthread_mutex_lock(&mutex_max);
				{max_squares = b.highest;}
				pthread_mutex_unlock(&mutex_max);
				printf("; updated max_squares");
			}
			printf("\n");
		}
		return;

	}else if(moveCount == 1){
		// print(b);
		b.currx = moves[0][0];
		b.curry = moves[0][1];
		b.highest+= 1;
		b.movenum += 1;
		function(b, moves[0][0], moves[0][1], b.highest);
	}else{
		pthread_t tid[moveCount];
		if(b.movenum == 1){
			printf("MAIN: %d possible moves after move #%d; creating %d child threads...\n", moveCount, b.movenum, moveCount);
		}else{
			printf("T%d: %d possible moves after move #%d; creating %d child threads...\n",b.thread_id, moveCount, b.movenum, moveCount);
		}
		for(int e = 0; e < moveCount; e++){
			struct Board* ptr = malloc(sizeof(struct Board));
			struct Board a;
			a.col = b.col;
			a.row = b.row;
			// printf("oppoespojsfo\n");
			a.board = (int**)calloc (a.row, sizeof(int*));
			for(int i = 0; i < a.row; i++){
				*(a.board + i) = (int *) calloc(a.col, sizeof(int));
			}
			for(int i = 0; i < a.row; i++){
				for(int j = 0; j < a.col; j++){
					a.board[i][j] = b.board[i][j];
				}
			}
			a.dim = b.dim;
			a.highest = b.highest+1;
			a.complete = b.complete;
			a.currx = b.currx;
			a.curry = b.curry;
			a.movenum = b.movenum;
			a.currx = moves[e][0];
			a.curry = moves[e][1];
			a.movenum += 1;
			a.thread_id = next_thread_id;
			// //create a main thread
			*(ptr) = a;
			// print(*ptr);
			int rc = pthread_create(&tid[e], NULL, whattodo, ptr);
			if(rc != 0){
				fprintf(stderr, "pthread_create() failed (%d)\n", rc);
			}			
			pthread_mutex_lock(&mutex_tid);
			{next_thread_id += 1;}
			pthread_mutex_unlock(&mutex_tid);
			struct Board * x;
			pthread_join(tid[e], (void**) &x);
			if(b.thread_id == 0){
				printf("MAIN: T%d joined\n",a.thread_id); 
			}else{
				printf("T%d: T%d joined\n",b.thread_id, a.thread_id);
			}
		}

	}
}
#endif


int simulate(int argc, char** argv){
	//Global variables
	next_thread_id = 1;
	max_squares = 0;
	total_tours = 0;

	if(argc < 5){
		fprintf(stderr, "ERROR: Too few arguments\n");
		//perror("ERROR: Too few arguments");
		return EXIT_FAILURE;
	}

	int rows = atoi(*(argv+1));
	int cols = atoi(*(argv+2));
	int startrow = atoi(*(argv+3));
	int startcol = atoi(*(argv+4));


	if((rows <= 2) || (cols <= 2)){
		fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: hw2.out <m> <n> <r> <c>\n");
		//perror("ERROR: Invalid argument(s)\nUSAGE: hw2.out <m> <n> <r> <c>\n");
		return EXIT_FAILURE;
	}
	
	//checking if it isn't on board (in negatives)
	if(startcol < 0 || startrow < 0){
		fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: hw2.out <m> <n> <r> <c>\n");
		return EXIT_FAILURE;
	}

	//checking if it is off of board
	if(startcol > (cols-1) || startrow > (rows-1)){
		fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: hw2.out <m> <n> <r> <c>\n");
		return EXIT_FAILURE;
	}

	//making the board
	struct Board b;
	b.board = (int**)calloc (rows, sizeof(int*));
	for(int i = 0; i < rows; i++){
		*(b.board + i) = (int *) calloc(cols, sizeof(int));
	}

	printf("MAIN: Solving Sonny's knight's tour problem for a %dx%d board\n",rows, cols);
	printf("MAIN: Sonny starts at row %d and column %d (move #1)\n", startrow, startcol);

	b.row = rows;
	b.col = cols;
	b.dim = rows*cols;
	b.highest = 1;
	b.complete = 0;
	b.currx = startrow;
	b.curry = startcol;
	b.movenum = 1;
	b.thread_id = 0;
	function(b, startrow, startcol, 1);
	printf("MAIN: Search complete;");
	if(total_tours > 0){
		if(total_tours == 1){
			printf(" found %d possible path to achieving a full knight's tour\n", total_tours);

		}else{
			printf(" found %d possible paths to achieving a full knight's tour\n", total_tours);
		}
	}else{
		if(max_squares == 1){
			printf(" best solution(s) visited %d square out of %d\n", max_squares, rows*cols);
		}else{
			printf(" best solution(s) visited %d squares out of %d\n", max_squares, rows*cols);
		}
	}

	return 1;
}
