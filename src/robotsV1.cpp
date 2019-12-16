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

//==================================================================================
//	Function prototypes
//==================================================================================
void displayGridPane(void);
void displayStatePane(void);
void initializeApplication(void);
void cleanupGridAndLists(void);
int GenerateRandomValue(int number);

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


    for(int i = 0; i < numBoxes; i++)
    {   
        doorRows.push_back(GenerateRandomValue(numRows));
        robotRows.push_back(GenerateRandomValue(numRows));
        boxRows.push_back(GenerateRandomValue(numRows));

        doorCols.push_back(GenerateRandomValue(numCols));
        robotCols.push_back(GenerateRandomValue(numCols));
        boxCols.push_back(GenerateRandomValue(numCols));

        doorAssignments.push_back(i);
    }

    for (int i=0; i<numBoxes; i++)
	{
		//	here I would test if the robot thread is still live
		//						row				column			row			column
		drawRobotAndBox(i, robotRows[i], robotCols[i], boxRows[i], boxCols[i], doorAssignments[i]);
	}

	for (int i=0; i<numDoors; i++)
	{
		//	here I would test if the robot thread is still alive
		//			row	     column	
		drawDoor(i, doorRows[i], doorCols[i]);
	}


    displayGridPane();
    displayStatePane();
    
}

int GenerateRandomValue(int number)
{
    return rand() % number;
}


