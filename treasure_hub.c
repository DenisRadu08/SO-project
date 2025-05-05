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

//Function from treasure_manager.c
void list(const char *hunt_id) 
{
    char file[256];
    snprintf(file, sizeof(file), "./%s/treasure.bin", hunt_id);

    // Check if file exists
    struct stat st;
    if (stat(file, &st) == -1) 
    {
        write(1, "Hunt ID is wrong", strlen("Hunt ID is wrong"));
        return;
    }

    write(1, "Name: ", strlen("Name: "));
    write(1, hunt_id, strlen(hunt_id));
    write(1, "\n", 1);

    char buffer[512];
    snprintf(buffer, sizeof(buffer), "File size: %ld bytes\n", st.st_size);
    write(1, buffer, strlen(buffer));

    // Display last modified time of the hunt
    char time_buffer[128];
    struct tm *time = localtime(&st.st_mtime);
    strftime(time_buffer, sizeof(time_buffer), "Last time modified: %Y-%m-%d %H:%M:%S\n", time);
    write(1, time_buffer, strlen(time_buffer));
    write(1, "\n", 1);

    int fd = open(file, O_RDONLY);
    if (fd == -1) 
    {
        write(1, "Error when opening treasure file", strlen("Error when opening treasure file"));
        return;
    }

    write(1, "Treasure list: ", strlen("Treasure list: "));
    Treasure tr;
    // Read and display each treasure
    while (read(fd, &tr, sizeof(Treasure)) == sizeof(Treasure)) 
    {
        char msg[2048];
        snprintf(msg, sizeof(msg), "\nID: %d\nName: %s\nLatitude: %.2f\nLongitude: %.2f\nClue: %s\nValue: %d\n",tr.id, tr.name, tr.latitude, tr.longitude, tr.clue, tr.value);
        write(1, msg, strlen(msg));
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
        write(1, "Error when opening treasure file", strlen("Error when opening treasure file"));
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
            write(1, msg, strlen(msg));
            ok = 1;
            break;
        }
    }

    if (ok == 0) 
    {
        write(1, "Non-existent ID\n", strlen("Non-existent ID\n"));
    }
    close(fd);
}

void list_hunts() 
{
    DIR *dir = opendir("."); //Open current directory
    if (dir == NULL) 
    {
        write(1, "Error: Failed to open directory\n", strlen("Error: Failed to open directory\n"));
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
                write(1, msg, strlen(msg)); //Print hunt and count
            }
        }
    }
    closedir(dir);
    write(1,"\n",1);
}

//Variables which track monitor state
static pid_t monitor_pid = 0; //Stores the monitor's pid
static int monitor_stopping = 0; //1 if stop_monitor was called, 0 if not

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
        write(1, "Error: Failed to read command file\n", strlen("Error: Failed to read command file\n"));
        return;
    }

    char buffer[256];
    int n = read(fd, buffer, sizeof(buffer) - 1); //Read command
    close(fd);
    if (n <= 0) 
    {
        write(1, "Error: Empty command file\n", strlen("Error: Empty command file\n"));
        return;
    }

    buffer[n] = '\0';

    //Check command
    char *cmd = strtok(buffer, " ");
     if (cmd == NULL) 
     {
         write(1, "Error: Invalid command format\n", strlen("Error: Invalid command format\n"));
         return;
     }
     if (strcmp(cmd, "list_hunts") == 0) 
     {
         list_hunts();
     } else if (strcmp(cmd, "list_treasures") == 0) 
     {
         char *hunt_id = strtok(NULL, " ");
         if (hunt_id == NULL) 
         {
             write(1, "Error: Missing hunt ID\n", strlen("Error: Missing hunt ID\n"));
             return;
         }
         list(hunt_id);
     } else if (strcmp(cmd, "view_treasure") == 0) 
     {
         char *hunt_id = strtok(NULL, " ");
         if (hunt_id == NULL) 
         {
             write(1, "Error: Missing hunt ID\n", strlen("Error: Missing hunt ID\n"));
             return;
         }
         char *tr_id = strtok(NULL, " ");
         if (tr_id == NULL) 
         {
             write(1, "Error: Missing treasure ID\n", strlen("Error: Missing treasure ID\n"));
             return;
         }
         view(hunt_id, tr_id);
     } else 
     {
         write(1, "Error: Unknown monitor command\n", strlen("Error: Unknown monitor command\n"));
     }
    

}

//Handler for SIGTERM in the monitor (for stop_monitor)
void handle_sigterm(int sig) 
{
    write(1, "Monitor stopping, waiting 2 seconds...\n",strlen("Monitor stopping, waiting 2 seconds...\n"));
    usleep(2000000); //Wait 2 seconds
    exit(0); //Exit monitor process
}

void monitor_process() 
{
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
        write(1, "Error: Monitor already running\n", strlen("Error: Monitor already running\n"));
        return;
    }

    pid_t pid = fork(); //Create a new process
    if (pid < 0) //Fork failed
    {
        write(1, "Error: Fork failed\n", strlen("Error: Fork failed\n"));
        return;
    } 
    else if (pid == 0)
    {
        monitor_process();
        exit(-1);
    } 
    else
    {
        monitor_pid = pid; //Save monitor's PID
        char msg[64];
        snprintf(msg, sizeof(msg), "Monitor started with PID %d\n", pid); //Confirm start
        write(1, msg, strlen(msg));
    }
}

