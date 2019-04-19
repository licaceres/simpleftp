#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef SFTP_H_INCLUDED
#define SFTP_H_INCLUDED

#define LINEMAX 30

bool busqEnArchivo(char *credentials, FILE *file) {
    char* line = (char*) malloc (sizeof(char) * LINEMAX);
    int i = 0;

    fgets(line, LINEMAX, file);

    while (!feof(file)){
        
        i++;
        if (!strcmp(credentials, line))
            return true;

        fgets(line, LINEMAX, file);
    }
    
    free (line);
    return false;
}

#endif // SFTP_H_INCLUDED