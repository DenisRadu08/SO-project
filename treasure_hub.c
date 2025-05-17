#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>

//Struct from treasure_manager.c
typedef struct {
    int id;
    char name[32];
    float latitude, longitude;
    char clue[1028];
    int value;
} Treasure;

//Function from treasure_manager.c
void read_input(char *buffer, size_t len)
{
    char c;
    int i = 0;
    // Read one char at a time until buffer limit, EOF, or newline
    while (i < len - 1 && read(0, &c, 1) > 0 && c != '\n') 
    {
        buffer[i++] = c;
    }
    buffer[i] = '\0';
}

static int monitor_pipe_fd[2]; // Monitor's pipe

//Function from treasure_manager.c
void list(const char *hunt_id) 
{
    char file[256];
    snprintf(file, sizeof(file), "./%s/treasure.bin", hunt_id);

    // Check if file exists
    struct stat st;
    if (stat(file, &st) == -1) 
    {
        write(monitor_pipe_fd[1], "Hunt ID is wrong", strlen("Hunt ID is wrong"));
        return;
    }

    write(monitor_pipe_fd[1], "Name: ", strlen("Name: "));
    write(monitor_pipe_fd[1], hunt_id, strlen(hunt_id));
    write(monitor_pipe_fd[1], "\n", 1);

    char buffer[512];
    snprintf(buffer, sizeof(buffer), "File size: %ld bytes\n", st.st_size);
    write(monitor_pipe_fd[1], buffer, strlen(buffer));

    // Display last modified time of the hunt
    char time_buffer[128];
    struct tm *time = localtime(&st.st_mtime);
    strftime(time_buffer, sizeof(time_buffer), "Last time modified: %Y-%m-%d %H:%M:%S\n", time);
    write(monitor_pipe_fd[1], time_buffer, strlen(time_buffer));
    write(monitor_pipe_fd[1], "\n", 1);

    int fd = open(file, O_RDONLY);
    if (fd == -1) 
    {
        write(monitor_pipe_fd[1], "Error when opening treasure file", strlen("Error when opening treasure file"));
        return;
    }

    write(monitor_pipe_fd[1], "Treasure list: ", strlen("Treasure list: "));
    Treasure tr;
    // Read and display each treasure
    while (read(fd, &tr, sizeof(Treasure)) == sizeof(Treasure)) 
    {
        char msg[2048];
        snprintf(msg, sizeof(msg), "\nID: %d\nName: %s\nLatitude: %.2f\nLongitude: %.2f\nClue: %s\nValue: %d\n",tr.id, tr.name, tr.latitude, tr.longitude, tr.clue, tr.value);
        write(monitor_pipe_fd[1], msg, strlen(msg));
    }
    close(fd);

}

//Function from treasure_manager.c
void view(const char *hunt_id, const char *tr_id) 
{
    char file[512];
    snprintf(file, sizeof(file), "./%s/treasure.bin", hunt_id);

    int fd = open(file, O_RDONLY);
    if (fd == -1) 
    {
        write(monitor_pipe_fd[1], "Error when opening treasure file", strlen("Error when opening treasure file"));
        return;
    }

    int ok = 0;
    Treasure tr;
    int target_id = atoi(tr_id);
    // Search for the treasure with matching ID
    while (read(fd, &tr, sizeof(Treasure)) == sizeof(Treasure)) 
    {
        if (tr.id == target_id) 
        {
            char msg[2048];
            snprintf(msg, sizeof(msg),"ID: %d\nName: %s\nLatitude: %.2f\nLongitude: %.2f\nClue: %s\nValue: %d\n",tr.id, tr.name, tr.latitude, tr.longitude, tr.clue, tr.value);
            write(monitor_pipe_fd[1], msg, strlen(msg));
            ok = 1;
            break;
        }
    }

    if (ok == 0) 
    {
        write(monitor_pipe_fd[1], "Non-existent ID\n", strlen("Non-existent ID\n"));
    }
    close(fd);
}

void list_hunts() 
{
    DIR *dir = opendir("."); //Open current directory
    if (dir == NULL) 
    {
        write(monitor_pipe_fd[1], "Error when opening directory\n", strlen("Error when opening directory\n"));
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)  //Read each directory entry
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) // Ignore . and .. directories
        {
            continue;
        }
        
        if (entry->d_type == DT_DIR) //Check if entry is a directory
        {
            char file[512];
            snprintf(file, sizeof(file), "./%s/treasure.bin", entry->d_name);
            struct stat st;

            if (stat(file, &st) == 0) //Check if treasure.bin exists
            {
                int count = st.st_size / sizeof(Treasure);
                char msg[512];
                snprintf(msg, sizeof(msg), "Hunt: %s, Treasures: %d\n", entry->d_name, count);
                write(monitor_pipe_fd[1], msg, strlen(msg)); //Print hunt and count
            }
        }
    }
    closedir(dir);
    write(monitor_pipe_fd[1],"\n",1);
}

