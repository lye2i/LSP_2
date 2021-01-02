#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

#define BUF_SIZE 1024
#define SECOND_TO_MICRO 1000000

int ssu_daemon_init(void); //디몬 코딩 수행하기
void ssu_runtime(struct timeval *begin_t, struct timeval *end_t); //실행시간 측정
void command_help(); //명령어 사용법 출력
int command_delete(char command_token[BUF_SIZE][BUF_SIZE]); //delete 명령어 수행
void rename_file(char *command_token, char *newname, char *dir_info, char *filename, char *absolute_path); //delete명령어를 수행하였을 때 파일을 rename
int info_directory_size(char *dir_info); //info_directory의 크기 확인
void oldest_file_remove(char *dir_info, char *dir_files); //오래된 파일 제거
int command_size(char command_token[BUF_SIZE][BUF_SIZE]); //size 명령어 수행
void size_path(char *filename); //파일의 크기와 상대경로 출력
int dir_size(char *pathname); //디렉토리의 크기 출력
int command_recover(char command_token[BUF_SIZE][BUF_SIZE]); //recover 명령어 수행
void strtok_path(char *file_path, char *no_file_path); //절대경로에서 최종파일 이름을 뺀 경로 구하기
void get_recover_path(char* recover_file, char *file_path); //복구하는 최종 경로 구하기
void recover_option(); //“-l” 옵션 실행
void command_tree(void); //tree 명령어 수행;
void find_dir(char *cwd_file, void (*func)(char*), int depth); //디렉토리에 있는 모든 파일 확인
void get_tree(char* file); //각 파일들의 절대경로를 트리형태로 저장하기
void old_dir(char *filename); //지정한 디렉토리의 기존의 파일 목록 가져오기
void new_dir(char *filename); //지정한 디렉토리의 현재 파일 목록 가져오기
void compare_dir(char old_file[BUF_SIZE][PATH_MAX], char new_file[BUF_SIZE][PATH_MAX]); //디렉토리 안에 있는 파일들 비교
void log_print(char *file, char *change_state); //변경상태 로그에 출력하기
int check_command(char command_token[BUF_SIZE][BUF_SIZE]); //명령어 확인

int delete=0;
int recover=0;
int size=0;
int tree=0;
int command_exit=0;
int help=0;

static int indent=0; //검색하는 디렉토리의 깊이
char ssu_file_path[PATH_MAX]; //지정한 디렉토리 절대경로
char cwd[PATH_MAX]; //현재 작업디렉토리 경로
char size_filename[BUF_SIZE]; //size 명령어를 실행시킬 파일 이름
int size_depth = 0; //size명령어에서 '-d'옵션 시 입력받은 depth
char old_file[BUF_SIZE][PATH_MAX]; //현재 지정한 디렉토리에 있는 파일
char new_file[BUF_SIZE][PATH_MAX]; //갱신된 지정한 디렉토리에 있는 파일
char delete_file[PATH_MAX]; //삭제된 파일
char create_file[PATH_MAX]; //생성된 파일
char modify_file[PATH_MAX]; //수정된 파일
char change_state[BUF_SIZE]; //변경된 상태
char old_mtime[BUF_SIZE][BUF_SIZE]; //기존 파일의 수정시간
char new_mtime[BUF_SIZE][BUF_SIZE]; //갱신한 파일의 수정시간
int old_num=0; //기존의 파일 갯수
int new_num; //갱신된 디렉토리의 파일의 갯수
int state_delete = 0;
int state_create = 0;
int state_modify = 0;

int main(void)
{
	pid_t pid;
	int fd, maxfd;
	struct timeval begin_t, end_t; //실행시작하기 전, 후 시간
	gettimeofday(&begin_t, NULL); //실행 전 시간

	if(getcwd(cwd, PATH_MAX)==NULL) //현재 작업디렉토리의 절대경로 가져오기
		fprintf(stderr,"getcwd error\n");
	
	sprintf(ssu_file_path, "%s/ssu_file", cwd); //지정한 디렉토리의 절대경로

	if((pid = fork())<0){ //자식프로세스 생성
		fprintf(stderr, "fork error\n");
		exit(1);}

	else if((pid==0)){ //자식프로세스라면 디몬프로세스 하기
		
		if(ssu_daemon_init()<0) { //디몬 코딩 수행 준비 에러시
		fprintf(stderr, "ssu_daemon_init failed\n"); //오류메시지 출력
		exit(1);} //에러시 종료

		while(1){

			new_num = 0; //새로운 디렉토리의 파일목록을 담기 위해 초기화

			if(old_num==0) //맨 처음 모니터링이 실행된다면
				find_dir(ssu_file_path, old_dir, 0); //지정된 디렉토리의 파일목록 만들기

			sleep(1); //1초동안 대기

			find_dir(ssu_file_path, new_dir, 0); //현재 지정된 디렉토리의 파일목록 만들기

			compare_dir(old_file, new_file); //두 디렉토리를 비교하여 변경된 파일들 찾기

			if(state_modify==1){
				state_modify = 0; //초기화
				strcpy(change_state, "modify");
				log_print(modify_file, change_state);} //변경상태 출력

			if(state_delete==1){
				state_delete = 0; //초기화
				strcpy(change_state,"delete");
				log_print(delete_file, change_state);} //변경상태 출력

			if(state_create==1){
				state_create = 0; //초기화
				strcpy(change_state,"create");
				log_print(create_file, change_state);} //변경상태 출력

			for(old_num=0; old_num<new_num; old_num++){
				strcpy(old_file[old_num], new_file[old_num]); //기준이 될 파일목록도 갱신
				strcpy(old_mtime[old_num], new_mtime[old_num]);} //기준이 될 파일 목록의 수정시간도 갱신
			}
		}
	
	else{ //부모프로세스인 경우 프롬프트 출력
		while(1){
			chdir(cwd); //원래의 작업디렉토리로 이동
			char command_line[BUF_SIZE]={0,}; //command line
			char command_token[BUF_SIZE][BUF_SIZE]={0,}; //command token
			int i=0;

			printf("20180787>"); //프롬프트 출력
			fgets(command_line, sizeof(command_line), stdin); //명령어 받기

			if(command_line[0]=='\n') //명령어를 입력받지 않았다면 다시 프롬프트 출력
				continue;

			command_line[strlen(command_line)-1]='\0'; //입력받은 개행문자는 빼기
		
			char *ptr = strtok(command_line, " "); //공백을 기준으로 입력받은 명령어 문자열 자르기
			while(ptr!=NULL){
				strncpy(command_token[i],ptr,strlen(ptr)); //공백문자를 기준으로 입력받은 명령어 문자열 자르기
				i++;
				ptr = strtok(NULL," "); //다음 문자열을 잘라서 포인터 반환
			}

			if(check_command(command_token)==0) //입력받은 명령어 확인
				continue; //인자를 입력받지 않은 경우

			if(delete){ //delete명령어를 입력받았다면
				delete=0;
				if(command_delete(command_token)==0) //delete 명령어가 제대로 실행되지 않았다면
					continue;
			}

			if(size){ //size명령어를 입력받았다면
				size=0;
				if(command_size(command_token)==0) //size 명령어가 제대로 실행되지 않았다면
					continue;
			}

			if(recover){ //recover명령어를 입력받았다면
				recover=0;
				if(command_recover(command_token)==0)
					continue;
			}

			if(tree){ //tree명령어를 입력받았다면
				tree=0;
				command_tree();}
		
			if(command_exit){ //exit명령어를 입력받았다면
				printf("프로그램을 종료합니다.\n");
				gettimeofday(&end_t, NULL); //실행 후 시간
				ssu_runtime(&begin_t, &end_t); //실행시간 구하기
				exit(0);} //프로그램 종료

			if(help){ //help명령어나 다음의 명령어들 이외의 명령어 입력시
				command_help();
				help=0;}
		}
	}
}

