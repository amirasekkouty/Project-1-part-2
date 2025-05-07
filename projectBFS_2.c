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

// Print how child terminated/exited to terminal
void childTerminationAnalysis(int pid, int w, int status){
	if (w) {
		if (WIFEXITED(status)){
		    printf("Child %d exited naturally\n", pid);
		}
		else if (WIFSIGNALED(status)){
			printf("Child %d killed by signal: %d\n", pid, WTERMSIG(status));
		}
		else if (WIFSTOPPED(status)){
			printf("Child %d stopped by signal %d\n", pid, WSTOPSIG(status));
		}
		else if (WIFCONTINUED(status)){
			printf("Child %d continued\n", pid);
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

    if (H > 60 || H < 0){
        printf("H must be between 30 and 60\n");
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
    snprintf(command, sizeof(command), "pstree %d", parentPid);   
    system(command);

    // Finding Max, Average, Hidden keys
    int fd[4][2];
    if (pipe(fd[0]) == -1 || pipe(fd[1]) == -1 || pipe(fd[2]) == -1 || pipe(fd[3]) == -1){
        printf("Error creating pipe\n");
        return -1;
    }

    int status;
    int maximum = input[0];
    double average = 0;
    int parentStart = 0;
    int childStart = L / 2;

    pid_t pid1 = fork();
    if (pid1 == -1){
        printf("Fork error\n");
        return -1;
    }  
    // Parent
    if (pid1 > 0){
        childStart = parentStart + L / 4;

        printf("Parent %d is waiting for child %d to terminate\n", getpid(), pid1);
        int w = waitpid(pid1, &status, 0);
        childTerminationAnalysis(pid1, w, status);
    } 
    // Child
    else{
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
        childStart = parentStart + L / 8;

        printf("Child %d is waiting for child %d to terminate\n", getpid(), pid2);
        int w = waitpid(pid2, &status, 0);
        childTerminationAnalysis(pid2, w, status);
    }
    // Child
    else{
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
                if (numHiddenNodes <= 2){
                    write(fd[2][1], &input[i], sizeof(int));
                }
            }
                average += input[i];
        } 
        fprintf(outputFile, "Hi I'm process %d with return arg %d and my parent is %d\n", getpid(), *childArg, getppid());

        write(fd[0][1], &maximum, sizeof(int));
        write(fd[1][1], &average, sizeof(double));
        write(fd[3][1], &maximum, sizeof(int));

        close(fd[0][1]);
        close(fd[1][1]);

        // Pause child
        raise(SIGTSTP);

        printf("Child %d continued\n", getpid());

        exit(*childArg);
        }
        // Parent
    else if (getpid() != parentPid) {
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
                if (numHiddenNodes <= 2){
                    write(fd[2][1], &input[i], sizeof(int));
                }
            }
            average += input[i];
        } 
        fprintf(outputFile, "Hi I'm process %d with return arg %d and my parent is %d\n", getpid(), *childArg, getppid());

        write(fd[0][1], &maximum, sizeof(int));
        write(fd[1][1], &average, sizeof(double));

        close(fd[0][1]);
        close(fd[1][1]);

        printf("Child %d is waiting for child %d to terminate\n", getpid(), pid3);
        int w = waitpid(pid3, &status, WUNTRACED);
        childTerminationAnalysis(pid3, w, status);

        int childMax;
        read(fd[3][0], &childMax, sizeof(int));

        // Rule 1
        if (childMax > maximum){
            kill(pid3, SIGCONT);
        }else{
            printf("Child %d terminated\n", pid3);
            kill(pid3, SIGKILL);
        }

        exit(*childArg);

    }

    // First Parent Process
    if (parentPid == getpid()){
        close(fd[0][1]);
        close(fd[1][1]);

        int w = waitpid(pid3, &status, WUNTRACED);
        childTerminationAnalysis(pid3, w, status);

        int childMax;
        read(fd[3][0], &childMax, sizeof(int));

        // Rule 1
        if (childMax > maximum){
            kill(pid3, SIGCONT);
        }else{
            printf("Child %d terminated\n", pid3);
            kill(pid3, SIGKILL);
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
        return 0;
    }
}