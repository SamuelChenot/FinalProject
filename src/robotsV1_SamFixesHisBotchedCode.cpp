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
#include <sstream>

#include <cstdlib>

#include <time.h>

//
#include "gl_frontEnd.h"

using namespace std;

// Our structs: ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

typedef struct Point {
	int x;
	int y;
} Point;

typedef struct BoxInfo{
    Point location;
} BoxInfo;

typedef struct DoorInfo{
    Point location;
} DoorInfo;

typedef struct RobotInfo{

	pthread_t	threadID;
	unsigned int index;
	
	Direction moveDirection;
    Direction pushDirection;
    bool end;
	
	Point location;
	
    BoxInfo box;
    DoorInfo door;
    
} RobotInfo;

//==================================================================================
//	Function prototypes
//==================================================================================
void displayGridPane(void);
void displayStatePane(void);
void initializeApplication(void);
void cleanupGridAndLists(void);

AssignmentInfo MakeAssignment();

/** 
 * @param number however many cols or rows 
 * @return 
**/
int GenerateRandomValue(int number);

void MoveRobot(int index, Direction direction);

bool CheckProximityOfRobotToBox(int index);

Direction DetermineDirection(int index);








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

void initializeApplication(void){

	//	Allocate the grid
	grid = (int**) malloc(numRows * sizeof(int*));
	for (int i=0; i<numRows; i++)
		grid[i] = (int*) malloc(numCols * sizeof(int));
	
	message = (char**) malloc(MAX_NUM_MESSAGES*sizeof(char*));
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		message[k] = (char*) malloc((MAX_LENGTH_MESSAGE+1)*sizeof(char));
		
	
	//	seed the pseudo-random generator
	startTime = time(NULL);
	srand((unsigned int) startTime);

	//	normally, here I would initialize the location of my doors, boxes,
	//	and robots, and create threads (not necessarily in that order).
	//	For the handout I have nothing to do.
    
    //will put this inside of the thread function later

	//We get the value numBoxes from the command line
	//loop for numBoxes times in order to generate new coords for each robot, box, and door, and door assignments
	//I opted to store the values in global vectors as they can be accessed when needed globally and in order to draw the initial grid
	//that will eventually include the doors, boxes, ect... the values needed to be stored globally to be drawn on the front end display.

	//TODO : need to make it so that none of the values here are the same as each other
	
	string line = to_string(numRows) + " " + to_string(numCols) + " " + to_string(numBoxes) + " " + to_string(numDoors);

	WriteToFile(line);

	int numRobots = numBoxes;
	
	BuildDoors();
	BuildBoxes();
	BuildRobots(numRobots);
	
}

DoorInfo createDoor(){
	DoorInfo door;
	
	int doorX, doorY;
	
	doorX = GenerateRandomValue(numCols);
	doorY = GenerateRandomValue(numRows);
	
	door.location = {doorX, doorY};
	
	return door;
}

BoxInfo createBox(){
	
	BoxInfo box;

	int boxX, boxY;

	boxX = GenerateRandomValue(numCols);
	boxY = GenerateRandomValue(numRows);

	box.location = {boxX, boxY};

	// TODO create boxes
	
	return box;
}

RobotInfo createRobot(int index){

	RobotInfo robot;

	int robotX, robotY;

	robotX = GenerateRandomValue(numCols);
	robotY = GenerateRandomValue(numRows);

	robot.end = false;
	robot.index = index;
	robot.location = {robotX, robotY};
	
	// TODO create the robot
	
	return robot;
}

/** Function to generate a random number between start and end INCLUSIVELY
*/
int GenerateRandomValue(int start, int end){

    return (rand() % (end-start+1)) + start;
}

void* robotThreadFunc(RobotInfo info){
	
	while(!boxAtEnd()){
		
		info.pushDirection = chooseNextPush(info);
		
		while(!ableToPush(info)){
			info.moveDirection = chooseMovement(info);
			
			move(info);
		}
		
		push(info);
	}
	
	info.done = true;
	
	return nullptr;
}

bool boxAtEnd(BoxInfo box, DoorInfo door){
	Point displacement = displacementBetweenPoints(box.location, door.location);
	
	return (displacement.x == 0) && (displacement.y == 0);
}


/** Returns the x,y displacements required to move a box to a door
 * @param : x, y coordinates of box to be checked 
 * @param : x, y coordinates of door to be checked 
 */
Point displacementBetweenPoints(Point p1, Point p2)
{
	int xDisplacement = p1.x-p2.x;
	int yDisplacement = p1.y-p2.y;

	Point returnPoint = {xDisplacement, yDisplacement};

	return returnPoint;
}

Direction chooseNextPush(RobotInfo info){

	Point distanceToMoveBox = displacementBetweenPoints(info.door.location, info.box.location);

	if(distanceToMoveBox.x != 0){
		return findXOrientation(distanceToMoveBox.x);
	}
	else{
		return findYOrientation(distanceToMoveBox.y);
	}
}

Direction findXOrientation(int x)
{
	if(x > 0) return EAST;
	else if(x < 0) return WEST;
	
	return;
}

Direction findYOrientation(int y)
{
	if(y > 0) return SOUTH;
	else if(y < 0) return NORTH;
	
	return;
}

Direction chooseMovement(RobotInfo info){	

	// TODO choose movement correctly.
	if(info.pushDirection = EAST)
		;// move robot to box.x - 1 (Note does not account for boxes against world edge)
	if(info.pushDirection = WEST)
		;// move robot to box.x + 1 (Note does not account for boxes against world edge)
	// move robot to box.y
	// set robot Direction = orientationX
	// push box distanceToMoveBox.x times
	
	
	if(info.pushDirection = SOUTH)
		;// move robot to box.y + 1 (Note does not account for boxes against world edge)
	if(info.pushDirection = NORTH)
		;// move robot to box.y - 1 (Note does not account for boxes against world edge)
	// move robot to box.x
	// set robot Direction = orientationY
	// push bot distanceToMoveBox.y times 		
}

