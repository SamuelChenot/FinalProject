//
//  main.c
//  Final Project CSC412
//
//  Created by Jean-Yves Hervé on 2019-12-12
//	This is public domain code.  By all means appropriate it and change is to your
//	heart's content.
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <fstream>

#include <cstdlib>

#include <time.h>
#include <pthread.h>
#include <unistd.h>

//
#include "gl_frontEnd.h"

using namespace std;

// Our structs: ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/** A struct to hold a single point on the grid.
*/
typedef struct Point {
	/** The x location of the point. */
	int x;
	/** The y location of the point. */
	int y;
} Point;

/** A struct of info for a box. 
*/
typedef struct BoxInfo{
	/** The location of the box in the grid. */
	Point location;
} BoxInfo;

/** A struct of info for a door.
*/
typedef struct DoorInfo{
	/** The location of the door in the grid. */
	Point location;
} DoorInfo;

/** A struct to hold all the info for a robot/thread.
*/
typedef struct RobotInfo{

	/** The thread ID of a given robot. */
	pthread_t	threadID;
	/** The index of the robot. */
	unsigned int index;

	
	/** The next direction the robot wants to move in. */
	Direction moveDirection;
	/** The direction that the robot wants to push the box in next. */
	Direction pushDirection;
	
	/** Whether or not the robot is able to push now. */
	bool canPush;
	
	/** Whether or not the robot is waiting on a grid spot. */
	bool waitingForGrid;
	
	/** Whether or not the robot has finished moving the box to the door. */
	bool end;
	
	/** The location on the grid of the robot. */
	Point location;
	
	/** The box that the robot needs to push. */
	BoxInfo *box;
	
	/** The door that the box needs to be pushed to. */
	DoorInfo *door;

} RobotInfo;

//==================================================================================
//	Function prototypes
//==================================================================================
void displayGridPane(void);
void displayStatePane(void);
void initializeApplication(void);
void cleanupGridAndLists(void);


// Our function prototypes
DoorInfo createDoor();
BoxInfo createBox();
RobotInfo* createRobot(int id);
int GenerateRandomValue(int start, int end);
void* robotThreadFunc(void* voidInfo);
bool boxAtEnd(BoxInfo box, DoorInfo door);
bool alreadyExist(int x, int y);
char getDirectionChar(Direction dir);
Point displacementBetweenPoints(Point p1, Point p2);
Direction chooseNextPush(RobotInfo* info);
Direction findXOrientation(int x);
Direction findYOrientation(int y);
Direction chooseMovement(RobotInfo* info);
bool ableToPush(RobotInfo* info);
void move(RobotInfo* info);
void push(RobotInfo* info);
void writeFileHeader();
void* deadlockThreadFunc(void* args);
void killRobot(RobotInfo* info);
bool deadlockExists(RobotInfo* r1, RobotInfo* r2);
Point getDest(RobotInfo* info);


//==================================================================================
//	Application-level global variables
//==================================================================================

//	Don't touch
extern int	GRID_PANE, STATE_PANE;
extern int 	GRID_PANE_WIDTH, GRID_PANE_HEIGHT;
extern int	gMainWindow, gSubwindow[2];

//	Don't rename any of these variables
//-------------------------------------
//	The state grid and its dimensions (arguments to the program)
int** grid;
int numRows = -1;	//	height of the grid
int numCols = -1;	//	width
int numBoxes = -1;	//	also the number of robots
int numDoors = -1;	//	The number of doors.

int numLiveThreads = 0;		//	the number of live robot threads

//	robot sleep time between moves (in microseconds)
const int MIN_SLEEP_TIME = 1000;
int robotSleepTime = 100000;

//	An array of C-string where you can store things you want displayed
//	in the state pane to display (for debugging purposes?)
//	Dont change the dimensions as this may break the front end
const int MAX_NUM_MESSAGES = 8;
const int MAX_LENGTH_MESSAGE = 32;
char** message;
time_t startTime;

