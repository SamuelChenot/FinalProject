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

#include <cstdlib>

#include <time.h>

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
    
    /** Whether or not the robot has finished moving the box to the door. */
    bool end;
	
	/** The location on the grid of the robot. */
	Point location;
	
	/** The box that the robot needs to push. */
    BoxInfo box;
    
    /** The door that the box needs to be pushed to. */
    DoorInfo door;
    
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
RobotInfo createRobot(int id);
int GenerateRandomValue(int start, int end);
void* robotThreadFunc(RobotInfo info);
bool boxAtEnd(BoxInfo box, DoorInfo door);
Point displacementBetweenPoints(Point p1, Point p2);
Direction chooseNextPush(RobotInfo info);
Direction findXOrientation(int x);
Direction findYOrientation(int y);
Direction chooseMovement(RobotInfo info);
bool ableToPush(RobotInfo info);
void move(RobotInfo info);
void push(RobotInfo info);


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

vector<RobotInfo> robots;
vector<BoxInfo> boxes;
vector<DoorInfo> doors;

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
	
    //can touch

    

	for (int i=0; i<numBoxes; i++)
	{
		//	here I would test if the robot thread is still live
		//				   row		 column	   row	   column
		drawRobotAndBox(i, robots[i].location.y, robots[i].location.x, 
						boxes[i].location.y, boxes[i].location.x, i);
	}

	for (int i=0; i<numDoors; i++)
	{
		//	here I would test if the robot thread is still alive
		//				row				column	
		drawDoor(i, doors[i].location.y, doors[i].location.x);
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
	
	// New counter for the sake of sanity.
	int numRobots = numBoxes;
	
	// Create all the doors.
	for(int i = 0; i < numDoors; ++i){
		doors.push_back(createDoor());
	}
	
	// Create all the boxes.
	for (int i = 0; i < numBoxes; i++){
		boxes.push_back(createBox());
	}
	
	// Create all of the robots.
	for (int i = 0; i < numRobots; i++){
	
		robots.push_back(createRobot(i));
		
		robotThreadFunc(robots[i]);
    }
}

/** This function creates a random door in a unique location.
	@return The info struct for the new door.
*/
DoorInfo createDoor(){
	DoorInfo door;
	
	int doorX, doorY;
	
	// TODO random door location.
	
	door.location = {doorX, doorY};
	
	return door;
}

/** This function creates a random box in a unique location.
	@return The info struct for the new box.
*/
BoxInfo createBox(){
	
	// TODO create boxes
	
	return NULL;
}

