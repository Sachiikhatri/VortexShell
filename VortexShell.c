#include <stdio.h>         
#include <stdlib.h>        
#include <string.h>      
#include <unistd.h>        
#include <sys/wait.h>      
#include <sys/types.h>     
#include <sys/stat.h>      
#include <fcntl.h>         
#include <signal.h>        
// Configuration constants to enforce shell limits
#define MAX_CMD_LEN 256        // Maximum characters allowed in a single command input
#define MAX_ARGS 6             // Restricts commands to 5 arguments plus the command name
#define MAX_PIPES 5            // Caps the number of commands that can be piped together
#define MAX_SEQUENTIAL 4       // Limits sequential commands separated by semicolons
#define MAX_LINE 256           // General-purpose buffer size for strings and lines
#define CMDLINE_PATH "/proc/self/cmdline" // System path to fetch this process's command line

// Command structure to organize execution parameters
typedef struct {
    char *args[MAX_ARGS];      // Array to hold command and its arguments for execvp
    int argc;                  // Counter for the number of arguments parsed
    char *input_file;          // Pointer to filename for input redirection (<)
    char *output_file;         // Pointer to filename for output redirection (>)
    int append_output;         // Boolean flag: 1 for append (>>), 0 for overwrite (>)
} Command;

// Function prototypes for modular design and forward declaration
void parse_command(char *input, Command *cmd);              // Breaks down input into a Command struct
void execute_single_command(Command *cmd);                 // Runs a single command with I/O redirection
void handle_pipes(Command *commands, int num_commands);     // Orchestrates standard pipe execution
void handle_reverse_pipes(Command *commands, int num_commands); // Manages reverse pipe execution
void handle_file_append(char *file1, char *file2);         // Swaps and appends content between two files
void count_words(char *filename);                          // Calculates word count in a file
void concatenate_files(char **files, int num_files);       // Merges file contents to stdout
void execute_sequential_commands(char *commands[], int num_commands); // Executes commands in order
void execute_conditional(Command *commands, int num_commands, char *operators); // Handles &&/|| logic
void free_command(Command *cmd);                           // Releases dynamically allocated memory
void get_process_name(char *name, size_t size);            // Extracts this process's name
void kill_all_shells(char *self_name);                     // Terminates all shell instances

// Global variable to unify process group management
pid_t shell_pgid;              // Process group ID to keep child processes organized

