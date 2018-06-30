/*	Author:	Matthew Silva
	
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <ncurses.h>			/* ncurses.h includes stdio.h */  
#include <string.h> 
#include <sys/time.h>
#include <stdint.h>
//
// TAKEN FROM tiff.h TIFF LIBRARY
#define TIFFGetR(abgr)   ((abgr) & 0xff)
#define TIFFGetG(abgr)   (((abgr) >> 8) & 0xff)
#define TIFFGetB(abgr)   (((abgr) >> 16) & 0xff)
#define TIFFGetA(abgr)   (((abgr) >> 24) & 0xff)

enum DifficultyLevel {Easy, Medium, Hard};
typedef enum Direction {UP, RIGHT, DOWN, LEFT};

// GOOD IDEA:
//	Have a renderObject method that
//	takes a void* argument that we cast to a
//	renderableObject type with a char* name
//	for determining the rendered character and
//	a row and col index
//
//
//
//
typedef struct Task
{
	char* indexName;
	
	int row;
	int col;
	int workLeft;
} Task;

typedef struct Mine
{
	char* indexName;
	
	int row;
	int col;
	
} Mine;

typedef struct ThreadInfo
{
	// Order in which thread was created
	int threadIndex;
	
	int* arr;
	int arrSize;
	int row;
	int col;
	int movesLeft;
	pthread_barrier_t* raceBarrier;
	float execTime;
	Task* taskArr;
	int numTasks;
	int AI;
	Mine* mineArr;
	int numMines;
	int resources;
	int refueling;
	enum Direction direction;
	enum DifficultyLevel diff;
	
} ThreadInfo;

typedef struct UserThreadInfo
{
	// Order in which thread was created
	int threadIndex;
	
	int* arr;
	int arrSize;
	int row;
	int col;
	int movesLeft;
	pthread_barrier_t* raceBarrier;
	float execTime;
	Task* taskArr;
	int numTasks;
	int AI;
	Mine* mineArr;
	int numMines;
	int resources;
	int refueling;
	enum Direction direction;
	
} UserThreadInfo;

typedef struct KeyboardThreadInfo
{
	// Order in which thread was created
	int threadIndex;
	
	int buildMode;
	int* curRow;
	int* curCol;
	
	ThreadInfo* thread;
	UserThreadInfo* userThread;
	pthread_mutex_t* directionLock;
	pthread_mutex_t* userDirectionLock;
	
} KeyboardThreadInfo;

