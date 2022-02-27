/********************************************************************
Class: CS 344 - Operating Systems
Author: KC
Date: May 1 2018
Description: Uses C strings, driectory traversal, along with kernel-level threads and mutexes to run
a text-based adventure game. A player explores rooms generated by buildrooms.c
until they reach the final escape room.
Input: Command line - room name or "time"
Output: Command line statements about location, connections, and time
End of game - congratulation with number of steps and path to victory
**********************************************************************/
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>

#define MAX_CXNS 6
#define NUM_ROOMS 7

struct room_adventure {
	char name[12];
	char roomType[12];
	char* cxns[MAX_CXNS];
	int numCxns;
};

int findDir(char*);
void readFiles(char*, struct room_adventure* []);
struct room_adventure* findStart(struct room_adventure* []);
int inputMatch(char*, struct room_adventure*);
void* writeTime(void*);
void readTime();
void* runGame(void*);

// Create threadA, threadB, and lock mutex
pthread_t threadA;
pthread_t threadB;
pthread_mutex_t lock;

/*****************************************************/
int main () {

	// Get the name of the most recently created directory with the room files
	char* dirName = malloc(40);
	memset(dirName, '\0', 40);
	if (findDir(dirName) != 0) {
		printf("Error calling findDir()\n");
		exit(1);
	}

	// Create array of room_adventure structs. Initialize connections to NULL
	struct room_adventure* rooms[NUM_ROOMS];
	for (int i=0; i<NUM_ROOMS; i++) {
		rooms[i] = malloc(sizeof(struct room_adventure));
		// Set numCxns to 0
		rooms[i]->numCxns = 0;
		// Initialize all cxn pointers
		for (int j=0; j<MAX_CXNS; j++) {
			rooms[i]->cxns[j] = malloc(sizeof(char)*100);
		}
	}

	// Read file data into rooms array
	readFiles(dirName, rooms);

	// Start threadA which runs the game, initialize mutex
	printf("\n*************************************************************\n");
	printf("******************  STARTING ESCAPE ROOM  *******************\n");
	printf("*************************************************************\n");
	printf("\n... YOU FIND YOURSELF TRAPPED ON A EVIL GENIUS'S ESTATE.\n");
	printf("... FIND YOUR WAY OUT OR YOU'RE TOAST!!!\n");
	printf("... ENTER 'time' AT ANY POINT IF YOU WANT THE CURRENT TIME\n\n");
	pthread_create(&threadA, NULL, runGame, (void *) &rooms);
	pthread_mutex_init(&lock, NULL);
	pthread_join(threadA, NULL);
	pthread_mutex_destroy(&lock);

	// Free memory from rooms[] and cxns[] in each room
	for (int i=0; i<NUM_ROOMS; i++) {
		for (int j=0; j<MAX_CXNS; j++) {
			free(rooms[i]->cxns[j]);
			rooms[i]->cxns[j] = NULL;
		}

		free(rooms[i]);
		rooms[i] = NULL;
	}

	// Free heap memory
	free(dirName);
	dirName = NULL;

	// Exit with status code 0
	printf("\n*************************************************************\n");
	printf("*******************  CLOSING ESCAPE ROOM  *******************\n");
	printf("*************************************************************\n\n");
	exit(0);
}


//*****************************************************/
// Find the most recent rooms directory in the current directory
int findDir(char* dirName) {
	// Find and open directory with most recent time stamp
	// Used much of the code from article 2.4 to navigate opening a directory
	struct dirent* fileInDir; // holds basically a directory stream
	struct stat attributes; // stat struct holds a ton of members describing file info returned from stat(), lstat(), and fstat()
	DIR* dirToCheck; // Pointer to active directory once it's open
	char dirPrefix[7] = "rooms_";

	// Open dirToCheck
	dirToCheck = opendir(".");
	int newestDirTime = -1;
	if (dirToCheck > 0) {	// opendir() successful
		// While the directory stream is not null (while reading)
		int foundDir = -1;
		while ( (fileInDir = readdir(dirToCheck)) != NULL ) {
			// If the filename of the current file contains the prefix
			if (strstr(fileInDir->d_name, dirPrefix) != NULL) {
				foundDir = 0;
				// Call stat on the file place in attributes
				stat(fileInDir->d_name, &attributes);
				// If time is greater as in more recent,
				if ((int)attributes.st_mtime > newestDirTime) {
					newestDirTime = (int)attributes.st_mtime; // update more recent time
					memset(dirName, '\0', 40);
					strcpy(dirName, fileInDir->d_name);	// Copy new dir name
				}
			}
		}

		if (foundDir != 0) {
			printf("Error: there is no 'rooms_' directory. Run ./buildrooms.\n");
			closedir(dirToCheck);
			return 1;
		}
		// printf("newest entry in: %s\n", dirName);
	}

	else {	// opendir() error
		return 1;
	}

	// Close dirToCheck
	closedir(dirToCheck);
	return 0;
}