int main() {
    char input[MAX_CMD_LEN];       // Buffer to capture user input from stdin
    char self_name[MAX_LINE];      // Stores this process's name for identification
    
    shell_pgid = getpid();         // Set this process's PID as the group leader
    setpgid(0, 0);                 // Assign this process to its own process group for signal control
    
    // Fetch the executable name to use in kill_all_shells
    get_process_name(self_name, sizeof(self_name)); // Populates self_name with process basename
    
    // Main shell loop to continuously accept and process commands
    while (1) {
        printf("w25shell$ ");      // Display a unique, hardcoded shell prompt
        fflush(stdout);            // Force the prompt to appear immediately on the terminal
        
        // Capture user input; skip to next iteration if fgets fails (e.g., EOF)
        if (!fgets(input, MAX_CMD_LEN, stdin)) {
            continue;              // Avoid processing invalid or empty input
        }
        
        input[strcspn(input, "\n")] = 0;  // Strip the newline character from input
        
        // Check for built-in termination commands before parsing
        if (strcmp(input, "killterm") == 0) {
            exit(0);               // Exit this shell instance cleanly
        }
        if (strcmp(input, "killallterms") == 0) {
            kill_all_shells(self_name); // Attempt to kill all running instances of this shell
            exit(0);               // Terminate this instance after kill operation
        }

        int num_commands = 0;      // Counter for the number of commands parsed from input
        
        // Detect and handle pipe operator (|) for sequential command execution
        if (strchr(input, '|') && !strstr(input, "||")) {
            char *commands_str[MAX_PIPES + 1]; // Array to hold command strings split by '|'
            char *token = strtok(input, "|");  // Begin tokenizing input at pipe symbols
            while (token && num_commands <= MAX_PIPES) {
                commands_str[num_commands++] = token; // Store each command segment
                token = strtok(NULL, "|"); // Move to next token
            }
            Command commands[MAX_PIPES + 1];   // Array to store parsed Command structs
            for (int i = 0; i < num_commands; i++) {
                parse_command(commands_str[i], &commands[i]); // Parse each command string
            }
            handle_pipes(commands, num_commands); // Execute commands with pipe logic
        }
        // Handle reverse pipe operator (=) for reverse-order execution
        else if (strchr(input, '=')) {
            char *commands_str[MAX_PIPES + 1]; // Temporary storage for command segments
            char *token = strtok(input, "=");  // Split input by '='
            while (token && num_commands <= MAX_PIPES) {
                commands_str[num_commands++] = token; // Collect each command
                token = strtok(NULL, "=");
            }
            Command commands[MAX_PIPES + 1];   // Array for parsed commands
            for (int i = 0; i < num_commands; i++) {
                parse_command(commands_str[i], &commands[i]); // Parse into Command structs
            }
            handle_reverse_pipes(commands, num_commands); // Execute in reverse order
        }
        // Process file append operator (~) to swap and append file contents
        else if (strchr(input, '~')) {
            char *file1 = strtok(input, "~");  // Extract first file name
            char *file2 = strtok(NULL, "~");   // Extract second file name
            if (file1 && file2) {              // Ensure both files are provided
                // Remove leading whitespace from file1
                while (*file1 == ' ') file1++;
                // Trim trailing whitespace from file1
                char *end1 = file1 + strlen(file1) - 1;
                while (end1 > file1 && *end1 == ' ') *end1-- = '\0';
                
                // Remove leading whitespace from file2
                while (*file2 == ' ') file2++;
                // Trim trailing whitespace from file2
                char *end2 = file2 + strlen(file2) - 1;
                while (end2 > file2 && *end2 == ' ') *end2-- = '\0';
                
                handle_file_append(file1, file2); // Perform the append operation
            } else {
                printf("Error: Two text files required for ~ operation\n"); // Error if missing files
            }
        }
        // Handle word count operator (#) for counting words in a file
        else if (input[0] == '#') {
            char *filename = input + 1;    // Skip '#' to access filename
            while (*filename == ' ') filename++; // Skip any leading spaces
            count_words(filename);         // Compute and display word count
        }
        // Process file concatenation operator (+) to merge files to stdout
        else if (strchr(input, '+')) {
            char *files[MAX_PIPES + 1];    // Array to store file names
            int num_files = 0;             // Counter for number of files
            char *token = strtok(input, "+"); // Split input by '+'
            while (token && num_files <= MAX_PIPES) {
                while (*token == ' ') token++; // Trim leading spaces
                char *end = token + strlen(token) - 1; // Point to end of token
                while (end > token && *end == ' ') *end-- = '\0'; // Trim trailing spaces
                files[num_files++] = token; // Add file to array
                token = strtok(NULL, "+");  // Next token
            }
            concatenate_files(files, num_files); // Concatenate and output files
        }
        // Handle sequential command execution with semicolon (;)
        else if (strchr(input, ';')) {
            char *commands_str[MAX_SEQUENTIAL]; // Array for sequential commands
            char input_copy[MAX_CMD_LEN];       // Copy of input to avoid modifying original
            strncpy(input_copy, input, MAX_CMD_LEN - 1); // Safe copy
            input_copy[MAX_CMD_LEN - 1] = '\0'; // Ensure null termination
            
            char *token = strtok(input_copy, ";"); // Split by semicolon
            num_commands = 0;
            while (token && num_commands < MAX_SEQUENTIAL) {
                while (*token == ' ') token++; // Remove leading spaces
                char *end = token + strlen(token) - 1; // End of current token
                while (end > token && *end == ' ') *end-- = '\0'; // Remove trailing spaces
                if (strlen(token) > 0) {   // Ignore empty commands
                    commands_str[num_commands++] = token; // Store command
                }
                token = strtok(NULL, ";"); // Move to next command
            }
            if (num_commands > 0) {        // Execute if there are valid commands
                execute_sequential_commands(commands_str, num_commands);
            }
        }
        // Process conditional execution with && (AND) or || (OR)
        else if (strstr(input, "&&") || strstr(input, "||")) {
            char input_copy[MAX_CMD_LEN];  // Working copy of input
            strncpy(input_copy, input, MAX_CMD_LEN - 1); // Safe copy
            input_copy[MAX_CMD_LEN - 1] = '\0'; // Null terminate
            
            Command commands[MAX_PIPES + 1]; // Array for parsed commands
            char operators[MAX_PIPES];       // Array to store operator types (& or |)
            char *token = input_copy;        // Start tokenizing from input copy
            int op_idx = 0;                  // Index for operators array
            num_commands = 0;                // Command counter
            while (token && num_commands <= MAX_PIPES) {
                char *and_pos = strstr(token, "&&"); // Find AND operator
                char *or_pos = strstr(token, "||");  // Find OR operator
                if (and_pos && (!or_pos || and_pos < or_pos)) { // AND takes precedence
                    *and_pos = '\0';         // Split at &&
                    parse_command(token, &commands[num_commands++]); // Parse command
                    operators[op_idx++] = '&'; // Record AND operator
                    token = and_pos + 2;     // Move past &&
                    while (*token == ' ') token++; // Skip spaces
                }
                else if (or_pos) {           // OR operator found
                    *or_pos = '\0';          // Split at ||
                    parse_command(token, &commands[num_commands++]); // Parse command
                    operators[op_idx++] = '|'; // Record OR operator
                    token = or_pos + 2;      // Move past ||
                    while (*token == ' ') token++; // Skip spaces
                }
                else {                       // No more operators
                    parse_command(token, &commands[num_commands++]); // Parse final command
                    break;                   // Exit loop
                }
            }
            if (num_commands > 0) {         // Execute if commands exist
                execute_conditional(commands, num_commands, operators);
            }
        }
        // Default case: execute a single command
        else {
            Command cmd = {0};             // Initialize empty Command struct
            parse_command(input, &cmd);    // Parse the input into cmd
            if (cmd.argc > 0) {            // Only execute if arguments were parsed
                execute_single_command(&cmd); // Run the command
            }
            free_command(&cmd);            // Clean up allocated memory
        }
    }
    return 0;                             // Exit main (unreachable due to infinite loop)
}

