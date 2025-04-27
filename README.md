🖥️ VortexShell - Advanced Custom Command Line Interface 🖥️
📜 Project Description
VortexShell is a custom-built, Unix-inspired command-line interface (CLI) designed to execute and manage commands with extended functionality over traditional shells. Developed from scratch in C using low-level POSIX system calls, it offers not only standard piping and file manipulation but also unique operations like reverse piping and dynamic file content merging.

VortexShell brings a new dimension of flexibility to command-line operations, giving users creative control over how processes, files, and executions interact — all inside a lightweight, efficient environment.

✨ Key Features
🔄 Piping Mechanisms
Standard Pipe (|): Chain commands by passing standard output of one as input to another.

Reverse Pipe (=): Reverse the traditional flow of data, feeding output backward between commands.

📄 Advanced File Operations
Swap and Append (~): Cross-append and swap content between two files seamlessly.

Concatenation (+): Merge multiple files into a single stream to stdout.

Word Counting (#): Quickly count words in a file with a special syntax.

⚡ Intelligent Command Execution
Sequential Execution (;): Execute commands one after another in strict sequence.

Conditional Execution (&&, ||): Perform commands conditionally based on the success or failure of previous commands.

📥 I/O Redirection
Input Redirection (<): Use a file as input for a command.

Output Redirection (>): Write output to a file (overwriting).

Output Append (>>): Append output to the end of a file without overwriting.

🔒 Robust Process Management
killterm: Terminate the current VortexShell instance safely.

killallterms: Terminate all instances of VortexShell simultaneously.

🔧 How It Works (Internals)
Command Parsing:
VortexShell reads user input, tokenizes it based on spaces and custom operators (|, =, ~, ;, &&, ||), and builds internal structures for processing.

Process Creation:
Uses fork(), execvp(), and waitpid() to create child processes for command execution, maintaining parent control for sequencing and signal handling.

Piping and Reverse Piping:
Standard pipes are implemented using pipe(), while reverse pipes rewire file descriptors (dup2()) to send data upstream.

Redirection Handling:
Redirects stdin/stdout streams to files dynamically before executing the actual commands.

Memory Management:
Dynamically allocates memory for command arguments and piping structures, ensuring stability even when executing multiple commands together.

Process Groups:
Sets up separate process groups for safe handling of termination signals.

🚀 Usage Examples
bash
Copy
# Standard pipe example
ls -l | grep "txt" | wc -l

# Reverse pipe (flow output backward)
cat file.txt = grep "keyword" = wc -l

# Cross-append content between two files
file1.txt ~ file2.txt

# Concatenate and print multiple files
file1.txt + file2.txt + file3.txt

# Count number of words in a file
#document.txt

# Conditional execution: success/failure branching
make project && echo "Build Successful" || echo "Build Failed"

# Sequential commands
echo "Initialize" ; mkdir new_dir ; cd new_dir
⚙️ Technical Specifications

Category	Limit
Maximum Command Length	256 characters
Maximum Arguments per Command	5
Maximum Piped Commands	5
Maximum Sequential Commands	4
Supported Redirections	<, >, >>
Supported Custom Operators	`
🏗️ Compilation Instructions
To compile VortexShell, use the following gcc command:

bash
Copy
gcc -o vortexshell vortexshell_sachi_khatri_110184013.c
This generates an executable named vortexshell.

🏃‍♂️ How to Run
After compiling:

bash
Copy
./vortexshell
You will be greeted with the custom VortexShell prompt where you can start executing your advanced CLI operations!

📉 Current Limitations
🚫 No background execution (&) or job control features.

🚫 No command history or tab completion like Bash.

🚫 Hard limit on maximum arguments per command (5).

🚫 No direct environment variable manipulation (export, etc.).

🚫 Limited error reporting for invalid syntax combinations.

🚫 Commands with more than 5 piped steps will be rejected.

🌟 Future Enhancements (Ideas)
Add support for background jobs and job tracking.

Implement basic tab-completion for known commands and filenames.

Extend argument and pipe limits dynamically.

Improve error handling and user feedback.

Build-in basic shell variables and simple scripting abilities.

📝 Author
👤 Sachi Khatri
🎓 University ID: 110184013
🛠️ Project for Systems Programming coursework

📚 Conclusion
VortexShell is more than just a basic shell — it's a dynamic environment that encourages experimentation with command-line operations through innovative piping and file manipulation tools. Designed for both stability and creativity, it reflects strong system-level programming concepts such as process management, file descriptor handling, and command parsing.

This project marks a solid foundation for further exploration into full-fledged shell development, operating system design, and advanced system programming topics.

🌪️ "Command the Chaos with VortexShell!"