//*****************************************************/
// Process all room files into a struct and operate the game on that
void readFiles(char* directoryName, struct room_adventure* roomsIn[]) {
	DIR* currDir;
	currDir = opendir(directoryName);
	if (currDir == NULL) {
		printf("Error: can't open directoryName %s:\n", directoryName);
	}

	// Open files and process all info into rooms array
	char pathname[100];
	memset(pathname, '\0', sizeof(pathname));
	strcpy(pathname, directoryName);

	struct dirent* fileInDir;
	FILE* roomFile;

	int i = -2; // Skips . and .. in directory for the rooms array iterator

	while ( (fileInDir = readdir(currDir)) != NULL ) {
		// Create pathname from directory name and name of current file in directory stream
		sprintf(pathname, "%s/%s", directoryName, fileInDir->d_name);
		roomFile = fopen(pathname, "r");
		if (roomFile == NULL) {
			printf("Error: fopen(%s) failed in readFiles()\n", pathname);
		}

		// Get line containing room type info
		char buf[100];
		int flag = 0;
		while ( fgets(buf, sizeof(buf), roomFile )) {
			char* token;
			char* num;
			token = strtok(buf, " :");

			while (token != NULL) {
				if ( strcmp(token, "NAME") == 0) { // If you find the NAME token set flag
					flag = 1; }
				if ( strcmp(token, "TYPE") == 0) { // If you find the TYPE token set flag
					flag = 2; }
				if ( strcmp(token, "CONNECTION") == 0) { // If you find the CONNECTION token set flag
					flag = 3;
					num = strtok(NULL, " :\n"); // Number token, must be decremented later
				}

				token = strtok(NULL, " :\n");

				if ( flag == 1) { // Set name token
					strcpy(roomsIn[i]->name, token);
					flag = 0; }
				if ( flag == 2) { // Set type token
					strcpy(roomsIn[i]->roomType, token);
					flag = 0; }
				if ( flag == 3) { // Set the connection token
					// Not all the files have been created yet... just add the names as connections instead
					int a = atoi(num);
					sprintf(roomsIn[i]->cxns[a], "%s", token);
				//sprintf(roomsIn[i]->cxns[atoi(num)-1], token);
					roomsIn[i]->numCxns++;
					flag = 0;
				}
			}
		}
		// Close file, increment iterator
		fclose(roomFile);
		i++;
	}
}

//*****************************************************/
// Find the starting room of the game by matching "START_ROOM" for roomType
struct room_adventure* findStart(struct room_adventure* rooms[]){
	int a=0;
	// Iterate through rooms[] to find roomType == START_ROOM
	for (int i=0; i<NUM_ROOMS; i++) {
		if ( strcmp(rooms[i]->roomType, "START_ROOM") == 0) {
			a = i;
		}
	}
	struct room_adventure* room = rooms[a];
	return(room);
}

//*****************************************************/
// Compare user input to cxns and "time" and return 0, 1, or 2
int inputMatch(char* input, struct room_adventure* room) {
	int match = 0;
	for (int i=0; i < room->numCxns; i++) {
		if (strcmp(input, room->cxns[i]) == 0) { match = 1; }
	}

	// If user wants the current time
	if (match == 0 && strcmp(input, "time") == 0 ) { match = 2; }

	return match;
}

