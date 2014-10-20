/*
#    Nameless shell
#
#    gcc ./nsh.c -o nsh
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

struct aliasNode {
    char alias[64];
    char cmd[256];
    struct aliasNode *next;
};

typedef struct aliasNode Alias;

void sigintHandler(int sig_num);
void printPrompt(char *user, char *host, char *home, char *cwd);
void executeCommand(char **argv);
void aliasCheck(Alias **head, char *buffer);
void printHelp();
void aliasListInsert(Alias **head, Alias *newAlias);
void aliasFind(Alias **head, char *arg);


int main() {

    char buffer[1024];               // Store one line of stdin
    char cwd[1024];                  // Store current working directory
    char *argv[64];                  // Initialize char pointer array for args
    char *a[2];                      // For alias and command from .nsh_alias
    char host[64];                   // Store hostname
    char ahome[64];                  // Store alias file path
    char *home, *user, *tmp, *t;     // Some needed char pointers
    int i;                           // For counting things
    size_t len = 0;                  // For file read/write
    ssize_t read;                    // For file read/write
    FILE *fp;                        // For file read/write

    // Get home dir, remove last slash
    home = getenv("HOME");
    strncpy(home, home, sizeof(home) - 1);
    // Get localhost hostname
    gethostname(host, sizeof(host));
    // Get user for session
    user = getlogin();

    // Initialize head of alias list
    Alias *alias_list = NULL;
    Alias *newAlias = NULL;

    // Setup CTRL+C trap
    signal(SIGINT, sigintHandler);

    // Check for ~/.nsh_alias
    strcpy(ahome, home);
    strcat(ahome, "/.nsh_alias");
    // If the alias file doesn't exist build skeleton file
    if (access(ahome, F_OK) == -1) {
        fp = fopen(ahome, "a+"); 
        fputs("###\n#\n# nsh aliases\n#\n###\n#\n\nalias ll=\'ls -lAh\'\n", fp);
        rewind(fp);
    }
    // Otherwise open for normal reading
    else { fp = fopen(ahome, "r"); }
    if (fp == NULL) { exit(EXIT_FAILURE); }
    // Build linked lists of aliases
    while ((read = getline(&tmp, &len, fp)) != -1) {
        if (strncmp(tmp, "alias", strlen("alias")) == 0) {
            newAlias = (Alias *) malloc(sizeof(Alias));
            t = tmp;
            a[0] = strsep(&t, "=");
            a[1] = strsep(&t, "=");
            memmove(newAlias->alias, a[0] + strlen("alias "), strlen(a[0]) - strlen("alias "));
            memmove(newAlias->cmd, a[1] + 1, strlen(a[1]) - 3);
            aliasListInsert(&alias_list, newAlias);
        } 
    }
    fclose(fp);

    /*  Ye Olde Loop of Brief Eternity  */
    while (1) {

        // get cwd, print new prompt
        getcwd(cwd, sizeof(cwd));
        printPrompt(user, host, home, cwd);

        // Read stdin until newline or eof read
        i = 0;
        buffer[i] = getc(stdin);
        while (buffer[i] != '\n' && buffer[i] != EOF) { buffer[++i] = getc(stdin); }
        // If EOF, time to leave.
        if (buffer[i] == EOF) { printf("\e[1m\x1b[32mBuh bye.\x1b[0m\e[0m\n"); exit(0); }
        // Otherwise, replace '\n' with '\0'
        else { buffer[i] = '\0'; }

        // Alias mutation on buffer
        aliasCheck(&alias_list, buffer);

        // Make char pointer array pointing to whitespace seperated element groups in buffer
        i = 0;
        tmp = buffer;
        argv[i] = strsep(&tmp, " \t");
        while ((i < sizeof(argv) - 1) && (argv[i] != '\0')) { argv[++i] = strsep(&tmp, " \t"); }

        // Check argv[0] for . . .
        // Empty commands
        if (strcmp(argv[0], "\0") == 0) {  }
        // cd and chdir
        else if (strcmp(argv[0], "cd") == 0 || strcmp(argv[0], "chdir") == 0) { 
            // If cd has an argument check for ~
            if (argv[1]) {
                if (strncmp(argv[1], "~", 1) == 0) {
                    memmove(argv[1] + strlen(home), argv[1] + 1, strlen(argv[1]));
                    memmove(argv[1], home, strlen(home));
                }
            }
            // If cd has no arguments use home
            else { argv[1] = home; }
            chdir(argv[1]);
        }
        // Alias print command
        else if (strcmp(argv[0], "alias") == 0) { if (argv[1]) { aliasFind(&alias_list, argv[1]); } else { aliasFind(&alias_list, "alias"); } }
        // Quit commands and other
        else if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "logout") == 0 || 
                 (strcmp(argv[0], ":q") == 0) || strcmp(argv[0], "quit") == 0) { exit(0); }
        else if (strcmp(argv[0], "halp") == 0 || strcmp(argv[0], "help") == 0) { printHelp(); }
        // Otherwise prepare argv for child
        else {
            // Make child process, execute argv
            executeCommand(argv);
        }
        // Clear buffer
        memset(buffer, 0, sizeof(buffer));
    }
    exit(0);
}


