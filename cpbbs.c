/********************************************************************
 * CPBBS - main C file.                                             *
 * Author: stderr (admin@chokepoint.net)                            *
 * Date: 06/03/05 - 12/03/05                                        *
 * About: BBS to be run with agetty, netcat, or as login shell.     *
 * Defalt user: cpbbs          Default pass: cpbbs                  *
 ********************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>

/* Definitions */
#define MAX_USER_LENGTH 17
#define MAX_DIR_LENGTH 100
#define MAX_CMD_LENGTH 256
#define MAX_TITLE_LENGTH 51
#define MAX_HELP_LENGTH 15
#define EXIT_APP 1
/* global variables containing the data in config.txt */
#define CONFIG_FILE "config.txt"
char PATH[MAX_DIR_LENGTH];
char BBS[MAX_TITLE_LENGTH];
char VERSION[MAX_TITLE_LENGTH];
char LOG_PATH[MAX_TITLE_LENGTH];
char BOARD_PATH[MAX_TITLE_LENGTH];
char PROFILE_PATH[MAX_TITLE_LENGTH];
char PRIVATE_PATH[MAX_TITLE_LENGTH];
char HELP_PATH[MAX_TITLE_LENGTH];
char AHELP_PATH[MAX_TITLE_LENGTH];

#define HELP 0
#define BANNER 1
#define MOTD 2
#define AHELP 3
/* Rock Paper Scissors definitions */
#define WIN 1
#define LOSE 2
#define DRAW 3

/* Function prototypes */
int sisalpha(char *string); /* Returns 1 if the string is alpha and returns 0 otherwise. */
int isnum(char *string); /* Returns 1 if the string is numeric and returns 0 otherwise. */
void stolower(char *string); /* Convert a string to lower case. */
void init_user_info(void);
int get_user_name(void); /* Returns 0 on failure and 1 on succes. */
int get_user_password(void); /* Returns 1 on successful login and returns 0 otherwise. */
void register_user(void);
int process_user_input(char *command);
void print_file(int file); /* Print a file, main files are #defines HELP, BANNER, and MOTD */
void list_boards(void);
void change_dir(char *new_board);
void print_dir(void);
void print_user_name(void);
void list_messages(void);
void read_message(int message_index);
void post_message(void);
/* update_titles() is called from within post_message to update
the list of available messages in the current board.
The code in post_message was getting long and confusing so
I broke it up into another function. */
void update_titles(int message_index,char *title,char *t_format);
void log_event(char *command); /* Writes the the current users log file. */
void add_board(char *board_string); /* admin only function */
void del_board(char *board_string); /* admin only function */ 
void change_password(char *old_pass, char *new_pass);
void user_add(char *user_name, int admin);
void add_admin(char *user_name);
int rps_comp(void); /* Returns a random number 1-3 */
void rock(void); /* Rock command for Rock Paper Scissors (RPS) */
void paper(void); /* Paper command for RPS */
void scissors(void); /* Scissors command for RPS */
void add_stat(int result); /* Writes the result of a game to stats file. */
void new_stat(void); /* Create a new stat file. */
void display_stats(char *user_name); /* Display the RPS stats. */
/*** Private Message stuff ***/
void list_pmessages(void); /* list private messages */
void read_pmessage(int message_index); /* read private messages */
void post_pmessage(void); /* send a private message */
void update_ptitles(int message_index,char *title,char *destination,char *t_format); /* update the username.title file */
void initiate_pmsg(char *user_name);
void load_config(void); /* load configuration variables. */
void clear_messages(void); /* Clear all private messages */

/* Global variable declaration */
struct struct_user_info {
  char user_name[MAX_USER_LENGTH];
  char directory[MAX_DIR_LENGTH]; /* Current board the user is viewing. The default is Welcome */
  int is_admin;
} user_info;

char help_name[MAX_HELP_LENGTH]; /* Used to store the name of the help file to view */
char ahelp_name[MAX_HELP_LENGTH]; /* Used to store the name of the ahelp file to view */

int main(void)
{
  char command[MAX_CMD_LENGTH];
  int count; 
  int pass = 0; /* 0 on bad pass, 1 on good pass. */

  init_user_info();
  load_config();
  
  setbuf(stdout,NULL); /* Unbuffer output file. */
  
  print_file(BANNER);

  while ((get_user_name()) == 0) /* keep looping until we get a valid user name. */
    printf("\n");
  /* Allow 3 bad passwords before kicking the user. */
  for (count = 0; count < 3; ++count) {
    if (get_user_password() == 1) {
      ++pass;
      break;
    }
  }
  
  if (pass != 1) {
    printf("Login failed.\n");
    exit(1);
  }

  log_event("User logged in.\n");
  printf("\n");
  print_file(MOTD); 
  
  while(1) {
    printf(": "); 
    fgets(command,MAX_CMD_LENGTH,stdin); 
    log_event(command);
    
    /* Remove the \n passed by fgets(). */
    command[strlen(command)-1] = '\0';
    stolower(&command[0]); /* Convert the input to all lower case characters. */
    
    /* process_user_input returns 0 on any non exit commands. */
    if ((process_user_input(command)) == EXIT_APP)
      break;
  }
  return 0;
}

/* Check if a string is all alpha characters or not.
 * Return 1 if the string is alpha and return 0 otherwise. */
int sisalpha(char *string)
{
  int counter;

  for (counter = 0; counter < strlen(string); ++counter) {
    if (isalpha(string[counter]) == 0)
      return 0;
  }
  return 1;
}

/* Check if a string is all digits or not.
 * Return 1 if the string is numeric and return 0 otherwise. */
int isnum(char *string)
{
  int counter;

  for (counter = 0; counter < strlen(string); ++counter) {
    if (isdigit(string[counter]) == 0) 
      return 0;
  }
  return 1;
}

/* Convert string to lower case */
void stolower(char *string)
{
  int counter;
  
  for (counter = 0; counter < strlen(string); ++counter)
    string[counter] = tolower(string[counter]);
}

/* Initialize the user_info structure. The default board is welcome. */
void init_user_info(void)
{
  memset(user_info.user_name,'\0',MAX_USER_LENGTH);
  strncpy(user_info.directory,"welcome",MAX_DIR_LENGTH);
  user_info.is_admin = 0;
}

/* Get the users login and make sure it's valid. If it's not, ask if he wants to register. */
int get_user_name(void)
{
  FILE *profile; 
  char path[MAX_DIR_LENGTH]; 
  char user_input[3]; 
  
  printf("Username: ");
  fgets(user_info.user_name,MAX_USER_LENGTH,stdin);
  if (user_info.user_name[0] == '\n')
    return 0;
  /* Remove the \n that comes in through fgets() function */
  user_info.user_name[strlen(user_info.user_name)-1] = '\0';

  /* Make sure username is only alpha characters for security reasons. */
  /* Changed 10/20/05: Added strlen test, because you can enter ^d to
   * bypass login all together. */
  if ((strlen(user_info.user_name) == 0) || (sisalpha(user_info.user_name) == 0)) {
    return 0;
  }  else { 
    stolower(user_info.user_name); /* convert user name to lower case */
    snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s",PATH,PROFILE_PATH,user_info.user_name); 
    profile = fopen(path,"r");
    if (profile == NULL) { /* The file doesn't exist so the user isn't registered yet. */
      printf("User name doesn't exist.\n");
      printf("Would you like to register? (Y/N): "); 
      fgets(user_input,sizeof(user_input),stdin);
      stolower(user_input); /* Convert response to lower case. */
      if (user_input[0] == 'y') 
	register_user();
      return 0; /* bad username */
    }
    fclose(profile);
    return 1; /* user name accepted */
  }
}

