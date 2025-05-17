#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

//Struct from treasure_manager.c
typedef struct {
    int id;
    char name[32];
    float latitude, longitude;
    char clue[1028];
    int value;
} Treasure;

int main(int argc, char **argv)
{
    if(argc != 2)
    {
        write(1, "Not enough arguments. Input should be: ./score_calculator <hunt_id>\n", strlen("Not enough arguments. Input should be: ./score_calculator <hunt_id>\n"));
        return -1;
    }
    
    char *hunt_id=argv[1];
    char file[512];
    snprintf(file, sizeof(file), "./%s/treasure.bin", hunt_id);
    
    struct stat st;
    if(stat(file, &st) == -1)
    {
        write(1, "Hunt ID is wrong\n", strlen("Hunt ID is wrong\n"));
        return -1;
    }

    int fd = open(file, O_RDONLY);
    if (fd == -1)
    {
        write(1, "Failed to open treasure file\n", strlen("Failed to open treasure file\n"));
        return -1;
    }

    Treasure tr;
    int total_score = 0;
    while (read(fd, &tr, sizeof(Treasure)) == sizeof(Treasure))
    {
        total_score += tr.value;
    }

    close(fd);
    char msg[512];
    snprintf(msg, sizeof(msg), "Hunt: %s, Score: %d\n", hunt_id, total_score);
    write(1, msg, strlen(msg));
    return 0;
}