void ssu_runtime(struct timeval *begin_t, struct timeval *end_t) //실행시간 구하기
{
	end_t->tv_sec -= begin_t->tv_sec; //실행이 끝났을 때의 초에서 실행 시작 전 초를 빼서 실행시간을 구한다.

	if(end_t->tv_usec < begin_t->tv_usec){ //실행 전의 usec가 더 크다면
		end_t->tv_sec--; //end의 초를 하나 감소하여
		end_t->tv_usec += SECOND_TO_MICRO; //단위를 바꿔 usec에 더한다.
	}

	end_t->tv_usec -= begin_t->tv_usec; //실행 후에서 전을 뺀 시간을 usec 단위로 구한다.
	printf("Runtime: %ld:%06ld(sec:usec)\n", end_t->tv_sec, end_t->tv_usec);
}

int check_command(char command_token[BUF_SIZE][BUF_SIZE]) //명령어 확인
{
	if(strncmp(command_token[0],"delete",strlen("delete"))==0){ //delete명령어인 경우
		if(strlen(command_token[1])<1){ //입력받은 인자 확인
			fprintf(stderr,"FILENAME을 입력해야합니다.\n");
			return 0;} //함수종료
		delete=1;
	}

	else if(strncmp(command_token[0], "size", strlen("size"))==0){ //size명령어인 경우
		if(strlen(command_token[1])<1){
			fprintf(stderr, "FILENAME을 입력해야합니다.\n");
			return 0;} //함수종료
		size=1;
	}
	else if(strncmp(command_token[0], "recover", strlen("recover"))==0){ //recover명령어인 경우
		if(strlen(command_token[1])<1){
			fprintf(stderr, "FILENAME을 입력해야합니다.\n");
			return 0; //함수종료
		}
		recover=1;
	}
	else if(strncmp(command_token[0], "tree", strlen("tree"))==0) //tree명령어인 경우
		tree = 1;
	
	else if(strncmp(command_token[0], "exit", strlen("exit"))==0) //exit인 경우
		command_exit = 1; //프로그램 종료

	else //help를 입력하거나 다른 명령어를 입력한다면 무조건 help
		help=1;

	return 1; //함수정상종료
}

