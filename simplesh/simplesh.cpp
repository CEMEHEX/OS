#include <unistd.h>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <fcntl.h>
#include <wait.h>
#include <iostream>
#include <cstdio>
#include <stdio.h>

using namespace std;

const size_t BUFF_SIZE = 1024;
vector<pid_t> childs;

void handler(int signum, siginfo_t* siginfo, void* context) {
    for (size_t i = 0; i < childs.size(); ++i) {
        kill(childs[i], signum);
    }
}

void execute(vector<pair<const char *, vector<const char *>>> commands, char* &rest, size_t &rest_size) {
    int in, new_in = STDIN_FILENO, out;
    int rp[2];
    rp[0] = rp[1] = -1;
    if (rest_size != 0) {
      pipe(rp);
      write(rp[1], rest, rest_size);
      close(rp[1]);
      new_in = rp[0];
    }
    for (size_t i = 0; i < commands.size(); ++i) {
        in = new_in;
        if (i == commands.size() - 1) {
            out = STDOUT_FILENO;
        } else {
            int pip[2];
            pipe2(pip, O_CLOEXEC);
            new_in = pip[0];
            out = pip[1];
        }
        pid_t pid = fork();
        if (pid > 0) {
            if (in != STDIN_FILENO && in != rp[0]) close(in);
            if (out != STDOUT_FILENO) close(out);
            childs.push_back(pid);
        } else {
            dup2(in, STDIN_FILENO);
            dup2(out, STDOUT_FILENO);
            //v - variable number of arguments, p - program will be found using PATH
            execvp(commands[i].first, (char *const *) commands[i].second.data());
        }

    }
    int status;
    for (size_t i = 0; i < childs.size(); ++i) {
      while (waitpid(childs[i], &status, 0) == -1) {}
    }
    if (rest_size != 0) {
      rest_size = read(rp[0], rest, BUFF_SIZE);
      close(rp[0]);
    }
}

vector<pair<const char *, vector<const char *>>> parse(string &comm) {
    vector<pair<const char *, vector<const char *>>> commands;
    string token;
    comm = comm.substr(comm.find_first_not_of(' '), (comm.find_last_not_of(' ') - comm.find_first_not_of(' ') + 1));
    stringstream ss(comm);
    while (getline(ss, token, '|')) {
        token = token.substr(token.find_first_not_of(' '), (token.find_last_not_of(' ') - token.find_first_not_of(' ') + 1));
        string tmp_token;
        stringstream ss1(token);
        pair<const char *, vector<const char *>> command;
        if (getline(ss1, tmp_token, ' ')) {
            char *name = new char[tmp_token.size() + 1];
            strcpy(name, tmp_token.c_str());
            command.first = name;
            command.second.push_back(name);
        }
        while (getline(ss1, tmp_token, ' ')) {
            char *arg = new char[tmp_token.size() + 1];
            strcpy(arg, tmp_token.c_str());
            command.second.push_back(arg);
        }
        command.second.push_back(NULL);
        commands.push_back(command);
    }
    return commands;
}

int main() {
    struct sigaction sigact;
    sigact.sa_sigaction = *handler;
    sigact.sa_flags |= SA_SIGINFO;
    sigset_t block_mask;
    sigfillset(&block_mask);
    sigact.sa_mask = block_mask;
    sigaction(SIGINT, &sigact, NULL);
    string command;
    write(STDOUT_FILENO, "$ ", 2);
    char* buff = new char[BUFF_SIZE];
    char* rest = new char[BUFF_SIZE];
    size_t rest_size = 0;
    ssize_t cmd_len = 0, size = 0;
    while (1) {
        if (rest_size == 0) {
          size = read(STDIN_FILENO, buff, BUFF_SIZE);
        } else {
          size = rest_size;
          rest_size = 0;
          memcpy(buff, rest, BUFF_SIZE);
        }
        if (size <= 0) break;
        command.append(buff, size);
        for (ssize_t i = 0; i < size; ++i) {
            cmd_len++;
            if (buff[i] == '\n') {
                if (i != size - 1) {
                  rest_size = size - i - 1;
                  memcpy(rest, buff + i + 1, rest_size);
                }
                command.erase(cmd_len - 1);
                if (command != "") {
                    execute(parse(command), rest, rest_size);
                }
                childs.clear();
                command.clear();
                cmd_len = 0;
                write(STDOUT_FILENO, "$ ", 2);
                break;
            }
        }
    }
}