/* Verify the user's password. Return 1 on correct and 0 on incorrect password. */
int get_user_password(void) 
{
  FILE *profile;
  char path[MAX_DIR_LENGTH]; 
  char salt[3]; /* Salt used for the crypt() function. */
  char admin[3]; 
  char file_pass[MAX_USER_LENGTH]; /* The encrypted password from the user profile. */
  char user_pass[MAX_USER_LENGTH]; /* The password given by the person logging in. */
  char encrypted_pass[MAX_USER_LENGTH]; /* The encrypted version of user_pass. */

  /* added 11/15/05 termios support */
  struct termios init_set, new_set;
  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s",PATH,PROFILE_PATH,user_info.user_name); 
  profile = fopen(path,"r"); 
  if (profile == NULL) { 
    printf("Error opening profile.\n");
    return 0;
  }  
  fgets(file_pass,MAX_USER_LENGTH,profile); /* Get the encrypted password from the file. */
  file_pass[strlen(file_pass)-1] = '\0'; /* Remove the \n passed by fgets() */
  /* The salt is the first two characters of the encrypted password. */
  salt[0] = file_pass[0];
  salt[1] = file_pass[1];
  salt[2] = '\0';

  /* The second line in the profile is 1 if the user is an admin, and 0 for non admins. */
  fgets(admin,sizeof(admin),profile); 
  if (admin[0] == '1')
    user_info.is_admin = 1;
  else
    user_info.is_admin = 0;
  fclose(profile); 

  printf("Password: ");

  /* added to mask password 11/15/05 */
  tcgetattr(fileno(stdin), &init_set);
  new_set = init_set;
  new_set.c_lflag &= ~ECHO;
  tcsetattr(fileno(stdin), TCSAFLUSH, &new_set);

  fgets(user_pass,MAX_USER_LENGTH,stdin); 
  user_pass[strlen(user_pass)-1] = '\0'; /* Remove the \n passed by fgets() */

  tcsetattr(fileno(stdin), TCSANOW, &init_set);

  /* Encrypt the password given, and put it in encrypted_pass. */
  strncpy(encrypted_pass,strdup(crypt(user_pass,salt)),MAX_USER_LENGTH);
  if (!strcmp(encrypted_pass,file_pass)) 
    return 1;
  else  /* Passwords didn't match, return 0 for error */
    return 0;
}

/* User registration process. */
void register_user(void)
{
  FILE *profile; 
  char path[MAX_DIR_LENGTH]; 
  char user_name[MAX_USER_LENGTH];
  char user_pass[MAX_USER_LENGTH]; /* New unencrypted password. */
  char encrypted_pass[MAX_USER_LENGTH]; /* New encrypted password. */
  char salt[3]; /* Salt used for the crypt() function. */

  printf("New user name: "); 
  fgets(user_name,MAX_USER_LENGTH,stdin);
  user_name[strlen(user_name)-1] = '\0'; /* Remove the \n passed by fgets() */

  printf("New password: "); 
  fgets(user_pass,MAX_USER_LENGTH,stdin);
  user_pass[strlen(user_pass)-1] = '\0'; 

  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s",PATH,PROFILE_PATH,user_name);
  profile = fopen(path,"r"); /* Open file as read to see if the account already exists. */
  if (profile != NULL) { 
    printf("User name already exists!\n");
    return;
  }
  profile = fopen(path,"w"); 
  if (profile == NULL) { 
    printf("Error creating profile.\n");
    return;
  }

  /* Set a new seed for randomization... is that a word? */
  srand((unsigned) time(NULL));
  /* Create a new salt with random characters. */
  salt[0] = 'a' + rand() % 26;
  salt[1] = 'a' + rand() % 26;
  salt[2] = '\0';

  /* Save the encrypted password in encrypted_pass */
  strncpy(encrypted_pass,strdup(crypt(user_pass,salt)),MAX_USER_LENGTH);
  fprintf(profile,"%s\n0\n",encrypted_pass); /* Write the password and 0 (non admin) to file */
  printf("New profile created.\n");
  fclose(profile);
  initiate_pmsg(user_name); /* Create private message information */
}