//Variables which track monitor state
static pid_t monitor_pid = 0; //Stores the monitor's pid
static int monitor_stopping = 0; //1 if stop_monitor was called, 0 if not
static volatile int command_done = 0; //Flag for SIGUSR2
//Handler for SIGCHLD to detect when monitor is stopped
void handle_sigchld(int sig) 
{
    int status;
    pid_t pid = waitpid(monitor_pid, &status, WNOHANG); //Check if monitor exited
    if (pid == monitor_pid) //Monitor is stopped
    {
        monitor_pid = 0;
        monitor_stopping = 0; //Allow commands
        char msg[64];
        if (WIFEXITED(status)) //Monitor exited normally
        {
            snprintf(msg, sizeof(msg), "Monitor exited with status %d\n", WEXITSTATUS(status));
        } 
        else //Monitor was killed by a signal
        {   
            snprintf(msg, sizeof(msg), "Monitor killed by signal %d\n", WTERMSIG(status));
        }
        write(1, msg, strlen(msg)); //Print status of termination
    }
}

void handle_sigusr1(int sig) 
{
    int fd = open("/tmp/treasure_hub_cmd.txt", O_RDONLY);
    if (fd == -1) 
    {
        write(monitor_pipe_fd[1], "Error when trying to read command file\n", strlen("Error when trying to read command file\n"));
        kill(getppid(), SIGUSR2);
        return;
    }

    char buffer[256];
    int n = read(fd, buffer, sizeof(buffer) - 1); //Read command
    close(fd);
    if (n <= 0) 
    {
        write(monitor_pipe_fd[1], "Empty command file\n", strlen("Empty command file\n"));
        kill(getppid(),SIGUSR2);
        return;
    }

    buffer[n] = '\0';

    //Check command
    char *cmd = strtok(buffer, " ");
     if (cmd == NULL) 
     {
         write(monitor_pipe_fd[1], "Invalid command format\n", strlen("Invalid command format\n"));
         kill(getppid(), SIGUSR2);
         return;
     }
     if (strcmp(cmd, "list_hunts") == 0) 
     {
         list_hunts();
         kill(getppid(), SIGUSR2);
     } else if (strcmp(cmd, "list_treasures") == 0) 
     {
         char *hunt_id = strtok(NULL, " ");
         if (hunt_id == NULL) 
         {
             write(monitor_pipe_fd[1], "Missing hunt ID\n", strlen("Missing hunt ID\n"));
             kill(getppid(), SIGUSR2);
             return;
         }
         list(hunt_id);
         kill(getppid(), SIGUSR2);
     } else if (strcmp(cmd, "view_treasure") == 0) 
     {
         char *hunt_id = strtok(NULL, " ");
         if (hunt_id == NULL) 
         {
             write(monitor_pipe_fd[1], "Missing hunt ID\n", strlen("Missing hunt ID\n"));
             kill(getppid(), SIGUSR2);
             return;
         }
         char *tr_id = strtok(NULL, " ");
         if (tr_id == NULL) 
         {
             write(monitor_pipe_fd[1], "Missing treasure ID\n", strlen("Missing treasure ID\n"));
             kill(getppid(), SIGUSR2);
             return;
         }
         view(hunt_id, tr_id);
         kill(getppid(), SIGUSR2);
     } 
     else if(strcmp(cmd, "calculate_score") == 0)
     {
        DIR *dir = opendir(".");
        if(dir == NULL)
        {
            write(monitor_pipe_fd[1], "Failed to open directory\n", strlen("Failed to open directory\n"));
            kill(getppid(), SIGUSR2);
            return;
        }
        rewinddir(dir); //Reset directory so all the time entries are read from the start
        
        struct dirent *entry = NULL;
        while((entry = readdir(dir)) != NULL)
        {
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }
            if(entry->d_type == DT_DIR)
            {
                char file[512];
                snprintf(file, sizeof(file), "./%s/treasure.bin", entry->d_name);
                struct stat st;
                if (stat(file, &st) == 0)
                {
                    int pipe_fd[2];
                    if(pipe(pipe_fd) == -1)
                    {
                        write(monitor_pipe_fd[1], "Failed to create pipe\n", strlen("Failed to create pipe\n"));
                        continue;
                    }
                    
                    pid_t pid = fork();
                    if (pid == -1)
                    {
                        write(monitor_pipe_fd[1], "Failed to fork\n", strlen("Failed to fork\n"));
                        close(pipe_fd[0]);
                        close(pipe_fd[1]);
                        continue;
                    }

                    if (pid == 0)
                    { //Child
                        close(pipe_fd[0]);
                        dup2(pipe_fd[1], 1);
                        close(pipe_fd[1]);
                        close(monitor_pipe_fd[0]);
                        close(monitor_pipe_fd[1]);
                        execl("./score_calculator", "score_calculator", entry->d_name, (char *)NULL);
                        
                        write(1, "Failed to execute\n", strlen("Failed to execute\n"));
                        exit(1);
                    }
                    else
                    {
                        //Parent
                        close(pipe_fd[1]);
                        char buffer[512];
                        int bytes;
                        int read_attempts = 0;
                        const int max_attempts = 100; //To solve the infinite loop glitch
                        while ((bytes = read(pipe_fd[0], buffer, sizeof(buffer) - 1)) > 0 && read_attempts < max_attempts)
                        {
                            buffer[bytes] = '\0';
                            write(monitor_pipe_fd[1], buffer, bytes);
                            read_attempts++;
                        }

                        if(read_attempts >= max_attempts)
                        {
                            write(monitor_pipe_fd[1], "Read timeout on pipe\n", strlen("Read timeout on pipe\n"));
                        }

                        if (bytes == -1)
                        {
                            write(monitor_pipe_fd[1], "Failed to read from pipe\n", strlen("Failed to read from pipe\n"));
                        }

                        close(pipe_fd[0]);
                        int status;
                        waitpid(pid, &status, 0);
                    }
                }
            }
        }
        closedir(dir);
        kill(getppid(), SIGUSR2);
     }
     else 
    {
        write(monitor_pipe_fd[1], "Unknown monitor command\n", strlen("Unknown monitor command\n"));
        kill(getppid(), SIGUSR2);
    }
    kill(getppid(), SIGUSR2);
}