/** This function creates a robot at a random, unique location, 
	and prepares the info struct to be used.
	@return The info struct for the robot, with the threadID still not set.
*/
RobotInfo createRobot(int id){
	
	// TODO create the robot
	
	return NULL;
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
void* robotThreadFunc(RobotInfo info){
	
	// Keep going until the box reaches the goal/door.
	while(!boxAtEnd()){
		
		// Choose which direction to push the box in next.
		info.pushDirection = chooseNextPush(info);
		
		// Keep moving until the robot reaches the right spot to push the box.
		while(!ableToPush(info)){
			
			info.moveDirection = chooseMovement(info);
			move(info);
		}
		
		// Push the box.
		push(info);
	}
	
	// Make sure to update to tell the main program that the robot is done.
	info.done = true;
	
	return nullptr;
}

/** A function that checks if the given box has reached the destination door.
	@param box The box to check.
	@param door The destination door.
	@return Whether or not the box has reached the door.
*/
bool boxAtEnd(BoxInfo box, DoorInfo door){
	// Comput the displacement between the box and the door.
	Point displacement = displacementBetweenPoints(box.location, door.location);
	
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
	int xdisplacement = p1.x-p2.x;
	int ydisplacement = p1.y-p2.y;

	Point returnPoint = {xDisplacement, yDisplacement};

	return returnPoint;
}

/** A function that chooses the direction of the next box push.
	@param info The struct of robot info to use to check.
	@return The direction that the box needs to be pushed in next.
*/
Direction chooseNextPush(RobotInfo info){

	Point distanceToMoveBox = displacementBetweenPoints(info.door.location, info.box.location);

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
	
	return;
}

/** This function finds the push direction for a given Y displacement.
	@param y The displacement on the y axis of the box from the door.
	@return The Y axis direction of movement needed.
*/
Direction findYOrientation(int y)
{
	if(y > 0) return SOUTH;
	else if(y < 0) return NORTH;
	
	return;
}

/** This function chooses which direction the robot should move in 
	to reach the correct side of the box.
	@param info The robot info struct to use to choose movement.
	@return The next direction to move in.
*/
Direction chooseMovement(RobotInfo info){	

	// TODO choose movement correctly.
	if(orientationX = EAST)
		;// move robot to box.x - 1 (Note does not account for boxes against world edge)
	if(orientationX = WEST)
		;// move robot to box.x + 1 (Note does not account for boxes against world edge)
	// move robot to box.y
	// set robot Direction = orientationX
	// push box distanceToMoveBox.x times
	
	
	if(orientationY = SOUTH)
		;// move robot to box.y + 1 (Note does not account for boxes against world edge)
	if(orientationY = NORTH)
		;// move robot to box.y - 1 (Note does not account for boxes against world edge)
	// move robot to box.x
	// set robot Direction = orientationY
	// push bot distanceToMoveBox.y times 		
}

/** A function that checks if the robot is in place to push the box.
	@param info The robot info struct used to check.
	@return Whether or not the robot is in place to push.
*/
bool ableToPush(RobotInfo info){
	// TODO check if in position to push box.
}

/** This function moves the robot by one square.
	@param info The robot info struct to be moved.
*/
void move(RobotInfo info){
	// TODO move
}

/** This function moves the robot, and pushes a box with it.
	@param info The robot info struct to be pushed, containing the box to be pushed as well.
*/
void push(RobotInfo info){
	// TODO push
}

//_______________________________________________________________________________________


void MoveRobot(int index, Direction direction)
{

	switch (direction)
	{
	case NORTH:
		assignmentInfos[index].robotRow--;
		break;
	case SOUTH:
		assignmentInfos[index].robotRow++;
		break;
	case EAST:
		assignmentInfos[index].robotCol--;
		break;
	case WEST:
		assignmentInfos[index].robotCol++;
		break;
	default:
		break;
	}
	
	//update the grid to show the newly moved robot
	displayGridPane();
    displayStatePane();


	//recursive calls that will keep moving the robot till we get to the box
	//TODO : Currently this function will run until we are directly on top of the box, we need to make it so it is next to the box.
	//TODO : Make a function that checks if the robot is NEXT to the box and that will be our new stopping point
	if(!CheckProximityOfRobotToBox(index))
	{
		//if for whatever reason the robot is not directly next to the box, we will call this function again until it is.
		MoveRobot(index, direction);
	}
}

//function checks if the robot is one grid space NORTH, SOUTH, EAST, or WEST of the box
bool CheckProximityOfRobotToBox(int index)
{
	if(assignmentInfos[index].robotRow == assignmentInfos[index].box.row+1 && assignmentInfos[index].robotCol == assignmentInfos[index].box.col)
	{
		return true;
	}
	else if(assignmentInfos[index].robotRow == assignmentInfos[index].box.row-1 && assignmentInfos[index].robotCol == assignmentInfos[index].box.col)
	{
		return true;
	}
	else if(assignmentInfos[index].robotRow == assignmentInfos[index].box.row && assignmentInfos[index].robotCol == assignmentInfos[index].box.col+1)
	{
		return true;
	}
	else if(assignmentInfos[index].robotRow == assignmentInfos[index].box.row && assignmentInfos[index].robotCol == assignmentInfos[index].box.col-1)
	{
		return true;
	}
	else
	{
		return false;
	}
}

AssignmentInfo MakeAssignment()
{
    AssignmentInfo assignmentInfo;

    assignmentInfo.assignment = GenerateRandomValue(3);

    assignmentInfo.box.col = GenerateRandomValue(numCols);
    assignmentInfo.box.row = GenerateRandomValue(numRows);

    assignmentInfo.robotCol = GenerateRandomValue(numCols);
    assignmentInfo.robotRow = GenerateRandomValue(numRows);
    
    assignmentInfo.door.col = GenerateRandomValue(numCols);
    assignmentInfo.door.row = GenerateRandomValue(numRows);

    return assignmentInfo;
}