int process_user_input(char *command)
{
  char cmd[MAX_CMD_LENGTH]; 
  char argument[MAX_CMD_LENGTH];
  char old_pass[MAX_USER_LENGTH],new_pass[MAX_USER_LENGTH];
  char user_name[MAX_USER_LENGTH]; /* Used for registering a new user */
  int admin_tag; /* Used for registering a new user */
  int index; /* Message index for use with 'read' command. */

  /* Parse the command into CMDs and Arguments */
  sscanf(command,"%s %s",&cmd,&argument);

  if (!strcmp(command,"exit")) {
    printf("Thanks %s for using %s.\n",user_info.user_name,BBS);
    return EXIT_APP;
  }
  else if (!strcmp(cmd,"help")) {
    /* Check to make sure the arguments are there and are legal. */
    if ((sisalpha(argument) == 1) && strcmp(command,"help")) { 
      strncpy(help_name,argument,MAX_HELP_LENGTH);
      print_file(HELP);
    } else if (!strcmp(command,"help")) { /* Check if help was entered without arguments. */
      strncpy(help_name,"index",MAX_HELP_LENGTH);
      print_file(HELP);
    } else {
      printf("Invalid help command.\n");
    }
    return 0;
  }
  else if (!strcmp(command,"boards")) {
    list_boards();
    return 0;
  }
  else if (!strcmp(command,"cd")) { /* Check if cd was entered with no arguments. */
    printf("Format: cd <board>\n");
    return 0;
  }
  else if (!strcmp(cmd,"cd")) { /* Pass the arguments to the change_dir function. */
    change_dir(argument);
    return 0;
  }
  else if (!strcmp(cmd,"clrmsgs")) { /* Clear private messages */
    clear_messages();
    return 0;
  }
  else if (!strcmp(command,"pwd")) {
    print_dir();
    return 0;
  }
  else if (!strcmp(command,"whoami")) {
    print_user_name();
    return 0;
  }
  /********** Private Message stuff ***********/
  else if (!strcmp(command,"plist")) { /* list private messages */
    list_pmessages();
    return 0;
  }
  else if (!strcmp(cmd,"pread")) { /* read a private message */
    if (isnum(argument)) {
      sscanf(argument,"%d",&index);
      read_pmessage(index);
    }
    else 
      printf("Invalid message index.\n");
    return 0;
  }
  else if (!strcmp(command,"ppost")) { /* Send a private message */
    post_pmessage();
    return 0;
  }
  else if (!strcmp(command,"mlist")) { /* list messages in a board */
    list_messages();
    return 0;
  }
  else if (!strcmp(cmd,"mread")) {
    if (isnum(argument)) { /* If argument was valid read that index. */
      sscanf(argument,"%d",&index);
      read_message(index);
    }
    else
      printf("Invalid message index.\n");
    return 0;
  }
  else if (!strcmp(command,"mpost")) {
    post_message();
    return 0;
  }
  /* Check if addboard was typed and the user is an admin. */
  else if (!strcmp(cmd,"addboard") && (user_info.is_admin == 1)) {
    /* Check to see if the argument is valid. */
    if ((sisalpha(argument) == 1) && strcmp(command,"addboard"))  
      add_board(argument);
    /* Check to see if no argument was sent. */
    else if (!strcmp(command,"addboard"))
      printf("Format: addboard <boardname>\n");
    return 0;
  }
  else if (!strcmp(cmd,"delboard") && (user_info.is_admin == 1)) {
    /*Check to see if the argument is valid. */
    if ((sisalpha(argument) == 1) && strcmp(command,"delboard"))
      del_board(argument);
    /* Check to see if no argument was sent. */
    else if (!strcmp(command,"delboard"))
      printf("Format: delboard <boardname>\n");
    return 0;
  }
  else if (!strcmp(cmd,"ahelp") && (user_info.is_admin == 1)) {
    /* Check to make sure the arguments are there and are legal. */
    if ((sisalpha(argument) == 1) && strcmp(command,"ahelp")) {
      strncpy(ahelp_name,argument,MAX_HELP_LENGTH);
      print_file(AHELP);
    } else if (!strcmp(command,"ahelp")) { /* Check if help was entered without arguments. */
      strncpy(ahelp_name,"index",MAX_HELP_LENGTH);
      print_file(AHELP);
    } else {
      printf("Invalid ahelp command.\n");
    }
    return 0;
  }
  /* New format for passwd command added 7/12/05 by stderr */
  else if (!strcmp(cmd,"passwd")) {
    /* Make sure 3 parameters were passed. */
    if (sscanf(command,"%s %s %s",&cmd,&old_pass,&new_pass) != 3) {
      printf("Format: passwd <old password> <new password>\n");
      return 0;
    }    
    change_password(old_pass,new_pass);
    return 0;
  }
  else if (!strcmp(cmd,"useradd") && (user_info.is_admin == 1)) {
    /* Make sure 3 parameters were passed. */
    if (sscanf(command,"%s %s %d",&cmd,&user_name,&admin_tag) != 3) {
      printf("Format: useradd <new user name> <(1/0) 1 = admin, 0 = non admin user>\n");
      return 0;
    }
    user_add(user_name,admin_tag);
    return 0;
  }
  else if (!strcmp(cmd,"addadmin") && (user_info.is_admin == 1)) {
    /* Make sure the argument is okay */
    if ((sisalpha(argument) == 1) && strcmp(command,"addadmin"))
      add_admin(argument);
    /* Check for no arguments */
    else if (!strcmp(command,"addadmin"))
      printf("Format: addadmin <user name>\n");
    return 0;
  }
  else if (!strcmp(command,"rock")) {
    rock();
    return 0;
  }
  else if (!strcmp(command,"paper")) {
    paper();
    return 0;
  }
  else if (!strcmp(command,"scissors")) {
    scissors();
    return 0;
  }
  else if (!strcmp(command,"stats")) { /* Check if stats was entered with no arguments. */
    display_stats(user_info.user_name);
    return 0;
  }
  else if (!strcmp(cmd,"stats")) { /* Pass the arguments to the display_stats function. */
    stolower(argument); /* Set string to lower case */
    if (sisalpha(argument) == 1) /* Make sure argument is only alpha characters */
      display_stats(argument);
    else
      printf("Illegal username.\n");
    return 0;
  }
  else {
    printf("Command \"%s\" not found.\n",command);
    return 0;
  }
}

/* Print a file specified by int file.
 * Several #defines are setup for this. HELP,BANNER, and MOTD. */
void print_file(int file)
{
  FILE *read_file;
  char read_line[MAX_CMD_LENGTH];
  char path[MAX_DIR_LENGTH];

  if (file == HELP)
    snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s.txt",PATH,HELP_PATH,help_name);
  else if (file == BANNER)
    snprintf(path,MAX_DIR_LENGTH,"%s/banner.txt",PATH);
  else if (file == MOTD) 
    snprintf(path,MAX_DIR_LENGTH,"%s/motd.txt",PATH);
  else if (file == AHELP)
    snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s.txt",PATH,AHELP_PATH,ahelp_name);
  else
    return;

  read_file = fopen(path,"r");
  if ((read_file == NULL) && (file != HELP)) {
    printf("Error opening file.\n");
    return;
  }
  if ((read_file == NULL) && (file == HELP)) {
    printf("Help file not found.\n");
    return;
  }
  
  while(fgets(read_line,MAX_CMD_LENGTH,read_file) != NULL)
    printf("%s",read_line);
  fclose(read_file);
}

/* List the boards found in BOARD_PATH/boards.txt */
void list_boards(void)
{
  FILE *read_topics;
  char topic_name[MAX_DIR_LENGTH];
  char path[MAX_DIR_LENGTH];
  int number_of_topics = 0;
  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/boards.txt",PATH,BOARD_PATH);
  read_topics = fopen(path,"r");
  if (read_topics == NULL) {
    printf("No topics available.\n");
    return;
  }
  
  printf("\nThe following boards are available.\n");
  while (fgets(topic_name,MAX_DIR_LENGTH,read_topics) != NULL) {
    printf("%s",topic_name);
    ++number_of_topics;
  }
  printf("%d boards found.\n\n",number_of_topics);
  fclose(read_topics);
}

/* Change your current board to another. */
void change_dir(char *new_board)
{
  FILE *read_topics; 
  char topic_name[MAX_DIR_LENGTH];
  char path[MAX_DIR_LENGTH]; 
  int match = 0; /* Used to keep track of matching boards. */
  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/boards.txt",PATH,BOARD_PATH); 
  read_topics = fopen(path,"r"); 
  if (read_topics == NULL) {
    printf("No boards available.\n");
    return;
  }
  
  stolower(new_board); /* Convert new_board to lowercase. */
  
  /* Loop until you reach the EOF (End of File). */
  while (fgets(topic_name,MAX_DIR_LENGTH,read_topics) != NULL) {
    topic_name[strlen(topic_name)-1] = '\0'; /* Remove the \n passed by fgets() */
    if (!strcmp(new_board,topic_name)) { /* Check if the board matches an existing board. */
      match = 1;
      break;
    }
  }
  
  if (match == 1) { /* If the board matched, then set the new directory. */
    strncpy(user_info.directory,new_board,MAX_DIR_LENGTH);
    printf("\nDirectory changed to %s\n\n",user_info.directory);
  } else { 
    printf("\nDirectory %s doesn't exist, couldn't change directories.\n\n",new_board);
  }
  fclose(read_topics); 
}