/* Print corresponding command for alias arg passed in */
void aliasFind(Alias **head, char *arg) {
    Alias *curr = *head;
    while (curr != NULL && strcmp(curr->alias, arg) != 0) { if (strcmp(arg, "alias") == 0) { printf("\e[1m%s:\e[0m %s\n", curr->alias, curr->cmd); } curr = curr->next; }
    if (curr != NULL) { printf("\e[1m%s:\e[0m %s\n", curr->alias, curr->cmd); }
}


/* Check for alias, mutate buffer accordingly. */
void aliasCheck(Alias **head, char *buffer) {
    Alias *curr = *head;
    while (curr != NULL) {
        if (strcmp(buffer, curr->alias) == 0) { memmove(buffer, curr->cmd, strlen(curr->cmd)); }
        curr = curr->next;
    }
    // Append --color to all ls commands, may be debian specific?
    if (strncmp(buffer, "ls", strlen("ls")) == 0) { memmove(buffer + strlen(buffer), " --color\0", strlen(" --color\0")); }
}


/* Print that fancy prompt. */
void printPrompt(char *user, char *host, char *home, char *cwd) {
    // Replace home in cwd with ~
    if (strncmp(cwd, home, sizeof(home)) == 0) {
        memmove(cwd + 1, cwd + strlen(home), strlen(cwd));
        memmove(cwd, "~", 1);
    }
    // Shorten cwd if too long
    if (strlen(cwd) > 48) {
        memmove(cwd + 2, cwd + strlen(cwd) - 48, 49);
        memmove(cwd, "..", 2);
    }
    // Print prompt
    printf("\e[1m\x1b[32m%s@\x1b[32m%s\x1b[34m %s \x1b[36m> \x1b[0m\e[0m", user, host, cwd);
}


/* Fork a child; child executes argv, parent waits for child. */
void executeCommand(char **argv) {
    int cpid = fork();
    int status;
    if (cpid < 0) { exit(EXIT_FAILURE); }
    else if (cpid == 0) { 
        if (execvp(argv[0], argv) != 0) {
            printf("\e[1m\x1b[31m --- Error:\x1b[0m\e[0m %s invalid command.\n", argv[0]);
            exit(0);
        }
    }
    else { waitpid(cpid, &status, 0); }
}


/* Handle SIGINT (CTRL+C) */
void sigintHandler(int sig_num) { printf("\b\b  \b\b"); fflush(stdout); }


/* Print out some helpful stuff maybe */
void printHelp() {
    printf("\t     \"Nameless shell..\"");
    printf("\n    (>^.^)>  __/\n");
    sleep(1);
    printf("\t       \\\n\t   \".. also a helpless shell!\"\n");
}


/* Insert alias object into alias list */
void aliasListInsert(Alias **head, Alias *newAlias) {
    // Base case of empty list
    if (*head == NULL) {
        newAlias->next = *head;
        *head = newAlias;
    }
    // Otherwise find end of list to append
    else {
        Alias *curr = *head;
        while (curr->next != NULL) { curr = curr->next; }
        newAlias->next = curr->next;
        curr->next = newAlias;
    }
}