void handle_sigusr2(int sig)
{
    command_done=1;
}

//Handler for SIGTERM in the monitor (for stop_monitor)
void handle_sigterm(int sig) 
{
    write(monitor_pipe_fd[1], "Monitor stopping, waiting 2 seconds...\n",strlen("Monitor stopping, waiting 2 seconds...\n"));
    sleep(2); //Wait 2 seconds
    exit(0); //Exit monitor process
}

void monitor_process(int pipe_write_fd) 
{
    monitor_pipe_fd[1]=pipe_write_fd;
    //Set up SIGTERM handler for stop_monitor
    struct sigaction sa;
    sa.sa_handler = handle_sigterm; //Call handle_sigterm on SIGTERM
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask); //No blocked signals during handler
    sigaction(SIGTERM, &sa, NULL); 

    //Set up SIGUSR1 handler for commands
    sa.sa_handler = handle_sigusr1; //Call handle_sigusr1 on SIGUSR1
    sigaction(SIGUSR1, &sa, NULL);

    while (1) 
    {
        pause(); //Wait for SIGTERM or SIGUSR1
    }
}

void start_monitor() 
{
    if (monitor_pid != 0) //Check if monitor is already running
    {    
        write(1, "Monitor already running\n", strlen("Monitor already running\n"));
        return;
    }

    int pipe_fd[2];
    if(pipe(pipe_fd) == -1)
    {
        write(1,"Error creating the pipe\n",strlen("Error creating the pipe\n"));
        return;
    }

    pid_t pid = fork(); //Create a new process
    
    if (pid < 0) //Fork failed
    {
        write(1, "Fork failed\n", strlen("Fork failed\n"));
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return;
    } 
    else if (pid == 0)
    {
        close(pipe_fd[0]);
        monitor_process(pipe_fd[1]);
        close(pipe_fd[1]);
        exit(0);
    } 
    else
    {
        close(pipe_fd[1]);
        monitor_pipe_fd[0] = pipe_fd[0];
        monitor_pipe_fd[1] = -1;
        monitor_pid = pid; //Save monitor's PID
        char msg[64];
        snprintf(msg, sizeof(msg), "Monitor started with PID %d\n", pid); //Confirm start
        write(1, msg, strlen(msg));
    }
}

void send_command_to_monitor(const char *cmd, const char *hunt_id, const char *tr_id) 
{

    int fd = open("/tmp/treasure_hub_cmd.txt", O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd == -1) 
    {
        write(1, "Failed to write command file\n", strlen("Failed to write command file\n"));
        return;
    }

    write(fd, cmd, strlen(cmd));
    
    if (hunt_id != NULL) 
    {
        write(fd, " ", 1);
        write(fd, hunt_id, strlen(hunt_id));
    }

    if (tr_id != NULL) 
    {
        write(fd, " ", 1);
        write(fd, tr_id, strlen(tr_id));
    }

    close(fd);
    command_done = 0; //Reset flag
    
    if (kill(monitor_pid, SIGUSR1) == -1) 
    {
        write(1, "Failed to signal monitor\n", strlen("Failed to signal monitor\n"));
        return;
    }

    char buffer[4096];
    int bytes;
    while (!command_done && (bytes = read(monitor_pipe_fd[0], buffer, sizeof(buffer) - 1)) > 0) 
    {
        buffer[bytes] = '\0';
        write(1, buffer, bytes);
    }

    usleep(100000);
}