//*****************************************************/
// Write time to currentTime.txt function
void* writeTime(void* arg) {

	// Establish lock on mutex for threadB
	pthread_mutex_lock(&lock);

	// Create file
	char filename[20] = "currentTime.txt";
	int file_desc = open(filename, O_RDWR | O_CREAT, 0600);

	// Get current time
	time_t rawtime;
	time(&rawtime); // Get time since the Epoch
	struct tm* info;
	info = localtime(&rawtime); 	// Converts to local time data
	char buffer[50];
	memset(buffer, '\0', sizeof(buffer));
	strftime(buffer, 50, "%l:%M%P, %A, %B %d, %Y\n", info); // Formats time data

	// Write to file and close
	write(file_desc, buffer, strlen(buffer)); // Stop writing at null char
	close(file_desc);

	// threadB unlocks mutex
	pthread_mutex_unlock(&lock);

	return NULL;
}

//*****************************************************/
// Read time function
void readTime() {
	// Create second thread that calls writeTime()
	pthread_create(&threadB, NULL, writeTime, NULL);
	pthread_join(threadB, NULL);

	// Cannot read from file until threadB unlocks the mutex in writeTime()
	pthread_mutex_lock(&lock);
	FILE* timeFile;
	timeFile = fopen("currentTime.txt", "r");
	char buf[50];
	fgets(buf, sizeof(buf), timeFile);
	printf("%s\n", buf);

	// Close and delete timeFile
	fclose(timeFile);
	if ( remove("currentTime.txt") != 0 ) {
		printf("Error: currentTime.txt could not be closed.\n");
	}

	pthread_mutex_unlock(&lock);
}

//*****************************************************/
// Present prompt to user and walk them through the rooms of the game until the end room is reached
void* runGame(void* arg) {

	// Cast void* to array of pointers to struct room_adventure
	struct room_adventure** rooms = (struct room_adventure**) arg;

	// Find the starting room
	struct room_adventure* room = findStart(rooms);

	// Declare and initialize pathHistory with starting room
	char* pathHistory[100];
	for (int i=0; i<100; i++) {
		pathHistory[i] = malloc(sizeof(char)*10);
	}
	sprintf(pathHistory[0], "%s", room->name);

	// Begin main game loop
	int done = 0;
	int steps = 0;
	char input[50];
	memset(input, '\0', sizeof(input));
	int match = 0;

	while (done != 1) {
		// Skip current location and possible connections if want time
		if (match != 2) {
			// Print prompt for current and next rooms
			printf("CURRENT LOCATION: %s\n", room->name);
			printf("CONNECTIONS: ");

			// Loop through room cxns
			for (int i=0; i < room->numCxns; i++) {
				printf("%s", room->cxns[i]);
				if ( i < room->numCxns - 1) {
					printf(", ");
				}
			}
			printf(".\n");
		}

		// Get user input
		printf("WHERE TO? > ");
		scanf("%s", input);
		printf("\n");

		// Compare input to room->cxns[] and check if user wants time
		match = inputMatch(input, room);
		// If the input doesn't match a room connections, repeat prompts
		if (match == 0) {
			printf("HUH, THAT DOESN'T SEEM TO MATCH.\n\n");
		}

		// If user wants current time
		else if ( match == 2 ) {
			readTime(); // creates threadB
		}

		// If the input does match a connection, increment steps, add to path history, update room
		else {
			steps++;
			sprintf(pathHistory[steps], "%s", input);
			// Update room
			int i;
			for (i=0; i < NUM_ROOMS; i++) {
				if ( strcmp(input, rooms[i]->name) == 0) {
					room = rooms[i];
				}
			}
		}

		// Check if the room is END_ROOM, set done = 1
		if ( strcmp(room->roomType, "END_ROOM") == 0) {
			sprintf(pathHistory[steps], "%s", room->name);
			done = 1;
		}
	}

	// Print victory statements, number of steps and pathHistory
	printf("CONGRATULATIONS!! YOU FOUND YOUR WAY OUT!\n");
	printf("YOU TOOK %d STEP(S). YOUR PATH TO VICTORY WAS:\n", steps);
	for (int i=1; i < steps+1; i++) { 	// Skip starting room, begin at i=1
		printf("   %s\n", pathHistory[i]);
	}

	// Free memory in pathHistory
	for (int i=0; i<100; i++) {
		free(pathHistory[i]);
		pathHistory[i] = NULL;
	}

	return NULL;
}