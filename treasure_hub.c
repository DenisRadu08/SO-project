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

}

//Function from treasure_manager.c
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
}

int main() 
{
    

    return 0;
}