//global variables 
vector<RobotInfo*> robots;
vector<BoxInfo> boxes;
vector<DoorInfo> doors;

/** The lock on writing to the output file.
*/
pthread_mutex_t outfileLock;

/** Locks to the various info structs.
*/
pthread_mutex_t robotLock, boxLock, doorLock;

/** Lock for the whole grid.
*/
pthread_mutex_t** gridLocks;

//==================================================================================
//	These are the functions that tie the simulation with the rendering.
//	Some parts are "don't touch."  Other parts need your intervention
//	to make sure that access to critical section is properly synchronized
//==================================================================================

void displayGridPane(void)
{
	//	This is OpenGL/glut magic.  Don't touch
	glutSetWindow(gSubwindow[GRID_PANE]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glTranslatef(0, GRID_PANE_HEIGHT, 0);
	glScalef(1.f, -1.f, 1.f);
	
	numLiveThreads = 0;
	pthread_mutex_lock(&robotLock);
	for(int i = 0; i < numBoxes; ++i){
		if(!robots[i]->end)
			++numLiveThreads;
	}
	pthread_mutex_unlock(&robotLock);
	
	for (int i=0; i<numBoxes; i++)
	{
		//	here I would test if the robot thread is still live
		//				   row		 column	   row	   column
		pthread_mutex_lock(&robotLock);
		pthread_mutex_lock(&boxLock);
		drawRobotAndBox(i, robots[i]->location.y, robots[i]->location.x,
						boxes[i].location.y, boxes[i].location.x, i);
		pthread_mutex_unlock(&boxLock);
		pthread_mutex_unlock(&robotLock);
	}

	for (int i=0; i<numDoors; i++)
	{
		//	here I would test if the robot thread is still alive
		//				row				column
		pthread_mutex_lock(&doorLock);
		drawDoor(i, doors[i].location.y, doors[i].location.x);
		pthread_mutex_unlock(&doorLock);
	}

	//	This call does nothing important. It only draws lines
	//	There is nothing to synchronize here
	drawGrid();

	//	This is OpenGL/glut magic.  Don't touch
	glutSwapBuffers();

	glutSetWindow(gMainWindow);
}

void displayStatePane(void)
{
	//	This is OpenGL/glut magic.  Don't touch
	glutSetWindow(gSubwindow[STATE_PANE]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//	Here I hard-code a few messages that I want to see displayed
	//	in my state pane.  The number of live robot threads will
	//	always get displayed.  No need to pass a message about it.
	time_t currentTime = time(NULL);
	double deltaT = difftime(currentTime, startTime);

	int numMessages = 3;
	sprintf(message[0], "We have %d doors", numDoors);
	sprintf(message[1], "I like cheese");
	sprintf(message[2], "Run time is %4.0f", deltaT);

	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//
	//	You *must* synchronize this call.
	//
	//---------------------------------------------------------

	

	drawState(numMessages, message);


	//	This is OpenGL/glut magic.  Don't touch
	glutSwapBuffers();

	glutSetWindow(gMainWindow);
}

//------------------------------------------------------------------------
//	You shouldn't have to touch this one.  Definitely if you don't
//	add the "producer" threads, and probably not even if you do.
//------------------------------------------------------------------------
void speedupRobots(void)
{
	//	decrease sleep time by 20%, but don't get too small
	int newSleepTime = (8 * robotSleepTime) / 10;

	if (newSleepTime > MIN_SLEEP_TIME)
	{
		robotSleepTime = newSleepTime;
	}
}

void slowdownRobots(void)
{
	//	increase sleep time by 20%
	robotSleepTime = (12 * robotSleepTime) / 10;
}




//------------------------------------------------------------------------
//	You shouldn't have to change anything in the main function besides
//	the initialization of numRows, numCos, numDoors, numBoxes.
//------------------------------------------------------------------------
int main(int argc, char** argv)
{
	//	We know that the arguments  of the program  are going
	//	to be the width (number of columns) and height (number of rows) of the
	//	grid, the number of boxes (and robots), and the number of doors.
	//	You are going to have to extract these.  For the time being,
	//	I hard code-some values
	numRows = stoi(argv[1]);
	numCols = stoi(argv[2]);
	numBoxes = stoi(argv[3]);
	numDoors = stoi(argv[4]);



	//	Even though we extracted the relevant information from the argument
	//	list, I still need to pass argc and argv to the front-end init
	//	function because that function passes them to glutInit, the required call
	//	to the initialization of the glut library.
	initializeFrontEnd(argc, argv, displayGridPane, displayStatePane);

	//	Now we can do application-level initialization
	initializeApplication();

	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();

	cleanupGridAndLists();

	//	This will probably never be executed (the exit point will be in one of the
	//	call back functions).
	return 0;
}

//---------------------------------------------------------------------
//	Free allocated resource before leaving (not absolutely needed, but
//	just nicer.  Also, if you crash there, you know something is wrong
//	in your code.
//---------------------------------------------------------------------
void cleanupGridAndLists(void)
{
	for (int i=0; i< numRows; i++)
		free(grid[i]);
	free(grid);
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		free(message[k]);
	free(message);
}


//==================================================================================
//
//	This is a part that you have to edit and add to.
//
//==================================================================================

#define FILE_NAME "robotSimulOut.txt"

/** An enum for the type of movement the robot can make.
*/
typedef enum MovementType{
	MOVE,
	PUSH
}MovementType;

/** This function sets up the application values needed to run, 
	while also creating all of the robots to push their blocks.
*/
void initializeApplication(void){

	//	Allocate the grid
	grid = (int**) malloc(numRows * sizeof(int*));
	for (int i=0; i<numRows; i++)
		grid[i] = (int*) malloc(numCols * sizeof(int));

	message = (char**) malloc(MAX_NUM_MESSAGES*sizeof(char*));
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		message[k] = (char*) malloc((MAX_LENGTH_MESSAGE+1)*sizeof(char));
	
	// Seed the pseudo-random generator
	startTime = time(NULL);
	srand((unsigned int) startTime);
	
	pthread_mutex_init(&outfileLock, NULL);
	pthread_mutex_init(&robotLock, NULL);
	pthread_mutex_init(&boxLock, NULL);
	pthread_mutex_init(&doorLock, NULL);
	
	gridLocks = (pthread_mutex_t**) calloc(numRows, sizeof(pthread_mutex_t*));
	for (int i=0; i<numRows; i++){
		gridLocks[i] = (pthread_mutex_t*) calloc(numCols, sizeof(pthread_mutex_t));
	}
		
	for(int i = 0; i < numRows; ++i){
		for(int j = 0; j < numCols; ++j){
			pthread_mutex_init(&gridLocks[i][j], NULL);
		}
	}
	
	// Create all the doors.
	pthread_mutex_lock(&doorLock);
	for(int i = 0; i < numDoors; ++i){
		doors.push_back(createDoor());
	}
	pthread_mutex_unlock(&doorLock);
	
	// Create all the boxes.
	pthread_mutex_lock(&boxLock);
	for(int i = 0; i < numBoxes; ++i){
		boxes.push_back(createBox());
	}
	pthread_mutex_unlock(&boxLock);
	
	// Create all the robots.
	pthread_mutex_lock(&robotLock);
	for(int i = 0; i < numBoxes; ++i){
		robots.push_back(createRobot(i));
	}
	pthread_mutex_unlock(&robotLock);
	
	
	writeFileHeader();
		
	// Start the robot threads.
	for(int i = 0; i < numBoxes; ++i){
		
		pthread_mutex_lock(&robotLock);
		int errCode = pthread_create (&(robots[i]->threadID), NULL,
                                 robotThreadFunc, robots[i]);
		pthread_mutex_unlock(&robotLock);
		
		if(errCode != 0){
			fprintf(stderr, "Thread %d could not be created.\n", i);
			exit(1);
		}
	}
	pthread_t deadlockPid;
	numLiveThreads = numBoxes;
	int errCode = pthread_create(&deadlockPid, NULL,
						deadlockThreadFunc, nullptr);
						
	if(errCode != 0){
		fprintf(stderr, "Deadlock thread could not be created.\n");
		exit(1);
	}
}

/** This function writes the top part of the output file.
*/
void writeFileHeader(){
	
	pthread_mutex_lock(&outfileLock);
	
	// Open the file.
	FILE* fp = fopen(FILE_NAME, "w");
	
	// Print the arguments we've recieved.
	fprintf(fp, "Rows: %d Cols: %d Boxes: %d Doors: %d\n\n", numRows, numCols, numBoxes, numDoors);
	
	// Print all the doors' initial locations.
	for(int i = 0; i < numDoors; ++i){
		fprintf(fp, "Door %d: X: %d Y: %d\n", i, doors[i].location.x, doors[i].location.y);
	}
	fprintf(fp, "\n");
	
	// Print all the boxes' initial locations.
	for(int i = 0; i < numBoxes; ++i){
		fprintf(fp, "Box %d: X: %d Y: %d\n", i, boxes[i].location.x, boxes[i].location.y);
	}
	fprintf(fp, "\n");
	
	// Print all the robots' initial locations, and which door they are connected to.
	for(int i = 0; i < numBoxes; ++i){
		fprintf(fp, "Robot %d: X: %d Y: %d Destination: %d\n", i, 
						robots[i]->location.x, robots[i]->location.y, i);
	}
	fprintf(fp, "\n");
	
	fclose(fp);
	
	pthread_mutex_unlock(&outfileLock);
}

/** This function writes a single type of movement to a file, be it a push or a normal move.
	@param info The robot's info struct.
	@param type The type of movement made.
*/
void writeToFile(RobotInfo* info, MovementType type){

	pthread_mutex_lock(&outfileLock);
	
	FILE* fp = fopen(FILE_NAME, "a");
	
	switch(type){
		case MOVE:
			fprintf(fp, "robot %d move %c\n", info->index, getDirectionChar(info->moveDirection));
			break;
			
		case PUSH:
			fprintf(fp, "robot %d push %c\n", info->index, getDirectionChar(info->pushDirection));
			break;
	}
	
	fclose(fp);
	
	pthread_mutex_unlock(&outfileLock);
}

/** A simple function to turn a directional move into the character ir represents.
	@param dir A direction of movement.
	@return The letter that refers to that direction, N, S, W, or E.
*/
char getDirectionChar(Direction dir){
	switch(dir){
		case NORTH:
			return 'N';
		case SOUTH:
			return 'S';
		case WEST:
			return 'W';
		case EAST:
			return 'E';
		// Should never reach this case
		default :
			return 'F';
	}
}

/** This function creates a random door in a unique location.
	@return The info struct for the new door.
*/
DoorInfo createDoor(){
	DoorInfo door;
	
	// Randomly generate a location.
	int doorX = GenerateRandomValue(0, numCols-1);
	int doorY = GenerateRandomValue(0, numRows-1);
	
	// Loop until it is a unique location.
	while(alreadyExist(doorX, doorY)){
		
		doorX = GenerateRandomValue(0, numCols);
		doorY = GenerateRandomValue(0, numRows);
	}
	
	door.location = {doorX, doorY};

	return door;
}

/** This function checks if a door, box, or robot already exists at the given location.
	@param x The x position of a location.
	@param y The y potition of a location.
	@return Whether or not that position is already occupied.
*/
bool alreadyExist(int x, int y){
	
	// Check all the doors.
	for(unsigned int i = 0; i < doors.size(); ++i){
		if(doors[i].location.x == x && doors[i].location.y == y)
			return true;
	}
	
	// Check all the boxes.
	for(unsigned int i = 0; i < boxes.size(); ++i){
		if(boxes[i].location.x == x && boxes[i].location.y == y)
			return true;
	}
	
	// Check all the robots.
	for(unsigned int i = 0; i < robots.size(); ++i){
		if(robots[i]->location.x == x && robots[i]->location.y == y)
			return true;
	}
	
	return false;
}

/** This function creates a random box in a unique location.
	@return The info struct for the new box.
*/
BoxInfo createBox(){

	BoxInfo box;
	
	// Randomly generate a location.
	int boxX = GenerateRandomValue(1, numCols-2);
	int boxY = GenerateRandomValue(1, numRows-2);
	
	// Loop until it is a unique location.
	while(alreadyExist(boxX, boxY)){
		boxX = GenerateRandomValue(1, numCols-2);
		boxY = GenerateRandomValue(1, numRows-2);
	}

	pthread_mutex_lock(gridLocks[boxY]+boxX);
	box.location = {boxX, boxY};
	
	return box;
}

/** This function creates a robot at a random, unique location, 
	and prepares the info struct to be used.
	@return The info struct for the robot, with the threadID still not set.
*/
RobotInfo* createRobot(int index){

	RobotInfo* robot = (RobotInfo*)calloc(1, sizeof(RobotInfo));

	// Randomly generate a location.
	int robotX = GenerateRandomValue(0, numCols-1);
	int robotY = GenerateRandomValue(0, numRows-1);
	
	// Loop until it is a unique location.
	while(alreadyExist(robotX, robotY)){
		robotX = GenerateRandomValue(0, numCols-1);
		robotY = GenerateRandomValue(0, numRows-1);
	}

	// Set all of the other robot values.
	robot->end = false;
	robot->canPush = false;
	robot->waitingForGrid = false;
	robot->index = index;
	
	pthread_mutex_lock(gridLocks[robotY]+robotX);
	robot->location = {robotX, robotY};
	
	robot->box = &boxes[index];
	robot->door = &doors[index];
	
	return robot;

}

/** Function to generate a random number between start and end INCLUSIVELY
	@param start The start of the set of random numbers.
	@param end The end of the set of random numbers.
	@return A random number between the given values.
*/
int GenerateRandomValue(int start, int end){

	return (rand() % (end-start+1)) + start;
}

/** The main function that runs the robot's code.
	@param info The struct of info for the robot to use.
*/
void* robotThreadFunc(void* voidInfo){

	// Get the info struct from the parameter.
	RobotInfo* info = (static_cast<RobotInfo*>(voidInfo));
	
	// Keep going until the box reaches the goal/door.
	while(!boxAtEnd(*info->box, *info->door)){
		
		// Choose which direction to push the box in next.
		pthread_mutex_lock(&robotLock);
		info->pushDirection = chooseNextPush(info);
		pthread_mutex_unlock(&robotLock);
		
		// Keep moving until the robot reaches the right spot to push the box.
		while(!ableToPush(info)){
			
			pthread_mutex_lock(&robotLock);
			info->canPush = false;
			info->moveDirection = chooseMovement(info);
			pthread_mutex_unlock(&robotLock);
			
			move(info);
		}
		pthread_mutex_lock(&robotLock);
		info->canPush = true;
		pthread_mutex_unlock(&robotLock);
		// Push the box.
		push(info);
	}
	
	// Make sure to update to tell the main program that the robot is done.
	pthread_mutex_lock(&robotLock);
	info->end = true;
	pthread_mutex_unlock(&robotLock);

	return nullptr;
}

/** A function that checks if the given box has reached the destination door.
	@param box The box to check.
	@param door The destination door.
	@return Whether or not the box has reached the door.
*/
bool boxAtEnd(BoxInfo box, DoorInfo door){
	
	// Comput the displacement between the box and the door.
	pthread_mutex_lock(&boxLock);
	pthread_mutex_lock(&doorLock);
	Point displacement = displacementBetweenPoints(box.location, door.location);
	pthread_mutex_unlock(&doorLock);
	pthread_mutex_unlock(&boxLock);
	
	// If that displacement is 0, this will evaluate to true.

	return (displacement.x == 0) && (displacement.y == 0);
}


/** Returns the x,y displacements required to move a box to a door

 * @param p1 x, y coordinates of the first point to be checked 
 * @param p2 x, y coordinates of the second point to be checked 
 * @return The X and Y displacement between the two points.

 */
Point displacementBetweenPoints(Point p1, Point p2)
{
	int xDisplacement = p1.x-p2.x;
	int yDisplacement = p1.y-p2.y;

	Point returnPoint = {xDisplacement, yDisplacement};

	return returnPoint;
}

/** A function that chooses the direction of the next box push.
	@param info The struct of robot info to use to check.
	@return The direction that the box needs to be pushed in next.
*/
Direction chooseNextPush(RobotInfo* info){

	pthread_mutex_lock(&boxLock);
	pthread_mutex_lock(&doorLock);
	Point distanceToMoveBox = displacementBetweenPoints(info->door->location, info->box->location);
	pthread_mutex_unlock(&doorLock);
	pthread_mutex_unlock(&boxLock);

	if(distanceToMoveBox.x != 0){
		return findXOrientation(distanceToMoveBox.x);
	}
	else{
		return findYOrientation(distanceToMoveBox.y);
	}
}

/** This function finds the push direction for a given X displacement.
	@param x The displacement on the X axis of the box from the door.
	@return The X axis direction of movement needed.
*/
Direction findXOrientation(int x)
{
	if(x > 0) return EAST;
	else if(x < 0) return WEST;

	//Should never reach this return
	return NUM_TRAVEL_DIRECTIONS;
}

/** This function finds the push direction for a given Y displacement.
	@param y The displacement on the y axis of the box from the door.
	@return The Y axis direction of movement needed.
*/
Direction findYOrientation(int y)
{
	if(y < 0) return SOUTH;
	else if(y > 0) return NORTH;

	//Should never reach this return
	return NUM_TRAVEL_DIRECTIONS;
}


/** This function chooses which direction the robot should move in 
	to reach the correct side of the box.
	@param info The robot info struct to use to choose movement.
	@return The next direction to move in.
*/
Direction chooseMovement(RobotInfo* info){
	
	//variables to store the location to to move to
	int destX, destY;
	
	pthread_mutex_lock(&boxLock);
	
	//Set the destination to move to for a push.
	switch(info->pushDirection){
		case NORTH: 
			destX = info->box->location.x;
			destY = info->box->location.y-1;
			break;
		case SOUTH:
			destX = info->box->location.x;
			destY = info->box->location.y+1;
			break;
		case WEST:
			destX = info->box->location.x+1;
			destY = info->box->location.y;
			break;
		case EAST:
			destX = info->box->location.x-1;
			destY = info->box->location.y;
			break;
		default :
			break;
	}
	
	Direction moveDir;
	
	// Find the movement direction based on the desired push direction.
	switch(info->pushDirection){
		case NORTH:
		case SOUTH:
			if(info->location.x == info->box->location.x && info->location.y != destY)
				moveDir = WEST;
			else{
				if(destY != info->location.y){
					if(info->location.y < destY)
						moveDir = NORTH;
					else
						moveDir = SOUTH;
				}
				else{
					if(info->location.x < destX)
						moveDir = EAST;
					else
						moveDir = WEST;
				}
			}
			break;
		case WEST:
		case EAST:
			if(info->location.y == info->box->location.y && info->location.x != destX)
				moveDir = NORTH;
			else{
				if(destX != info->location.x){
					if(info->location.x < destX)
						moveDir = EAST;
					else
						moveDir = WEST;
				}
				else{
					if(info->location.y < destY)
						moveDir = NORTH;
					else
						moveDir = SOUTH;
				}
			}
			break;
		default : 
			// Should never reach this line
			moveDir = NUM_TRAVEL_DIRECTIONS;
			break;
	}
	
	pthread_mutex_unlock(&boxLock);
	return moveDir;
}

/** A function that checks if the robot is in place to push the box.
	@param info The robot info struct used to check.
	@return Whether or not the robot is in place to push.
*/
bool ableToPush(RobotInfo* info){
	
	pthread_mutex_lock(&robotLock);
	
	switch(info->pushDirection)
	{
		case NORTH:
			//If the box is not on the top row and the robot is below it
			pthread_mutex_unlock(&robotLock);
			return info->box->location.y != numRows-1 && info->location.y+1 == info->box->location.y 
						&& info->location.x == info->box->location.x;
		case EAST:
			//If the box is not on the right side and the robot is to the left of it
			pthread_mutex_unlock(&robotLock);
			return info->box->location.x != numCols-1 && info->location.x+1 == info->box->location.x
						&& info->location.y == info->box->location.y;
		case SOUTH:
			//If the box is not on the top row and the robot is below it
			pthread_mutex_unlock(&robotLock);
			return info->box->location.y != 0 && info->location.y-1 == info->box->location.y 
						&& info->location.x == info->box->location.x;
		case WEST:
			//If the box is not on the left side and the robot is to the right of it
			pthread_mutex_unlock(&robotLock);
			return info->box->location.x != 0 && info->location.x-1 == info->box->location.x
						&& info->location.y == info->box->location.y;
		default :
			pthread_mutex_unlock(&robotLock);
			return false;
	}
}

/** This function moves the robot by one square.
	@param info The robot info struct to be moved.
*/
void move(RobotInfo* info){
	
	int destX, destY;
	
	pthread_mutex_lock(&robotLock);
	
	int oldX = info->location.x, oldY = info->location.y;
	
	switch (info->moveDirection){
		
		case NORTH:
			destX = info->location.x;
			destY = info->location.y+1;
			break;
		case SOUTH:
			destX = info->location.x;
			destY = info->location.y-1;
			break;
		case EAST:
			destX = info->location.x+1;
			destY = info->location.y;
			break;
		case WEST:
			destX = info->location.x-1;
			destY = info->location.y;
			break;
		default:
			break;
	}
	info->waitingForGrid = true;
	pthread_mutex_unlock(&robotLock);
			
	pthread_mutex_lock(gridLocks[destY]+destX);
	pthread_mutex_lock(&robotLock);
	
	info->waitingForGrid = false;
	
	info->location.x = destX;
	info->location.y = destY;
	
	pthread_mutex_unlock(&robotLock);
	pthread_mutex_unlock(gridLocks[oldY]+oldX);

	writeToFile(info, MOVE);
	
	usleep(robotSleepTime);
}

/** This function moves the robot, and pushes a box with it.
	@param info The robot info struct to be pushed, containing the box to be pushed as well.
*/
void push(RobotInfo* info){

	int destX, destY;

	pthread_mutex_lock(&robotLock);
	
	int oldX = info->location.x, oldY = info->location.y;
	
	pthread_mutex_lock(&boxLock);
	
	int oldBoxX = info->box->location.x, oldBoxY = info->box->location.y;
	
	switch (info->pushDirection){
		
		case NORTH:
			destX = info->box->location.x;
			destY = info->box->location.y+1;
			break;
		case SOUTH:
			destX = info->box->location.x;
			destY = info->box->location.y-1;
			break;
		case EAST:
			destX = info->box->location.x+1;
			destY = info->box->location.y;
			break;
		case WEST:
			destX = info->box->location.x-1;
			destY = info->box->location.y;
			break;
		default:
			break;
	}
		
	pthread_mutex_unlock(&boxLock);
	
	info->waitingForGrid = true;
	pthread_mutex_unlock(&robotLock);
	
	pthread_mutex_lock(gridLocks[destY]+destX);
	pthread_mutex_lock(&robotLock);
	
	info->waitingForGrid = false;
	
	pthread_mutex_lock(&boxLock);
	
	info->box->location.x = destX;
	info->box->location.y = destY;
	
	info->location.x = oldBoxX;
	info->location.y = oldBoxY;
	
	pthread_mutex_unlock(&boxLock);
	pthread_mutex_unlock(&robotLock);
	pthread_mutex_unlock(gridLocks[oldY]+oldX);
	
	
	writeToFile(info, PUSH);
	
	usleep(robotSleepTime);
}

void* deadlockThreadFunc(void* args){

	std::cout << "HERE" << std::endl;
	bool hasDeadlock = false;
	while(numLiveThreads > 0){
		std::cout << "HERE" << std::endl;

		
		pthread_mutex_lock(&robotLock);
		
		for(int i = 0; i < numBoxes-1; ++i){
			
			
			for(int j = i+1; j < numBoxes; ++j){
				if(deadlockExists(robots[i], robots[j])){
					hasDeadlock = true;
					break;
				}
			}
			
			if(hasDeadlock){
				killRobot(robots[i]);
			}
		}
		
		pthread_mutex_unlock(&robotLock);
		
		usleep(robotSleepTime);
		
	}
	
	return nullptr;
}

bool deadlockExists(RobotInfo* r1, RobotInfo* r2){
	
	Point r1dest = getDest(r1), r2dest = getDest(r2);

	// Check r1 dest agasint r2 location
	if(r2->location.x == r1dest.x && r2->location.y == r1dest.y && r2->waitingForGrid)
		return true;
		
	if(r2->canPush)
		if(r2->box->location.x == r1dest.x && r2->box->location.y == r1dest.y && r2->waitingForGrid)
			return true;
	
	// Check r2 dest agasint r1 location
	if(r1->location.x == r2dest.x && r1->location.y == r2dest.y && r1->waitingForGrid)
		return true;
		
	if(r1->canPush)
		if(r1->box->location.x == r2dest.x && r1->box->location.y == r2dest.y && r1->waitingForGrid)
			return true;
	
	return false;
	
}

Point getDest(RobotInfo* info){
	int destX, destY;
	if(info->canPush){
		switch (info->pushDirection){
		
			case NORTH:
				destX = info->box->location.x;
				destY = info->box->location.y+1;
				break;
			case SOUTH:
				destX = info->box->location.x;
				destY = info->box->location.y-1;
				break;
			case EAST:
				destX = info->box->location.x+1;
				destY = info->box->location.y;
				break;
			case WEST:
				destX = info->box->location.x-1;
				destY = info->box->location.y;
				break;
			default:
				break;
		}
	}
	else{
		switch (info->moveDirection){
			
			case NORTH:
				destX = info->location.x;
				destY = info->location.y+1;
				break;
			case SOUTH:
				destX = info->location.x;
				destY = info->location.y-1;
				break;
			case EAST:
				destX = info->location.x+1;
				destY = info->location.y;
				break;
			case WEST:
				destX = info->location.x-1;
				destY = info->location.y;
				break;
			default:
				break;
		}
	}
	
	Point p = {destX, destY};
	return p;
}

void killRobot(RobotInfo* info){

	printf("Killing\n");
	
	info->end = true;
	
	info->location = {-1, -1};
	pthread_mutex_unlock(gridLocks[info->location.y]+info->location.x);
	
	info->box->location = {-1, -1};
	pthread_mutex_unlock(gridLocks[info->box->location.y]+info->box->location.x);
	
	pthread_cancel(info->threadID);
	
}