// Parses raw input into a structured Command object for execution
void parse_command(char *input, Command *cmd) {
    cmd->argc = 0;            // Reset argument count to zero
    cmd->input_file = NULL;   // No input file by default
    cmd->output_file = NULL;  // No output file by default
    cmd->append_output = 0;   // Default to overwrite mode for output
    
    char input_copy[MAX_CMD_LEN]; // Local copy to avoid modifying original input
    strncpy(input_copy, input, MAX_CMD_LEN - 1); // Copy input safely
    input_copy[MAX_CMD_LEN - 1] = '\0'; // Ensure null termination
    
    char *token = strtok(input_copy, " "); // Start tokenizing by spaces
    while (token && cmd->argc < MAX_ARGS) { // Continue until no tokens or limit reached
        if (strcmp(token, "<") == 0) {     // Check for input redirection
            cmd->input_file = strtok(NULL, " "); // Set input file from next token
            token = strtok(NULL, " ");     // Skip to next token
            continue;                      // Move to next iteration
        }
        if (strcmp(token, ">") == 0) {     // Check for output redirection (overwrite)
            cmd->output_file = strtok(NULL, " "); // Set output file
            cmd->append_output = 0;        // Set overwrite mode
            token = strtok(NULL, " ");     // Skip to next token
            continue;
        }
        if (strcmp(token, ">>") == 0) {    // Check for output redirection (append)
            cmd->output_file = strtok(NULL, " "); // Set output file
            cmd->append_output = 1;        // Set append mode
            token = strtok(NULL, " ");     // Skip to next token
            continue;
        }
        cmd->args[cmd->argc] = strdup(token); // Duplicate token into args array
        cmd->argc++;                       // Increment argument count
        token = strtok(NULL, " ");         // Move to next token
    }
    cmd->args[cmd->argc] = NULL;           // Null-terminate the args array for execvp
    
    // Enforce argument count limits (1-5 args + command)
    if (cmd->argc < 1 || cmd->argc > 5) {
        printf("Error: Command '%s' must have 1-5 arguments\n", cmd->args[0] ? cmd->args[0] : input);
        free_command(cmd);                 // Clean up on error
        cmd->argc = 0;                     // Reset argc to indicate failure
    }
}

