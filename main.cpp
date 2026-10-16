#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <limits.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
struct ParsedCommand {
    std::vector<std::string> args;
    std::string input_file;
    std::string output_file;
    bool append_output = false;
    bool background = false;
};

std::vector<std::string> history;
constexpr std::size_t MAX_HISTORY = 100;

void reap_background(int) {
    int saved_errno = errno;
    while (waitpid(-1, nullptr, WNOHANG) > 0) {}
    errno = saved_errno;
}

bool parse_line(const std::string& line, std::vector<ParsedCommand>& commands,
                std::string& error) {
    std::istringstream input(line);
    std::string token;
    ParsedCommand current;
    bool saw_pipe = false;

    while (input >> token) {
        if (token == "|") {
            if (current.args.empty()) {
                error = "syntax error near '|'";
                return false;
            }
            commands.push_back(current);
            current = ParsedCommand{};
            saw_pipe = true;
        } else if (token == "<") {
            if (!(input >> current.input_file)) {
                error = "expected file after '<'";
                return false;
            }
        } else if (token == ">" || token == ">>") {
            if (!(input >> current.output_file)) {
                error = "expected file after redirection";
                return false;
            }
            current.append_output = token == ">>";
        } else if (token == "&") {
            current.background = true;
        } else {
            current.args.push_back(token);
        }
    }

    if (current.args.empty()) {
        if (saw_pipe) {
            error = "missing command after '|'";
            return false;
        }
        if (commands.empty()) return true;
        error = "missing command";
        return false;
    }

    commands.push_back(current);
    if (commands.back().background) {
        for (auto& command : commands) command.background = true;
    }
    return true;
}

std::vector<char*> make_argv(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    return argv;
}

bool apply_redirections(const ParsedCommand& command) {
    if (!command.input_file.empty()) {
        int fd = open(command.input_file.c_str(), O_RDONLY);
        if (fd < 0) {
            std::cerr << "shell: " << command.input_file << ": " << std::strerror(errno) << "\n";
            return false;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            std::cerr << "shell: dup2: " << std::strerror(errno) << "\n";
            close(fd);
            return false;
        }
        close(fd);
    }

    if (!command.output_file.empty()) {
        int flags = O_WRONLY | O_CREAT | (command.append_output ? O_APPEND : O_TRUNC);
        int fd = open(command.output_file.c_str(), flags, 0644);
        if (fd < 0) {
            std::cerr << "shell: " << command.output_file << ": " << std::strerror(errno) << "\n";
            return false;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            std::cerr << "shell: dup2: " << std::strerror(errno) << "\n";
            close(fd);
            return false;
        }
        close(fd);
    }
    return true;
}

void run_pipeline(std::vector<ParsedCommand> commands) {
    if (commands.empty()) return;

    if (commands.size() == 1 && commands[0].args[0] == "cd") {
        if (commands[0].args.size() > 2) {
            std::cerr << "shell: cd: too many arguments\n";
            return;
        }
        const char* target = commands[0].args.size() == 2
            ? commands[0].args[1].c_str() : std::getenv("HOME");
        if (!target || chdir(target) < 0)
            std::cerr << "shell: cd: " << (target ? target : "(HOME not set)")
                      << ": " << std::strerror(errno) << "\n";
        return;
    }

    std::vector<pid_t> children;
    int previous_read = -1;

    for (std::size_t i = 0; i < commands.size(); ++i) {
        int pipe_fds[2] = {-1, -1};
        bool has_next = i + 1 < commands.size();
        if (has_next && pipe(pipe_fds) < 0) {
            std::cerr << "shell: pipe: " << std::strerror(errno) << "\n";
            if (previous_read >= 0) close(previous_read);
            return;
        }

        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "shell: fork: " << std::strerror(errno) << "\n";
            if (previous_read >= 0) close(previous_read);
            if (pipe_fds[0] >= 0) close(pipe_fds[0]);
            if (pipe_fds[1] >= 0) close(pipe_fds[1]);
            return;
        }

        if (pid == 0) {
            if (previous_read >= 0) {
                if (dup2(previous_read, STDIN_FILENO) < 0) _exit(126);
            }
            if (has_next && dup2(pipe_fds[1], STDOUT_FILENO) < 0) _exit(126);

            if (previous_read >= 0) close(previous_read);
            if (pipe_fds[0] >= 0) close(pipe_fds[0]);
            if (pipe_fds[1] >= 0) close(pipe_fds[1]);

            if (!apply_redirections(commands[i])) _exit(1);

            auto argv = make_argv(commands[i].args);
            execvp(argv[0], argv.data());
            std::cerr << "shell: " << argv[0] << ": " << std::strerror(errno) << "\n";
            _exit(errno == ENOENT ? 127 : 126);
        }

        children.push_back(pid);
        if (previous_read >= 0) close(previous_read);
        if (pipe_fds[1] >= 0) close(pipe_fds[1]);
        previous_read = has_next ? pipe_fds[0] : -1;
    }

    if (previous_read >= 0) close(previous_read);

    if (!commands.back().background) {
        for (pid_t child : children) {
            int status = 0;
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
        }
    } else {
        std::cout << "[background job started";
        for (pid_t child : children) std::cout << " " << child;
        std::cout << "]\n";
    }
}

void show_history() {
    for (std::size_t i = 0; i < history.size(); ++i)
        std::cout << i + 1 << "  " << history[i] << "\n";
}

bool is_builtin_history(const std::string& line) {
    std::istringstream input(line);
    std::string first, extra;
    input >> first;
    return first == "history" && !(input >> extra);
}
}

int main() {
    struct sigaction action{};
    action.sa_handler = reap_background;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &action, nullptr);

    std::string line;
    while (true) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)))
            std::cout << "minishell:" << cwd << "$ " << std::flush;
        else
            std::cout << "minishell$ " << std::flush;

        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }

        if (line.empty()) continue;

        if (history.size() == MAX_HISTORY) history.erase(history.begin());
        history.push_back(line);

        if (line == "exit") break;
        if (is_builtin_history(line)) {
            show_history();
            continue;
        }

        std::vector<ParsedCommand> commands;
        std::string error;
        if (!parse_line(line, commands, error)) {
            std::cerr << "minishell: " << error << "\n";
            continue;
        }
        run_pipeline(std::move(commands));
    }

    return 0;
}