/* pwd type command to display the current directory in user_info structue. */
void print_dir(void)
{
  printf("\nCurrent directory: %s\n\n",user_info.directory);
}

/* whoami type command, lists user name from user_info structue. */
void print_user_name(void)
{
  printf("\nCurrent username: %s\n\n",user_info.user_name);
}

void list_messages(void)
{
  FILE *title_file;
  int count;
  int message_index;
  char title_line[MAX_CMD_LENGTH];
  char *title_string;
  char path[MAX_DIR_LENGTH];
  char yes_no[3];
  char *user_ptr; /* used to extract the poster from the title.txt file. */
  char user_name[MAX_USER_LENGTH];
  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/title.txt",PATH,BOARD_PATH,user_info.directory);
  title_file = fopen(path,"r");
  if (title_file == NULL) {
    printf("No messages in %s\n",user_info.directory);
    return;
  }
  
  printf("\nFirst 10 messages for \"%s\"\n",user_info.directory);
  while(1) {
    for (count = 0; count < 10; ++count) {
      if (fgets(title_line,MAX_CMD_LENGTH,title_file) == NULL)
	break;
      /* parse the line from title.txt file */
      sscanf(title_line,"%d",&message_index);
      user_ptr = &title_line[3];
      sscanf(user_ptr,"%s",&user_name);
      title_string = strchr(user_ptr,'('); 
      
      printf("%d - (%s) %s",message_index,user_name,title_string);
    }
    if (message_index > 0) {
      printf("\nView next ten topics? (Y/N): ");
      fgets(yes_no,sizeof(yes_no),stdin);
      stolower(yes_no); /* convert yes_no to lower case */
      
      log_event(yes_no);
      
      printf("\n");
      if (!strcmp(yes_no,"y\n"))
	continue;
      else if (!strcmp(yes_no,"n\n"))
	break;
      else {
	yes_no[strlen(yes_no)-1] = '\0'; /* remove the \n from the end of the string. */
	printf("%s unknown, not listing the rest.\n",yes_no);
	break;
      }
    }
    break;
  }
  printf("\n");
  fclose(title_file);
}

/* int index is in case the user supplied an argument to the read command. */
void read_message(int message_index)
{
  FILE *msg_file; 
  char user_input[6]; 
  char path[MAX_DIR_LENGTH]; 
  char msg_line[MAX_CMD_LENGTH]; 
  int counter; 
  int line_number; 
  char *msg_data;
  char user_name[MAX_CMD_LENGTH]; /* The user that wrote the message. */

  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/%d.txt",PATH,BOARD_PATH,user_info.directory,message_index);
  
  msg_file = fopen(path,"r");
  if (msg_file == NULL) {
    printf("\nMessage %d doesn't exist.\n\n",message_index);
    return;
  }
  
  fgets(user_name,MAX_CMD_LENGTH,msg_file);
  printf("\nPosted by: %s",user_name);
  fgets(msg_line,MAX_CMD_LENGTH,msg_file);
  printf("Subject: %s\n",msg_line);
  
  while(1) {
    for (counter = 0; counter < 10; ++counter) {
      if (fgets(msg_line,MAX_CMD_LENGTH,msg_file) == NULL)
	break;
      sscanf(msg_line,"%d",&line_number);
      msg_data = &msg_line[3];
      printf("%s",msg_data);
    }
    if (line_number > 0) {
      printf("\nPress enter to view the next 10 lines.\n");
      fgets(user_input,5,stdin);
      
      log_event(user_input);
      continue;
    }
    break;
  }
  printf("\n");
  fclose(msg_file);
}