void stop_monitor() 
{
    if (monitor_pid == 0) //Check if monitor is running
    {  
        write(1, "No monitor running\n", strlen("No monitor running\n"));
        return;
    }

    if (kill(monitor_pid, SIGTERM) == -1)  //Send SIGTERM to monitor
    {   
        write(1, "Failed to stop monitor\n", strlen("Failed to stop monitor\n"));
        return;
    }

    monitor_stopping = 1; //Mark monitor as stopping (other commands will not work anymore)
    write(1, "Stopping monitor\n", strlen("Stopping monitor\n"));

    char buffer[4096];
    int bytes;
    while((bytes = read(monitor_pipe_fd[0], buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes] = '\0';
        write(1, buffer, bytes);
    }

    //Wait for monitor to terminate
    int waited = 0;
    while (monitor_pid != 0 && waited < 3000000)
    {    
        usleep(100000); //Wait 100ms per iteration
        waited += 100000;
    }
    if (monitor_pid != 0)  //Monitor didn't terminate in time
    {
        write(1, "Monitor termination timed out\n",strlen("Monitor termination timed out\n"));
    }
    close(monitor_pipe_fd[0]);
    monitor_pipe_fd[0] = -1;
    monitor_pipe_fd[1] = -1;
}


int main() 
{
    //Set up SIGCHLD handler to catch monitor termination
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sa.sa_flags = SA_RESTART;//Restart interrupted system calls
    sigemptyset(&sa.sa_mask); 
    sigaction(SIGCHLD, &sa, NULL);
    sa.sa_handler = handle_sigusr2;
    sigaction(SIGUSR2, &sa, NULL);

    char command[512];
    while (1)  //Loop to read user commands
    {    
        write(1, "-> Enter command: ", strlen("-> Enter command: "));
        read_input(command, sizeof(command));
        if(command[0]=='\0') //Skip empty output
        {
            continue;
        }

        if (monitor_stopping && strcmp(command, "exit") != 0) 
        {
            write(1, "Monitor is stopping\n", strlen("Monitor is stopping\n"));
            continue;
        }

        if (strcmp(command, "start_monitor") == 0) 
        {
            start_monitor();
        } 
        else if (strcmp(command, "stop_monitor") == 0) 
        {
            stop_monitor();
        }
        else if (strcmp(command, "list_hunts") == 0) 
        {
            if (monitor_pid == 0) 
            {
                write(1, "No monitor running\n", strlen("No monitor running\n"));
                continue;
            }
            send_command_to_monitor("list_hunts",NULL,NULL);
        }
        else if (strcmp(command, "list_treasures") == 0) 
        {
            if (monitor_pid == 0) 
            {
                write(1, "No monitor running\n", strlen("No monitor running\n"));
                continue;
            }
            write(1,"Enter hunt ID: ",strlen("Enter hunt ID: "));
            char hunt_id[256];
            read_input(hunt_id,sizeof(hunt_id));
            send_command_to_monitor("list_treasures",hunt_id,NULL);

        }
        else if (strcmp(command, "view_treasure") == 0)
        {
            if (monitor_pid == 0) 
            {
                write(1, "No monitor running\n", strlen("No monitor running\n"));
                continue;
            } 
            write(1, "Enter hunt ID: ", strlen("Enter hunt ID: "));
            char hunt_id[256];
            read_input(hunt_id, sizeof(hunt_id));
            
            write(1, "Enter treasure ID: ", strlen("Enter treasure ID: "));
            char tr_id[256];
            read_input(tr_id, sizeof(tr_id));
            send_command_to_monitor("view_treasure",hunt_id,tr_id);
        }
        else if(strcmp(command, "calculate_score") == 0)
        {
            if(monitor_pid == 0)
            {
                write(1, "No monitor running\n", strlen("No monitor running\n"));
                continue;
            }
            send_command_to_monitor("calculate_score", NULL, NULL);
        }
        else if (strcmp(command, "exit") == 0) 
        {
            if (monitor_pid != 0) 
            {
                write(1, "Stop monitor first\n", strlen("Stop monitor first\n"));
            } 
            else 
            {
                write(1, "Exiting\n", strlen("Exiting\n"));
                break;
            }
        } 
        else 
        {
            write(1, "Unknown command\n", strlen("Unknown command\n"));
        }
    }

    return 0;
}