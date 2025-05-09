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
#include <string.h>


int secret_pipe[2]; // Pipe for passing secret numbers
int metrics_pipe[2]; // Pipe for passing metrics
int hidden_pipe[2];  // Pipe for passing hidden node counts

// Signal handler for SIGUSR1 
void sigusr1_handler(int signum) {
    (void)signum;
    int secret;
    read(secret_pipe[0], &secret, sizeof(int));
    printf("Process %d received secret number: %d\n", getpid(), secret);
    raise(secret);
}


void termination_handler(int signum) {   // Handler for termination signals
    printf("Process %d terminating due to signal %d\n", getpid(), signum);
    exit(0);
}


void sigint_handler(int signum) { // Custom SIGINT handler for Rule 3
    (void)signum;
    printf("Child %d received SIGINT (parent %d). Continuing execution.\n", 
           getpid(), getppid());
}

void sigquit_handler(int signum) {  // SIGQUIT handler for Rule 3 final termination
    printf("Process %d received SIGQUIT. Terminating cleanly.\n", getpid());
    exit(0);
}


void analyze_termination(int pid, int status) { // Analyze child termination status
    if (WIFEXITED(status)) {
        printf("Child %d exited with status %d\n", pid, WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status)) {
        printf("Child %d killed by signal %d\n", pid, WTERMSIG(status));
    }
    else if (WIFSTOPPED(status)) {
        printf("Child %d stopped by signal %d\n", pid, WSTOPSIG(status));
    }
}