int command_delete(char command_token[BUF_SIZE][BUF_SIZE]) //delete 명령어
{
	char cwd_file[PATH_MAX]={0,}; //삭제할 파일이 있는 디렉토리의 절대경로
	char absolute_path[PATH_MAX]={0,}; //절대경로
	char relative_path[PATH_MAX]={0,}; //상대경로
	char filename[PATH_MAX]={0,}; //최종파일이름
	DIR *dirp; //디렉토리 구조체
	char trash_path[BUF_SIZE]={0,}; //tash디렉토리 절대경로
	char newname[BUF_SIZE]={0,}; //trash로 옮길 이름
	struct stat statbuf; //입력받은 파일의 stat 구조체
	char dir_info[PATH_MAX]={0,}; //info directory 절대경로
	char dir_files[PATH_MAX]={0,}; //files directory 절대경로
	pid_t pid; //프로세스 ID

	sprintf(cwd_file, "%s%s", cwd, "/ssu_file"); //지정한 디렉토리의 절대경로

	if(chdir(cwd_file)<0) //지정한 디렉토리로 이동
		fprintf(stderr, "chdir_1 error\n");

	if(strchr(command_token[1], '/')==NULL){ //'/'없이 최종파일이름으로 입력받은 경우
		strcpy(filename, command_token[1]); //검사할 filename에 입력받은 파일이름 넣기
		if(access(command_token[1], F_OK)==-1){ //지정한 디렉토리에 입력받은 파일이 존재하는지 확인
			fprintf(stderr, "파일이 존재하지 않습니다.\n");
			chdir(cwd); //원래의 작업디렉토리로 이동
			return 0;}
		sprintf(absolute_path, "%s/%s", cwd_file, command_token[1]); //파일이 존재한다면 이 파일의 절대경로 저장
	}

	else if((strstr(command_token[1], cwd_file))!=NULL){ //입력받은 filename이 절대경로라면
		if(access(command_token[1], F_OK)==-1){ //입력받은 파일이 존재하는지 확인
			fprintf(stderr, "파일이 존재하지 않습니다.\n");
			chdir(cwd); //원래의 작업디렉토리로 이동
			return 0;}
		strcpy(absolute_path,command_token[1]); //파일이 존재한다면 이 파일의 절대경로 저장
	}

	else{ //입력받은 파일이름이 상대경로라면
		if(realpath(command_token[1], absolute_path)==NULL){ //절대경로 구하기
			fprintf(stderr, "파일이 존재하지 않습니다.\n");
			chdir(cwd); //원래의 작업디렉토리로 이동
			return 0;}
		
		if(access(absolute_path, F_OK)==-1){ //파일이 존재하는지 확인
			fprintf(stderr, "파일이 존재하지 않습니다.\n");
			chdir(cwd); //원래의 작업디렉토리로 이동
			return 0;}
	}

	if(strlen(filename)<1){ //최종파일 이름이 없다면
		char *pointer=absolute_path; //pointer포인터는 절대경로를 가리킴
		strcpy(relative_path, pointer+(strlen(cwd_file)+1)); //지정한 디렉토리의 상대경로 넣기
		char *start=relative_path; //start포인터는 상대경로를 가리킴
		
		while((strchr(start,'/'))!=NULL){ //최종파일이름 찾기
			start++;}

		strcpy(filename, start); //최종파일이름 저장
	}

	sprintf(trash_path, "%s%s", cwd, "/trash"); //trash디렉토리 절대경로 구하기

	if(opendir(trash_path)==NULL){ //trash 디렉토리 없으면 만들기
		if(mkdir(trash_path, 0777)==-1)
			fprintf(stderr, "trash directory make error\n");
		chdir(trash_path); //작업 디렉토리 trash로 이동
		if(mkdir("files", 0766)==-1) //files디렉토리 만들기
			fprintf(stderr, "files directory make error\n");
		if(mkdir("info", 0766)==-1) //info디렉토리 만들기
			fprintf(stderr, "info directory make error\n");
	}

	sprintf(dir_info, "%s/info", trash_path); //info디렉토리 절대경로
	sprintf(dir_files, "%s/files", trash_path); //files디렉토리의 절대경로
	sprintf(newname, "%s/%s", dir_files, filename); //trash디렉토리로 이동할 경우의 파일의 절대경로를 새 이름으로 저장

	if(strlen(command_token[2])<1){ //시간을 입력하지 않았을 경우 바로 이동
		rename_file(command_token[1], newname, dir_info, filename, absolute_path);
	}

	else if(strcmp(command_token[2], "-i")==0){ //시간을 입력하지 않은 경우, '-i' 옵션 시 경로 이동 안하고 파일 삭제
		if(lstat(absolute_path, &statbuf)<0)
			fprintf(stderr, "lstat error\n");
		if(S_ISDIR(statbuf.st_mode)){ //삭제하려는 파일이 디렉토리인지 확인
			fprintf(stderr, "디렉토리는 '-i'옵션을 사용할 수 없습니다.\n");
			chdir(cwd);
			return 0;}
		remove(absolute_path); //파일 즉시 제거
	}

	else if(strlen(command_token[2])>2 && strlen(command_token[3])>2){ //시간을 입력받았다면
		time_t t = time(NULL); //현재 시간 구하기
		char date[BUF_SIZE]={0,}; //날짜
		char current[BUF_SIZE]={0,}; //시간
		struct tm tm = *localtime(&t); //시간을 구하기 위한 tm 구조체
		sprintf(date, "%04d-%02d-%02d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday); //오늘의 날짜
		sprintf(current, "%02d:%02d", tm.tm_hour, tm.tm_min); //현재 시간

		if(strcmp(date,command_token[2])>0 || strcmp(current, command_token[3])>0 || strlen(date)!=strlen(command_token[2]) || strlen(current)!=strlen(command_token[3])){ //현재 시간보다 입력받은 시간이 더 전인 경우, 잘못된 시간을 입력한 경우 삭제안함
			fprintf(stderr, "입력한 시간은 잘못된 시간입니다.\n");
			chdir(cwd); //작업디렉토리 이동
			return 0;}

		if((pid=fork())<0){ //자식프로세스 생성
			fprintf(stderr, "fork error\n"); //에러 시
			chdir(cwd); //작업디렉토리 이동
			return 0;}

		else if(pid==0){ //자식프로세스 생성

			if(strcmp(command_token[4], "-i")==0){ //시간을 입력받고 "-i" 옵션을 받았다면
				if(lstat(absolute_path, &statbuf)<0)
                        		fprintf(stderr, "lstat error\n");
                 		if(S_ISDIR(statbuf.st_mode)){ //삭제하려는 파일 디렉토리인지 확인
                         		fprintf(stderr, "디렉토리는 '-i'옵션을 사용할 수 없습니다.\n");
					chdir(cwd); //작업디렉토리로 이동
					exit(0);}

				while(1){
                                	time_t t = time(NULL); //시간 구하기
                                 	char date[BUF_SIZE]={0,}; //날짜
                                	char current[BUF_SIZE]={0,}; //시간
                                 	struct tm tm = *localtime(&t); //시간을 구하기 위한 tm 구조체
                                	sprintf(date, "%04d-%02d-%02d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday); //오늘의 날짜
                                 	sprintf(current, "%02d:%02d", tm.tm_hour, tm.tm_min); //현재 시간

                                 	if(strcmp(date,command_token[2])==0 && strcmp(current, command_token[3])==0){ //입력받은 날짜와 시간이 일치한다면
                                         	remove(absolute_path); //경로 이동 안하고 파일 삭제
						chdir(cwd); //작업디렉토리 이동
						break;}
				}
			}
			
			/*else if(strcmp(command_token[4], "-r")==0){ //시간을 입력받고 "-r" 옵션을 받았다면
				char ans[BUF_SIZE]={0,};
				while(1){
					time_t t = time(NULL); //시간 구하기
					char date[BUF_SIZE]={0,}; //날짜
					char time[BUF_SIZE]={0,}; //시간
					struct tm tm = *localtime(&t); //시간을 구하기 위한 tm 구조체
					sprintf(date, "%04d-%02d-%02d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday); //오늘의 날짜
					sprintf(time, "%02d:%02d", tm.tm_hour, tm.tm_min); //현재 시간
				
					if(strcmp(date,command_token[2])==0 && strcmp(time, command_token[3])==0){ //입력받은 날짜와 시간이 일치한다면
						fflush(stdin);
						printf("Delete? [y/n] "); //삭제 시간이 되었을 때 삭제 하는지 한번 더 물어보기
						fgets(ans, BUF_SIZE, stdin); //대답 저장
						if(ans[0]=='y'){
							rename_file(command_token[1], newname, dir_info, filename, absolute_path); //rename 실행
							fflush(stdin);
							break;}
						else if(ans[0]=='n'){
							printf("delete cancel\n");
							fflush(stdin);
							break;}
						printf("a");
					}
				
				}
				//exit(0);
			}*/

			else{ //옵션 없이 시간만 입력받은 경우
				while(1){
                                         time_t t = time(NULL); //시간 구하기
                                         char date[BUF_SIZE]={0,}; //날짜
                                         char current[BUF_SIZE]={0,}; //시간
                                         struct tm tm = *localtime(&t); //시간을 구하기 위한 tm 구조체
                                         sprintf(date, "%04d-%02d-%02d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday); //오늘의 날짜
                                         sprintf(current, "%02d:%02d", tm.tm_hour, tm.tm_min); //현재 시간

                                         if(strcmp(date,command_token[2])==0 && strcmp(current, command_token[3])==0){ //입력받은 날짜와 시간이 일치한다면
                                                 rename_file(command_token[1], newname, dir_info, filename, absolute_path); //rename 실행
						 chdir(cwd); //작업디렉토리 이동
                                                 break;}
				}
			}
			exit(0); //자식프로세스 종료
		}
		else{ //부모프로세스인 경우
			chdir(cwd);
			return 1; //함수 실행 종료
		}
	}

	if(info_directory_size(dir_info)) //info디렉토리의 파일이 2KB를 넘는다면
		oldest_file_remove(dir_info, dir_files); //가장 오래된 파일 제거

	if(chdir(cwd)==-1) //작업디렉토리 이동
		fprintf(stderr, "chdir_cwd error\n");

	return 1; //명령어 정상 실행
}

void rename_file(char *command_token, char *newname, char *dir_info, char *filename, char *absolute_path) //delete명령어를 입력받았을 때 rename하는 함수
{
	FILE *fp; //info에 만들 파일의 파일디스크립터
	char buf[BUF_SIZE][BUF_SIZE]={0,}; //info에 만들 파일의 내용
	struct stat statbuf; //입력받은 파일의 stat구조체
	struct tm *mtime; //최종 수정시간 구하기
	struct tm *ctime; //삭제 시간 구하기

	if(access(newname, F_OK)==0) { //trash에 이미 동일한 이름의 파일이 존재한다면
		while(access(newname, F_OK)==0){ 
			strcat(newname,"*"); //동일한 파일이름을 구별하는 '*'추가
		}
	}

	if(rename(command_token, newname)!=0) //trash디렉토리로 이동
		fprintf(stderr, "rename error\n");

	if((lstat(newname, &statbuf))<0) //파일의 정보를 담은 stat구조체
		fprintf(stderr,"stat error\n");
		
	if(chdir(dir_info)==-1) //info 디렉토리로 이동
		fprintf(stderr, "chdir_info error\n");

	if((fp=fopen(filename, "a+"))<0) //삭제된 파일의 정보를 담는 파일 만들기
		fprintf(stderr, "open_info error\n");

	strcpy(buf[0], "[Trash info]");
	strcpy(buf[1], absolute_path); //절대경로

	ctime = localtime(&statbuf.st_ctime); //삭제시간
	sprintf(buf[2], "D : %04d-%02d-%02d %02d:%02d:%02d", ctime->tm_year+1900, ctime->tm_mon+1, ctime->tm_mday, ctime->tm_hour, ctime->tm_min, ctime->tm_sec);
	mtime = localtime(&statbuf.st_mtime); //최종 수정시간
	sprintf(buf[3], "M : %04d-%02d-%02d %02d:%02d:%02d", mtime->tm_year+1900, mtime->tm_mon+1, mtime->tm_mday, mtime->tm_hour, mtime->tm_min, mtime->tm_sec);

	fprintf(fp, "%s\n", buf[0]);
	fprintf(fp, "%s\n", buf[1]);
	fprintf(fp, "%s\n", buf[2]);
	fprintf(fp, "%s\n", buf[3]);

	fclose(fp); //오픈된 파일 닫기
}

int info_directory_size(char *dir_info) //info 디렉토리의 파일 크기 구하기
{
	DIR *dp;
	struct dirent *dirp;
	char tmp[BUF_SIZE][BUF_SIZE]={0,};
	int file=0;
	struct stat statbuf; //파일의 정보를 담을 stat 구조체
	int dir_size=0;

	if((dp=opendir(dir_info))==NULL) //info 디렉토리 오픈
		fprintf(stderr, "dir_info open error\n");

	while((dirp = readdir(dp)) != NULL) //open된 디렉토리의 파일들 읽기
	{
		if(!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, "..")) //'.'과 '..'는 현재 폴더와 이전 폴더를 가리키므로 제외
			continue;

		sprintf(tmp[file], "%s", dirp->d_name); //tmp에 그 파일의 이름을 씀

		file++;
	}

	for(int i=0; i<file; i++)
	{
		char file_path[BUF_SIZE]={0,};

		sprintf(file_path, "%s/%s", dir_info, tmp[i]); //info디렉토리에 있는 파일들의 절대경로 구하기
		lstat(file_path, &statbuf); //파일의 정보를 담는 stat구조체 가져오기
		dir_size = dir_size + (statbuf.st_size); //파일의 크기 구하기
	}

	closedir(dp); //dp 디렉토리를 닫음

	if(dir_size>2048) //info 디렉토리에 있는 파일의 크기가 2KB를 넘는다면
		return 1;
	else //넘지 않는다면
		return 0;
}

void oldest_file_remove(char *dir_info, char *dir_files) //오래된 파일 삭제
{
	DIR *dp_files;
	struct dirent *dirp_files;
	char tmp[BUF_SIZE][BUF_SIZE]={0,}; //files디렉토리에 있는 파일들의 이름
	int file=0;
	struct stat statbuf;
	char time[BUF_SIZE]={0,}; //시간 비교할 배열
	char oldest_time[BUF_SIZE]={0,}; //가장 오래된 시간
	char oldest_file[BUF_SIZE]={0,}; //가장 오래된 시간을 가진 파일
	char oldest_files_path[BUF_SIZE]={0,}; //files디렉토리에서의 파일
	char oldest_info_path[BUF_SIZE]={0,}; //info디렉토리의 파일

	if((dp_files=opendir(dir_files))==NULL) //files 디렉토리 오픈
		fprintf(stderr, "dir_files open error\n");

	while((dirp_files = readdir(dp_files)) != NULL) //open된 디렉토리의 파일들 읽기
	{
		if(!strcmp(dirp_files->d_name, ".") || !strcmp(dirp_files->d_name, "..")) //'.'과 '..'는 현재 폴더와 이전 폴더를 가리키므로 제외
			continue;
		sprintf(tmp[file], "%s", dirp_files->d_name); //tmp에 그 파일의 이름을 씀
		file++;
	}

	for(int i=0; i<file; i++)
	{
		char file_path[BUF_SIZE]={0,}; //반복할때마다 초기화
		char time[BUF_SIZE]={0,};
		struct tm *ctime;

		sprintf(file_path, "%s/%s", dir_files, tmp[i]); //files디렉토리에 있는 파일들의 절대경로 구하기
		lstat(file_path, &statbuf); //파일의 정보를 담는 stat구조체 가져오기
		ctime = localtime(&statbuf.st_ctime); //삭제시간
		sprintf(time, "%04d%02d%02d%02d%02d%02d", ctime->tm_year+1900, ctime->tm_mon+1, ctime->tm_mday, ctime->tm_hour, ctime->tm_min, ctime->tm_sec);
		
		if(i==0){ //맨 처음 파일은 시간비교할 배열에 넣기
			strcpy(oldest_time, time);
			strcpy(oldest_file, tmp[i]);}

		if(strcmp(oldest_time,time)>0){ //파일의 시간이 더 작은게 있다면 더 오래된 것이므로
			strcpy(oldest_time, time); //더 오래된 시간 저장
			strcpy(oldest_file, tmp[i]); //그 파일의 이름 저장
		}
	}

	closedir(dp_files); //오픈한 디렉토리 닫기

	if(strchr(oldest_file, '*')!=NULL){ //중복된 이름을 가진 파일이 있는지 확인
		char *ptr = strtok(oldest_file,"*");
		sprintf(oldest_file, "%s", ptr);
	}

	sprintf(oldest_files_path, "%s/%s", dir_files, oldest_file); //files디렉토리에서 제거할 파일 경로
	sprintf(oldest_info_path, "%s/%s", dir_info, oldest_file); //info디렉토리에서 제거할 파일 경로

	if(access(oldest_files_path, F_OK)==0){ //files에 해당 파일이 존재한다면 동일한 이름을 가진 파일이 있는지 확인
		while(access(oldest_files_path, F_OK)==0){
			char tmp_path[BUF_SIZE]={0,};
			strcpy(tmp_path, oldest_files_path);
			remove(oldest_files_path); //해당 파일 삭제
			sprintf(oldest_files_path,"%s*",tmp_path); //동일한 파일이름을 구별하는 '*'추가
		}
	}
	
	if(remove(oldest_info_path)==-1) //info디렉토리의 파일 제거
		fprintf(stderr, "remove info failed\n");
}

int command_size(char command_token[BUF_SIZE][BUF_SIZE]) //size 명령어
{
	if((strlen(command_token[2])>0)&&(strcmp(command_token[2], "-d")==0)&&(strlen(command_token[3])>0))//-d옵션 시
		size_depth = atoi(command_token[3]); //입력받은 size_depth 저장

	else if((strlen(command_token[2])>0&&strcmp(command_token[2],"-d")!=0)||((strlen(command_token[2])>0)&&strcmp(command_token[2], "-d")==0&&(strlen(command_token[3])<1))){
		printf("size 명령어의 '-d'옵션을 다시 입력해주세요.\n");
		return 0;
	}

	strcpy(size_filename, command_token[1]); //명령어로 입력받은 파일이름 복사
	find_dir(cwd, size_path, 0); //입력받은 파일 검사
	size_depth = 0; //depth 초기화
	chdir(cwd); //작업디렉토리 이동
	return 1;
}

void size_path(char *filename) //파일의 크기와 상대경로 출력
{
	char pathname[PATH_MAX] = {0,}; //해당 파일의 절대경로
	char file_cwd[PATH_MAX] = {0,}; //입력받은 파일의 경로를 담을 배열
	char relative_path[PATH_MAX] = {0,}; //파일의 상대경로
	struct stat statbuf; //해당 파일의 정보를 담은 stat 구조체
	int size = 0; //출력할 크기


	getcwd(file_cwd, PATH_MAX); //해당파일의 경로 저장

	sprintf(pathname, "%s/%s", file_cwd, filename); //파일의 절대경로 구하기

	if(strcmp(size_filename, filename)==0){ //입력받은 파일이름과 같다면 실행

		char *start = pathname; //포인터는 cwd를 포함한 경로를 가리킴
		start = start + strlen(cwd); //cwd를 제외한 경로를 가리키도록 시작포인터 위치 이동
		sprintf(relative_path, ".%s", start); //출력할 상대경로
		lstat(pathname, &statbuf); //해당 파일의 구조체 정보 가져오기

		if(S_ISDIR(statbuf.st_mode)) //디렉토리라면 안에 있는 파일들의 크기 구하기
			size = dir_size(pathname);
		else if(S_ISREG(statbuf.st_mode)) //파일이라면 그 자체의 크기 구하기
			size = statbuf.st_size;

		printf("%d     %s\n",size, relative_path); //크기와 상대경로 출력
	}

	else if(size_depth>1&&(strstr(pathname, size_filename)!=NULL)){ //d옵션을 사용하여 깊이를 입력받았다면

		char *start = pathname; //포인터는 cwd를 포함한 경로를 가리킴
		start = start + strlen(cwd); //cwd를 제외한 경로를 가리키도록 시작포인터 위치 이동
		sprintf(relative_path, ".%s", start); //출력할 상대경로
		lstat(pathname, &statbuf); //해당 파일의 구조체 정보 가져오기

		if(S_ISDIR(statbuf.st_mode)) //디렉토리라면 안에 있는 파일들의 크기 구하기
			size = dir_size(pathname);
		else if(S_ISREG(statbuf.st_mode)) //파일이라면 그 자체의 크기 구하기
			size = statbuf.st_size;

		printf("%d     %s\n",size, relative_path); //크기와 상대경로 출력
	}
}

int dir_size(char *pathname) //디렉토리의 크기 구하기
{
	DIR *dp;
	struct dirent *dirp;
	char tmp[BUF_SIZE][BUF_SIZE]={0,}; //파일의 이름을 저장
	int file=0; //디렉토리안에 있는 파일의 갯수
	struct stat statbuf; //파일의 정보를 담을 stat 구조체
	int size=0; //디렉토리의 크기
	int file_size=0; //디렉토리 안에 있는 file의 크기

	if((dp=opendir(pathname))==NULL) //info 디렉토리 오픈
		fprintf(stderr, "dir_info open error\n");

	while((dirp = readdir(dp)) != NULL) //open된 디렉토리의 파일들 읽기
	{
		if(!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, "..")) //'.'과 '..'는 현재 폴더와 이전 폴더를 가리키므로 제외
			continue;

		sprintf(tmp[file], "%s", dirp->d_name); //tmp에 그 파일의 이름을 씀

		file++; //파일의 갯수 증가
	}

	closedir(dp); //dp 디렉토리를 닫음

	for(int i=0; i<file; i++)
	{
		char file_path[BUF_SIZE]={0,};

		sprintf(file_path, "%s/%s", pathname, tmp[i]); //info디렉토리에 있는 파일들의 절대경로 구하기
		lstat(file_path, &statbuf); //파일의 정보를 담는 stat구조체 가져오기

		if(S_ISDIR(statbuf.st_mode)) //디렉토리라면 안에 있는 파일들의 크기 구하기
			file_size = dir_size(file_path);
		else if(S_ISREG(statbuf.st_mode)) //파일이라면 그 자체의 크기 구하기
			file_size = statbuf.st_size;
		size = size + file_size; //파일의 크기 구하기
	}
	return size; //디렉토리의 크기 리턴
}

int command_recover(char command_token[BUF_SIZE][BUF_SIZE]) //recover 명령어
{
	char dir_files[BUF_SIZE] = {0,}; //info디렉토리 경로
	char dir_info[BUF_SIZE] = {0,};
	DIR *dp;
	struct dirent *dirp;
	struct stat statbuf;
	int file_num=0; //일치하는 파일의 갯수
	FILE *fp;
	char file_path[PATH_MAX] = {0,}; //입력받은 파일의 복구경로
	char recover_file[BUF_SIZE][BUF_SIZE] = {0,}; //trash에 입력받은 파일이 있다면 그 파일의 이름을 저장할 배열
	char info_buf[BUF_SIZE][BUF_SIZE] = {0,}; //info에 있는 파일의 내용 저장할 배열
	char filename[BUF_SIZE] = {0,}; //복구 시 이름이 중복될때 파일의 이름을 바꾸기 위해 필요한 배열
	char no_file_path[PATH_MAX] = {0,}; //절대경로에서 최종파일 이름을 뺀 경로
	char trash_path[PATH_MAX] = {0,};
	
	sprintf(dir_files, "%s/trash/files", cwd); //files디렉토리 경로 구하기
	sprintf(dir_info, "%s/trash/info", cwd); //info디렉토리 경로 구하기

	if(chdir(dir_files)<0) //trash의 files디렉토리로 이동
		fprintf(stderr, "chdir dir_info error\n");

	if((dp=opendir(dir_files))==NULL) //files 디렉토리 오픈
		fprintf(stderr, "dir_info open error\n");

	while((dirp = readdir(dp)) != NULL) //open된 디렉토리 안의 파일들 읽기
	{
		if(!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, "..")) //'.'과 '..'는 현재 폴더와 이전 폴더를 가리키므로 제외
			continue;

		char tmp[BUF_SIZE] = {0,}; //trash/files디렉토리에 있는 파일의 이름을 담을 배열
		sprintf(tmp, "%s", dirp->d_name); //tmp에 파일의 이름을 씀
		char *start = tmp;

		if(strstr(tmp,command_token[1])==start){ //입력받은 파일이름의 문자열이 포함된 파일이름이 있다면
			strcpy(recover_file[file_num], tmp); //그 파일의 이름 저장
			file_num++;}
	}

	closedir(dp); //오픈된 디렉토리 닫기

	if(strcmp(command_token[2],"-l")==0) //"-l"옵션을 받았을 때
		recover_option(); //옵션실행함수 호출

	chdir(dir_info); //info로 작업디렉토리 이동

	if(file_num==0){ //입력받은 파일의 이름과 일치하는 파일이 없다면
		fprintf(stderr, "There is no %s in the 'trash' directory\n", command_token[1]);
		chdir(cwd); //원래 작업디렉토리로 이동
		return 0;}

	else if(file_num==1){ //입력받은 파일의 이름과 일치하는 파일이 하나라면
		sprintf(trash_path, "%s/trash/files/%s", cwd, recover_file[0]); //현재파일의 경로 구하기
		get_recover_path(recover_file[0], file_path); //선택한 파일의 복구 경로 구하기

		if(rename(trash_path, file_path)!=0) {//파일 복구하기
			fprintf(stderr, "복구 경로가 존재하지 않습니다.\n"); //복구경로가 없다면 에러
			chdir(cwd); //원래 작업디렉토리로 이동
			return 0;}
		
		remove(command_token[1]);//trash/files에 있던 파일이 복구에 성공했다면 info에 있는 해당 파일은 삭제		
	
	}

	else { //입력받은 파일의 이름과 일치하는 파일이 여러개라면
		char num[BUF_SIZE];

		for(int i=0; i<file_num; i++) //해당 파일의 시간 저장하기
		{
			char trash_file_path[BUF_SIZE] = {0,};
			char d_time[BUF_SIZE]={0,}; //삭제시간
			char m_time[BUF_SIZE]={0,}; //수정시간
			struct tm *ctime; //삭제시간
			struct tm *mtime; //수정시간

			sprintf(trash_file_path, "%s/%s",dir_files,recover_file[i]); //files디렉토리에 있는 파일들의 절대경로 구하기
			lstat(trash_file_path, &statbuf); //파일의 정보를 담는 stat구조체 가져오기
			ctime = localtime(&statbuf.st_ctime); //삭제시간
			sprintf(d_time, "D : %04d-%02d-%02d %02d:%02d:%02d", ctime->tm_year+1900, ctime->tm_mon+1, ctime->tm_mday, ctime->tm_hour, ctime->tm_min, ctime->tm_sec);

			mtime = localtime(&statbuf.st_mtime); //수정시간
			sprintf(m_time, "M : %04d-%02d-%02d %02d:%02d:%02d", mtime->tm_year+1900, mtime->tm_mon+1, mtime->tm_mday, mtime->tm_hour, mtime->tm_min, mtime->tm_sec);

			printf("%d. %s %s %s\n",i+1, command_token[1], d_time, m_time); //선택할 수 있도록 파일의 시간정보 출력

		}

		printf("Choose : ");
		fgets(num, BUF_SIZE, stdin); //선택한 파일의 숫자 저장

		sprintf(trash_path, "%s/%s", dir_files, recover_file[atoi(num)-1]); //선택한 파일의 현재 경로 저장
		get_recover_path(recover_file[atoi(num)-1], file_path); //선택한 파일의 복구 경로 구하기

		if(rename(trash_path, file_path)!=0) {//파일 복구하기
			fprintf(stderr, "복구 경로가 존재하지 않습니다.\n"); //복구경로가 없다면 에러
			chdir(cwd); //원래 작업디렉토리로 이동
			return 0;}

	}

	chdir(cwd); //함수가 종료되기 전 원래 작업디렉토리로 이동
	return 1;
}

