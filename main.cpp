#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>


class Exception{
  private:
    int statusCode;
    std::string statusMessage;
  public:
    Exception(int statusCode, std::string statusMessage){
      this->statusCode = statusCode;
      this->statusMessage = statusMessage;
    }

    int getStatusCode(){
      return this->statusCode;
    }
    
    std::string getStatusMessage(){
      return this->statusMessage;
    }
};

class Command{
  protected:
    std::string cmd;
    std::string ctrl;

    std::string trim(std::string line){
      int start = 0;
      int end = line.length()-1;
      while(start < line.length() && line[start] == ' ')
        ++start;
      while(end >= 0 && line[end] == ' ')
        --end;
      if(start > end)
        return "";
      else
        return line.substr(start, end-start+1);
    }
  public:
    std::string getCmd(){
      return this->cmd;
    }

    std::string getCtrl(){
      return this->ctrl;
    }
    virtual int execute() = 0;
};

class SingleCommand : public Command{
  private:
    std::vector< std::string > parsedArguments;

    std::vector< std::string > parseArguments(std::string cmd){
      std::vector< std::string > arguments;
      std::string wordToPut;
      for(int i = 0; i < cmd.length(); ++i){
        if(cmd[i] != ' ')
          wordToPut += cmd[i];
        else if(wordToPut.length()){
          arguments.push_back(wordToPut);
          wordToPut = "";
        }
      }
      if(wordToPut.length())
        arguments.push_back(wordToPut);
      if(arguments.size() == 0)
        throw Exception(1, "Single command syntax error.");
      return arguments;
    }

    char** stringVectToArray(std::vector< std::string > stringVect){
      int argumentsSize = stringVect.size();
      char** stringArr = new char*[argumentsSize+1];
      for(int i = 0; i < argumentsSize; ++i){
        const char* tmpC = stringVect[i].c_str();
        char* tmp = new char[stringVect[i].length()+1];
        strcpy(tmp, tmpC);
        stringArr[i] = tmp;
      }
      stringArr[argumentsSize] = NULL;
      return stringArr;
    }
        
  public:
    SingleCommand(std::string cmd, std::string ctrl){
      this->cmd = this->trim(cmd);
      this->parsedArguments = this->parseArguments(this->cmd);
      this->ctrl = ctrl;
    }

    virtual int execute(){
      char** args = stringVectToArray(this->parsedArguments);
      if(strcmp(args[0], "cd") == 0){
        if(this->parsedArguments.size() < 2 || chdir(args[1]) != 0){
          std::printf("cd incorrect args");
          return 1;
        }
      }else if(strcmp(args[0], "exec") == 0){
        if(this->parsedArguments.size() < 2 || execvp(args[1], args+1) < 0){
          std::printf("exec incorrect args");
          return 1;
        }
      }else{
        int rc = fork();
        if(rc < 0){
          std::printf("Fork failed while executing %s", this->getCmd().c_str());
          return 1;      
        }else if(rc == 0){
          if(execvp(args[0], args) < 0)
            return 1;
        }else{
          int exitCode;
          waitpid(rc, &exitCode, 0);
          for(int i = 0; i < this->parsedArguments.size(); ++i)
            delete [] args[i];
          return exitCode;
        }
      }
    }
};

class CommandLine : public Command{
  private:
    std::vector<Command* > cmds;

    void addCmdPtrToVector(std::vector<Command*> & vect, std::string cmd, std::string ctrl){
      cmd = this->trim(cmd);

      if(cmd.length() < 2 || cmd[0] != '(' || cmd[cmd.length()-1] != ')')
        vect.push_back(new SingleCommand(cmd, ctrl));
      else
        vect.push_back(new CommandLine(cmd.substr(1, cmd.length()-2), ctrl));
    }

    std::vector<Command* > parseLine(std::string cmdLine){
      std::vector<Command* > cmds;
      std::string cmd;

      for(int i = 0; i < cmdLine.length(); ++i){
        std::string ctrl;
        if(cmdLine[i] == ';')
          ctrl = ";";
        else if((i < cmdLine.length() - 1) && ((cmdLine[i] == '&' && cmdLine[i+1] == '&') || (cmdLine[i] == '|' && cmdLine[i+1] == '|'))){
          ctrl += cmdLine[i];
          ctrl += cmdLine[i+1];
          ++i;
        }
        if(ctrl == "")
          cmd += cmdLine[i];
        else{
          this->addCmdPtrToVector(cmds, cmd, ctrl);
          cmd = ""; 
        }
      }

      this->addCmdPtrToVector(cmds, cmd, ctrl);
            
      if(cmds.size() == 0)
        throw Exception(1, "Command line parsing error.");

      return cmds;
    }

  public:
    CommandLine(std::string cmdLine, std::string ctrl){
      this->cmd = this->trim(cmdLine);
      this->cmds = this->parseLine(this->cmd);
      this->ctrl = ctrl;
    }

    virtual int execute(){
      int rc = fork();
      if(rc < 0){
        std::printf("Fork failed while executing %s", this->getCmd().c_str());
        return 1;
      }else if(rc == 0){ 
        exit(executeWithoutChild());
      }else{ 
        int exitCode;
        waitpid(rc, &exitCode, 0);
        return exitCode;
      }     
    }

    int executeWithoutChild(){
      bool status = true;
      std::string prevCtrl = "";
      for(auto cmd : this->cmds){
        int statusCode = cmd->execute();
        std::string ctrl = cmd->getCtrl();
        //if(statusCode != 0)
        //  std::printf("%s command failed with %d status", cmd->getCmd().c_str(), status);
        if(prevCtrl != "||")
          status = status && (statusCode == 0);
        else
          status = status || (statusCode == 0);

        fflush(stdout);
        if((ctrl == "&&" && !status) || (ctrl == "||" && status))
          break;
        prevCtrl = ctrl;
      }
      return status == 0;
  }
};




void getCurrentDirectory(char* cwd){
  if (getcwd(cwd, PATH_MAX) == NULL) {
    perror("getcwd() error");
    exit(1);
  }
}

void printPrompt(){
  char cwd[PATH_MAX];
  getCurrentDirectory(cwd);
  std::cout << cwd << " $: ";
}

std::string getLineInput(){
  std::string line;
  getline(std::cin,line);
  if(feof(stdin)) exit(0);
  return line;
}

int main(){

  while(true){
    printPrompt();
    std::string line = getLineInput();
    try{
        CommandLine cmdLine = CommandLine(line, "");
        cmdLine.executeWithoutChild();
    }catch(Exception err){
      std::cout << err.getStatusMessage() << '\n';
    }
    std::cout << '\n';
  }	  

  return 0;
}
