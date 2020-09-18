#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define BUFFER_SIZE 1024
#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64

void execute_command(char **tokens, int command_start_index, int stdin_fd);
int get_next_pipe_index(char **tokens, int command_start_index);
int check_exit_status(int status);

/* Splits the string by space and returns the array of tokens
*
*/
char **tokenize(char *line)
{
  char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *)); // 분리된 토큰들을 저장할 배열
  char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char)); // 토큰을 분리하는데 사용할 배열
  int i, tokenIndex = 0, tokenNo = 0;

  // 명령어(line)에서 white space로 구분된 토큰들을 분류해서 tokens에 각각 저장한다
  for(i =0; i < strlen(line); i++){

    char readChar = line[i]; // line을 한글자씩 읽는다

    if (readChar == ' ' || readChar == '\n' || readChar == '\t'){ // 읽은 문자가 white space라면
      token[tokenIndex] = '\0';
      if (tokenIndex != 0){ // 읽어들인 토큰이 있는 경우 (해당 토큰을 다 읽어들임)
	tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char)); // 토큰을 저장할 새로운 저장공간 할당
	strcpy(tokens[tokenNo++], token); // 새로운 저장공간에 분리해둔 토큰을 복사
	tokenIndex = 0; // tokenIndex를 0으로 초기화 하여 다음 토큰을 읽을 준비를 한다.
      }
    } else { // 읽은 문자가 white space가 아닌 경우 (토큰을 구성하는 문자인 경우)
      token[tokenIndex++] = readChar; // token 배열에 덧붙여 저장.
    }
  }
 
  free(token);
  tokens[tokenNo] = NULL ; // 끝이라는걸 표시하기 위해 마지막 토큰 다음에 NULL을 넣는다.
  return tokens; // 분리된 토큰들 리턴
}


int main(int argc, char* argv[]) {
	char  line[MAX_INPUT_SIZE]; // 명령어 입력받을 배열 
	char  **tokens; // 명령어에서 분리한 토큰들이 저장된 배열을 담을 포인터 
	int i;
	char current_dir_name[BUFFER_SIZE];
	char *path_env_value;
	char new_path_env_value[BUFFER_SIZE];

	// for run ttop and pps commands, add current working directory to PATH env
	getcwd(current_dir_name, BUFFER_SIZE);
	path_env_value = getenv("PATH");
	sprintf(new_path_env_value, "%s:%s", path_env_value, current_dir_name);
	setenv("PATH", new_path_env_value, 1);

	FILE* fp;
	if(argc == 2) { // 배치식인 경우 파일 open
		fp = fopen(argv[1],"r");
		if(fp < 0) {
			printf("File doesn't exists.");
			return -1;
		}
	}


	while(1) {			
		/* BEGIN: TAKING INPUT */
		bzero(line, sizeof(line));
		if(argc == 2) { // batch mode
			if(fgets(line, sizeof(line), fp) == NULL) { // file reading finished
				break;	
			}
			line[strlen(line) - 1] = '\0';
		} else { // interactive mode
			printf("$ ");
			scanf("%[^\n]", line);
			getchar();
		}
		//printf("Command entered: %s (remove this debug output later)\n", line); 
		/* END: TAKING INPUT */

		line[strlen(line)] = '\n'; //terminate with new line -> tokenize()의 정상적 수행을 위해 명령어 마지막 문자를 white space로 해야함
		tokens = tokenize(line);
   
       //do whatever you want with the commands, here we just print them

//		for(i=0;tokens[i]!=NULL;i++){
//			printf("found token %s (remove this debug output later)\n", tokens[i]);
//		}


		// 여기서 토큰 이용해 명령어 실행
		execute_command(tokens, 0, 0);


		// Freeing the allocated memory	
		for(i=0;tokens[i]!=NULL;i++){
			free(tokens[i]);
		}
		free(tokens);

	}
	return 0;
}

void execute_command(char **tokens, int command_start_index, int stdin_fd){
	pid_t pid;
	int status;
	int pipe_fd[2];
	int pipe_index;

	if (!tokens[command_start_index]) { // 첫번째 토큰이 NULL인 경우
		fprintf(stderr, "invalid command.\n");
		return;
	}

	if ((pipe_index = get_next_pipe_index(tokens, command_start_index)) > 0) { // 실행하지 않은 토큰 중 파이프 명령어 있다면
		tokens[pipe_index] = NULL;
		if (pipe(pipe_fd) == -1) { // pipe 생성
			fprintf(stderr, "pipe() error.\n");
		}
	}

	if ((pid = fork()) > 0) { // 부모 프로세스
		waitpid(pid, &status, WUNTRACED);
		if (!check_exit_status(status)){ // 자식 프로세스가 정상적으로 종료되지 않았다면
			return;
		}
		if (pipe_index > 0) { // 파이프 명령어 있었다면
			close(pipe_fd[1]); // 안쓰는 파이프 파일 close
			// execute_command 재귀호출하며 stdin_fd로 파이프 넘겨줌, command_start_index는 pipe_index + 1
			execute_command(tokens, pipe_index + 1, pipe_fd[0]);
			// pipe close
			close(pipe_fd[0]);
		}
	} else if (pid == 0) { // 자식 프로세스
		if (pipe_index > 0) {
			close(pipe_fd[0]); // 안쓰는 파이프 파일 close
			if (dup2(pipe_fd[1], 1) != 1) { // 표준 출력을 파이프로 리디렉션, 이 이후부터 exec 까지 표준 출력에 출력 하면 절대 안됨!!!
				fprintf(stderr, "dup2() error 2\n");
			}
		}

		if (stdin_fd > 0) { // 파이프 뒤에 있는 명령어의 경우
			dup2(stdin_fd, 0); // 표준 입력을 파이프로 리디렉션
		}

		if (execvp(tokens[command_start_index], tokens + command_start_index) < 0) {
			fprintf(stderr, "command execution failed.\n");
			_exit(1);
		}

	} else { // fork 에러
		fprintf(stderr, "fork() error.\n");
	}

	return;
}

int check_exit_status(int status) {
	if (WIFEXITED(status)) {
		return 1;
	} else if (WIFSIGNALED(status)) {
		fprintf(stderr, "abnormal termination, signal number = %d%s\n", 
				WTERMSIG(status), 
#ifdef WCOREDUMP
				WCOREDUMP(status) ? " (core file generated)" : "");
#else
				"");
#endif
		return 0;
	} else if (WIFSTOPPED(status)) { 
		fprintf(stderr, "child stopped, signal number = %d\n", WSTOPSIG(status));
		return 0;
	}

	return 0;
}

// 토큰중에서 command_start_index 뒤의 첫번째 pipe의 인덱스 리턴, 없을경우 -1 리턴
int get_next_pipe_index(char **tokens, int command_start_index){
	int pipe_index = -1;
	int token_index = command_start_index;

	while (tokens[token_index]) {
		if (!strcmp(tokens[token_index], "|")) {
			pipe_index = token_index; // 파이프 찾으면 인덱스 저장하고
			break; // break
		}
		++token_index; // 다음 토큰으로 이동
	}

	return pipe_index;
}