void strtok_path(char *file_path, char *no_file_path) //절대경로에서 최종파일 이름을 뺀 경로 구하기
{
	char path[BUF_SIZE] = {0,}; //최종 절대경로를 구하기 위해 저장할 배열 
	strcpy(path, file_path); //인자로 받은 절대경로 넣기

	char path_token[BUF_SIZE][BUF_SIZE]={0,}; //path의 토큰을 담을 배열

	char *ptr = strtok(path, "/"); //각 토큰을 가리킬 포인터

	int i=1; //토큰의 갯수
	strcpy(path_token[0], "/"); //첫번째 토큰은 "/"

	while(ptr!=NULL) //"/"를 기준으로 경로 자르고 각 토큰 저장
	{
		sprintf(path_token[i],"%s/",ptr);
		ptr = strtok(NULL, "/");
		i++;
	}

	for(int j=1; j<i-1; j++){ //최종파일 이름을 제외하여 토큰들 합치기
		strcat(path_token[0], path_token[j]);
	}

	strcpy(no_file_path, path_token[0]); //합쳐서 만든 문자열 인자로 보냄
}

void get_recover_path(char* recover_file, char *file_path ) //복구하는 최종 경로 구하기
{
	char info_buf[BUF_SIZE][BUF_SIZE] = {0,}; //선택한 파일의 info 정보를 담을 배열
	FILE *fp; //file 구조체 포인터
	char filename[BUF_SIZE] = {0,}; //최종파일 이름
	char file_path_buf[PATH_MAX] = {0,}; //파일경로 구할 때 저장할 배열
	char no_file_path[PATH_MAX] = {0,}; //절대경로에서 최종파일 이름을 뺀 경로
	int count = 0;

	if(strchr(recover_file, '*')!=NULL){ //중복된 이름을 가진 파일이라면 구별을 했던 기준으로 원래 파일 이름 구하기
		for(int i=0; i<strlen(recover_file); i++){ //info디렉토리의 파일에서 몇번째에 위치했는지 구하기
			if(recover_file[i]=='*')
				count++;
		}

		char real_filename[BUF_SIZE]={0,}; //구별 문자를 뻰 실제 파일이름
		char *ptr = strtok(recover_file,"*");
		sprintf(real_filename, "%s", ptr);

		if((fp=fopen(real_filename, "r"))==NULL) //info에 있는 해당 파일 오픈
			fprintf(stderr, "fopen error for %s\n", recover_file);

		int i=0;
		while(fgets(info_buf[i], BUF_SIZE, fp)!=NULL){ //파일의 내용을 줄단위로 읽음
			i++;}

		strncpy(file_path_buf, info_buf[4*count+1], strlen(info_buf[4*count+1])-1); //절대경로를 이용하여 복구하기 위해 저장, 개행문자는 제외
		strtok_path(file_path_buf, no_file_path); //절대경로에서 최종파일 이름을 뺀 경로 구하기

		fclose(fp); //오픈한 파일 닫기
	}

	else{ //중복된 파일이름이 없다면 그 파일이름과 같은 이름을 가진 info디렉토리에 있는 파일 오픈
		if((fp=fopen(recover_file, "r"))==NULL) //info에 있는 해당 파일 오픈
			fprintf(stderr, "fopen error for %s\n", recover_file);

		int i=0;
		while(fgets(info_buf[i], BUF_SIZE, fp)!=NULL){ //파일의 내용을 줄단위로 읽음
			i++;}

		strncpy(file_path_buf, info_buf[1], strlen(info_buf[1])-1); //절대경로를 이용하여 복구하기 위해 저장, 개행문자는 제외
		strtok_path(file_path_buf, no_file_path); //절대경로에서 최종파일 이름을 뺀 경로 구하기
	
		fclose(fp); //오픈한 파일 닫기
	}

	if(access(file_path_buf, F_OK)==0) { //복구하는 디렉토리에 이미 동일한 이름을 가진 파일이 존재한다면
		int j=1; //파일 구별 숫자
		char recover_filename[BUF_SIZE];
		strcpy(recover_filename, file_path_buf+strlen(no_file_path)); //파일의 원래 복구 이름 구하기

		while(access(file_path_buf, F_OK)==0){ //동일한 파일이름이 있는 동안 새로운 파일 이름 만들기
			file_path_buf[0] = '\0'; //버퍼 초기화
			filename[0] = '\0'; //버퍼 초기화
			sprintf(filename, "%d_%s", j, recover_filename); //동일한 파일이름을 구별할 수 있도록 파일 이름에 숫자 추가
			sprintf(file_path_buf, "%s%s", no_file_path, filename); //새로운 파일이름으로 경로 만들기
			j++; //아직 중복된 이름을 가진 파일이 남아있다면 숫자를 바꿈
			}
		}

	strcpy(file_path, file_path_buf); //최종적으로 구한 경로를 인자로 받은 배열에 저장
}

