#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_URL_LENGTH 200
#define MAX_PROCESS_COUNT 500

int strings_count(FILE *file) {
  fseek(file, 0, SEEK_SET);
  int result = 0;
  char c;
  while ((c = fgetc(file)) != EOF) {
    if (c == '\n')
      result++;
  }

  fseek(file, 0, SEEK_SET);

  return result;
}

void goto_string(FILE *file, int num) {
  fseek(file, 0, SEEK_SET);

  char str[MAX_URL_LENGTH];
  for (int i = 0; i < num; i++)
    fgets(str, MAX_URL_LENGTH, file);
}

static int count = 0;
static int launched = 0;
static char DEF_NAME[] = "links.txt";

void wait_process(void) {
  static int i = 1;
  static int failures = 0;

  if (i > count)
    return;

  launched--;
  int status;
  wait(&status);
  if ((WIFEXITED(status) && WEXITSTATUS(status) != 0) || WIFSIGNALED(status))
    failures++;

  printf("Processing (%d/%d)... Successed: %d, Failures: %d\n", i, count,
         i - failures, failures);

  i++;
}

// Gets the contents of the url after '='
char *get_directory(char *url) {
  char *result = strchr(url, '=');
  if (result == NULL)
    result = url;
  else
    result++;

  return result;
}

int main(int argc, char **argv) {
  char *file_name = DEF_NAME;
  if (argc > 1)
    file_name = argv[1];

  if (fork() == 0) {
    close(1);
    close(2);
    execlp("youtube-dl", "youtube-dl", NULL);
    return 127;
  }

  int status;
  wait(&status);
  if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
    printf("Warning! The \"youtube-dl\" utility is required for the script to "
           "work.\nInstall it with your package manager and try again");
    return 1;
  }

  FILE *file = fopen(file_name, "r");
  if (file == NULL) {
    printf("No such file: %s\n", file_name);
    return 2;
  }

  count = strings_count(file);

  printf("Started. Total count: %d\n----------------------\n", count);

  int log_fd =
      open("youtube_log_output.txt", O_CREAT | O_WRONLY | O_TRUNC, 0666);

  for (int i = 0; i < count; i++) {
    if (launched > MAX_PROCESS_COUNT)
      wait_process();

    launched++;
    if (fork() == 0) {
      dup2(log_fd, 1);
      dup2(log_fd, 2);

      goto_string(file, i);

      char url[MAX_URL_LENGTH];
      fgets(url, MAX_URL_LENGTH, file);
      fclose(file);

      if (url[strlen(url) - 1] == '\n')
        url[strlen(url) - 1] = '\0';

      int dir_created = 0;
      char *directory;

      if (strstr(url, "playlist") != NULL) {
        directory = get_directory(url);

        char dir_command[MAX_URL_LENGTH + 6];
        strcpy(dir_command, "mkdir ");
        strcpy(dir_command + 6, directory);
        dir_created = system(dir_command) == 0;

        if (chdir(directory) != 0)
          return 2;
      }

      if (fork() == 0) {
        execlp("youtube-dl", "youtube-dl", url, NULL);
        return 127;
      }

      wait(&status);
      if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return 0;

      if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
        return 127;

      if (!dir_created)
        return 1;

      chdir("..");

      char del_command[MAX_URL_LENGTH + 7];
      strcpy(del_command, "rm -rf ");
      strcpy(del_command + 7, directory);
      system(del_command);

      return 1;
    }
  }
  close(log_fd);

  for (int i = 0; i < MAX_PROCESS_COUNT; i++)
    wait_process();

  printf("----------------------\nFinished!\n");

  fclose(file);
}