// Frees dynamically allocated memory within a Command structure
void free_command(Command *cmd) {
    for (int i = 0; i < cmd->argc; i++) {  // Iterate through all arguments
        if (cmd->args[i]) {                // Check if pointer is non-null
            free(cmd->args[i]);            // Release allocated memory
            cmd->args[i] = NULL;           // Prevent double-free
        }
    }
    cmd->argc = 0;                         // Reset argument count to zero
}

// Executes a single command with possible I/O redirection
void execute_single_command(Command *cmd) {
    if (cmd->argc == 0) return;            // Exit if no valid command to execute
    
    pid_t pid = fork();                    // Fork to create a child process
    if (pid == 0) {                        // Child process block
        setpgid(0, shell_pgid);            // Assign child to shell's process group
        
        if (cmd->input_file) {             // Handle input redirection if specified
            int fd = open(cmd->input_file, O_RDONLY); // Open file for reading
            if (fd < 0) {                  // Check for file open failure
                printf("Error: Cannot open input file %s\n", cmd->input_file);
                exit(1);                   // Exit child with error
            }
            dup2(fd, STDIN_FILENO);        // Redirect stdin to file
            close(fd);                     // Close file descriptor
        }
        if (cmd->output_file) {            // Handle output redirection if specified
            int fd = open(cmd->output_file, 
                         O_WRONLY | O_CREAT | (cmd->append_output ? O_APPEND : O_TRUNC), 
                         0666);            // Open file with write/create, mode based on append
            if (fd < 0) {                  // Check for file open failure
                printf("Error: Cannot open output file %s\n", cmd->output_file);
                exit(1);                   // Exit child with error
            }
            dup2(fd, STDOUT_FILENO);       // Redirect stdout to file
            close(fd);                     // Close file descriptor
        }
        execvp(cmd->args[0], cmd->args);   // Execute the command with arguments
        // If execvp returns, it failed
        printf("Error: Command '%s' not found\n", cmd->args[0]);
        exit(1);                       // Exit child with error status
    }
    wait(NULL);                            // Parent waits for child to finish
    free_command(cmd);                     // Clean up command memory in parent
}

// Executes multiple commands sequentially as separated by semicolons
void execute_sequential_commands(char *commands[], int num_commands) {
    for (int i = 0; i < num_commands; i++) { // Loop through each command
        Command cmd = {0};                   // Initialize a new Command struct
        char command_copy[MAX_CMD_LEN];      // Local copy of command string
        strncpy(command_copy, commands[i], MAX_CMD_LEN - 1); // Safe copy
        command_copy[MAX_CMD_LEN - 1] = '\0'; // Null terminate
        
        char *args[MAX_ARGS + 1];            // Temporary array for arguments
        int arg_count = 0;                   // Counter for arguments
        char *token = strtok(command_copy, " "); // Tokenize command by spaces
        while (token && arg_count < MAX_ARGS) { // Parse tokens
            args[arg_count++] = token;       // Store token
            token = strtok(NULL, " ");       // Next token
        }
        args[arg_count] = NULL;              // Null terminate args array

        if (arg_count > 0) {                 // Proceed if arguments exist
            cmd.argc = arg_count;            // Set argument count
            for (int j = 0; j < arg_count; j++) { // Copy arguments into cmd
                cmd.args[j] = strdup(args[j]); // Duplicate string for safety
            }
            cmd.args[arg_count] = NULL;      // Null terminate cmd args

            if (cmd.argc < 1 || cmd.argc > 5) { // Validate argument count
                printf("Error: Command '%s' must have 1-5 arguments\n", cmd.args[0]);
                free_command(&cmd);          // Clean up on error
            } else {
                execute_single_command(&cmd); // Execute valid command
            }
        }
    }
}

