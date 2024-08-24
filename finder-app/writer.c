#include <stdio.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char **argv)
{
	int numberOfArgs;
	char * filePath;
        char * writeStr;
        FILE * file;
	
	openlog(argv[0], LOG_PID, LOG_USER);

	numberOfArgs = argc;
	if(numberOfArgs < 3)
	{
		syslog(LOG_ERR, "Error: Incorrect number of parameters");
		return 1;
	}

	filePath = argv[1];
	writeStr = argv[2];
	
	file = fopen(filePath, "w");

	syslog(LOG_DEBUG, "Writing %s to %s", writeStr,filePath);	
	fwrite(writeStr, sizeof(char), strlen(writeStr), file);

	fclose(file);

	closelog();

	
	return 0;
}