uint64_t get_posix_clock_time ()
{
    struct timespec ts;

    if (clock_gettime (CLOCK_MONOTONIC, &ts) == 0)
        return (uint64_t) (ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
    else
        return 0;
}

// PROTOTYPES ------------------------------------------------------------

void* threadFunc(void* arg);

void* userThreadFunc(void* arg);

void* keyboardThreadFunc(void* arg);

void swap(int* arr, int index1, int index2);

void sort(int* arr, int arrLen);

void threadAction(void* info, int numRows, int numCols);

int same(int* arr1, int* arr2, int arrLen);

int inTaskSquare(int row, int col, ThreadInfo* thread, UserThreadInfo* user, char* toPrint);

int inMineSquare(int row, int col, ThreadInfo* thread, UserThreadInfo* user, char* toPrint);

enum Direction randDirection(void);

int isValidMove(int row, int col, enum Direction direction, int numRows, int numCols);

int isInside(int row, int col, int numRows, int numCols);

void threadAction(void* threadArg, int numRows, int numCols);

int getProgress(void* thread);

int validCurPos(int curRow, int curCol);

void userSort(int* arr, int arrLen);

//------------------------------------------------------------------------


// THREAD DESIGN:
// Threads keep track of their own movement and array correctness locally
// All the main thread does is synchronize their movements, render the grid, and supply new data


// Array of threads to create the difference image
pthread_t* thread;
// Array of parameter structs for the threadFunc
ThreadInfo* threadInfo;
	
// Barrier to synchronize the computation of the difference image by threads
pthread_barrier_t diffBarrier;

static int ARR_SIZE = 1000;

char* grid;

int main(int argc, char** argv)
{
	int diffNum;
	
	
	sscanf(argv[1], "%d", &diffNum);
	
	enum DifficultyLevel difficulty;
	
	switch(diffNum)
	{
		case(1):
			difficulty = Easy;
			break;
		case(2):
			difficulty = Medium;
			break;
		case(3):
			difficulty = Hard;
			break;
	}
	
	int timedout = 0;
	
	// TODO Set up timeoutThread
	
	pthread_barrier_t raceBarrier;
	pthread_barrier_init(&raceBarrier, NULL, 3);
	
	ThreadInfo params;
	UserThreadInfo userParams;
	KeyboardThreadInfo keyboardParams;
			
	
	int* arr = (int*) malloc(ARR_SIZE*sizeof(int));
	int* userArr = (int*) malloc(ARR_SIZE*sizeof(int));
	int* ansArr = (int*) malloc(ARR_SIZE*sizeof(int));
	
	initscr();
	clear();
	refresh();
	int numRows,numCols;
    getmaxyx(stdscr,numRows,numCols);
    
    int gameRows = numRows-8;
	int gameCols = numCols;
	
	int curRow = 9;
	int curCol = 1;
	
	
	// Maybe sub in my linked list implementation to have them get killed and mem removed as needed?
	Task* taskArr = (Task*) malloc(10*sizeof(Task));
	Task* userTaskArr = (Task*) malloc(10*sizeof(Task));
	
	Mine* mineArr = (Mine*) malloc(3*sizeof(Mine));
	
	for (int i = 0; i < 3; i++) {
		
		mineArr[i].indexName = (char*) malloc(3*sizeof(char));
		sprintf(mineArr[i].indexName, "%d", i);
		
		mineArr[i].row = rand() % gameRows;
		mineArr[i].col = rand() % gameCols;
		
	}
	
	/*
	for (int i = 0; i < 10; i++) {
		
		taskArr[i].indexName = (char*) malloc(3*sizeof(char));
		userTaskArr[i].indexName = (char*) malloc(3*sizeof(char));
		sprintf(taskArr[i].indexName, "%d", i);
		sprintf(userTaskArr[i].indexName, "%d", i);
		
		taskArr[i].row = userTaskArr[i].row = rand() % gameRows;
		taskArr[i].col = userTaskArr[i].col = rand() % gameCols;
		
		taskArr[i].workLeft = userTaskArr[i].workLeft = 9;
	}
	*/
    
    
    // ROW MAJOR ORDER
    grid = (char*) malloc(numRows*numCols*sizeof(int*));
	
	params.arr = arr;
	userParams.arr = userArr;
	params.arrSize = userParams.arrSize = ARR_SIZE;
	params.raceBarrier = userParams.raceBarrier = &raceBarrier;
	
	params.diff = difficulty;
	
	// TODO Create 2nd keyboardThread that outputs into a 2nd keyPress variable
    char keyPress = '~';
    keyboardParams.thread = &params;
    keyboardParams.userThread = &userParams;    
	
	pthread_mutex_t directionLock;
    pthread_mutex_init(&directionLock, NULL);
	pthread_mutex_t userDirectionLock; 
    pthread_mutex_init(&userDirectionLock, NULL);
    
   	keyboardParams.userDirectionLock = &userDirectionLock;
   	
   	keyboardParams.buildMode = 0;
   	keyboardParams.curRow = &curRow;
   	keyboardParams.curCol = &curCol;
   	
   	params.taskArr = taskArr;
   	userParams.taskArr = userTaskArr;
   	params.numTasks = 0;
   	userParams.numTasks = 0;
   	params.AI = 1;
   	userParams.AI = 0;
   	
   	params.mineArr = userParams.mineArr = mineArr;
	params.numMines = userParams.numMines = 3;
   	params.resources = userParams.resources = 25;
   	params.refueling = userParams.refueling = 0;
	
	pthread_t thread;
	pthread_t userThread;
	pthread_t keyboardThread;
	
	pthread_create(&thread, NULL, threadFunc, &params);
	pthread_create(&userThread, NULL, userThreadFunc, &userParams);
	pthread_create(&keyboardThread, NULL, keyboardThreadFunc, &keyboardParams);
	
	
	
/*    initscr();
    int row,col;				
	refresh();
	printw("THREAD PROGRESS:");
		
	for (int i = 0; i < 10; i++) {
        printw("#");
    }
    for (int i = 0; i < 0; i++) {
        printw("=");
    }
    
	printw("\n");
    printw("USER THREAD PROGRESS:");
    for (int i = 0; i < 10; i++) {
        printw("#");
    }
    for (int i = 0; i < 0; i++) {
        printw("=");
    }
	getch(); 
*/	char* screen = (char*) malloc(10000*sizeof(char));
	initscr();
	clear();
	
	
	char* message = (char*) malloc(1000*sizeof(char));
	char* nextLine = (char*) malloc(1000*sizeof(char));
	
	noecho();
	//keypad(stdstr, TRUE)
	
	refresh();
	printw("\n _____ _                        _  ______                    ");
	printw("\n|_   _| |                      | | | ___ \\                   ");              
	printw("\n  | | | |__  _ __ ___  __ _  __| | | |_/ /__ _  ___ ___ _ __ ");
	printw("\n  | | | '_ \\| '__/ _ \\/ _` |/ _` | |    // _` |/ __/ _ \\ '__|");
	printw("\n  | | | | | | | |  __/ (_| | (_| | | |\\ \\ (_| | (_|  __/ |   ");
	printw("\n  \\_/ |_| |_|_|  \\___|\\__,_|\\__,_| \\_| \\_\\__,_|\\___\\___|_|   ");
	                                                     
	printw("\n");
	for (int i = 0; i < numRows-8; i++) {
		for (int k = 0; k < numCols/2; k++) {
			if (i%2 == 0) {
				printw("-");
				printw("+");
			}
			if (i%2 != 0) {
				printw("+");
				printw("-");
			}
		}
		printw("\n");
	}
	refresh();
	
	
	sleep(2);
	clear();
	refresh();
    
	char* toPrint = (char*) malloc(3*sizeof(char));		
    
	int gameFinished = 0;
	int threadWon = 0;
	int userWon = 0;
	while (gameFinished != 1) {

		// Create a new random array and generate the answer, storing both inside params and userParams
		for (int i = 0; i < ARR_SIZE; i++) {
			arr[i] = userArr[i] = ansArr[i] = rand()%5000;
		}
		// ALREADY STORED INSIDE PARAMS AND USERPARAMS SINCE ITS A POINTER
		
		sort(ansArr, ARR_SIZE);//TODO
		
		// Tell threads to do one computation
		pthread_barrier_wait(&raceBarrier);
		// Wait for threads to finish computation
		pthread_barrier_wait(&raceBarrier);
		
		if (same(arr, ansArr, ARR_SIZE) != 1) {
			clear();
			refresh();
			printw("Thread code did not correctly sort the array. Exiting program, sorry.");
		    printw("\n\n\n");
			printw("\nPress any key to exit the game...");
			getch();
			endwin();
		}
		if (same(userArr, ansArr, ARR_SIZE) != 1) {
			clear();
			refresh();
			printw("Your (user) thread code did not correctly sort the array. Exiting program, try again.");
		    printw("\n\n\n");
			printw("\nPress any key to exit the game...");
			getch();
			endwin();
		}
		
		// TODO change this to something that makes the thread 2x faster for being 2x faster
		// BUT ONly AFTER I'VE ADDED MERGESORT OR SOME OTHER FUNCTION THAT WILL ACTUALLY BE 2x FASTER
		float speedup = params.execTime / userParams.execTime;
		//float pctFaster = speedup*100.0f - 100.f;
		int moves = (5.0f*speedup);
		if (moves < 5) {
			moves = 5;
		}
		
		params.movesLeft = 5;
		userParams.movesLeft = moves;
		if (keyboardParams.buildMode == 0) {
		
			while (params.movesLeft > 0 || userParams.movesLeft > 0) {
			
				pthread_mutex_lock(&directionLock);
				threadAction(&params, gameRows, gameCols);
				pthread_mutex_unlock(&directionLock);
			
				pthread_mutex_lock(&userDirectionLock);
				threadAction(&userParams, gameRows, gameCols);
				pthread_mutex_unlock(&userDirectionLock);
			
				// TODO
				//
				// DISPLAY PCT FASTER ON TOP ROW, must subtract 1 from screen size to find numRows and 
				// then do a manual print of the top row every frame here
		
				strcpy(screen, "");
		
				clear();
			
			
				sprintf(nextLine,"Thread Row = %d, Thread Col = %d, Thread Moves Left = %d, Thread resources = %d",params.row, params.col, params.movesLeft, params.resources);
				strcpy(message,nextLine);
				sprintf(nextLine,"\nU Thread Row = %d, U Thread Col = %d, U Thread Moves Left = %d, U Thread resources = %d", userParams.row, userParams.col, userParams.movesLeft, userParams.resources);
				strcat(message,nextLine);
				sprintf(nextLine,"\nThread Exec Time = %f, U Thread Exec Time = %f, speedup = %f", params.execTime, userParams.execTime, speedup);
				strcat(message,nextLine);
				//vv CAUSES GAME TO FREEZE================================================================================
				sprintf(nextLine,"\n     Thread Lap Progress:");
				int threadProgBarLen = gameCols - strlen(nextLine);
				int threadProgress = getProgress(&params);
				if (params.numTasks == 10 && threadProgress == params.numTasks) {
					gameFinished = 1;
					threadWon = 1;
				}
				for (int i = 0; i < ((float) threadProgress / (float) params.numTasks)*(float) threadProgBarLen; i++) {
					if (i+1 >= ((float) threadProgress / (float) params.numTasks)*(float) threadProgBarLen) {
						strcat(nextLine, ">");
					}
					else {
						strcat(nextLine, "=");
					}
				}
				strcat(message,nextLine);
			
				sprintf(nextLine,"\nUser Thread Lap Progress:");
				int userThreadProgBarLen = gameCols - strlen(nextLine);
				int userThreadProgress = getProgress(&userParams);
				if (userParams.numTasks == 10 && userThreadProgress == userParams.numTasks) {
					gameFinished = 1;
					userWon = 1;
				}
				for (int i = 0; i < ((float) userThreadProgress / (float) userParams.numTasks)*(float) userThreadProgBarLen; i++) {
					if (i+1 >= ((float) userThreadProgress / (float) userParams.numTasks)*(float) userThreadProgBarLen) {
						strcat(nextLine, ">");
					}
					else {
						strcat(nextLine, "=");
					}
				}
				strcat(message,nextLine);
			
				strcat(screen, message);
				strcat(screen,"\n\n");
				for (int i = 0; i < gameCols; i++) {
					strcat(screen, "+");
				}
				strcat(screen,"\n");
			
				//^^ CAUSES GAME TO FREEZE================================================================================
			

		 		for (int i = 0; i < gameRows; i++) {
		 			for (int k = 0; k < gameCols; k++) {
		 				if (i == params.row && k == params.col) {
		 					strcat(screen, "T");
		 				}
		 				else if (i == userParams.row && k == userParams.col) {
		 					strcat(screen, "U");
		 				}
		 				else if (inMineSquare(i, k, &params, &userParams, toPrint) == 1) {
		 					strcat(screen, toPrint);
		 				} 
		 				else if (inTaskSquare(i, k, &params, &userParams, toPrint) == 1) {
		 					strcat(screen, toPrint);
		 				}
		 				else {
		 					strcat(screen, ",");
		 				}
		 			}
		 			printw("\n");
		 		}
		 		
				move(0,0);
				printw(screen);
				refresh();
		
				usleep(10000);
			}
		} // if (buildMode == 0)
		
		else {
			move(curRow,curCol);
			refresh();
			usleep(10000);
			
		}


	
	}
	
	clear();
	move(0,0);
	
	if (threadWon == 1 && userWon == 1) {
		printw("\n\nTIE GAME! Try again!");
	}
	else if (threadWon == 1) {
		printw("\n\nUSER LOST! Try again!");
	}
	else if (userWon == 1) {
		printw("\n\nUSER WON! Good job!");
	}
	
	refresh();
	
	/*
    if (timedout == 0) {
        if (userParams.execTime < params.execTime) {
            printw("Your implementation was %f seconds faster. Good job!", params.execTime - userParams.execTime);
        }
        else {
            printw("Your implementation was %f seconds worse. Try again!", userParams.execTime - params.execTime);
        }
    }
	
    else {
        printw("Your code did not finish executing. Exiting program, try again.");
        printw("\n\n\n");
		printw("\nPress any key to exit the game...");
		getch();
		endwin();

    }
    */
    
    
    // TODO pthread_join(
	
	printw("\n\n\n");
	printw("\nPress any key to exit the game...");
	getch();
	endwin();
	return 0;
	
}

void swap(int* arr, int index1, int index2) {
	int temp = arr[index1];
	arr[index1] = arr[index2];
	arr[index2] = temp;
}

// Implement merge sort using cool C pointer arithmetic stuff passing calculated array lengths to simulate passing actual smaller arrays
void sort(int* arr, int arrLen) {
	
	for (int numProcessed = 0; numProcessed < arrLen; numProcessed++) {
		int min = arr[numProcessed];
		int minIndex = numProcessed;	
		for (int i = numProcessed; i < arrLen; i++) {
			if (arr[i] < min) {
				min = arr[i];
				minIndex = i;
			}
		}
		swap(arr, minIndex, numProcessed);
	}
}

void merge(int* arr, int* split1Arr, int split1Len, int* split2Arr, int split2Len) {
	int split1Counter = 0;
	int split2Counter = 0;
	for (int i = 0; i < split1Len + split2Len; i++) {
		if (split1Counter == split1Len) {
			arr[i] = split2Arr[split2Counter];
			split2Counter++;
		}
		else if (split2Counter == split2Len) {
			arr[i] = split1Arr[split1Counter];
			split1Counter++;
		}
		
		else if (split1Arr[split1Counter] <= split2Arr[split2Counter]) {
			arr[i] = split1Arr[split1Counter];
			split1Counter++;
		}
		else {
			arr[i] = split2Arr[split2Counter];
			split2Counter++;
		}
	}
}

void mergeSort(int* arr, int arrLen) {
	if (arrLen == 1) {
		return;
	}
	
	// Create a copy for the halves
	int* copyArr = (int*) malloc(arrLen*sizeof(int));
	memcpy(copyArr, arr, arrLen*sizeof(int));
	
	// Split the array
	int* arr1 = copyArr;
	int arr1Len = arrLen / 2;
	int* arr2 = copyArr + arr1Len;
	int arr2Len = arrLen - arr1Len;
	
	// Sort each split array
	mergeSort(arr1, arr1Len);
	mergeSort(arr2, arr2Len);
	
	// Merge them in order
	merge(arr, arr1, arr1Len, arr2, arr2Len);
	
	free(copyArr);
	/*
		EXAMPLE
		====================
		arr = 0
		arrLen = 5
		arr1 = 0
		arr1Len = 2
		arr2 = 2
		arr2Len = 3
		
		EXAMPLE
		====================
		arr = 0
		arrLen = 2
		arr1 = 0
		arr1Len = 1
		arr2 = 1
		arr2Len = 1
	*/
}

int same(int* arr1, int* arr2, int arrLen) {
	for (int i = 0; i < arrLen; i++) {
		if (arr1[i] != arr2[i]) {
			return 0;
		}
	}
	return 1;
}

int inTaskSquare(int row, int col, ThreadInfo* thread, UserThreadInfo* user, char* toPrint) {
	for (int i = 0; i < thread->numTasks; i++) {
		int taskRow = thread->taskArr[i].row;
		int taskCol = thread->taskArr[i].col;
		if (row > taskRow-2 && row < taskRow+2 && col > taskCol-2 && col < taskCol+2) {
			if (thread->taskArr[i].workLeft <= 0 && user->taskArr[i].workLeft <= 0) {
				strcpy(toPrint, "X");
			}
			
			else if (row == taskRow && col == taskCol) {
				strcpy(toPrint, thread->taskArr[i].indexName);
			}
			
			else if ((row == taskRow-1 && col == taskCol-1) || (row == taskRow-1 && col == taskCol+1)
					|| (row == taskRow+1 && col == taskCol-1) || (row == taskRow+1 && col == taskCol+1)) {
				
				if (thread->taskArr[i].workLeft <= 0) {
					strcpy(toPrint, "X");

				}
				else {
					char* workStr = (char*) malloc(2*sizeof(char));
					sprintf(workStr, "%d", thread->taskArr[i].workLeft);
					strcpy(toPrint, workStr);
					free(workStr);

				}
				
			}
			else if ((row == taskRow && col == taskCol-1) || (row == taskRow && col == taskCol+1)
					|| (row == taskRow+1 && col == taskCol) || (row == taskRow-1 && col == taskCol)) {
				
				if (user->taskArr[i].workLeft <= 0) {
					strcpy(toPrint, "X");

				}
				else {
					char* workStr = (char*) malloc(2*sizeof(char));
					sprintf(workStr, "%d", user->taskArr[i].workLeft);
					strcpy(toPrint, workStr);
					free(workStr);

				}
				
			}
			return 1;
		}
	}
	return 0;
}

int inMineSquare(int row, int col, ThreadInfo* thread, UserThreadInfo* user, char* toPrint) {
	for (int i = 0; i < thread->numMines; i++) {
		int mineRow = thread->mineArr[i].row;
		int mineCol = thread->mineArr[i].col;
		if (row > mineRow-2 && row < mineRow+2 && col > mineCol-2 && col < mineCol+2) {
			if (row == mineRow && col == mineCol) {
				strcpy(toPrint, thread->mineArr[i].indexName);
			}
			
			else {
				strcpy(toPrint, "M");
			}
			return 1;
		}
	}
	return 0;
}

// Threads use this function to compute the part of the difference image they were assigned to compute
void* threadFunc(void* arg) {
	// Cast the void* parameter struct for actual use
	ThreadInfo* info = ((ThreadInfo*)arg);
	
	info->row = 1;
	info->col = 1;
	
	info->direction = DOWN;
	int speed;
	
	uint64_t prev_time_value, time_value;
	uint64_t time_diff;
	
	while (1) {
		// Wait for signal to perform 1 operation
		pthread_barrier_wait(info->raceBarrier);

		/* Initial time */
		prev_time_value = get_posix_clock_time ();
		
		sort(info->arr, info->arrSize);
		
		/* Final time */
		time_value = get_posix_clock_time ();

		/* Time difference */
		time_diff = time_value - prev_time_value;
		info->execTime = (float) time_diff / 1000000.0f;
	
	
	//	double progress = checkProgress(info->arr, info->ansArr, &(info->doneWithArr));
	//	speed = 1+(progress/25.0);
	//	move(info, speed, direction);
		
		// Signal that we are done with our sort operation and move
		pthread_barrier_wait(info->raceBarrier);
		
	}
	
	// Won't ever get here
	exit(1);
	
	
}

// Threads use this function to compute the part of the difference image they were assigned to compute
void* userThreadFunc(void* arg) {
	// Cast the void* parameter struct for actual use
	UserThreadInfo* info = ((UserThreadInfo*)arg);
	
	info->row = 5;
	info->col = 5;
	
	info->direction = DOWN;
	
	uint64_t prev_time_value, time_value;
	uint64_t time_diff;
	
	while (1) {
		// Wait for signal to perform 1 operation
		pthread_barrier_wait(info->raceBarrier);
		
		/* Initial time */
		prev_time_value = get_posix_clock_time ();
		
		userSort(info->arr, info->arrSize);
		
		/* Final time */
		time_value = get_posix_clock_time ();

		/* Time difference */
		time_diff = time_value - prev_time_value;
		info->execTime = (float) time_diff / 1000000.0f;
	
	
	//	double progress = checkProgress(info->arr, info->ansArr, &(info->doneWithArr));
	//	speed = 1+(progress/25.0);
	//	move(info, speed, direction);
		
		// Signal that we are done with our sort operation and move
		pthread_barrier_wait(info->raceBarrier);
		
	}
	
	// Won't ever get here
	exit(1);
	
	
}

enum Direction randDirection(void) {
	int dirNum = rand()%4;
	enum Direction dir;
	switch(dirNum)
	{
		case(0):
			dir = UP;
			return dir;
		case(1):
			dir = RIGHT;
			return dir;
		case(2):
			dir = DOWN;
			return dir;
		case(3):
			dir = LEFT;
			return dir;
	}
	exit(1);
}

int isInside(int row, int col, int numRows, int numCols) {
	if (row > -1 && row < numRows && col > -1 && col < numCols) {
		return 1;
	}
	else {
		return 0;
	}
}

int isValidMove(int row, int col, enum Direction direction, int numRows, int numCols) {
				
	switch(direction)
	{
		case(UP):
			return isInside(row-1, col, numRows, numCols);
			break;
		
		case(RIGHT):
			return isInside(row, col+1, numRows, numCols);
			break;
			
		case(DOWN):
			return isInside(row+1, col, numRows, numCols);
			break;
			
		case(LEFT):
			return isInside(row, col-1, numRows, numCols);
			break;
	}
	exit(1);
}


void threadAction(void* threadArg, int numRows, int numCols) {
	ThreadInfo* info = ((ThreadInfo*)threadArg);
	
	if (info->AI == 1) {
		// IF NOT OUT OF RESOURCES and !Refueling
		if (info->resources > 0 && info->refueling != 1) {
		
			int i = 0;
			while (info->taskArr[i].workLeft <= 0 && i < info->numTasks) {
				i++;
			}
			int taskRow = info->taskArr[i].row;
			int taskCol = info->taskArr[i].col;
		
			if (info->row > taskRow) {
				info->direction = UP;
			}
			else if (info->col > taskCol) {
				info->direction = LEFT;
			}
			else if (info->row < taskRow) {
				info->direction = DOWN;
			}
			else if (info->col < taskCol) {
				info->direction = RIGHT;
			}
		}
		// ELSE OUT OF RESOURCES 
		else {
		// Set the Refueling flag to 1
		// Then go to the NEAREST mine and get resources until you have 50, in which case you set the flag to 0
			info->refueling = 1;
			
			int minDistance = numRows*numCols;
			int closestMineIndex = 0;
			for (int i = 0; i < info->numMines; i++) {
				int mineRow = info->mineArr[i].row;
				int mineCol = info->mineArr[i].col;
				int yDist = abs(mineRow - info->row);
				int xDist = abs(mineCol - info->col);
				
				int distance = (int) sqrt((double) (xDist*xDist + yDist*yDist));
				if (distance < minDistance) {
					minDistance = distance;
					closestMineIndex = i;
				}
			}
			
			int mineRow = info->mineArr[closestMineIndex].row;
			int mineCol = info->mineArr[closestMineIndex].col;
			
			if (info->row > mineRow) {
				info->direction = UP;
			}
			else if (info->col > mineCol) {
				info->direction = LEFT;
			}
			else if (info->row < mineRow) {
				info->direction = DOWN;
			}
			else if (info->col < mineCol) {
				info->direction = RIGHT;
			}
			
			if (info->resources >= 50) {
				info->refueling = 0;
			} 
		}
	}
	if (info->movesLeft > 0) {
		for (int i = 0; i < info->numTasks; i++) {
			int row = info->taskArr[i].row;
			int col = info->taskArr[i].col;
			if (info->row > row-2 && info->row < row+2 && info->col > col-2 && info->col < col+2) {
				// NEED MUTEX FOR THIS CHECK
				if (info->taskArr[i].workLeft > 0 && info->resources > 0) {
					info->taskArr[i].workLeft--;
					info->resources--;
				}
			} 
		}
		for (int i = 0; i < info->numMines; i++) {
			int row = info->mineArr[i].row;
			int col = info->mineArr[i].col;
			if (info->row > row-2 && info->row < row+2 && info->col > col-2 && info->col < col+2) {
				// NEED MUTEX FOR THIS CHECK
				if (info->resources <= 50) {
					info->resources++;
				}
			} 
		}
		
		switch(info->direction)
		{
			case(UP):
				if (isValidMove(info->row, info->col, info->direction, numRows, numCols) == 1) {
					info->row--;
					info->movesLeft--;
				}
				else {
					info->movesLeft=0;
				}
				break;
		
			
			case(RIGHT):
				if (isValidMove(info->row, info->col, info->direction, numRows, numCols) == 1) {
					info->col++;
					info->movesLeft--;
				}
				else {
					info->movesLeft=0;
				}
				break;
		
			
			case(DOWN):
				if (isValidMove(info->row, info->col, info->direction, numRows, numCols) == 1) {
					info->row++;
					info->movesLeft--;
				}
				else {
					info->movesLeft=0;
				}
				break;
		
			
			case(LEFT):
				if (isValidMove(info->row, info->col, info->direction, numRows, numCols) == 1) {
					info->col--;
					info->movesLeft--;
				}
				else {
					info->movesLeft=0;
				}
				break;
			
		}
	}
	
}

int getProgress(void* arg) {
	ThreadInfo info = *((ThreadInfo*)arg);
	
	int tasksCompleted = 0;
	for (int i = 0; i < info.numTasks; i++) {
		if (info.taskArr[i].workLeft <= 0) {
			tasksCompleted++;
		}
	}
	return tasksCompleted;
} 

// TODO Add 2nd thread for 2nd player??? Nope
//
// TODO ADD MUTEX for direction and make change immediately to prevent long moves before dir change

void* keyboardThreadFunc(void* arg) {
	// Cast the void* parameter struct for actual use
	KeyboardThreadInfo* info = ((KeyboardThreadInfo*)arg);
	
	char keyPress;
	int i = 0;
	while(1) {
		keyPress = getch();
		
		if (keyPress == 'b') {
			info->buildMode = !(info->buildMode);
		}
		
		else if (keyPress == 'w') {
			pthread_mutex_lock(info->userDirectionLock);
			info->userThread->direction = UP;
			pthread_mutex_unlock(info->userDirectionLock);
		}
		else if (keyPress == 'a') {
			pthread_mutex_lock(info->userDirectionLock);
			info->userThread->direction = LEFT;
			pthread_mutex_unlock(info->userDirectionLock);
		}
		else if (keyPress == 's') {
			pthread_mutex_lock(info->userDirectionLock);
			info->userThread->direction = DOWN;
			pthread_mutex_unlock(info->userDirectionLock);
		}
		else if (keyPress == 'd') {
			pthread_mutex_lock(info->userDirectionLock);
			info->userThread->direction = RIGHT;
			pthread_mutex_unlock(info->userDirectionLock);
		}
		
		else if (keyPress == '8') {
			if (validCurPos((*info->curRow)-1,(*info->curCol)) == 1) {
				(*info->curRow)--;
			}
		}
		else if (keyPress == '4') {
			if (validCurPos((*info->curRow),(*info->curCol)-1) == 1) {
				(*info->curCol)--;
			}
		}
		else if (keyPress == '5') {
			if (validCurPos((*info->curRow)+1,(*info->curCol)) == 1) {
				(*info->curRow)++;
			}
		}
		else if (keyPress == '6') {
			if (validCurPos((*info->curRow),(*info->curCol)+1) == 1) {
				(*info->curCol)++;
			}
		}
		
		else if (keyPress == 'n' && info->buildMode == 1) {
			if (i < 10) {
				printw("X");
				info->thread->taskArr[i].indexName = (char*) malloc(3*sizeof(char));
				info->userThread->taskArr[i].indexName = (char*) malloc(3*sizeof(char));
				sprintf(info->thread->taskArr[i].indexName, "%d", i);
				sprintf(info->userThread->taskArr[i].indexName, "%d", i);
		
				info->thread->taskArr[i].row = info->userThread->taskArr[i].row = *(info->curRow);
				info->thread->taskArr[i].col = info->userThread->taskArr[i].col = *(info->curCol);
		
				info->thread->taskArr[i].workLeft = info->userThread->taskArr[i].workLeft = 9;
				info->thread->numTasks++;
				info->userThread->numTasks++;
				i++;
			}
		}
		
		// TODO Add a keypress to add a new task
		// TODO create another kind of task (e.g. wallbuilding
		
	}
	exit(1);
}

// TODO TODO TODO
int validCurPos(int curRow, int curCol) {
	return 1;
}


	
// Please fill in a function that will sort arr from smallest to largest

// In/Out: arr (array of positive integers from 0 to 5000)
// In:	arrSize (How many integers are in this array

void userSort(int* arr, int arrSize) {
	mergeSort(arr,arrSize);

}
