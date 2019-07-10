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
        if (!strcmp(credentials, line)){
            free (line);
            return true;
        }

        fgets(line, LINEMAX, file);
    }

    free (line);
    return false;
}

char* calc_port (int port){
    char* portstr = (char*) malloc (sizeof(char)*7);
    char* aux = (char*) malloc (sizeof(char)*3);

    sprintf(aux, "%d", port % 256);
    sprintf(portstr, "%d", (port-(port % 256))/256);
    strcat(portstr, ",");
    strcat(portstr, aux);

    free(aux);
    return portstr;
}

#endif // SFTP_H_INCLUDED