/* post a new message in the current board. */
void post_message(void)
{
  FILE *temp_file;
  FILE *new_file;
  int message_index;
  int line_number = -1;
  char new_line[MAX_CMD_LENGTH];
  char path[MAX_DIR_LENGTH];
  char subject[MAX_TITLE_LENGTH];
  int counter;

  /* added 11/08/05 for time stamping posts */
  time_t t_now;
  struct tm *t_ptr;
  char t_format[51];

  time(&t_now);
  t_ptr = localtime(&t_now);
  strftime(t_format,sizeof(t_format), "(%m/%d/%Y @ %H:%M:%S)",t_ptr);

  printf("Enter subject: ");
  fgets(subject,MAX_TITLE_LENGTH,stdin);
  subject[strlen(subject)-1] = '\0';

  log_event(subject);
  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/title.txt",PATH,BOARD_PATH,user_info.directory);
  temp_file = fopen(path,"r");
  if (temp_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  fgets(new_line,MAX_CMD_LENGTH,temp_file);
  sscanf(new_line,"%d",&message_index);
  ++message_index;
  fclose(temp_file);
  update_titles(message_index,subject,t_format);

  /* the first time through we have to write to a temp file, then correct the format  */
  /* second time through numbers the lines correctly. */
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/temp.txt",PATH,BOARD_PATH,user_info.directory);
  temp_file = fopen(path,"w");
  if (temp_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  
  fprintf(temp_file,"%s",user_info.user_name);
  /* added 11/08/05 time stamping */
  fprintf(temp_file," %s\n",t_format);

  fprintf(temp_file,"\"%s\"\n",subject);

  printf("Enter a . on a line by itself to end post.\n");
  while(1) {
    printf(": ");
    fgets(new_line,MAX_CMD_LENGTH,stdin);
    
    log_event(new_line);
    
    if (!strcmp(new_line,".\n"))
      break;
    fprintf(temp_file,"%s",new_line);
    ++line_number;
  }
  fclose(temp_file);
  
  temp_file = fopen(path,"r");
  if (temp_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/%d.txt",PATH,BOARD_PATH,user_info.directory,message_index);
  new_file = fopen(path,"w");
  if (new_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  for (counter = 0; counter < 2; ++counter) {
    fgets(new_line,MAX_CMD_LENGTH,temp_file);
    fprintf(new_file,"%s",new_line);
  }
  
  while(fgets(new_line,MAX_CMD_LENGTH,temp_file) != NULL) {
    if (line_number < 10)
      fprintf(new_file,"0%d %s",line_number,new_line);
    else
      fprintf(new_file,"%d %s",line_number,new_line);
    --line_number;
  }
  
  fclose(temp_file);
  fclose(new_file);
  
  printf("New post created.\n");
}

/* The post_message() code was getting out of hand so I split it up into two functions.
 * post_message adds the actual post, while this function updates the title.txt file. */
void update_titles(int message_index,char *title, char *t_format)
{
  FILE *temp_file;
  FILE *title_file;
  char path[MAX_DIR_LENGTH];
  char line[MAX_CMD_LENGTH];
  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/temp.txt",PATH,BOARD_PATH,user_info.directory);
  temp_file = fopen(path,"w");
  if (temp_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/title.txt",PATH,BOARD_PATH,user_info.directory);
  title_file = fopen(path,"r");
  if (title_file == NULL) {
    printf("Error opening file.\n");
    return;
  }

  /* adjust the post number in title.txt so it's easier to parse later. */  
  if (message_index < 10)
    fprintf(temp_file,"00%d %s %s \"%s\"\n",message_index,user_info.user_name,t_format,title);
  else if (message_index < 100)
    fprintf(temp_file,"0%d %s %s \"%s\"\n",message_index,user_info.user_name,t_format,title);
  else
    fprintf(temp_file,"%d %s %s\"%s\"\n",message_index,user_info.user_name,t_format,title);
  
  while(fgets(line,MAX_CMD_LENGTH,title_file) != NULL)
    fprintf(temp_file,"%s",line);
  
  fclose(temp_file);
  fclose(title_file);
  
  title_file = fopen(path,"w");
  if (title_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/temp.txt",PATH,BOARD_PATH,user_info.directory);
  temp_file = fopen(path,"r");
  if (temp_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  
  while(fgets(line,MAX_CMD_LENGTH,temp_file) != NULL)
    fprintf(title_file,"%s",line);
  
  fclose(temp_file);
  fclose(title_file);
}

/* Write some event to the current user's log file. */
void log_event(char *command)
{
  FILE *log_file;
  char path[MAX_DIR_LENGTH];
  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s.txt",PATH,LOG_PATH,user_info.user_name);
  log_file = fopen(path,"a");
  if (log_file == NULL) {
    printf("Error opening log file.\n");
    return;
  }
  
  fprintf(log_file,"%s",command); 
  fclose(log_file);
}

/* Admin command for adding a new board. */
void add_board(char *board_string)
{
  FILE *board_file;
  FILE *title_file;
  char path[MAX_DIR_LENGTH];

  time_t t_now;
  struct tm *t_ptr;
  char t_format[51];

  time(&t_now);
  t_ptr = localtime(&t_now);
  strftime(t_format,sizeof(t_format), "(%m/%d/%Y @ %H:%M:%S)",t_ptr);

  if (!strcmp(board_string,"private")) {
    printf("Can't create that board.\n");
    return;
  }

  snprintf(path,MAX_DIR_LENGTH,"%s/%s/boards.txt",PATH,BOARD_PATH);
  board_file = fopen(path,"a");
  if (board_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  fprintf(board_file,"%s\n",board_string);
  fclose(board_file);

  /**************************************************************
   * Add "mkdir board_string" command. This is linux specific   *
   **************************************************************/
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s",PATH,BOARD_PATH,board_string);
  mkdir(path,S_IRWXU);
  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/title.txt",PATH,BOARD_PATH,board_string);
  title_file = fopen(path,"w");
  if (title_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  fprintf(title_file,"000 %s %s \"New board created\"\n",user_info.user_name,t_format);
  fclose(title_file);
  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/0.txt",PATH,BOARD_PATH,board_string);
  title_file = fopen(path,"w");
  if (title_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  fprintf(title_file,"%s",user_info.user_name);
  fprintf(title_file," %s\n",t_format);
  fprintf(title_file,"\"New board created\"\n");
  fprintf(title_file,"00 Have fun on this board.\n");
  fclose(title_file);
  
  printf("New board \"%s\" added.\n",board_string);
}

/* Delete a given board (board_string). */
void del_board(char *board_string)
{
  FILE *board_file;
  FILE *temp_file;
  char in_string[MAX_CMD_LENGTH];
  char path[MAX_DIR_LENGTH];
  int removed = 0;

  snprintf(path,MAX_DIR_LENGTH,"%s/%s/boards.txt",PATH,BOARD_PATH);
  board_file = fopen(path,"r");
  if (board_file == NULL) {
    printf("Error opening file.\n");
    return;
  }

  snprintf(path,MAX_DIR_LENGTH,"%s/%s/temp.txt",PATH,BOARD_PATH);
  temp_file = fopen(path,"w");
  if (temp_file == NULL) {
    printf("Error opening file.\n");
    return;
  }

  while(fgets(in_string,MAX_CMD_LENGTH,board_file) != NULL) {
    in_string[strlen(in_string)-1] = '\0'; /* Remove the \n passed by fgets() */
    
    if (!strcmp(in_string,board_string)) {
      removed = 1;
      continue;
    }
    fprintf(temp_file,"%s\n",in_string);
  }
  fclose(board_file);
  fclose(temp_file);

  snprintf(path,MAX_DIR_LENGTH,"mv %s/%s/temp.txt %s/%s/boards.txt",PATH,BOARD_PATH,PATH,BOARD_PATH);
  system(path);

  if (removed == 0) /* Board wasn't found */
    printf("Board %s wasn't found.\n",board_string);
  else {
    printf("Board %s was removed.\n",board_string);
    snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/removed.txt",PATH,BOARD_PATH,board_string);
    temp_file = fopen(path,"w");
    fprintf(temp_file,"Board removed by %s\n",user_info.user_name);
    fclose(temp_file);
  }
}

/* Change the password in the user's profile. */
void change_password(char *old_pass, char *new_pass)
{
  FILE *profile;
  char path[MAX_DIR_LENGTH];
  char file_pass[MAX_USER_LENGTH];
  char encrypted_pass[MAX_USER_LENGTH];
  char salt[3];

  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s",PATH,PROFILE_PATH,user_info.user_name);
  profile = fopen(path,"r");
  if (profile == NULL) {
    printf("Error opening user profile.\n");
    return;
  }
  fgets(file_pass,MAX_USER_LENGTH,profile);
  file_pass[strlen(file_pass)-1] = '\0'; /* remove the \n that was passed by fgets() */
  /* The salt is the first 2 characters of the encrypted password. */
  salt[0] = file_pass[0];
  salt[1] = file_pass[1];
  salt[2] = '\0';
  fclose(profile);
  
  strncpy(encrypted_pass,strdup(crypt(old_pass,salt)),MAX_USER_LENGTH);
 
  if (!strcmp(encrypted_pass,file_pass)) {
    /* The new password contains the old salt. */    
    strncpy(encrypted_pass,strdup(crypt(new_pass,salt)),MAX_USER_LENGTH);
    
    profile = fopen(path,"w");
    if (profile == NULL) {
      printf("Error opening profile.\n");
      return;
    }
    fprintf(profile,"%s\n%d\n",encrypted_pass,user_info.is_admin);    
    fclose(profile);
    
    printf("Password changed.\n");
  } else {
    printf("Incorrect password.\n");
    return;
  }
}

/* Create a new user account. 
 * Called by admin command useradd. */
void user_add(char *user_name, int admin)
{
  FILE *profile;
  char path[MAX_DIR_LENGTH];
  char user_password[5];
  char encrypted_pass[MAX_USER_LENGTH];
  char salt[3];
  int count;

  if ((admin != 1) && (admin != 0)) {
    printf("Bad argument, should be 1 or 0.\n");
    return;
  }

  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s",PATH,PROFILE_PATH,user_name);
  profile = fopen(path,"r");
  if (profile != NULL) { /* Check if the user already exists. */
    printf("User already exists.\n");
    return;
  }
  profile = fopen(path,"w");
  if (profile == NULL) {
    printf("Error opening file.\n");
    return;
  }
  /* Randomize the password and salt. */
  srand((unsigned) time(NULL));
  for (count = 0; count < 4; ++count) {
    if (count < 2)
      salt[count] = 'a' + rand() % 26;
    user_password[count] = 'a' + rand() %26;
  }
  salt[3] = '\0';
  user_password[4] = '\0';
  
  /* Encrypt the new password and save it to encrypted_pass */
  strncpy(encrypted_pass,strdup(crypt(user_password, salt)),MAX_USER_LENGTH);
  
  fprintf(profile,"%s\n%d\n",encrypted_pass,admin); /* print new info to the profile */
  printf("User %s created.\n",user_name);
  printf("New password is: %s\n",user_password);
  fclose(profile);
  initiate_pmsg(user_name); /* create new directory for private messages */
}

/* Make an existing user an admin. */
void add_admin(char *user_name)
{
  FILE *profile;
  char path[MAX_DIR_LENGTH];
  char encrypted_pass[MAX_USER_LENGTH];
  char admin_tag[3];

  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s",PATH,PROFILE_PATH,user_name);
  profile = fopen(path,"r");
  if (profile == NULL) {
    printf("User %s doesn't exist.\n",user_name);
    return;
  }
  fgets(encrypted_pass,MAX_USER_LENGTH,profile);
  fgets(admin_tag,sizeof(admin_tag),profile);
  fclose(profile);

  if (admin_tag[0] == '1') {
    printf("%s is already an admin.\n",user_name);
    return;
  }

  profile = fopen(path,"w");
  if (profile == NULL) {
    printf("Error opening file.\n");
    return;
  }
  /* fprintf() doesn't have a \n between %s and 1 because it's already there
   * it was passed from fgets() */
  fprintf(profile,"%s1\n",encrypted_pass);
  fclose(profile);
  printf("User %s is now an admin.\n",user_name);
}

/* Return a random number for rock paper scissors. */
int rps_comp(void)
{
  srand((unsigned) time(NULL));
  return (1 + rand() % 3);
}

/* Rock command for RPS game. */
void rock(void)
{
  int comp = rps_comp();

  if (comp == 1) {
    printf("Computer chose Rock.\nYou Tied\n");
    add_stat(DRAW);
  }
  else if (comp == 2) {
    printf("Computer chose Paper.\nYou Lost\n");
    add_stat(LOSE);
  }
  else if (comp == 3) {
    printf("Computer chose Scissors.\nYou Won\n");
    add_stat(WIN);
  }
}

/* Paper command for RPS game. */
void paper(void)
{

  int comp = rps_comp();

  if (comp == 1) {
    printf("Computer chose Rock.\nYou Won\n");
    add_stat(WIN);
  }
  else if (comp == 2) {
    printf("Computer chose Paper.\nYou Tied\n");
    add_stat(DRAW);
  }
  else if (comp == 3) {
    printf("Computer chose Scissors.\nYou Lost\n");
    add_stat(LOSE);
  }
}

/* Scissors command for RPS game. */
void scissors(void)
{
  int comp = rps_comp();

  if (comp == 1) {
    printf("Computer chose Rock.\nYou Lost\n");
    add_stat(LOSE);
  }
  else if (comp == 2) {
    printf("Computer chose Paper.\nYou Won\n");
    add_stat(WIN);
  }
  else if (comp == 3) {
    printf("Computer chose Scissors.\nYou Tied\n");
    add_stat(DRAW);
  }
}

/* Rock Paper Scissors stat adder */
void add_stat(int result)
{
  FILE *rps_stat;
  char filename[MAX_DIR_LENGTH];
  char line[MAX_CMD_LENGTH];
  int win=0,lose=0,draw=0;

  snprintf(filename,MAX_DIR_LENGTH,"%s/%s/%s.rps",PATH,PROFILE_PATH,user_info.user_name);
  rps_stat = fopen(filename,"r");
  
  /* If stat file doesn't exist, create it, and open the newly formed file. */
  if (rps_stat == NULL) {
    new_stat();
    rps_stat = fopen(filename,"r");
    if (rps_stat == NULL) {
      printf("Can't open file.\n");
      return;
    }
  }
  /* Get the stats.Format: Win Lose Draw example: 0 0 0 */
  fgets(line,MAX_CMD_LENGTH,rps_stat);
  sscanf(line,"%d %d %d",&win,&lose,&draw);
  fclose(rps_stat);

  /* Write the new stats for the player. */
  rps_stat = fopen(filename,"w");
  switch(result) {
  case WIN:
    fprintf(rps_stat,"%d %d %d\n",++win,lose,draw);
    break;
  case LOSE:
    fprintf(rps_stat,"%d %d %d\n",win,++lose,draw);
    break;
  case DRAW:
    fprintf(rps_stat,"%d %d %d\n",win,lose,++draw);
    break;
  default:
    break;
  }
  fclose(rps_stat);
}

/* Create a new stat file. */
void new_stat(void) 
{
  FILE *out_file;
  char filename[MAX_DIR_LENGTH];

  snprintf(filename,MAX_DIR_LENGTH,"%s/%s/%s.rps",PATH,PROFILE_PATH,user_info.user_name);
  out_file = fopen(filename,"w");

  /* Stat file format: win lose draw */
  fprintf(out_file,"0 0 0\n");
  fclose(out_file);
}

/* Display stats for the RPS game. */
void display_stats(char *user_name)
{
  FILE *in_file;
  char line[MAX_CMD_LENGTH];
  char filename[MAX_DIR_LENGTH];
  int win=0,lose=0,draw=0;

  snprintf(filename,MAX_DIR_LENGTH,"%s/%s/%s.rps",PATH,PROFILE_PATH,user_name);
  in_file = fopen(filename,"r");
  /* See if player even has any statistics. */
  if (in_file == NULL) {
    printf("%s hasn't played Rock Paper Scissors yet.\n",user_name);
    return;
  }
  fgets(line,MAX_CMD_LENGTH,in_file);
  sscanf(line,"%d %d %d",&win,&lose,&draw);
  fclose(in_file);

  printf("\nStats for: %s\n",user_info.user_name);
  printf("Wins: %d\n",win);
  printf("Losses: %d\n",lose);
  printf("Ties: %d\n\n",draw);
}

/* Private Messages */
/* list private messages */
void list_pmessages(void)
{
  FILE *title_file;
  int count;
  int message_index;
  char title_line[MAX_CMD_LENGTH];
  char *title_string;
  char path[MAX_DIR_LENGTH];
  char yes_no[3];
  char *user_ptr; /* used to extract the poster from the title.txt file. */
  char user_name[MAX_USER_LENGTH];

  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/%s/title.txt",PATH,BOARD_PATH,PRIVATE_PATH,user_info.user_name);
  title_file = fopen(path,"r");
  if (title_file == NULL) {
    printf("No messages in %s\n",user_info.user_name);
    return;
  }
  
  printf("\nFirst 10 messages for \"%s\"\n",user_info.user_name);
  while(1) {
    count = 0;
    for (count = 0; count < 10; ++count) {
      if (fgets(title_line,MAX_CMD_LENGTH,title_file) == NULL)
	break;
      /* parse the line from title.txt file */
      sscanf(title_line,"%d",&message_index);
      user_ptr = &title_line[3];
      sscanf(user_ptr,"%s",&user_name);
      title_string = strchr(user_ptr,'(');
      
      printf("%d (%s) %s",message_index,user_name,title_string);
    }
    if (message_index > 0) {
      printf("\nView next ten topics? (Y/N): ");
      fgets(yes_no,sizeof(yes_no),stdin);
      stolower(yes_no); /* convert yes_no to lower case */
      
      log_event(yes_no);
      
      printf("\n");
      if (!strcmp(yes_no,"y\n"))
	continue;
      else if (!strcmp(yes_no,"n\n"))
	break;
      else {
	yes_no[strlen(yes_no)-1] = '\0'; /* remove the \n from the end of the string. */
	printf("%s unknown, not listing the rest.\n",yes_no);
	break;
      }
		}
    break;
  }
  printf("\n");
  fclose(title_file);
}

/* Private message reader */
void read_pmessage(int message_index)
{
  FILE *msg_file; 
  char user_input[6]; 
  char path[MAX_DIR_LENGTH]; 
  char msg_line[MAX_CMD_LENGTH]; 
  int counter; 
  int line_number; 
  char *msg_data;
  char user_name[MAX_CMD_LENGTH]; /* The user that wrote the message. */
  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/%s/%d.txt",PATH,BOARD_PATH,PRIVATE_PATH,user_info.user_name,message_index);
  
  msg_file = fopen(path,"r");
  if (msg_file == NULL) {
    printf("\nMessage %d doesn't exist.\n\n",message_index);
    return;
  }
  
  fgets(user_name,MAX_CMD_LENGTH,msg_file);
  printf("\nPosted by: %s",user_name);
  fgets(msg_line,MAX_CMD_LENGTH,msg_file);
  printf("Subject: %s\n",msg_line);
  
  while(1) {
    for (counter = 0; counter < 10; ++counter) {
      if (fgets(msg_line,MAX_CMD_LENGTH,msg_file) == NULL)
	break;
      sscanf(msg_line,"%d",&line_number);
      msg_data = &msg_line[3];
      printf("%s",msg_data);
    }
    if (line_number > 0) {
      printf("\nPress enter to view the next 10 lines.\n");
      fgets(user_input,5,stdin);
      
      log_event(user_input);
      continue;
    }
    break;
  }
  printf("\n");
  fclose(msg_file);
}

/* post a new message in the current board. */
void post_pmessage(void)
{
  FILE *temp_file;
  FILE *new_file;
  int message_index;
  int line_number = -1;
  char new_line[MAX_CMD_LENGTH];
  char path[MAX_DIR_LENGTH];
  char subject[MAX_TITLE_LENGTH];
  int counter;
  char destination[MAX_USER_LENGTH];

  time_t t_now;
  struct tm *t_ptr;
  char t_format[51];

  time(&t_now);
  t_ptr = localtime(&t_now);
  strftime(t_format,sizeof(t_format), "(%m/%d/%Y @ %H:%M:%S)",t_ptr);

  printf("User to message: ");
  fgets(destination,MAX_USER_LENGTH,stdin);
  destination[strlen(destination)-1] = '\0';
  if (!sisalpha(destination)) {
    printf("Invalid user!\n");
    return;
  }

  printf("Enter subject: ");
  fgets(subject,MAX_TITLE_LENGTH,stdin);
  subject[strlen(subject)-1] = '\0';
  
  log_event(subject);

  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s",PATH,PROFILE_PATH,destination);
  temp_file = fopen(path,"r");
  if (temp_file == NULL) {
    printf("Invalid user!\n");
    return;
  }
  fclose(temp_file);

  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/%s/title.txt",PATH,BOARD_PATH,PRIVATE_PATH,destination);
  temp_file = fopen(path,"r");
  if (temp_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  fgets(new_line,MAX_CMD_LENGTH,temp_file);
  sscanf(new_line,"%d",&message_index);
  ++message_index;
  fclose(temp_file);
  update_ptitles(message_index,subject,destination,t_format);

  /* the first time through we have to write to a temp file, then correct the format  */
  /* second time through numbers the lines correctly. */
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/temp.txt",PATH,BOARD_PATH,PRIVATE_PATH);
  temp_file = fopen(path,"w");
  if (temp_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  
  fprintf(temp_file,"%s",user_info.user_name);
  fprintf(temp_file," %s\n",t_format);
  fprintf(temp_file,"\"%s\"\n",subject);
  
  printf("Enter a . on a line by itself to end post.\n");
  while(1) {
    printf(": ");
    fgets(new_line,MAX_CMD_LENGTH,stdin);
    
    log_event(new_line);
    
    if (!strcmp(new_line,".\n"))
      break;
    fprintf(temp_file,"%s",new_line);
    ++line_number;
  }
  fclose(temp_file);
  
  temp_file = fopen(path,"r");
  if (temp_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/%s/%d.txt",PATH,BOARD_PATH,PRIVATE_PATH,destination,message_index);
  new_file = fopen(path,"w");
  if (new_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  for (counter = 0; counter < 2; ++counter) {
    fgets(new_line,MAX_CMD_LENGTH,temp_file);
    fprintf(new_file,"%s",new_line);
  }
  
  while(fgets(new_line,MAX_CMD_LENGTH,temp_file) != NULL) {
    if (line_number < 10)
      fprintf(new_file,"0%d %s",line_number,new_line);
    else
      fprintf(new_file,"%d %s",line_number,new_line);
    --line_number;
  }
  
  fclose(temp_file);
  fclose(new_file);
  
  printf("New post created.\n");
}

/* The post_message() code was getting out of hand so I split it up into two functions.
 * post_message adds the actual post, while this function updates the title.txt file. */
void update_ptitles(int message_index,char *title,char *destination,char *t_format)
{
  FILE *temp_file;
  FILE *title_file;
  char path[MAX_DIR_LENGTH];
  char line[MAX_CMD_LENGTH];
  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/temp.txt",PATH,BOARD_PATH,PRIVATE_PATH);
  temp_file = fopen(path,"w");
  if (temp_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/%s/title.txt",PATH,BOARD_PATH,PRIVATE_PATH,destination);
  title_file = fopen(path,"r");
  if (title_file == NULL) {
    printf("Error opening file.\n");
    return;
  }

  /* adjust the post number in title.txt so it's easier to parse later. */  
  if (message_index < 10)
    fprintf(temp_file,"00%d %s %s \"%s\"\n",message_index,user_info.user_name,t_format,title);
  else if (message_index < 100)
    fprintf(temp_file,"0%d %s %s \"%s\"\n",message_index,user_info.user_name,t_format,title);
  else
    fprintf(temp_file,"%d %s %s \"%s\"\n",message_index,user_info.user_name,t_format,title);
  
  while(fgets(line,MAX_CMD_LENGTH,title_file) != NULL)
    fprintf(temp_file,"%s",line);
  
  fclose(temp_file);
  fclose(title_file);
  
  title_file = fopen(path,"w");
  if (title_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  
  snprintf(path,MAX_DIR_LENGTH,"%s/%s/%s/temp.txt",PATH,BOARD_PATH,PRIVATE_PATH);
  temp_file = fopen(path,"r");
  if (temp_file == NULL) {
    printf("Error opening file.\n");
    return;
  }
  
  while(fgets(line,MAX_CMD_LENGTH,temp_file) != NULL)
    fprintf(title_file,"%s",line);
  
  fclose(temp_file);
  fclose(title_file);
}

/* Initiate private message directory and files. */
void initiate_pmsg(char *user_name)
{
  FILE *new_file;
  char dir_path[MAX_DIR_LENGTH];

  time_t t_now;
  struct tm *t_ptr;
  char t_format[51];

  time(&t_now);
  t_ptr = localtime(&t_now);
  strftime(t_format,sizeof(t_format), "(%m/%d/%Y @ %H:%M:%S)",t_ptr);

  snprintf(dir_path,MAX_DIR_LENGTH,"%s/%s/%s/%s",PATH,BOARD_PATH,PRIVATE_PATH,user_name);
  mkdir(dir_path,S_IRWXU);
  snprintf(dir_path,MAX_DIR_LENGTH,"%s/%s/%s/%s/title.txt",PATH,BOARD_PATH,PRIVATE_PATH,user_name);

  new_file = fopen(dir_path,"w");
  if (new_file == NULL) {
    printf("Error creating file.\n");
    return;
  }
  fprintf(new_file,"000 admin %s \"Welcome\"\n",t_format);
  fclose(new_file);
  snprintf(dir_path,MAX_DIR_LENGTH,"%s/%s/%s/%s/0.txt",PATH,BOARD_PATH,PRIVATE_PATH,user_name);

  new_file = fopen(dir_path,"w");
  if (new_file == NULL) {
    printf("Error creating file.\n");
    return;
  }
  fprintf(new_file,"admin %s\n\"Welcome\"\n00 Welcome to %s v. %s\n",t_format,BBS,VERSION);
  fclose(new_file);
}

/* load configuration data from file */
void load_config(void)
{
  char input[MAX_CMD_LENGTH];
  char variable[21];
  char value[256];
  FILE *config;

  config = fopen(CONFIG_FILE,"r");
  if (config == NULL) {
    printf("Error loading config file!\nCheck config.txt\n");
    exit(1);
  }

  /* Format: Variable = Value */
  while (fgets(input,MAX_CMD_LENGTH,config) != NULL) {
    sscanf(input,"%s = %s\n",&variable,&value);
    if (!strcmp(variable,"BBS"))
      strncpy(BBS,value,MAX_TITLE_LENGTH);
    else if(!strcmp(variable,"VERSION"))
      strncpy(VERSION,value,MAX_TITLE_LENGTH);
    else if(!strcmp(variable,"PATH"))
      strncpy(PATH,value,MAX_DIR_LENGTH);
    else if(!strcmp(variable,"LOG_PATH"))
      strncpy(LOG_PATH,value,MAX_TITLE_LENGTH);
    else if(!strcmp(variable,"BOARD_PATH"))
      strncpy(BOARD_PATH,value,MAX_TITLE_LENGTH);
    else if(!strcmp(variable,"PROFILE_PATH"))
      strncpy(PROFILE_PATH,value,MAX_TITLE_LENGTH);
    else if(!strcmp(variable,"PRIVATE_PATH"))
      strncpy(PRIVATE_PATH,value,MAX_TITLE_LENGTH);
    else if(!strcmp(variable,"HELP_PATH"))
      strncpy(HELP_PATH,value,MAX_TITLE_LENGTH);
    else if(!strcmp(variable,"AHELP_PATH"))
      strncpy(AHELP_PATH,value,MAX_TITLE_LENGTH);
  }
  fclose(config);
}

/* clear all private messages */
void clear_messages(void)
{
  char dir_path[MAX_DIR_LENGTH];
  char msg_path[MAX_DIR_LENGTH];
  char input[MAX_CMD_LENGTH];
  int msg_num;
  FILE *index;
  FILE *new_file;

  time_t t_now;
  struct tm *t_ptr;
  char t_format[51];

  /* Make sure the user really wants to delete his messages */
  printf("Do you wish to delete all private messages? [y/n]: ");
  fgets(input,MAX_CMD_LENGTH,stdin);
  input[0] = tolower(input[0]);
  if (input[0] == 'n')
    return;

  /* Open the index file */  
  snprintf(dir_path,MAX_DIR_LENGTH,"%s/%s/%s/%s/title.txt",PATH,BOARD_PATH,PRIVATE_PATH,user_info.user_name);
  index = fopen(dir_path,"r");
  if (index == NULL) {
    printf("Error opening file.\n");
    return;
  }

  /* Loop through the messages and delete the individual files */
  while (fgets(input,MAX_CMD_LENGTH,index) != NULL) {
    sscanf(input,"%d ",&msg_num);
    snprintf(msg_path,MAX_DIR_LENGTH,"%s/%s/%s/%s/%d.txt",PATH,BOARD_PATH,PRIVATE_PATH,user_info.user_name,msg_num);
    unlink(msg_path);
  }
  fclose(index);
  unlink(dir_path); /* delete the current title.txt file */

  /* Get current time and put it into a pretty format */
  time(&t_now);
  t_ptr = localtime(&t_now);
  strftime(t_format,sizeof(t_format), "(%m/%d/%Y @ %H:%M:%S)",t_ptr);

  /* Make the new title.txt */
  snprintf(dir_path,MAX_DIR_LENGTH,"%s/%s/%s/%s/title.txt",PATH,BOARD_PATH,PRIVATE_PATH,user_info.user_name);
  new_file = fopen(dir_path,"w");
  if (new_file == NULL) {
    printf("Error creating file.\n");
    return;
  }
  /* Create a new message */
  fprintf(new_file,"000 %s %s \"Messages Cleared\"\n",user_info.user_name,t_format);
  fclose(new_file);

  /* Write to the actual 0.txt file */
  snprintf(dir_path,MAX_DIR_LENGTH,"%s/%s/%s/%s/0.txt",PATH,BOARD_PATH,PRIVATE_PATH,user_info.user_name);
  new_file = fopen(dir_path,"w");
  if (new_file == NULL) {
    printf("Error creating file.\n");
    return;
  }
  fprintf(new_file,"%s %s\n\"Messages Cleared\"\n00 You have cleared all of your messages.\n",user_info.user_name,t_format);
  fclose(new_file);

  printf("All personal messages have been deleted.\n");
}
