#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>

//Define the Treasure struct to store treasure data
typedef struct {
    int id;
    char name[32];
    float latitude, longitude;
    char clue[1028];
    int value;
} Treasure;


//Read input from stdin until newline
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

// Log an action for a hunt to a log file
void log_hunt(const char *hunt_id,const char *action)
{
    char path[512];
    // Construct path to log file: <hunt_id>/logged_hunt
    snprintf(path,sizeof(path),"%s/logged_hunt",hunt_id);

    int fd=open(path, O_WRONLY | O_CREAT | O_APPEND, 0777);
    if(fd==-1)
    {
        printf("Error opening file");
        exit(-1);
    }

    // Get current time for timestamp
    time_t now=time(NULL);
    char time_buffer[128];
    strftime(time_buffer,sizeof(time_buffer),"%Y-%m-%d %H:%M:%S",localtime(&now));

    // Create log entry: [timestamp] action
    char entry[1024];
    snprintf(entry,sizeof(entry),"[%s] %s\n",time_buffer,action);
    write(fd,entry,strlen(entry));
    close(fd);
}

// Create a symbolic link to the hunt's log file
void symlink_create(const char *hunt_id)
{
    char link[256];
    snprintf(link,sizeof(link),"logged_hunt-%s",hunt_id);
    char next[512];
    // Target path: <hunt_id>/logged_hunt
    snprintf(next,sizeof(next),"%s/logged_hunt",hunt_id);
    symlink(next,link);
}

// Add a new treasure to a hunt
void add(const char *hunt_id) 
{
    char dir[256];
    char file[512];
    char text[1024];
    // Create directory for hunt: ./<hunt_id>
    snprintf(dir, sizeof(dir), "./%s", hunt_id);
    if (mkdir(dir, 0777) == -1 && errno != EEXIST) 
    {
        write(1, "Directory create error", strlen("Directory create error"));
        exit(-1);
    }

    snprintf(file, sizeof(file), "%s/treasure.bin", dir);

    // Open file for writing binary data (if it doesn't exists, it creates a new file)
    int fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0777);
    if (fd == -1) 
    {
        write(1, "Error when opening file", strlen("Error when opening file"));
        exit(-1);
    }

    Treasure tr;
    // Read treasure details and store them in a variable
    write(1, "ID: ", strlen("ID: "));
    read_input(text, sizeof(text));
    tr.id = atoi(text);

    write(1, "Treasure name: ", strlen("Treasure name: "));
    read_input(tr.name, sizeof(tr.name));

    write(1, "Latitude: ", strlen("Latitude: "));
    read_input(text, sizeof(text));
    tr.latitude = atof(text);

    write(1, "Longitude: ", strlen("Longitude: "));
    read_input(text, sizeof(text));
    tr.longitude = atof(text);

    write(1, "Clue: ", strlen("Clue: "));
    read_input(tr.clue, sizeof(tr.clue));

    write(1, "Value: ", strlen("Value: "));
    read_input(text, sizeof(text));
    tr.value = atoi(text);

    // Write the treasure in a file in binary format
    if (write(fd, &tr, sizeof(Treasure)) != sizeof(Treasure)) 
    {
        write(1, "Error writing to file", strlen("Error writing to file"));
        close(fd);
        exit(-1);
    }

    close(fd);
    write(1, "Treasure was added successfully.\n",strlen("Treasure was added successfully.\n"));
    
    log_hunt(hunt_id,"Added a treasure");
    symlink_create(hunt_id);
}

