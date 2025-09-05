Just a mini-shell that doesn't do much.

Used sigaction from the 'signal.h' header for interrupt signal handling to prevent the shell from terminating when user inputs Ctrl + c. But the child processes inside the shell are still terminated using Ctrl + C command.

