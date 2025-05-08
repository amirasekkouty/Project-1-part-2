#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h> // For system commands

// Global variables for signal handling
int secret_pipe[2]; // Pipe for passing secret number

// Signal handler for SIGUSR1
void sigusr1_handler(int signum) {
    (void)signum; // To avoid unused parameter warning
    
    // Read the secret number from the pipe
    int secret;
    read(secret_pipe[0], &secret, sizeof(int));
    
    printf("Process %d received secret number: %d\n", getpid(), secret);
    // Raise the signal corresponding to the secret number
    raise(secret);
}

// Signal handler for the secret number signal
void secret_signal_handler(int signum) {
    printf("Process %d terminating due to secret signal %d\n", getpid(), signum);
    exit(0); // Terminate without becoming a zombie
}

// SIGINT handler for Rule 3
void sigint_handler(int signum) {
    (void)signum; // To avoid unused parameter warning
    printf("Child %d received SIGINT but will not terminate. My parent is %d\n", 
           getpid(), getppid());
    // Do not terminate, just print info
}

// Print how child terminated/exited to terminal
void childTerminationAnalysis(int pid, int w, int status){
    if (w) {
        if (WIFEXITED(status)){
            printf("Child %d exited naturally with exit code %d\n", pid, WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status)){
            printf("Child %d killed by signal: %d\n", pid, WTERMSIG(status));
        }
        else if (WIFSTOPPED(status)){
            printf("Child %d stopped by signal %d\n", pid, WSTOPSIG(status));
        }
        // Note: WIFCONTINUED might not be available on all systems
        else if (WIFSTOPPED(status) == 0){ // Check if process is not stopped
            printf("Child %d status changed\n", pid);
        }
    }
}