// Manages execution of commands connected by standard pipes (|)
void handle_pipes(Command *commands, int num_commands) {
    int pipes[MAX_PIPES][2];               // Array of pipe file descriptors
    pid_t pids[MAX_PIPES + 1];             // Array to store child PIDs
    
    // Create pipes for all commands except the last
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) < 0) {          // Attempt to create a pipe
            printf("Error: Pipe creation failed\n"); // Report failure
            return;                        // Abort function
        }
    }
    
    // Fork and execute each command in the pipeline
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();                  // Create a child process
        if (pids[i] == 0) {                // Child process
            if (i > 0) {                   // Not the first command
                dup2(pipes[i-1][0], STDIN_FILENO); // Redirect stdin from previous pipe
                close(pipes[i-1][1]);      // Close write end of previous pipe
            }
            if (i < num_commands - 1) {    // Not the last command
                dup2(pipes[i][1], STDOUT_FILENO); // Redirect stdout to next pipe
                close(pipes[i][0]);        // Close read end of current pipe
            }
            // Close all pipe ends to prevent leaks in child
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);        // Close read end
                close(pipes[j][1]);        // Close write end
            }
            execvp(commands[i].args[0], commands[i].args); // Execute command
            printf("Error: Command not found\n"); // Exec failed if this runs
            exit(1);                       // Exit child with error
        }
    }
    
    // Parent closes all pipe ends to avoid interference
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);                // Close read end
        close(pipes[i][1]);                // Close write end
    }
    // Wait for all child processes to complete
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], NULL, 0);         // Wait for specific child
        free_command(&commands[i]);        // Free command memory
    }
}

// Executes commands with reverse pipe logic using '=' operator
void handle_reverse_pipes(Command *commands, int num_commands) {
    int pipes[MAX_PIPES][2];               // Pipe file descriptors for reverse piping
    pid_t pids[MAX_PIPES + 1];             // PIDs for child processes
    
    // Set up pipes for reverse execution
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) < 0) {          // Create pipe
            printf("Error: Pipe creation failed\n"); // Report error
            return;                        // Exit on failure
        }
    }
    
    // Fork and execute commands in reverse order
    for (int i = num_commands - 1; i >= 0; i--) {
        pids[i] = fork();                  // Create child process
        if (pids[i] == 0) {                // Child process
            if (i < num_commands - 1) {    // Not the last command in reverse
                dup2(pipes[i][0], STDIN_FILENO); // Read from next pipe
                close(pipes[i][1]);        // Close write end
            }
            if (i > 0) {                   // Not the first command in reverse
                dup2(pipes[i-1][1], STDOUT_FILENO); // Write to previous pipe
                close(pipes[i-1][0]);      // Close read end
            }
            // Close all pipe ends in child
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);        // Close read end
                close(pipes[j][1]);        // Close write end
            }
            execvp(commands[i].args[0], commands[i].args); // Execute command
            printf("Error: Command not found\n"); // Exec failed
            exit(1);                       // Exit child with error
        }
    }
    
    // Parent closes all pipe ends
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);                // Close read end
        close(pipes[i][1]);                // Close write end
    }
    // Wait for all children and clean up
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], NULL, 0);         // Wait for child to finish
        free_command(&commands[i]);        // Free command memory
    }
}