void recover_option() //"-l" 옵션 실행
{
	struct dirent **namelist;
	struct stat statbuf;
	int nitems;
	int filenum=0;
	char filename[BUF_SIZE][BUF_SIZE]={0,};
	char time[BUF_SIZE][BUF_SIZE]={0,};
	char tmp_time[BUF_SIZE]={0,};
	char tmp_name[BUF_SIZE]={0,};

	nitems = scandir(".", &namelist, NULL, NULL); //현재 디렉토리의 모든 파일과 디렉토리 >     내용 가져옴
	
	for(int i=0; i<nitems; i++){
		if((strcmp(namelist[i]->d_name, ".")==0)||(strcmp(namelist[i]->d_name,"..")==0)) //현재디렉토리, 상위 디렉토리는 제외해서 검색
			continue;
		struct tm *ctime;
		lstat(namelist[i]->d_name, &statbuf); //저장된 파일 stat구조체 가져오기
		ctime = localtime(&statbuf.st_ctime); //삭제시간 구하기
		sprintf(time[filenum], "%04d-%02d-%02d %02d:%02d:%02d", ctime->tm_year+1900, ctime->tm_mon+1, ctime->tm_mday, ctime->tm_hour, ctime->tm_min, ctime->tm_sec);

		if(strchr(namelist[i]->d_name,'*')!=NULL){ //파일이름이 중복된 파일이 존재한다면
			char *ptr = strtok(namelist[i]->d_name,"*");
			sprintf(filename[filenum], "%s", ptr);} //원래의 이름 저장
		
		else //아니라면
			sprintf(filename[filenum],"%s", namelist[i]->d_name);
		
		filenum++; //파일갯수
	}

	for(int i = 0; i<filenum-1; i++){ //시간 순으로 정렬
		for(int j = 0; j<filenum-1-i; j++){
			if(strcmp(time[j], time[j+1]) > 0){
				strcpy(tmp_time, time[j]);
				strcpy(tmp_name, filename[j]);
				strcpy(time[j], time[j+1]);
				strcpy(filename[j], filename[j+1]);
				strcpy(time[j+1], tmp_time);
				strcpy(filename[j+1], tmp_name);
			}
		}
	}

	for(int i=0; i<filenum; i++) //정렬된 파일이름과 삭제시간 출력
		printf("%d. %s     %s\n", i+1, filename[i], time[i]);
}

