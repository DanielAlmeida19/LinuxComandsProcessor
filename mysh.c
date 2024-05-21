#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <signal.h>

#define MAX_SIZE 1024

typedef struct{
	
	// Nome do comando
	char name[MAX_SIZE];
	
	// Argumentos do comando
	char* args[MAX_SIZE];
	
}COMMAND;

// Nome do usuário
char *username;
	
// Nome do hospedeiro
char hostname[MAX_SIZE];

// Variável do endereço inicial
char *home;

// Vetor do pipe
int fd[2];

// Descritor de arquivo para a entrada do próximo comando
int prev_fd = 0;

// Função que executa os comandos 
int spawn (int indexCmd, int totalCommands, char* program, char** arg_list)
{
	pid_t child_pid;
	/* Duplicar este processo. */
	child_pid = fork ();
	
	
	if (child_pid != 0)
		/* Este é o processo pai. */
		return child_pid;
	else {
		// Fechando a leitura do pipe
		close(fd[0]);
		
		// Redireciona a entrada padrão para o pipe de entrada, exceto para o primeiro comando
		if (indexCmd != 0) {
			dup2(prev_fd, STDIN_FILENO);
        }
        
        // Redireciona a saída padrão para o pipe de saída, exceto para o último comando
        if (indexCmd != totalCommands - 1) {
            dup2(fd[1], STDOUT_FILENO);
        }
		
		/* Agora execute PROGRAM, buscando-o no path. */
		execvp (program, arg_list);
		/* A função execvp só retorna se um erro ocorrer. */
		fprintf (stderr, "um erro ocorreu em execvp: programa ou argumentos inexistentes\n");
		abort ();
	}
}

// Função que muda o caminho caso ele seja /home/{user}
void changeHomePath(char* path){
	char *token;
    const char delimiter[] = "/";

    // Usando strtok para dividir a string em partes
    token = strtok(path, delimiter);
    if (token != NULL && strcmp(token, "home") == 0) {
        token = strtok(NULL, delimiter);
        if (token != NULL && strcmp(token, username) == 0) {
            // Substituindo "home/aluno" por "~"
            strcpy(path, "~/");
            // Concatenando o restante do caminho, se houver
            token = strtok(NULL, "");
            if (token != NULL) {
                strcat(path, token);
            }
        }
    }
}

// Função responsável por mudar de diretório atual da shell
void cd(char *path, char requiredPath[MAX_SIZE]){
	
	
	
	char* finalPath;
	
	// cd sem argumentos, voltar para /home/{user}
	if (requiredPath == NULL){
		strcpy(path, home);
		chdir(path);
		return;
	}
	
	// cd requerido para /home/{user}
	if(strcmp(requiredPath,"~") == 0){
		strcpy(path, "~/");
		chdir(path);
		return;
	}
	
	// Salva o diretório atual em caso de erro
	finalPath = path;
	
	
	
	// Concatena argumento e verifica se o diretório existe
	
	if(chdir(requiredPath) == -1){
		fprintf(stderr, "O caminho requerido não existe\n"); // Caminho não existe
	}
}

// Função de tratamento para ignorar CTRL+C e CTRL+Z	
void handle_signal_do_nothing(int sig){
	// Do nothing
}

int main()
{
	
	// Registrando função de tratamento para CTRL+C
	signal(SIGINT, handle_signal_do_nothing);
	
	// Registrando função de tratamento para CTRL+Z
	signal(SIGTSTP, handle_signal_do_nothing);
	
	
	
	// Variável de endereço
	home = getenv("HOME");
	
	char input[MAX_SIZE];
	char *token;
	char *command;
	
	
	// Obtendo o nome do hospedeiro
	gethostname(hostname, sizeof(hostname));
	if(hostname == NULL){
		fprintf(stderr, "Não foi possível obter o nome do hospedeiro: %s\n", hostname);
		return EXIT_FAILURE;
	}
	
	// Obtendo o nome do usuário
	username = getenv("USER");
	if(username == NULL){
		fprintf(stderr, "Não foi possível obter o nome do usuário\n");
		return EXIT_FAILURE;
	}
	
	
	// Loop infinito que recebe comandos
	while(1){
		
		
		
		// Número de comandos
		int numCommands = 0;
		
		// Números de argumentos do comando
		int numTokens = 0;
		
		// Vetor de comandos
		char *commands[MAX_SIZE];
		
		// Vetor de argumentos do comando
		char *tokens[MAX_SIZE];
		
		// Diretório atual
		char cwd[MAX_SIZE];
		
		// Obtendo o endereço atual do shell
		if(getcwd(cwd, sizeof(cwd)) == NULL){
			fprintf(stderr, "Não foi possível obter o diretório atual\n");
			return EXIT_FAILURE;
		}
		
		// Caso o caminho seja home, modifica-o
		changeHomePath(cwd);
		
		// Exibindo linha do terminal
		printf("[MySh] %s@%s %s ", username, hostname, cwd);
		fflush(stdout);
		
		// Obtendo a entrada pelo teclado
		if(scanf(" %[^\n]%*c", input) == -1){
			printf("\nSaindo...\n");
			return EXIT_SUCCESS;
			
		}
		//fgets(input, sizeof(input), stdin);
		
		// Remove o \n do final da entrada
		input[strcspn(input, "\n")] = '\0';
		 
		// Desmembra a entrada por comandos
		command = strtok(input, "|");
		while(command != NULL && numCommands < MAX_SIZE){
			commands[numCommands++] = command;
			command = strtok(NULL, "|");
		}
		commands[numCommands] = NULL;
		

		for(int i = 0; i < numCommands; i++){
			
			pipe(fd);
			
			// Nome do comando
			char commandName[MAX_SIZE];
			
			// Zerando o vetor de argumentos
			memset(tokens, '\0', sizeof(tokens));
			numTokens = 0;
			tokens[1] = NULL;
			
			// Desmembra o comando por argumentos
			token = strtok(commands[i], " ");
			while(token != NULL && numTokens < MAX_SIZE){
				tokens[numTokens++] = token;
				token = strtok(NULL, " ");
			}
			
			// Obtendo o nome do comando
			strcpy(commandName, tokens[0]);
			
			// Caso seja comando de mudar de diretório
			if (strcmp(commandName, "cd") == 0){	
				cd(cwd, tokens[1]);
			} else
			if (strcmp(commandName, "exit") == 0){	
				
				printf("Saindo...\n");
				return EXIT_SUCCESS;
			} else {
				
				int pid = spawn(i, numCommands, commandName, tokens);
				if (pid != 0){
					
					close(fd[1]); // Fecha a escrita do pipe

					if (i != 0) {
						close(prev_fd); // Fecha o descritor de arquivo usado pelo comando anterior
					}

					prev_fd = fd[0]; // Mantém o descritor de arquivo para a entrada do próximo comando
					
				}
			}
		}
		
		// Aguarda todos os processos filhos terminarem
		for (int i = 0; i < numCommands; i++) {
			wait(NULL);
		}
		
	}
	
	return EXIT_SUCCESS;
}