// Display process tree
void show_process_tree(pid_t pid) {
    char command[50];
    snprintf(command, sizeof(command), "pstree -p %d", pid);
    printf("Process tree for %d:\n", pid);
    system(command);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <L> <H>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const int L = atoi(argv[1]);
    const int H = atoi(argv[2]);
    const int PN = 8; // Number of child processes

    // Validate inputs
    if (H > 60 || H < 0 || L <= 0) {
        fprintf(stderr, "Invalid arguments: H must be 0-60, L must be positive\n");
        return EXIT_FAILURE;
    }

    // Create pipes
    if (pipe(secret_pipe) == -1 || pipe(metrics_pipe) == -1 || pipe(hidden_pipe) == -1) {
        perror("pipe creation failed");
        return EXIT_FAILURE;
    }

    // Initialize random array with hidden nodes
    int input[L];
    srand(time(NULL));
    for (int i = 0; i < L; i++) {
        input[i] = rand() % 51; // Regular values 0-50
    }
    for (int i = 0; i < H; i++) {
        input[rand() % L] = -1 * (rand() % 60 + 1); // Hidden nodes -1 to -60
    }

    // Create shared memory for child return arguments
    int shm_id = shmget(IPC_PRIVATE, sizeof(int), 0666);
    if (shm_id == -1) {
        perror("shmget failed");
        return EXIT_FAILURE;
    }
    int *childArg = (int*)shmat(shm_id, NULL, 0);
    *childArg = 1;

    // Main process starts
    const pid_t parentPid = getpid();
    printf("Parent process %d starting with L=%d, H=%d\n", parentPid, L, H);
    show_process_tree(parentPid);

    // Fork child processes
    pid_t childPids[PN];
    int childHiddenCounts[PN];
    int activeChildren = 0;

    for (int i = 0; i < PN; i++) {
        pid_t pid = fork();
        
        if (pid == -1) {
            perror("fork failed");
            continue;
        }

        if (pid == 0) { // Child process code
            // Set up signal handlers
            signal(SIGUSR1, sigusr1_handler);
            signal(SIGTERM, termination_handler);
            signal(SIGALRM, termination_handler);
            signal(SIGINT, sigint_handler);
            signal(SIGQUIT, sigquit_handler);

            printf("Child %d (parent %d) starting\n", getpid(), getppid());
            
            // Process assigned portion of the array
            const int start = i * (L / PN);
            const int end = (i + 1) * (L / PN);
            int local_max = input[start];
            double local_sum = 0;
            int hidden_count = 0;

            for (int j = start; j < end; j++) {
                if (input[j] > local_max) local_max = input[j];
                if (input[j] < 0) hidden_count++;
                local_sum += input[j];
            }

            // Send metrics to parent
            write(metrics_pipe[1], &local_max, sizeof(int));
            write(metrics_pipe[1], &local_sum, sizeof(double));
            write(hidden_pipe[1], &hidden_count, sizeof(int));

            printf("Child %d found %d hidden nodes. Pausing\n", getpid(), hidden_count);
            raise(SIGTSTP); // Pause and wait for parent's decision

            printf("Child %d resumed execution\n", getpid());
            
            // Create grandchild to test process inheritance
            if (fork() == 0) {
                printf("Grandchild %d (parent %d) created\n", getpid(), getppid());
                sleep(30); // Sleep to observe process tree
                printf("Grandchild %d exiting\n", getpid());
                exit(0);
            }

            sleep(100); // Sleep to allow observation
            exit(*childArg); // Exit with assigned code
        }
        else { // Parent records child PID
            childPids[activeChildren++] = pid;
        }
    }

    // Parent process management of children
    if (getpid() == parentPid) {
        // Collect metrics from children
        int global_max = input[0];
        double global_sum = 0;
        
        for (int i = 0; i < activeChildren; i++) {
            // Read child metrics
            int child_max;
            double child_sum;
            read(metrics_pipe[0], &child_max, sizeof(int));
            read(metrics_pipe[0], &child_sum, sizeof(double));
            read(hidden_pipe[0], &childHiddenCounts[i], sizeof(int));

            if (child_max > global_max) global_max = child_max;
            global_sum += child_sum;
        }

        // Process parent's portion of the array
        const int parent_start = PN * (L / PN);
        for (int i = parent_start; i < L; i++) {
            if (input[i] > global_max) global_max = input[i];
            global_sum += input[i];
        }
        printf("Global max: %d, Avg: %.2f\n", global_max, global_sum / L);

        // Find min and max hidden counts
        int min_hidden = childHiddenCounts[0], max_hidden = childHiddenCounts[0];
        for (int i = 1; i < activeChildren; i++) {
            if (childHiddenCounts[i] < min_hidden) min_hidden = childHiddenCounts[i];
            if (childHiddenCounts[i] > max_hidden) max_hidden = childHiddenCounts[i];
        }

        // Apply rules to each child
        for (int i = 0; i < activeChildren; i++) {
            const pid_t pid = childPids[i];
            const int hidden = childHiddenCounts[i];
            int status;

            if (hidden == max_hidden) {
                // Rule 1: Highest hidden nodes - continue
                printf("Applying Rule 1 to child %d (%d hidden nodes)\n", pid, hidden);
                kill(pid, SIGCONT);
                waitpid(pid, &status, 0);
                analyze_termination(pid, status);
            }
            else if (hidden == min_hidden) {
                // Rule 3: Lowest hidden nodes 
                printf("Applying Rule 3 to child %d (%d hidden nodes)\n", pid, hidden);
                kill(pid, SIGCONT); // Unblock
                sleep(10);
                kill(pid, SIGINT);  // First signal 
                sleep(5);
                kill(pid, SIGQUIT); // Final termination
                waitpid(pid, &status, 0);
                analyze_termination(pid, status);
                
                // Check grandchild status
                printf("Checking grandchild of %d:\n", pid);
                system("ps -o pid,ppid,comm | grep project");
            }
            else {
                // Rule 2: Middle hidden nodes 
                printf("Applying Rule 2 to child %d (%d hidden nodes)\n", pid, hidden);
                const int secret = SIGTERM;
                write(secret_pipe[1], &secret, sizeof(int));
                kill(pid, SIGCONT); // Unblock
                kill(pid, SIGUSR1);  // Send secret number
                waitpid(pid, &status, 0);
                analyze_termination(pid, status);
            }
        }

        // Final cleanup
        shmdt(childArg);
        shmctl(shm_id, IPC_RMID, NULL);
        close(secret_pipe[0]); close(secret_pipe[1]);
        close(metrics_pipe[0]); close(metrics_pipe[1]);
        close(hidden_pipe[0]); close(hidden_pipe[1]);

        printf("Parent process %d completed\n", parentPid);
        show_process_tree(parentPid);
    }

    return EXIT_SUCCESS;
}