void command_tree(void) //tree명령어 실행
{

	printf("ssu_file\n"); //지정한 디렉토리
	find_dir(ssu_file_path,get_tree,0); //지정한 디렉토리의 모든 파일의 절대경로 확인
	chdir(cwd); //작업디렉토리 이동
}

void find_dir(char *ssu_file_path, void (*func)(char*), int depth) //지정 디렉토리에 있는 파일 확인
{
	struct dirent **items;
	int nitems, i, result;

	if(chdir(ssu_file_path)<0) //인자로 받은 디렉토리로 이동, 에러 시 출력
	{
		fprintf(stderr, "chdir error\n");
		exit(1);
	}

	nitems = scandir(".", &items, NULL, alphasort); //현재 디렉토리의 모든 파일과 디렉토리 내용 가져옴

	for(i=0; i<nitems; i++) //가져온 파일과 디렉토리의 갯수만큼 반복
	{
		struct stat fstat; //파일 상태 저장

		if((strcmp(items[i]->d_name, ".")==0)||(strcmp(items[i]->d_name,"..")==0)) //현재디렉토리, 상위 디렉토리는 제외해서 검색
			continue;

		if(size_depth<=indent&&size_depth>1){ //size명령어를 실행할때 '-d'옵션을 입력받았다면 파일의 depth 검사
			continue;}

		func(items[i]->d_name); //파일의 이름을 인자로 하는 함수 호출

		lstat(items[i]->d_name, &fstat); //그 파일 또는 디렉토리의 상태를 가져온다.

		if((fstat.st_mode&S_IFDIR)==S_IFDIR) //만약 디렉토리라면
		{
			if(indent<(depth-1) || (depth==0)) //depth를 증가시켜 재귀적으로 그 디렉토리의 파일들을 검사
			{
				indent++; //디렉토리의 depth 증가
				find_dir(items[i]->d_name, func, depth); //재귀적으로 find_dir을 호출하여 하위 디렉토리의 파일들도 검색
			}
		}
	}
	if(indent>0){
		indent--; //검사가 끝난 디렉토리의 depth를 감소시킴
		chdir("..");} //상위디렉토리로 이동
}

