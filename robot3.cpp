#include <iostream>  // Input/Ouput
#include <unistd.h>  // usleep 
#include <math.h>    // Euclidean Distance calculation
#include <fstream>   // File handling for rollnumbers.txt
#include <sys/shm.h> // Shared Memory to send/receive data
#include <pthread.h> // Threads for reading and writing

using namespace std;

// ================ CHANGEABLE VARIABLES SECTION (IMPORTANT) ================

// Self-explanatory. If this is changed, please adjust identifiers and number of robot files 
const int totalRobots = 4;

// Identifier is the index we will assign this process for accessing its own shared memory segment.
// E.g Identifier = 2 would allocate this robot to open the 2nd shared memory id for writing its own coordinates there.
const int identifier = 3;

// Set this to true if this robot is the first one to run (To allow a portion of the code to run which "flushes" the shared memory segment)
const bool initial = false;

// ================ VARIABLES SECTION ================

// Contains list of all robot ids + the current robot's own id, read from rollnumbers.txt 
int RobotID[totalRobots];

// Stored the coordinates of all the robots after the read/write sequences + the current robot's own coordinates
int robotCoords[totalRobots][2];

// Labels for generating/getting the Id for each shared memory segment
const char* labels[totalRobots];
string generateLabels[totalRobots];

// Id's used for reading/writing into those shared memory segments
int shmidStorage[totalRobots];

// Stores euclidean distances between current robot w.r.t other robots by using robotCoords variable
int euclideans[totalRobots];

// Threads for handling reading other people's coordinates and writing its own coordinates separately (Allow real-time updation)
pthread_t readThread, writeThread; 

// Forward decleration for reading and writing coordinate functions
void* writeCoordinates(void* args);
void* readCoordinates(void* args);

// UpdateOwnCoordinates acts as a decrementing variable which allows the read function to check euclidean distance w.r.t all the
// other robots in the system (totalRobots - 1)
int updateOwnCoordinate = totalRobots - 1;

// changeWhileReading only updates to True when a robot's coordinate in the shared memory segment is different from locally stored coordinate.
// This also signals to check the euclidean distance w.r.t to current robot 
bool changeWhileReading = false;


int main() {

    // ================ CODE SECTION ================

    // Initializing labels for each robot's shared memory segment. A single shared memory segment can be combined 
    // into one but i don't want to implement it since this is simpler to understand.
    for (int i = 0; i < totalRobots; i++) {
        generateLabels[i] = string("./robot") + to_string(i) + string(".txt");
        labels[i] = generateLabels[i].data();
    }

    // Initialize robot locations randomly (Between 1 and 30 inclusive)
    for(int i = 0; i < totalRobots; i++)
        for(int j = 0; j < 2; j++)
            robotCoords[i][j] = 1 + (rand() % 30);


    // Read coordinates of all robot ID's and store it in RobotID's
    ifstream rollNumberFile("rollnumbers.txt");
    if (rollNumberFile.is_open()) {
        for (int i = 0; i < totalRobots; i++) {
            rollNumberFile >> RobotID[i];
        }
        rollNumberFile.close();
    } else {
        cout << "Error in reading file name, maybe the file doesn't exist in the specified location?";
        return 0;
    }


    // Creating Shared memory segments for each robot w.r.t their labels generated previously
    for (int i = 0; i < totalRobots; i++) {

        // Shared Memory writing segment initialization
        key_t key = ftok(labels[i], 1);
        shmidStorage[i]= shmget(key, sizeof(int) * 2, 0666 | IPC_CREAT); 

        // Error Handling
        if (shmidStorage[i] == -1) {
            perror("Error in creating shared memory segment"); 
            return 0;
        }

    }


    // Flushing shared memory
    if (initial) {
        for (int i = 0; i < totalRobots; i++) {
            int* coordinate = (int*)shmat(shmidStorage[i], (void*)0, 0);
            *(coordinate) = -1;
            *(coordinate + 1) = -1;
            shmdt(coordinate);
        }
    }


    // Write robot's own initial coordinate to its own shared memory segment
    int* coordinate = (int*)shmat(shmidStorage[identifier], (void*)0, 0);
    *(coordinate) = robotCoords[identifier][0];
    *(coordinate + 1) = robotCoords[identifier][1];
    shmdt(coordinate);


    // Launch reading and writing functions
    pthread_create(&writeThread, NULL, writeCoordinates, NULL);
    pthread_create(&readThread, NULL, readCoordinates, NULL);


    // Join writing function when this robot has decided to stop updating coordinates and kill its reading function (personal preference)
    pthread_join(writeThread, NULL);
    pthread_cancel(readThread);


    // Write robot's own coordinates as (-1, -1) into shared memory. This is to signify it's no longer in the system (personal preference)
    coordinate = (int*)shmat(shmidStorage[identifier], (void*)0, 0);
    *(coordinate) = -1;
    *(coordinate + 1) = -1;
    shmdt(coordinate);

    return 0;
}