bool ableToPush(RobotInfo info){
	// TODO check if in position to push box.
}

void move(RobotInfo info){
	// TODO move
}

void push(RobotInfo info){
	// TODO push
}

void BuildRobots(int numRobots)
{
	RobotInfo previousRobot;
	for (int i = 0; i < numRobots; i++){
		RobotInfo currentRobot;
		currentRobot = createRobot(i);
		bool safe = false;
		if(currentRobot.location.x != previousRobot.location.x && currentRobot.location.y != previousRobot.location.y)
		{
			while (!safe)
			{
				for(int i = 0; i < numDoors; i++)
				{
					if(doors[i].location.x != currentRobot.location.x && doors[i].location.y != currentRobot.location.y &&
					doors[i].location.x != previousRobot.location.x 
					&& doors[i].location.y != previousRobot.location.y)
					{
						safe = true;
					}
					else
					{
						safe = false;
					}	
				}
				for(int i = 0; i < numBoxes; i++)
				{
					if(boxes[i].location.x != currentRobot.location.x && boxes[i].location.y != currentRobot.location.y &&
					boxes[i].location.x != previousRobot.location.x 
					&& boxes[i].location.y != previousRobot.location.y)
					{
						safe = true;
					}
					else
					{
						safe = false;
					}	
				}
			}
			if(safe)
			{
				robots.push_back(currentRobot);
			}
		}
		else
		{
			while(currentRobot.location.x != previousRobot.location.x && currentRobot.location.y != previousRobot.location.y)
			{
				currentRobot = createRobot(i);
				while (!safe)
				{
					for(int i = 0; i < numDoors; i++)
					{
						if(doors[i].location.x != currentRobot.location.x && doors[i].location.y != currentRobot.location.y &&
						doors[i].location.x != previousRobot.location.x 
						&& doors[i].location.y != previousRobot.location.y)
						{
							safe = true;
						}
						else
						{
							safe = true;
						}
					}
					for(int i = 0; i < numBoxes; i++)
					{
						if(boxes[i].location.x != currentRobot.location.x && boxes[i].location.y != currentRobot.location.y &&
						boxes[i].location.x != previousRobot.location.x 
						&& boxes[i].location.y != previousRobot.location.y)
						{
							safe = true;
						}
						else
						{
							safe = false;
						}	
					}
				}
				if(safe)
				{
					robots.push_back(currentRobot);
				}
			}
		}
		previousRobot = currentRobot;
		
		robotThreadFunc(robots[i]);
    }
}

void BuildBoxes()
{
	BoxInfo previousBox;
	for (int i = 0; i < numBoxes; i++){
		BoxInfo currentBox;
		currentBox = createBox();
		bool safe = false;
		if(currentBox.location.x != previousBox.location.x && currentBox.location.y != previousBox.location.y)
		{
			while (!safe)
			{
				for(int i = 0; i < numDoors; i++)
				{
					if(doors[i].location.x != currentBox.location.x && doors[i].location.y != currentBox.location.y &&
					doors[i].location.x != previousBox.location.x 
					&& doors[i].location.y != previousBox.location.y)
					{
						safe = true;
					}
					else
					{
						safe = false;
					}	
				}
			}
			if(safe)
			{
				boxes.push_back(currentBox);
			}
		}
		else
		{
			while(currentBox.location.x != previousBox.location.x && currentBox.location.y != previousBox.location.y)
			{
				currentBox = createBox();
				while (!safe)
				{
					for(int i = 0; i < numDoors; i++)
					{
						if(doors[i].location.x != currentBox.location.x && doors[i].location.y != currentBox.location.y &&
						doors[i].location.x != previousBox.location.x 
						&& doors[i].location.y != previousBox.location.y)
						{
							safe = true;
						}
						else
						{
							safe = true;
						}
					}
				}
				if(safe)
				{
					boxes.push_back(currentBox);
				}
			}
		}
		previousBox = currentBox;
	}
}

void BuildDoors()
{
	DoorInfo previousDoor;
	for(int i = 0; i < numDoors; ++i){
		DoorInfo currentDoor = createDoor();
		if(currentDoor.location.x != previousDoor.location.x && currentDoor.location.y != previousDoor.location.y)
		{
			doors.push_back(currentDoor);
		}
		else
		{
			while(currentDoor.location.x == previousDoor.location.x && currentDoor.location.y == previousDoor.location.y)
			{
				currentDoor = createDoor();
			}
			doors.push_back(currentDoor);
		}
		previousDoor = currentDoor;	
	}
}

void WriteToFile(string content)
{
	string filename = "log.txt";

	ofstream file(filename, fstream::app);

	if(file.is_open())
	{
		file << content << endl;
	}
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

//AssignmentInfo MakeAssignment()
//{
//  AssignmentInfo assignmentInfo;
//
//  assignmentInfo.assignment = GenerateRandomValue(3);
//
//    assignmentInfo.box.col = GenerateRandomValue(numCols);
//    assignmentInfo.box.row = GenerateRandomValue(numRows);
//
//    assignmentInfo.robotCol = GenerateRandomValue(numCols);
//    assignmentInfo.robotRow = GenerateRandomValue(numRows);
//    
//    assignmentInfo.door.col = GenerateRandomValue(numCols);
//    assignmentInfo.door.row = GenerateRandomValue(numRows);
//
//   return assignmentInfo;
//}