void get_tree(char* filename) //파일의 절대경로를 트리형태로 출력
{
	char line[BUF_SIZE] = {0,}; //각 파일의 절대경로를 트리형태로 저장할 배열
	char file_cwd[PATH_MAX] = {0,}; //해당 파일의 절대경로
	int root_num=0; //지정디렉토리 경로에 있는 '/'갯수
	int file_root_num=0 ; //해당 파일의 경로에 있는 '/'갯수
	getcwd(file_cwd,PATH_MAX); //해당 파일의 절대경로 저장

	for(int i=0; i<strlen(ssu_file_path); i++){ //지정 디렉토리의 경로에 있는 '/'갯수 구하기
	       if(ssu_file_path[i] == '/')
		       root_num++;
	}

	for(int i=0; i<strlen(file_cwd); i++){ //해당 파일의 절대경로에 있는 '/'갯수 구하기
		if(file_cwd[i] == '/')
			file_root_num++;
	}

	if(file_root_num==root_num){ //지정한 디렉토리 바로 밑의 파일이라면
		line[0] = '|';
		for(int i=1; i<strlen(ssu_file_path); i++){
			line[i]='_';
		}
		printf("|\n");
	}
	else{ //지정한 디렉토리 안에 존재하는 디렉토리의 파일이라면

		for(int i=0; i<file_root_num-root_num+1; i++){
			line[i*strlen(ssu_file_path)]='|';
			if(i==(file_root_num-root_num)){
				for(int j = 1; j<strlen(ssu_file_path); j++)
					line[i*strlen(ssu_file_path)+j] = '_';
				break;
			}
			for(int j=1; j<strlen(ssu_file_path); j++)
				line[i*strlen(ssu_file_path)+j] = ' ';
		}
	}
	printf("%s%s\n",line, filename); //각 파일의 절대경로를 트리형태로 출력
}

void old_dir(char *filename) //현재 디렉토리의 파일 목록과 수정시간 저장하기
{
	struct stat statbuf_old; //해당 파일의 정보를 담은 stat 구조체
	struct tm *mtime_old; //해당 파일의 수정시간을 저장할 tm 구조체
	char cwd_file[PATH_MAX]; //현재 파일의 경로를 저장

	getcwd(cwd_file, PATH_MAX); //현재 파일의 작업경로 가져오기
	sprintf(old_file[old_num],"%s/%s", cwd_file, filename); //디렉토리 파일 목록에 파일의 절대경로 저장하기

	lstat(old_file[old_num], &statbuf_old); //파일의 구조체 정보 가져오기
	mtime_old = localtime(&statbuf_old.st_mtime); //기존 디렉토리의 파일 수정시간 저장
	sprintf(old_mtime[old_num++], "%04d%02d%02d%02d%02d%02d", mtime_old->tm_year+1900, mtime_old->tm_mon+1, mtime_old->tm_mday, mtime_old->tm_hour, mtime_old->tm_min, mtime_old->tm_sec);

	return;
}

