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

typedef struct DisplacementDirections
{
	int north;
	int south;
	int east;
	int west;

} DisplacementDirections;

//==================================================================================
//	Function prototypes
//==================================================================================
void displayGridPane(void);
void displayStatePane(void);
void initializeApplication(void);
void cleanupGridAndLists(void);


int GenerateRandomValue(int number);

DisplacementDirections FindDisplacementValues(int rowOne, int colOne, int rowTwo, int colTwo);


int FindNorthDisplacement(int rowOne, int rowTwo);

int FindSouthDisplacement(int rowOne, int rowTwo);

int FindEastDisplacement(int colOne, int colTwo);

int FindWestDisplacement(int colOne, int colTwo);

void MoveRobotToBox(int index, int direction, int loc);

bool CheckProximity(int index, int direction, int loc);

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

vector<int> robotRows;
vector<int> robotCols;

vector<int> doorRows;
vector<int> doorCols;

vector<int> boxRows;
vector<int> boxCols;

vector<int> doorAssignments;

vector<bool> activeStatus;
vector<bool> unaquiredBox;

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
		drawRobotAndBox(i, robotRows[i], robotCols[i], boxRows[i], boxCols[i], doorAssignments[i]);
	}

	for (int i=0; i<numDoors; i++)
	{
		//	here I would test if the robot thread is still alive
		//				row				column	
		drawDoor(i, doorRows[i], doorCols[i]);
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


void initializeApplication(void)
{
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
    for(int i = 0; i < numBoxes; i++)
    {   
        doorRows.push_back(GenerateRandomValue(numRows));
        robotRows.push_back(GenerateRandomValue(numRows));
        boxRows.push_back(GenerateRandomValue(numRows));

        doorCols.push_back(GenerateRandomValue(numCols));
        robotCols.push_back(GenerateRandomValue(numCols));
        boxCols.push_back(GenerateRandomValue(numCols));

		//will likely be used as a means to determine if the box has reached a door
		activeStatus.push_back(true);
		//will be used to determine if we have reached the boxes location and can continue to the door
		unaquiredBox.push_back(true);

        doorAssignments.push_back(i);

		//going to draw each of the boxes individually in this loop as it will work the same as with a seperate loop
		drawRobotAndBox(i, robotRows[i], robotCols[i], boxRows[i], boxCols[i], doorAssignments[i]);
		drawDoor(i, doorRows[i], doorCols[i]);
    }

	
	//Loop to get robot to box
	for(int index = 0; index < numBoxes; index++)
	{

		std::cout << "Rows Robot : " << robotRows[index] << " Cols Robot: " << robotCols[index] << std::endl;
		std::cout << "Rows Box : " << boxRows[index] << " Cols Box: " << boxCols[index] << std::endl;

		//row1 row2, col1 col2
		//this is dependent on what you need to find the displacement to, possibly include a vector that stores
		//if the robot has aquired its box or not.
		DisplacementDirections displacements = FindDisplacementValues(robotRows[index], boxRows[index], robotCols[index], boxCols[index]);

		std::cout << "NORTH: " << displacements.north << std::endl;
		std::cout << "SOUTH: " << displacements.south << std::endl;
		std::cout << "EAST: " << displacements.east << std::endl;
		std::cout << "WEST: " << displacements.west << std::endl;


		vector<int> directions = {displacements.north, displacements.south, displacements.east, displacements.west};
		int direction;
		int loc;
		direction = directions[0];
		for(unsigned int i = 0; i < directions.size(); i++)
		{
			if(direction < directions[i])
			{
				direction = directions[i];
				loc = i;
			}
		}

		std::cout << loc << " : " << direction << std::endl;



		


		MoveRobotToBox(index, direction, loc);



		//displacement to the box is now found we should only need to move the robot to the box now
		

		drawRobotAndBox(index, robotRows[index], robotCols[index], boxRows[index], boxCols[index], doorAssignments[index]);
		drawDoor(index, doorRows[index], doorCols[index]);

		displayGridPane();
    	displayStatePane();

		std::cout << "Rows Robot : " << robotRows[index] << " Cols Robot: " << robotCols[index] << std::endl;
		std::cout << "Rows Box : " << boxRows[index] << " Cols Box: " << boxCols[index] << std::endl;
	}   
}

int GenerateRandomValue(int number)
{
    return rand() % number;
}

//this will be where we get our direction from
//this function will return a struct with 4 directional values
//use this function to calculate the movements that our box will need to make
DisplacementDirections FindDisplacementValues(int rowBox, int colBox, int rowDoor, int colDoor)
{
	DisplacementDirections displacementDirections;

	displacementDirections.north = FindNorthDisplacement(rowBox, rowDoor);
	displacementDirections.south = FindSouthDisplacement(rowBox, rowDoor);
	displacementDirections.east = FindEastDisplacement(colBox, colDoor);
	displacementDirections.west = FindWestDisplacement(colBox, colDoor);

	return displacementDirections;
}

int FindNorthDisplacement(int rowOne, int rowTwo)
{	
	int displacement = 0;
	if(rowOne > rowTwo)
	{
		displacement = 0;
	}
	else if(rowTwo > 0)
	{	
		displacement = rowTwo - rowOne;
	}
	return displacement;
	
}

int FindSouthDisplacement(int rowOne, int rowTwo)
{
	int displacement = 0;
	if(rowOne > rowTwo)
	{
		if(rowOne > 0)
		{	
			displacement = rowOne - rowTwo;
		}
	}
	return displacement;
}

int FindEastDisplacement(int colOne, int colTwo)
{
	int displacement = 0;
	if(colTwo > colOne)
	{
		if(colTwo > 0)
		{	
			displacement = colTwo - colOne;
		}
	}
	return displacement;
}

int FindWestDisplacement(int colOne, int colTwo)
{
	int displacement = 0;
	if(colOne > colTwo)
	{
		if(colOne > 0)
		{
			displacement = colOne - colTwo;
		}
	}
	return displacement;
}

//TODO : I think that using a direction variable we can we the robot to move to the correct position for each directional movement
//		 so if the direction is NORTH we can have the robot position south and then push NORTH.
//		 Same goes for any other direction, we just put the robot in the opposite position that we need to push
void MoveRobotToBox(int index, int direction, int loc)
{
	if(robotRows[index] > boxRows[index])
	{
		robotRows[index]--;
	}
	else if(robotRows[index] < boxRows[index])
	{
		robotRows[index]++;
	}
	if(robotCols[index] > boxCols[index])
	{
		robotCols[index]--;
	}
	else if(robotCols[index] < boxCols[index])
	{
		robotCols[index]++;
	}
	//update the grid to show the newly moved robot
	displayGridPane();
    displayStatePane();


	//recursive calls that will keep moving the robot till we get to the box
	//TODO : Currently this function will run until we are directly on top of the box, we need to make it so it is next to the box.
	//TODO : Make a function that checks if the robot is NEXT to the box and that will be our new stopping point
	if(!CheckProximity(index, direction, loc))
	{
		MoveRobotToBox(index, direction, loc);
	}
}

bool CheckProximity(int index, int direction, int loc)
{
	if(robotRows[index] == boxRows[index]+1 && robotCols[index] == boxCols[index])
	{
		return true;
	}
	else if(robotRows[index] == boxRows[index]-1 && robotCols[index] == boxCols[index])
	{
		return true;
	}
	else if(robotRows[index] == boxRows[index] && robotCols[index] == boxCols[index]+1)
	{
		return true;
	}
	else if(robotRows[index] == boxRows[index] && robotCols[index] == boxCols[index]-1)
	{
		return true;
	}
	else
	{
		return false;
	}
}