#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>

std::string trim(std::string str){
  int start = 0;
  int end = str.length()-1;
  while(start < str.length() && str[start] == ' ')
    ++start;
  while(end >= 0 && str[end] == ' ')
    --end;
  if(start > end)
    return "";
  else
    return str.substr(start, end-start+1);
}

std::vector<std::string> splitSeparateCommands(std::string str){

    std::vector<std::string> commands;
    std::string command;

    for(int i = 0; i < str.length(); ++i){
        command += str[i];
        if(str[i] == ';'){
          commands.push_back(command);
          command = "";
        }else if((i < str.length() - 1) && ((str[i] == '&' && str[i+1] == '&') || (str[i] == '|' && str[i+1] == '|'))){
          command += str[i];
          commands.push_back(command);
          ++i;
          command = "";
        }
    }

    if(command.length())
      commands.push_back(command);
    
    return commands;
}

std::vector<std::string> split(std::string str, char delimiter = ' '){
  
  std::vector<std::string> splitWords;
  std::string wordToPut;

  for(int i = 0; i < str.length(); ++i){
    if(str[i] != delimiter)
      wordToPut += str[i];
    else if(wordToPut.length()){
      splitWords.push_back(wordToPut);
      wordToPut = "";
    }
  }
  
  if(wordToPut.length())
    splitWords.push_back(wordToPut);
  
  return splitWords;

}


int main(){
  while(true){

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
       perror("getcwd() error");
       return 1;
    }

    std::cout << '\n' << cwd << " $: ";
    std::string line;
    getline(std::cin,line);
    if(feof(stdin)) exit(0);
    std::vector<std::string> commands = splitSeparateCommands(line);
    

    if(commands.size() == 0){
      fprintf(stderr, "Wrong argument line [cmd]\n");
      exit(1);
    }

    bool exitStatus = true;
    bool executeNext = true;
    int exitCode;
    std::string prevControl = "";

    int i = 0;
    while(i < commands.size()){
      exitCode = 0;
      std::string command = trim(commands[i]);
      if(command.length() == 0){
        fprintf(stderr, "Wrong command line\n");
        exit(1);
      }
      std::string control = "";
      if(command[command.length()-1] == ';'){
        control = ";";
        command = command.substr(0, command.length()-1);
      }
      else if(command.length() >= 2){
        if(command[command.length()-1] == '&' && command[command.length()-2] == '&'){
          control = "&&";
          command = command.substr(0, command.length()-2);
        }
        else if(command[command.length()-1] == '|' && command[command.length()-2] == '|'){
          control = "||"; 
          command = command.substr(0, command.length()-2); 
        }
      }
      std::vector<std::string> arguments = split(command);
      if(arguments.size() < 1){
        fprintf(stderr, "Wrong argument line\n");
        exit(1);
      }
      if(executeNext){
        int argumentsSize = arguments.size();
        char* args[argumentsSize+1];
        for(int i = 0; i < argumentsSize; ++i){
          const char* tmpC = arguments[i].c_str();
          char* tmp = new char[arguments[i].length()+1];
          strcpy(tmp, tmpC);
          args[i] = tmp;
        }
        args[argumentsSize] = NULL;
        
        if(strcmp(args[0], "cd") == 0){
          if(arguments.size() < 2 || chdir(args[1]) != 0)
              exitCode = -1;
        }else if(strcmp(args[0], "exec") == 0){
          if(arguments.size() < 2 || execvp(args[1], args+1) < 0)
              exit(1);
        }else{
          int rc = fork();
          if(rc < 0){
            fprintf(stderr, "Fork failed\n");
            exit(1);      
          }else if(rc == 0){
            if(execvp(args[0], args) < 0)
              exit(-1);
          }else{ 
            waitpid(rc, &exitCode, 0);
            for(int i = 0; i < argumentsSize; ++i)
              delete [] args[i];
          }        
        }
      }

      if(prevControl != "||")
        exitStatus = exitStatus && (exitCode == 0);
      else
        exitStatus = exitStatus || (exitCode == 0);

      if((control == "&&" && !exitStatus) || (control == "||" && exitStatus))
        executeNext = false;
      else
        executeNext = true;
      prevControl = control;

      ++i;

    }
  }	  
  return 0;
}