// Handles file append operation (~) by cross-appending file contents
void handle_file_append(char *file1, char *file2) {
    FILE *f1_read = fopen(file1, "a+");   // Open file1 for reading and appending
    FILE *f2_read = fopen(file2, "a+");   // Open file2 for reading and appending
    
    if (!f1_read) {                        // Check if file1 opened successfully
        printf("Error: Cannot open or create file %s\n", file1);
        if (f2_read) fclose(f2_read);      // Close file2 if opened
        return;                            // Exit function
    }
    if (!f2_read) {                        // Check if file2 opened successfully
        printf("Error: Cannot open or create file %s\n", file2);
        fclose(f1_read);                   // Close file1
        return;                            // Exit function
    }

    // Get sizes of both files
    fseek(f1_read, 0, SEEK_END);           // Move to end of file1
    long f1_size = ftell(f1_read);         // Get file1 size
    fseek(f2_read, 0, SEEK_END);           // Move to end of file2
    long f2_size = ftell(f2_read);         // Get file2 size

    char *buf1 = malloc(f1_size + 1);      // Allocate buffer for file1 content
    char *buf2 = malloc(f2_size + 1);      // Allocate buffer for file2 content
    
    if (!buf1 || !buf2) {                  // Check for memory allocation failure
        printf("Error: Memory allocation failed\n");
        if (buf1) free(buf1);              // Free buf1 if allocated
        if (buf2) free(buf2);              // Free buf2 if allocated
        fclose(f1_read);                   // Close file1
        fclose(f2_read);                   // Close file2
        return;                            // Exit function
    }

    // Read file contents into buffers
    fseek(f1_read, 0, SEEK_SET);           // Rewind file1 to start
    fseek(f2_read, 0, SEEK_SET);           // Rewind file2 to start
    size_t read1 = fread(buf1, 1, f1_size, f1_read); // Read file1 content
    size_t read2 = fread(buf2, 1, f2_size, f2_read); // Read file2 content
    buf1[read1] = '\0';                    // Null terminate buffer1
    buf2[read2] = '\0';                    // Null terminate buffer2

    fclose(f1_read);                       // Close file1 after reading
    fclose(f2_read);                       // Close file2 after reading

    // Reopen files for appending
    FILE *f1_append = fopen(file1, "a");   // Open file1 for appending
    FILE *f2_append = fopen(file2, "a");   // Open file2 for appending
    
    if (!f1_append || !f2_append) {        // Check if reopening failed
        printf("Error: Cannot open files for appending\n");
        if (f1_append) fclose(f1_append);  // Close file1 if opened
        if (f2_append) fclose(f2_append);  // Close file2 if opened
        free(buf1);                        // Free buffer1
        free(buf2);                        // Free buffer2
        return;                            // Exit function
    }

    // Append file2 content to file1
    if (fwrite(buf2, 1, f2_size, f1_append) != f2_size) {
        printf("Error: Failed to append to %s\n", file1); // Report write failure
    }
    // Append file1 content to file2
    if (fwrite(buf1, 1, f1_size, f2_append) != f1_size) {
        printf("Error: Failed to append to %s\n", file2); // Report write failure
    }

    fclose(f1_append);                     // Close file1 after appending
    fclose(f2_append);                     // Close file2 after appending
    free(buf1);                            // Free buffer1 memory
    free(buf2);                            // Free buffer2 memory
}

// Counts the number of words in a specified file
void count_words(char *filename) {
    FILE *f = fopen(filename, "r");        // Open file for reading
    if (!f) {                              // Check if file opened successfully
        printf("Error: Cannot open file %s\n", filename);
        return;                            // Exit function on failure
    }
    
    int words = 0;                         // Accumulator for word count
    char c;                                // Character buffer for reading
    int in_word = 0;                       // Flag to track if currently in a word
    
    while ((c = fgetc(f)) != EOF) {        // Read file character by character
        if (c == ' ' || c == '\n' || c == '\t') { // Check for whitespace
            in_word = 0;                   // End of a word
        }
        else if (!in_word) {               // Start of a new word
            words++;                       // Increment word count
            in_word = 1;                   // Mark as inside a word
        }
    }
    
    printf("%d\n", words);                 // Print total word count
    fclose(f);                             // Close the file
}