// L integers, H hidden keys, PN process number
int main(int argc, char* argv[]){
    if (argc != 3){
        printf("usage: ./projectBFS <L> <H>\n");
        return -1;
    }
    
    clock_t start, end;
    double programTime;
    start = clock();                
    
    // Getting user input
    int L = atoi(argv[1]);
    int H = atoi(argv[2]);
    int PN = 8;

    // Create pipe for secret number
    if (pipe(secret_pipe) == -1) {
        printf("Error creating secret pipe\n");
        return -1;
    }

    if (H > 60 || H < 0){
        printf("H must be between 0 and 60\n");
        return -1;
    } else if (H < 0 || L < 0){
        printf("All arguments must be positive\n");
        return -1;
    } else if (L > 1500000){
        printf("L is too large, might crash program\n");
        return -1;
    }

    int partition = floor(L / PN);
    int parentEnd = (L % PN) + partition;

    // Generating text file
    int input[L];
    srand(time(NULL));
    for (int i = 0; i < L; i++){
        // Integers from 0 to 50
        int num = rand() % 51; 
        input[i] = num; 
    } 

    for (int i = 0; i < H; i++){
        int location = rand() % L;
        // Integers from -1 to -60
        int hiddenKey = -1 * ((rand() % 60) + 1);
        input[location] = hiddenKey;
    }

    FILE *fp = fopen("inputBFS_2.txt", "w+");

    if (fp == NULL){
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < L; i++){
        fprintf(fp, "%d ", input[i]);
    }

    fclose(fp);

    // Generating output file
    FILE* outputFile = fopen("outputBFS_2.txt", "w");

    printf("Parent %d starts\n", getpid()); 
    pid_t parentPid = getpid();

    int id;
    if ((id = shmget(0, sizeof(int), IPC_CREAT|0666)) < 0){
        printf("Error");
        return -1;
    }
    int *childArg = (int *)shmat(id, NULL, 0);
    *childArg = 1;

    char command[50];
    snprintf(command, sizeof(command), "pstree -p %d", parentPid);   
    system(command);

    // Finding Max, Average, Hidden keys
    int fd[4][2];
    int hiddenCount[4][2]; // New pipe for hidden counts
    
    // Create pipes for communication
    if (pipe(fd[0]) == -1 || pipe(fd[1]) == -1 || pipe(fd[2]) == -1 || pipe(fd[3]) == -1){
        printf("Error creating pipe\n");
        return -1;
    }
    
    // Create pipes for hidden node counts
    for (int i = 0; i < 4; i++) {
        if (pipe(hiddenCount[i]) == -1) {
            printf("Error creating hidden count pipe\n");
            return -1;
        }
    }

    // Automatically reap terminated children to prevent zombies
    signal(SIGCHLD, SIG_IGN);

    int status;
    int maximum = input[0];
    double average = 0;
    int parentStart = 0;
    int childStart = L / 2;
    
    // Store PIDs for each child
    pid_t childPids[8];
    int childHiddenCounts[8];
    int childCount = 0;

    pid_t pid1 = fork();
    if (pid1 == -1){
        printf("Fork error\n");
        return -1;
    }  
    // Parent
    if (pid1 > 0){
        childPids[childCount++] = pid1;
        childStart = parentStart + L / 4;

        printf("Parent %d is waiting for child %d to terminate\n", getpid(), pid1);
        int w = waitpid(pid1, &status, 0);
        childTerminationAnalysis(pid1, w, status);
    } 
    // Child
    else{
        // Setup signal handlers for child
        signal(SIGUSR1, sigusr1_handler);
        signal(SIGTERM, secret_signal_handler);
        signal(SIGALRM, secret_signal_handler);
        signal(SIGINT, sigint_handler); // Use special handler for SIGINT (Rule 3)
        
        printf("Child %d starts\n", getpid());

        parentStart = childStart;
        childStart += L / 4;
    }

    pid_t pid2 = fork();
    if (pid2 == -1){
        printf("Fork error\n");
        return -1;
    } 
    // Parent
    if (pid2 > 0){
        if (getpid() != parentPid) {
            childPids[childCount++] = pid2;
        }
        childStart = parentStart + L / 8;

        printf("Child %d is waiting for child %d to terminate\n", getpid(), pid2);
        int w = waitpid(pid2, &status, 0);
        childTerminationAnalysis(pid2, w, status);
    }
    // Child
    else{
        // Setup signal handlers for child
        signal(SIGUSR1, sigusr1_handler);
        signal(SIGTERM, secret_signal_handler);
        signal(SIGALRM, secret_signal_handler);
        signal(SIGINT, sigint_handler); // Use special handler for SIGINT (Rule 3)
        
        printf("Child %d starts\n", getpid());

        parentStart = childStart;
        childStart += L / 8;
    }

    pid_t pid3 = fork();
    if (pid3 == -1){
        printf("Fork error\n");
        return -1;
    } 
    // Child
    if (pid3 == 0){
        // Setup signal handlers for child
        signal(SIGUSR1, sigusr1_handler);
        signal(SIGTERM, secret_signal_handler);
        signal(SIGALRM, secret_signal_handler); 
        signal(SIGINT, sigint_handler); // Use special handler for SIGINT (Rule 3)
        
        printf("Child %d starts\n", getpid());
        close(fd[0][0]);
        close(fd[1][0]);

        system(command);
        ++(*childArg);

        int numHiddenNodes = 0;

        printf("Child %d is computing max and average\n", getpid());
        int lengthToProcess = childStart + partition;
        for (int i = childStart; i < lengthToProcess; i++){
            if (input[i] > maximum){
                maximum = input[i];
            } else if (input[i] < 0){
                fprintf(outputFile, "Hi I'm process %d with return arg %d. I found the hidden key in position A[%d].\n", getpid(), *childArg, i);
                numHiddenNodes++;
                if (numHiddenNodes <= 2){
                    write(fd[2][1], &input[i], sizeof(int));
                }
            }
            average += input[i];
        } 
        fprintf(outputFile, "Hi I'm process %d with return arg %d and my parent is %d\n", getpid(), *childArg, getppid());

        // Write the hidden node count to its parent
        write(hiddenCount[0][1], &numHiddenNodes, sizeof(int));
        
        write(fd[0][1], &maximum, sizeof(int));
        write(fd[1][1], &average, sizeof(double));
        write(fd[3][1], &maximum, sizeof(int));

        close(fd[0][1]);
        close(fd[1][1]);
        close(hiddenCount[0][1]);

        // Pause child
        printf("Child %d pausing with %d hidden nodes\n", getpid(), numHiddenNodes);
        raise(SIGTSTP);

        printf("Child %d continued\n", getpid());
        
        // Create a grandchild to test what happens to offspring of terminated children
        pid_t grandchild_pid = fork();
        if (grandchild_pid == 0) {
            // This is the grandchild
            printf("Grandchild %d created with parent %d\n", getpid(), getppid());
            sleep(10); // Give time for parent to potentially be terminated
            printf("Grandchild %d now has parent %d\n", getpid(), getppid());
            exit(42); // Exit with special code
        }
        
        sleep(100);  // For Rule 1 - allow time to observe process tree

        exit(*childArg);
    }
    // Parent
    else if (getpid() != parentPid) {
        if (getpid() != parentPid) {
            childPids[childCount++] = pid3;
        }
        
        close(fd[0][0]);
        close(fd[1][0]);

        system(command);
        ++(*childArg);

        int numHiddenNodes = 0;

        printf("Child %d is computing max and average\n", getpid());
        int lengthToProcess = parentStart + partition;
        for (int i = parentStart; i < lengthToProcess; i++){
            if (input[i] > maximum){
                maximum = input[i];
            } else if (input[i] < 0){
                fprintf(outputFile, "Hi I'm process %d with return arg %d. I found the hidden key in position A[%d].\n", getpid(), *childArg, i);
                numHiddenNodes++;
                if (numHiddenNodes <= 2){
                    write(fd[2][1], &input[i], sizeof(int));
                }
            }
            average += input[i];
        } 
        fprintf(outputFile, "Hi I'm process %d with return arg %d and my parent is %d\n", getpid(), *childArg, getppid());

        // Write the hidden node count to its parent
        write(hiddenCount[0][1], &numHiddenNodes, sizeof(int));
        
        write(fd[0][1], &maximum, sizeof(int));
        write(fd[1][1], &average, sizeof(double));

        close(fd[0][1]);
        close(fd[1][1]);
        close(hiddenCount[0][1]);

        printf("Child %d is waiting for child %d to terminate\n", getpid(), pid3);
        int w = waitpid(pid3, &status, WUNTRACED);
        childTerminationAnalysis(pid3, w, status);

        int childMax;
        read(fd[3][0], &childMax, sizeof(int));
        
        // Read child's hidden node count
        int childHiddenCount;
        read(hiddenCount[0][0], &childHiddenCount, sizeof(int));
        close(hiddenCount[0][0]);
        
        childHiddenCounts[childCount-1] = childHiddenCount;
        
        // Find max and min hidden counts
        int maxHiddenCount = childHiddenCount;
        int minHiddenCount = childHiddenCount;
        
        for (int i = 0; i < childCount; i++) {
            if (childHiddenCounts[i] > maxHiddenCount) {
                maxHiddenCount = childHiddenCounts[i];
            }
            if (childHiddenCounts[i] < minHiddenCount) {
                minHiddenCount = childHiddenCounts[i];
            }
        }
        
        // Rule 1: If child has highest number of hidden nodes
        if (childHiddenCount == maxHiddenCount) {
            printf("Child %d has highest hidden nodes (%d). Sending SIGCONT.\n", 
                   pid3, childHiddenCount);
            kill(pid3, SIGCONT);
        }
        // Rule 2: If child has neither highest nor lowest number of hidden nodes
        else if (childHiddenCount != minHiddenCount) {
            printf("Child %d has middle hidden nodes (%d). Sending SIGUSR1 with secret number.\n", 
                   pid3, childHiddenCount);
            
            // Set secret number (using SIGTERM) and write to pipe
            int secret = SIGTERM;
            write(secret_pipe[1], &secret, sizeof(int));
            
            // Wake up the child first
            kill(pid3, SIGCONT);
            
            // Send SIGUSR1 to communicate the secret number
            kill(pid3, SIGUSR1);
            
            // Wait for the child to terminate
            int term_status;
            waitpid(pid3, &term_status, 0);
            printf("Child %d has terminated with status %d\n", pid3, term_status);
            
            // Check if the child's children were terminated or adopted
            char ps_command[100];
            sprintf(ps_command, "ps -o pid,ppid,state,cmd --forest | grep -v grep | grep %d", pid3);
            system(ps_command);
            
            // Print explanation of what's happening with offspring
            printf("Checking if offspring of terminated process %d were adopted by init or terminated\n", pid3);
        }

        exit(*childArg);
    }

    // First Parent Process
    if (parentPid == getpid()) {
        close(fd[0][1]);
        close(fd[1][1]);

        int w = waitpid(pid1, &status, WUNTRACED);
        childTerminationAnalysis(pid1, w, status);

        // Read child's hidden node count
        int childHiddenCount;
        read(hiddenCount[0][0], &childHiddenCount, sizeof(int));
        close(hiddenCount[0][0]);
        
        childPids[childCount++] = pid1;
        childHiddenCounts[childCount-1] = childHiddenCount;
        
        // Find max and min hidden counts
        int maxHiddenCount = childHiddenCount;
        int minHiddenCount = childHiddenCount;
        
        for (int i = 0; i < childCount; i++) {
            if (childHiddenCounts[i] > maxHiddenCount) {
                maxHiddenCount = childHiddenCounts[i];
            }
            if (childHiddenCounts[i] < minHiddenCount) {
                minHiddenCount = childHiddenCounts[i];
            }
        }
        
        // Rule 1: If child has highest number of hidden nodes
        if (childHiddenCount == maxHiddenCount) {
            printf("Child %d has highest hidden nodes (%d). Sending SIGCONT.\n", 
                   pid1, childHiddenCount);
            kill(pid1, SIGCONT);
        }
        // Rule 2: If child has neither highest nor lowest number of hidden nodes
        else if (childHiddenCount != minHiddenCount) {
            printf("Child %d has middle hidden nodes (%d). Sending SIGUSR1 with secret number.\n", 
                   pid1, childHiddenCount);
            
            // Set secret number (using SIGTERM) and write to pipe
            int secret = SIGTERM;
            write(secret_pipe[1], &secret, sizeof(int));
            
            // Wake up the child first
            kill(pid1, SIGCONT);
            
            // Send SIGUSR1 to communicate the secret number
            kill(pid1, SIGUSR1);
            
            // Wait for the child to terminate and analyze
            int term_status;
            waitpid(pid1, &term_status, 0);
            printf("Child %d has terminated with status %d\n", pid1, term_status);
            
            // Check if the child's children were terminated or adopted
            char ps_command[100];
            sprintf(ps_command, "ps -o pid,ppid,state,cmd --forest | grep -v grep | grep %d", pid1);
            system(ps_command);
            
            // Print explanation of what's happening with offspring
            printf("Checking if offspring of terminated process %d were adopted by init or terminated\n", pid1);
        }
        

        int hiddenNodesTotal = 0;

        for(int i = 0; i < 7; i++) {
            int tempMaximum;
            read(fd[0][0], &tempMaximum, sizeof(int));
            if(tempMaximum > maximum) {
                maximum = tempMaximum;
            }

            double tempAverage;
            read(fd[1][0], &tempAverage, sizeof(double));
            average += tempAverage;

            double tempHidden;
            read(fd[2][0], &tempHidden, sizeof(int));
            hiddenNodesTotal += tempHidden;
        }
        close(fd[0][0]);
        close(fd[1][0]);

        printf("Parent %d is computing max and average\n", getpid());
        for (int i = 0; i < parentEnd; i++){
            if (input[i] > maximum){
                maximum = input[i];
            } else if (input[i] < 0){
                fprintf(outputFile, "Hi I'm process %d with return arg %d. I found the hidden key in position A[%d].\n", getpid(), 1, i);
                hiddenNodesTotal += input[i];
            }
            average += input[i];
        } 
        fprintf(outputFile, "Max = %d, Avg = %f\n", maximum, average / L);

        system(command);

        fprintf(outputFile, "Hidden Nodes Total = %d\n", hiddenNodesTotal);

        end = clock();
        programTime = ( (double) (end - start)) / CLOCKS_PER_SEC;

        printf("Program took %f seconds to run\n", programTime);
        
        // Close pipes that are no longer needed
        close(secret_pipe[0]);
        close(secret_pipe[1]);
        
        return 0;
    }
}