void stop_monitor() 
{
    if (monitor_pid == 0) //Check if monitor is running
    {  
        write(1, "Error: No monitor running\n", strlen("Error: No monitor running\n"));
        return;
    }

    if (kill(monitor_pid, SIGTERM) == -1)  //Send SIGTERM to monitor
    {   
        write(1, "Error: Failed to stop monitor\n", strlen("Error: Failed to stop monitor\n"));
        return;
    }

    monitor_stopping = 1; //Mark monitor as stopping (other commands will not work anymore)
    write(1, "Stopping monitor\n", strlen("Stopping monitor\n"));

    //Wait for monitor to terminate
    int waited = 0;
    while (monitor_pid != 0 && waited < 3000000)
    {    
        usleep(100000); //Wait 100ms per iteration
        waited += 100000;
    }
    if (monitor_pid != 0)  //Monitor didn't terminate in time
    {
        write(1, "Warning: Monitor termination timed out\n",strlen("Warning: Monitor termination timed out\n"));
    }
}


int main() 
{
    //Set up SIGCHLD handler to catch monitor termination
    struct sigaction sa;
    sa.sa_handler = handle_sigchld; //Call handle_sigchld on SIGCHLD
    sa.sa_flags = SA_RESTART;//Restart interrupted system calls
    sigemptyset(&sa.sa_mask); 
    sigaction(SIGCHLD, &sa, NULL);

    char command[256];
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
            write(1, "Error: Monitor is stopping\n", strlen("Error: Monitor is stopping\n"));
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
            if (monitor_pid == 0) //Check if monitor is running
            { 
                write(1, "Error: No monitor running\n", strlen("Error: No monitor running\n"));
                continue;
            }

            int fd = open("/tmp/treasure_hub_cmd.txt", O_WRONLY | O_CREAT | O_TRUNC, 0777); //Write command to file
            if (fd == -1) 
            {
                write(1, "Error: Failed to write command file\n",strlen("Error: Failed to write command file\n"));
                continue;
            }
            write(fd, "list_hunts", strlen("list_hunts")); //Write command
            close(fd);

            //Signal monitor to process command
            if (kill(monitor_pid, SIGUSR1) == -1) 
            {
                write(1, "Error: Failed to signal monitor\n",strlen("Error: Failed to signal monitor\n"));
                continue;
            }
            usleep(100000);
        }
        else if (strcmp(command, "list_treasures") == 0) 
        {
            if (monitor_pid == 0) //Check if monitor is running
            {
                write(1, "Error: No monitor running\n", strlen("Error: No monitor running\n"));
                continue;
            }

            write(1, "Enter hunt ID: ", strlen("Enter hunt ID: "));
            char hunt_id[256];
            read_input(hunt_id, sizeof(hunt_id));

            int fd = open("/tmp/treasure_hub_cmd.txt", O_WRONLY | O_CREAT | O_TRUNC, 0777); //Write command and hunt_id to file
            if (fd == -1) 
            {
                write(1, "Error: Failed to write command file\n",strlen("Error: Failed to write command file\n"));
                continue;
            }
            write(fd, "list_treasures ", strlen("list_treasures "));
            write(fd, hunt_id, strlen(hunt_id));
            close(fd);
            if (kill(monitor_pid, SIGUSR1) == -1) //Signal monitor to process command
            {
                write(1, "Error: Failed to signal monitor\n",strlen("Error: Failed to signal monitor\n"));
                continue;
            }
            usleep(100000); //Wait 100ms for monitor output to complete
        }
        else if (strcmp(command, "view_treasure") == 0)
        { 
            if (monitor_pid == 0) 
            { 
                write(1, "Error: No monitor running\n", strlen("Error: No monitor running\n"));
                continue;
            }
        
            write(1, "Enter hunt ID: ", strlen("Enter hunt ID: "));
            char hunt_id[256];
            read_input(hunt_id, sizeof(hunt_id));
            
            write(1, "Enter treasure ID: ", strlen("Enter treasure ID: "));
            char tr_id[256];
            read_input(tr_id, sizeof(tr_id));
          
            int fd = open("/tmp/treasure_hub_cmd.txt", O_WRONLY | O_CREAT | O_TRUNC, 0777);
            if (fd == -1) 
            {
                write(1, "Error: Failed to write command file\n",strlen("Error: Failed to write command file\n"));
                continue;
            }
            write(fd, "view_treasure ", strlen("view_treasure "));
            write(fd, hunt_id, strlen(hunt_id));
            write(fd, " ", 1);
            write(fd, tr_id, strlen(tr_id));
            close(fd);

            if (kill(monitor_pid, SIGUSR1) == -1) 
            {
                write(1, "Error: Failed to signal monitor\n",strlen("Error: Failed to signal monitor\n"));
                continue;
            }
            usleep(100000);
        }
        else if (strcmp(command, "exit") == 0) 
        {
            if (monitor_pid != 0) 
            {
                write(1, "Error: Stop monitor first\n", strlen("Error: Stop monitor first\n"));
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