// Concatenates multiple files and outputs to stdout
void concatenate_files(char **files, int num_files) {
    for (int i = 0; i < num_files; i++) {  // Iterate through each file
        FILE *f = fopen(files[i], "r");    // Open current file for reading
        if (!f) {                          // Check for open failure
            printf("Error: Cannot open file %s\n", files[i]);
            continue;                      // Skip to next file
        }
        
        fseek(f, 0, SEEK_END);             // Move to end of file
        long size = ftell(f);              // Get file size
        fseek(f, 0, SEEK_SET);             // Rewind to start
        
        char *buffer = malloc(size + 1);   // Allocate buffer for file content
        if (!buffer) {                     // Check for allocation failure
            printf("Error: Memory allocation failed for %s\n", files[i]);
            fclose(f);                     // Close file
            continue;                      // Skip to next file
        }
        
        size_t read_size = fread(buffer, 1, size, f); // Read file into buffer
        if (read_size > 0) {               // If content was read
            fwrite(buffer, 1, read_size, stdout); // Write to stdout
        }
        
        free(buffer);                      // Free allocated buffer
        fclose(f);                         // Close file
    }
    fflush(stdout);                        // Flush stdout to ensure output displays
}

// Executes commands conditionally based on && and || operators
void execute_conditional(Command *commands, int num_commands, char *operators) {
    int last_status = 0;                   // Tracks exit status of previous command
    for (int i = 0; i < num_commands; i++) { // Iterate through commands
        // Execute if first command, or AND succeeds, or OR fails
        if (i == 0 || 
            (operators[i-1] == '&' && last_status == 0) || 
            (operators[i-1] == '|' && last_status != 0)) {
            pid_t pid = fork();            // Fork a child process
            if (pid == 0) {                // Child process
                setpgid(0, shell_pgid);    // Join shell's process group
                execvp(commands[i].args[0], commands[i].args); // Execute command
                printf("Error: Command '%s' not found\n", commands[i].args[0]);
                exit(1);                   // Exit child on failure
            }
            int status;                    // Status variable for waitpid
            waitpid(pid, &status, 0);      // Wait for child to finish
            // Set last_status based on child's exit code
            last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
            free_command(&commands[i]);    // Free command memory
        } else {
            free_command(&commands[i]);    // Skip and clean up if condition fails
        }
    }
}

// Retrieves the current process's name from /proc/self/cmdline
void get_process_name(char *name, size_t size) {
    FILE *fp = fopen(CMDLINE_PATH, "r");   // Open proc file for reading
    if (fp) {                              // Check if file opened
        fread(name, 1, size - 1, fp);      // Read command line into name
        fclose(fp);                        // Close file
        // Extract basename from full path
        char *basename = strrchr(name, '/'); // Find last slash
        if (basename) {                    // If path exists
            strcpy(name, basename + 1);    // Copy basename to name
        }
    } else {                               // Handle failure to open file
        perror("Failed to get process name"); // Print error
        strcpy(name, "w25shell");          // Use default name as fallback
    }
}

// Kills all instances of this shell, self last
void kill_all_shells(char *self_name) {
    FILE *fp;                              // File pointer for popen
    char line[MAX_LINE];                   // Buffer for ps output lines
    pid_t pid = getpid();                  // Get current process ID
    
    // Open pipe to ps command to list user processes
    fp = popen("ps -u $(whoami) -o pid,comm", "r");
    if (!fp) {                             // Check if popen failed
        perror("Failed to run ps command"); // Report error
        return;                            // Exit function
    }

    int self_killed = 0;                   // Flag to defer killing self

    // Read ps output line by line
    while (fgets(line, sizeof(line), fp) != NULL) {
        int proc_id;                       // Process ID from ps
        char command[MAX_LINE];            // Command name from ps

        // Parse PID and command name from line
        if (sscanf(line, "%d %s", &proc_id, command) == 2) {
            if (strcmp(command, self_name) == 0) { // Match process name
                if (proc_id != pid) {      // Not the current process
                    printf("Killing process: %d (%s)\n", proc_id, command);
                    kill(proc_id, SIGKILL); // Send SIGKILL to terminate
                } else {                   // Current process
                    self_killed = 1;       // Mark for later kill
                }
            }
        }
    }

    pclose(fp);                            // Close pipe to ps command

    // Kill self last if marked
    if (self_killed) {
        printf("Killing self: %d (%s)\n", pid, self_name);
        kill(pid, SIGKILL);                // Terminate this process
    }
}