void* writeCoordinates(void* args) {

    bool keepRunning = true;
    cout << "Hello, I am Robot #" << RobotID[identifier] << ". " << endl;

    while(keepRunning) {
        // Now input the robot locations from the user
        cout << "My current coordinates are (" << robotCoords[identifier][0] << "," << robotCoords[identifier][1] << "). ";
        cout << "Please enter my new coordinates (Write -1 to exit): \n";

        // For X coordinate and then Y coordinate
        for(int i = 0; i < 2; i++) {
            // Keep taking input until input is between 1 and 30 inclusive
            do {
                string output = (i == 0) ? "X = " : "Y = ";
                cout << output;
                cin >> robotCoords[identifier][i];
                while (!cin.good()) {
                    cin.clear();
                    cin.ignore(1024, '\n');
                    cout << "Please enter a number!" << endl;
                    cout << output;
                    cin >> robotCoords[identifier][i];
                }

                // Exit out of function is user wants to leave
                if(robotCoords[identifier][i] == -1) {
                    keepRunning = false;
                    i += 10;
                    break;
                }

                if(robotCoords[identifier][i] < 0 || robotCoords[identifier][i] > 30)
                    cout << "Sorry, the acceptable range of values is 1 to 30. Please enter the coordinate again.\n";
            } while (robotCoords[identifier][i] < 0 || robotCoords[identifier][i] > 30);
        }

        if (keepRunning) {
            cout << endl << "Broadcasting my new coordinates (" << robotCoords[identifier][0] << "," << robotCoords[identifier][1] << ") " << endl;;
                    
            // Writing coordinates into shared memory segment
            int* coordinate = (int*)shmat(shmidStorage[identifier], (void*)0, 0);
            *(coordinate) = robotCoords[identifier][0];
            *(coordinate + 1) = robotCoords[identifier][1];
            shmdt(coordinate);
            updateOwnCoordinate = totalRobots - 1;
        }
    }
}

void* readCoordinates(void* args) {

    // Read perpetually (with some 100ms breaks so it feels almost like real-time updation)
    while(true) {

        for (int i = 0; i < totalRobots; i++) {

            // Update to False whenever you're reading a new robot's coordinate
            changeWhileReading = false;

            // Skip reading current robot's coordinate
            if (i == identifier)
                continue;


            // Read robot's coordinate from shared memory segment
            key_t key = ftok(labels[i], 1);
            shmidStorage[i] = shmget(key, sizeof(int) * 2, 0666 | IPC_CREAT);
            int* coordinate = (int*)shmat(shmidStorage[i], (void*)0, 0);

            // cout << endl << labels[i] << "'s data in SHM ID: " << shmidStorage[i] << ": " << coordinate[0] << coordinate[1] << endl;


            // Only update local coordinate storage if coordinate in shared memory segment is different from local coordinate storage
            if ((robotCoords[i][0] != *(coordinate)) && (robotCoords[i][1] != *(coordinate + 1))) {
                robotCoords[i][0] = *(coordinate);
                robotCoords[i][1] = *(coordinate + 1);

                // Also set this to true to allow euclidean distance operation to happen later on
                changeWhileReading = true;
            }
            shmdt(coordinate);

            // Just to not make my PC explode god forbid
            usleep(100000);

            // cout << endl << labels[i] << " " << robotCoords[i][0] << robotCoords[i][1] << endl;

            // If a another robot's coordinate changed or current robot's coordinate was changed
            // Then compute euclidean between another robot and current robot
            if (changeWhileReading || (updateOwnCoordinate > 0)) {
                int x = robotCoords[i][0] - robotCoords[identifier][0]; 
                int y = robotCoords[i][1] - robotCoords[identifier][1]; 
                euclideans[i] = sqrt(pow(x, 2) + pow(y, 2));

                // cout << "Coordinates (" << robotCoords[i][0] << "," << robotCoords[i][1] << ") received from: " << labels[i] << ". ";
                // cout << "Distance = " << euclideans[i] << " units. " << endl;

                // Decrement this, when this goes to 0, it won't call euclidean function again while the program is perpetually reading
                updateOwnCoordinate--;
                    
                // The message to send to terminal if computed euclidean is less than 10 units and coordinates in the shared memory
                // are >0 (Since -1 coordinates refer to "dead" robots)
                if (euclideans[i] <= 10 && robotCoords[i][0] >= 0 && robotCoords[i][0] >= 0) {
                    cout << "\nMessage Received: " << "Hello " << RobotID[i] << ", we are neighbours!" << endl; 
                }
            }
        }
    }
}