// List all treasures in a hunt
void list(const char *hunt_id) 
{
    char file[256];
    snprintf(file, sizeof(file), "./%s/treasure.bin", hunt_id);

    // Check if file exists
    struct stat st;
    if (stat(file, &st) == -1) 
    {
        write(1, "ID is wrong", strlen("ID is wrong"));
        exit(-1);
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
        write(1, "Error when opening file", strlen("Error when opening file"));
        exit(-1);
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

    // Log the action
    log_hunt(hunt_id,"Listed the treasures");
}

// View a specific treasure by ID
void view(const char *hunt_id, const char *tr_id) 
{
    char file[512];
    snprintf(file, sizeof(file), "./%s/treasure.bin", hunt_id);

    int fd = open(file, O_RDONLY);
    if (fd == -1) 
    {
        write(1, "Error when opening file", strlen("Error when opening file"));
        exit(-1);
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

    log_hunt(hunt_id,"Viewed a specific treasure");
}

// Remove a specific treasure by ID
void remove_treasure(const char *hunt_id, const char *tr_id) 
{
    char file[512];
    snprintf(file, sizeof(file), "./%s/treasure.bin", hunt_id);

    int fd = open(file, O_RDONLY);
    if (fd == -1) 
    {
        write(1, "Error opening the file\n", strlen("Error opening the file\n"));
        exit(-1);
    }

    // Create temporary file
    char aux_file[512];
    snprintf(aux_file, sizeof(aux_file), "./%s/treasure_temp.bin", hunt_id);

    int aux_fd = open(aux_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (aux_fd == -1) 
    {
        write(1, "Error opening temporary file\n", strlen("Error opening temporary file\n"));
        close(fd);
        exit(-1);
    }

    int ok = 0;
    int target_id = atoi(tr_id);
    Treasure tr;
    // Copy all treasures except the one which is removed
    while (read(fd, &tr, sizeof(Treasure)) == sizeof(Treasure)) 
    {
        if (tr.id != target_id) 
        {
            if (write(aux_fd, &tr, sizeof(Treasure)) != sizeof(Treasure)) 
            {
                write(1, "Error writing to temporary file\n",strlen("Error writing to temporary file\n"));
                close(fd);
                close(aux_fd);
                exit(-1);
            }
        } 
        else 
        {
            ok = 1;
        }
    }

    close(fd);
    close(aux_fd);

    // Replace original file with temporary file
    if (remove(file) != 0) 
    {
        write(1, "Error removing old file\n", strlen("Error removing old file\n"));
        exit(-1);
    }

    if (rename(aux_file, file) != 0) 
    {
        write(1, "Error renaming temporary file\n", strlen("Error renaming temporary file\n"));
        exit(-1);
    }

    // Report result
    if (ok == 1) 
    {
        write(1, "Treasure was removed successfully.\n",strlen("Treasure was removed successfully.\n"));
    } 
    else 
    {
        write(1, "Non existent ID so there was no treasure removed.\n",strlen("Non existent ID so there was no treasure removed.\n"));
    }

    log_hunt(hunt_id,"A treasure was removed");
}

// Remove an entire hunt, preserving log in a new file
void remove_hunt(const char *hunt_id) 
{
    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "./%s", hunt_id);

    struct stat st;
    if (stat(dir_path, &st) == -1 || !S_ISDIR(st.st_mode)) 
    {
        write(1, "Non-existent hunt\n", strlen("Non-existent hunt\n"));
        exit(-1);
    }

    char treasure_file[512];
    char log_file[512];
    snprintf(treasure_file, sizeof(treasure_file), "./%s/treasure.bin", hunt_id);
    snprintf(log_file, sizeof(log_file), "./%s/logged_hunt", hunt_id);

    // Create new log file to preserve logged_hunt contents
    char deleted_log[512];
    snprintf(deleted_log, sizeof(deleted_log), "deleted_log-%s.txt", hunt_id);

    int deleted_fd = open(deleted_log, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (deleted_fd == -1) 
    {
        write(1, "Error creating deleted log file\n", strlen("Error creating deleted log file\n"));
        exit(-1);
    }

    // Copy existing log entries to new file
    int log_fd = open(log_file, O_RDONLY);
    if (log_fd != -1) 
    {
        char buffer[1024];
        int bytes_read;
        while ((bytes_read = read(log_fd, buffer, sizeof(buffer))) > 0) 
        {
            if (write(deleted_fd, buffer, bytes_read) != bytes_read) 
            {
                write(1, "Error writing to deleted log file\n",strlen("Error writing to deleted log file\n"));
                close(log_fd);
                close(deleted_fd);
                exit(-1);
            }
        }
        close(log_fd);
    }

    // Append record of hunt deletion
    time_t now = time(NULL);
    char time_buffer[128];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
    char deletion_entry[512];
    snprintf(deletion_entry, sizeof(deletion_entry), "[%s] Hunt '%s' was deleted\n", time_buffer, hunt_id);
    write(deleted_fd, deletion_entry, strlen(deletion_entry));
    close(deleted_fd);

    // Remove treasure and log files
    if (remove(treasure_file) == -1 && errno != ENOENT) 
    {
        write(1, "Error removing treasure file\n", strlen("Error removing treasure file\n"));
        exit(-1);
    }
    if (remove(log_file) == -1 && errno != ENOENT) 
    {
        write(1, "Error removing log file\n", strlen("Error removing log file\n"));
        exit(-1);
    }

    // Remove the directory
    if (rmdir(dir_path) == -1) 
    {
        write(1, "Error removing hunt directory\n", strlen("Error removing hunt directory\n"));
        exit(-1);
    }

    // Update symbolic link to point to new log file
    char symlink_name[256];
    snprintf(symlink_name, sizeof(symlink_name), "logged_hunt-%s", hunt_id);
    unlink(symlink_name);// Remove existing symlink
    if (symlink(deleted_log, symlink_name) == -1) 
    {
        write(1, "Error creating symbolic link\n", strlen("Error creating symbolic link\n"));
        exit(-1);
    }

    write(1, "Hunt removed\n", strlen("Hunt removed\n"));
}

int main(int argc, char **argv) 
{
    if (argc < 3) 
    {
        write(1, "Not enough arguments", strlen("Not enough arguments"));
        return -1;
    }

    // Dispatch to appropriate function based on command
    if (strcmp(argv[1], "add") == 0) 
    {
        add(argv[2]);
    } 
    else if (strcmp(argv[1], "list") == 0) 
    {
        list(argv[2]);
    } 
    else if (strcmp(argv[1], "view") == 0) 
    {
        if (argc < 4) 
        {
            write(1, "Missing treasure ID", strlen("Missing treasure ID"));
            return -1;
        }
        view(argv[2], argv[3]);
    }
    else if (strcmp(argv[1], "remove_treasure") == 0) 
    {
        if (argc < 4) 
        {
            write(1, "Missing treasure ID", strlen("Missing treasure ID"));
            return -1;
        }
        remove_treasure(argv[2], argv[3]);
    }
    else if (strcmp(argv[1], "remove_hunt") == 0) 
    {
        remove_hunt(argv[2]);
    } 
    else 
    {
        write(1, "Wrong command", strlen("Wrong command"));
    }
    return 0;
}