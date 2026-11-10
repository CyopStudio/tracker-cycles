#include "backend.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>

namespace proto {

uint32_t stmtHash(const std::string& stmt) {
    size_t a = stmt.find_first_not_of(" \t\n"), b = stmt.find_last_not_of(" \t\n");
    uint32_t h = 2166136261u;
    if (a == std::string::npos) return h;
    for (size_t i = a; i <= b; i++) h = (h ^ (uint8_t)stmt[i]) * 16777619u;
    return h;
}

double steadySeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

bool isStepAtom(tc::Kind k) {
    using tc::Kind;
    return k == Kind::MWord || k == Kind::MNum || k == Kind::MNote || k == Kind::MChord || k == Kind::MBool ||
           k == Kind::MRange;
}

void atomOrder(const tc::Program& p, uint16_t root, std::vector<uint16_t>& out) {
    if (!root) return;
    if (isStepAtom(p.at(root).kind)) out.push_back(root);
    for (uint16_t c = p.at(root).first; c; c = p.at(c).next) atomOrder(p, c, out);
}

void Backend::addLog(const std::string& line) {
    log.push_back(line);
    while (log.size() > 200) log.pop_front();
}

void PrintBackend::send(const std::string& stmt) {
    if (stdout_) printf("> %s\n", stmt.c_str());
    addLog("> " + stmt);
}

GhciBackend::GhciBackend(const std::string& ghci, const std::string& boot) {
    int toChild[2], fromChild[2];
    if (pipe(toChild) != 0 || pipe(fromChild) != 0) {
        addLog(std::string("pipe failed: ") + strerror(errno));
        return;
    }
    pid_ = fork();
    if (pid_ == 0) {
        dup2(toChild[0], 0);
        dup2(fromChild[1], 1);
        dup2(fromChild[1], 2);
        close(toChild[1]);
        close(fromChild[0]);
        execlp(ghci.c_str(), ghci.c_str(), "-ghci-script", boot.c_str(), (char*)nullptr);
        fprintf(stderr, "could not start %s: %s\n", ghci.c_str(), strerror(errno));
        _exit(127);
    }
    close(toChild[0]);
    close(fromChild[1]);
    in_ = toChild[1];
    out_ = fromChild[0];
    fcntl(out_, F_SETFL, fcntl(out_, F_GETFL) | O_NONBLOCK);
    signal(SIGPIPE, SIG_IGN);
    addLog("started " + ghci + " -ghci-script " + boot);
}

GhciBackend::~GhciBackend() {
    if (in_ >= 0) {
        const char* q = ":quit\n";
        (void)!write(in_, q, strlen(q));
        close(in_);
    }
    if (out_ >= 0) close(out_);
    if (pid_ > 0) {
        usleep(200000);
        kill(pid_, SIGTERM);
        waitpid(pid_, nullptr, 0);
    }
}

bool GhciBackend::alive() const {
    if (pid_ <= 0) return false;
    int st;
    return waitpid(pid_, &st, WNOHANG) == 0;
}

void GhciBackend::send(const std::string& stmt) {
    addLog("> " + stmt);
    if (in_ < 0) return;
    std::string line = stmt + "\n";
    if (write(in_, line.data(), line.size()) < 0) addLog(std::string("write failed: ") + strerror(errno));
}

void GhciBackend::poll() {
    if (out_ < 0) return;
    char buf[4096];
    for (;;) {
        ssize_t n = read(out_, buf, sizeof buf);
        if (n <= 0) break;
        partial_.append(buf, (size_t)n);
    }
    size_t nl;
    while ((nl = partial_.find('\n')) != std::string::npos) {
        std::string line = partial_.substr(0, nl);
        partial_.erase(0, nl + 1);
        if (!line.empty()) addLog(line);
    }
}

}  // namespace proto