void new_dir(char *filename) //현재 디렉토리의 파일 목록과 수정시간 저장하기
{
	struct stat statbuf_new; //해당 파일의 정보를 담은 stat 구조체
	struct tm *mtime_new; //해당 파일의 수정시간을 저장할 tm 구조체
	char cwd_file[PATH_MAX]; //현재 파일의 경로 저장

	getcwd(cwd_file, PATH_MAX); //현재 파일의 작업경로 가져오기
	sprintf(new_file[new_num],"%s/%s", cwd_file, filename); //디렉토리 파일 목록에 파일의 절대경로 저장하기

	lstat(new_file[new_num], &statbuf_new); //일정 시간이 지난 후 디렉토리의 파일 구조체 정보 가져오기
	mtime_new = localtime(&statbuf_new.st_mtime); //시간이 지난 디렉토리의 파일 수정시간 저장
	sprintf(new_mtime[new_num++], "%04d%02d%02d%02d%02d%02d", mtime_new->tm_year+1900, mtime_new->tm_mon+1, mtime_new->tm_mday, mtime_new->tm_hour, mtime_new->tm_min, mtime_new->tm_sec);

	return;
}

void compare_dir(char old_file[BUF_SIZE][PATH_MAX], char new_file[BUF_SIZE][PATH_MAX]) //기존의 디렉토리와 현재 디렉토리의 변경된 파일 확인하기
{
	for(int i=0; i<old_num; i++){ //수정과 삭제된 파일 찾기, old_dir을 기준으로
		for(int j=0; j<new_num; j++){ //수정시간 비교
			if(strcmp(old_file[i],new_file[j])==0){ //동일한 이름의 파일이라면 수정 시간 비교
				if(strstr(old_file[i], "swp")!=NULL) //swp파일 제외
					break;

				else if(strcmp(old_mtime[i], new_mtime[j])!=0){ //수정시간이 다르다면
					strcpy(modify_file, new_file[j]); //그 파일이름을 수정배열에 저장
					state_modify = 1;} //수정시간이 변한 파일이 있다고 표시
				break;
			}

			if(j==new_num-1){ //시간이 지난 디렉토리의 모든 파일과 비교했지만 파일을 찾을 수 없다면 그 파일은 삭제된 것
				if(strstr(old_file[i],"swp")!=NULL) //swp파일 제외
					break;
				strcpy(delete_file, old_file[i]); //그 파일이름을 삭제배열에 저장
				state_delete = 1;} //삭제된 파일이 있다고 표시
		}
		if(state_delete==1) //삭제된 파일이 있다면 log에 출력
			break;
	}

	for(int i=0; i<new_num; i++){ //새로 만들어진 파일 찾기, new_dir을 기준으로
		for(int j=0; j<old_num; j++){
			if(strcmp(new_file[i],old_file[j])==0){ //파일 이름이 같다면
				break;
			}

			if(j==old_num-1){ //마지막 파일까지 검사했지만 같지 않다면 그 파일은 새로 만들어진 것
				if(strstr(new_file[i],"swp")!=NULL) //swp파일 제외
					break;
				strcpy(create_file, new_file[i]); //그 파일이름을 생성배열에 저장
				state_create = 1;} //생성된 파일이 있다고 표시
		}
		if(state_create==1) //생성된 파일을 찾았다면 log에 출력
			break;
	}
	return;
}

void log_print(char *file, char *change_state) //로그 파일에 변경상태 출력하기
{
	time_t t = time(NULL); //현재 시간 구하기
	char date[BUF_SIZE]={0,}; //날짜
	char current[BUF_SIZE]={0,}; //시간
	struct tm tm = *localtime(&t); //시간을 구하기 위한 tm 구조체
	sprintf(date, "%04d-%02d-%02d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday); //오늘의 날짜
	sprintf(current, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec); //현재 시간

	char *start = file;
	start = start + (strlen(ssu_file_path)+1); //지정디렉토리의 절대경로 이외의 경로를 가리키도록 시작포인터 위치 이동

	if(strchr(start, '/')==NULL) //지정한 디렉토리의 바로 밑의 있는 파일이라면
		printf("[%s %s][%s_%s]\n ", date, current, change_state, start); //출력

	else{ //지정한 디렉토리의 바로 밑에 있는 파일이 아니라면
		char *ptr = strtok(start, "/"); //각 토큰을 가리킬 포인터
		char path_token[100][100]={0,}; //각 토큰을 저장할 배열
		char tmp[100]={0,};
		int i=0;

		while(ptr!=NULL) //"/"를 기준으로 경로 자르고 각 토큰 저장
		{
			sprintf(path_token[i],"%s",ptr);
			ptr = strtok(NULL, "/");
			i++;
		}

		for(int j=0; j<i-1; j++){ //최종파일 구하기
			sprintf(tmp, "%s_%s", path_token[j], path_token[j+1]);
			strcpy(path_token[j+1],tmp);}

		printf("[%s %s][%s_%s]\n ", date, current, change_state, path_token[i-1]); //출력
	}
	return;
}

int ssu_daemon_init(void){ //디몬코딩수행 준비
	pid_t pid;
	int fd, maxfd;
	char logfile[BUF_SIZE] = {0,};
	sprintf(logfile, "%s/log.txt", cwd);

	if((pid=fork())<0){ //자식프로세스 생성
		fprintf(stderr, "fork error\n"); //에러 시 오류메시지 출력
		exit(1); //에러 시 종료
	}
	
	else if(pid!=0){ //부모프로세스라면
		exit(0);} //1.종료시킴

	printf(" ");
	setsid(); //2.자식프로세스를 새로운 세션 리더로 만든다.
	
	/*3.티미널 입출력 시그널 무시*/
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	maxfd = getdtablesize(); //허용된 파일디스크립터 갯수 구하기

	/*6.허용된 파일디스크립터 닫기*/
	for(fd = 0; fd<maxfd; fd++)
		close(fd);

	umask(0); //4.파일모드 생성 마스크 해제
	chdir("/"); //5.루트 디렉토리로 이동

	/*7.표준입출력과 표준에러 재지정*/
	open(logfile, O_RDWR|O_APPEND|O_CREAT);
	dup(0);
	dup(0);

	return 0;
}

void command_help() //명령어 사용법
{
	printf("<Usage : ssu_mntr>\n\n");
	printf("Command : \n");
	printf("DELETE [FILENAME] [END_TIME] [OPTION]     지정한 삭제 시간([날짜], [시])에 자동으로 파일을 삭제해주는 명령어\n");
	printf("SIZE [FILENAME] [OPTION]                  파일경로(상대경로), 파일 크기 출력하는 명령어\n");
	printf("RECOVER [FILENAME] [OPTION]               'trash' 디렉토리 안에 있는 파일을 원래 경로로 복구하는 명령어\n");
	printf("TREE     디렉토리의 구조를 tree 형태로 보여주는 명령어\n");
	printf("EXIT     프로그램 종료시키는 명령어\n");
	printf("HELP     명령어 사용법을 출력하는 명령어\n");
	printf("[OPTION] : \n");
	printf("DELETE -i : 삭제 시 'trash' 디렉토리로 삭제파일과 정보를 이동시키지 않고 파일 삭제\n");
	printf("DELETE -r : 지정한 시간에 삭제 시 삭제 여부 재확인\n");
	printf("SIZE -d NUMBER : NUMBER 단계만큼의 하위 디렉토리까지 출력\n");
	printf("RECOVER -l : 'trash' 디렉토리의 밑에 있는 파일과 삭제 시간들을 삭제 시간이 오래된 순으로 출력 후, 명령어 진행\n\n");
}
