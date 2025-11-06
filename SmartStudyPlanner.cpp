// SmartStudyPlanner.cpp
// Smart Study Planner with AI Scheduling (single-file project)
// Features:
//  - Subject class (OOP)
//  - Planner class (scheduling, adaptive AI adjustment)
//  - Persistence to CSV (save/load)
//  - Templates, operator overloading, smart pointers, exceptions
//  - Menu-driven demo UI
//
// Compile:
//   g++ -std=c++17 -O2 -o SmartStudyPlanner SmartStudyPlanner.cpp
// Run:
//   ./SmartStudyPlanner
//
// Author: ChatGPT (educational example)

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <map>
#include <stdexcept>
#include <cmath>
#include <functional>

template <typename T>
T clamp(const T& v, const T& lo, const T& hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

std::string fmtd(double v, int prec = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << v;
    return oss.str();
}

class Subject {
private:
    std::string name_;
    int difficulty_;
    int importance_;
    double perfScore_;
    double allocatedHours_;
    std::vector<double> historyScores_;

public:
    Subject() : name_(""), difficulty_(5), importance_(5),
                perfScore_(100.0), allocatedHours_(0.0) {}

    Subject(const std::string& name, int difficulty, int importance, double perfScore = 100.0)
        : name_(name),
          difficulty_(clamp(difficulty, 1, 10)),
          importance_(clamp(importance, 1, 10)),
          perfScore_(clamp(perfScore, 0.0, 100.0)),
          allocatedHours_(0.0) {}

    std::string name() const { return name_; }
    int difficulty() const { return difficulty_; }
    int importance() const { return importance_; }
    double perfScore() const { return perfScore_; }
    double allocatedHours() const { return allocatedHours_; }

    double priorityWeight() const {
        double perfFactor = 1.5 - (perfScore_ / 100.0);
        double base = static_cast<double>(difficulty_) * static_cast<double>(importance_);
        return base * perfFactor;
    }

    void setAllocatedHours(double hrs) {
        allocatedHours_ = std::max(0.0, hrs);
    }

    void updatePerformance(double newScore) {
        newScore = clamp(newScore, 0.0, 100.0);
        historyScores_.push_back(newScore);
        if (historyScores_.size() > 10) historyScores_.erase(historyScores_.begin());
        double sum = std::accumulate(historyScores_.begin(), historyScores_.end(), 0.0);
        perfScore_ = sum / historyScores_.size();
    }

    void setPerformance(double score) {
        perfScore_ = clamp(score, 0.0, 100.0);
    }

    std::string toCSV() const {
        std::ostringstream oss;
        oss << name_ << "," << difficulty_ << "," << importance_ << "," << perfScore_ << "," << allocatedHours_;
        return oss.str();
    }

    static Subject fromCSV(const std::string& line) {
        std::istringstream iss(line);
        std::string tok;
        std::vector<std::string> parts;
        while (std::getline(iss, tok, ',')) parts.push_back(tok);
        if (parts.size() < 5) throw std::runtime_error("Invalid subject CSV line");
        Subject s(parts[0], std::stoi(parts[1]), std::stoi(parts[2]), std::stod(parts[3]));
        s.setAllocatedHours(std::stod(parts[4]));
        return s;
    }

    std::string summary() const {
        std::ostringstream oss;
        oss << std::left << std::setw(15) << name_
            << " | diff: " << std::setw(2) << difficulty_
            << " imp: " << std::setw(2) << importance_
            << " perf: " << std::setw(6) << fmtd(perfScore_,1)
            << " hrs: " << std::setw(5) << fmtd(allocatedHours_,2);
        return oss.str();
    }
};

struct Schedule {
    std::map<std::string, double> alloc;
    Schedule() = default;
    double totalHours() const {
        double sum = 0.0;
        for (const auto& kv : alloc) sum += kv.second;
        return sum;
    }
    Schedule operator+(const Schedule& other) const {
        Schedule out = *this;
        for (const auto& kv : other.alloc) out.alloc[kv.first] += kv.second;
        return out;
    }
    std::string toString() const {
        std::ostringstream oss;
        oss << "Schedule (total " << fmtd(totalHours(),2) << " hrs):\n";
        for (const auto& kv : alloc)
            oss << "  - " << std::left << std::setw(15) << kv.first << " -> " << fmtd(kv.second,2) << " hrs\n";
        return oss.str();
    }
};

class StudyPlanner {
private:
    std::vector<std::shared_ptr<Subject>> subjects_;
    double totalDailyHours_;

public:
    StudyPlanner() : totalDailyHours_(4.0) {}
    void addSubject(const std::string& name, int diff, int imp, double perf = 100.0) {
        if (findSubject(name)) throw std::runtime_error("Subject already exists: " + name);
        subjects_.push_back(std::make_shared<Subject>(name, diff, imp, perf));
    }
    void removeSubject(const std::string& name) {
        auto it = std::remove_if(subjects_.begin(), subjects_.end(),
            [&](const std::shared_ptr<Subject>& s){ return s->name() == name; });
        subjects_.erase(it, subjects_.end());
    }
    std::shared_ptr<Subject> findSubject(const std::string& name) const {
        for (const auto& s : subjects_) if (s->name() == name) return s;
        return nullptr;
    }
    void setTotalDailyHours(double hrs) {
        if (hrs < 0.0) throw std::runtime_error("Hours must be non-negative");
        totalDailyHours_ = hrs;
    }
    double getTotalDailyHours() const { return totalDailyHours_; }
    Schedule generateSchedule() {
        Schedule schedule;
        if (subjects_.empty()) return schedule;
        std::vector<double> weights;
        for (const auto& s : subjects_) weights.push_back(s->priorityWeight());
        double sumWeights = std::accumulate(weights.begin(), weights.end(), 0.0);
        if (sumWeights <= 0.0) {
            double per = totalDailyHours_ / static_cast<double>(subjects_.size());
            for (const auto& s : subjects_) { schedule.alloc[s->name()] = per; s->setAllocatedHours(per); }
            return schedule;
        }
        const double minSlot = 0.25;
        std::vector<double> rawAlloc(weights.size(), 0.0);
        for (size_t i = 0; i < weights.size(); ++i) {
            rawAlloc[i] = (weights[i] / sumWeights) * totalDailyHours_;
            rawAlloc[i] = std::max(rawAlloc[i], minSlot);
        }
        double rawSum = std::accumulate(rawAlloc.begin(), rawAlloc.end(), 0.0);
        if (rawSum > 0.0) {
            for (size_t i = 0; i < rawAlloc.size(); ++i)
                rawAlloc[i] *= (totalDailyHours_ / rawSum);
        }
        for (size_t i = 0; i < subjects_.size(); ++i) {
            double hrs = std::round(rawAlloc[i] * 100.0) / 100.0;
            schedule.alloc[subjects_[i]->name()] = hrs;
            subjects_[i]->setAllocatedHours(hrs);
        }
        return schedule;
    }
    void adaptiveAdjust(double lowThreshold = 70.0, double highThreshold = 90.0,
                        double boostFactor = 1.15, double reduceFactor = 0.9) {
        for (auto& s : subjects_) {
            double perf = s->perfScore();
            double current = s->allocatedHours();
            double newHours = current;
            if (perf < lowThreshold) newHours = current * boostFactor;
            else if (perf > highThreshold) newHours = current * reduceFactor;
            newHours = clamp(newHours, 0.1, totalDailyHours_);
            s->setAllocatedHours(newHours);
        }
        double sum = 0.0;
        for (const auto& s : subjects_) sum += s->allocatedHours();
        if (sum <= 0) return;
        double scale = totalDailyHours_ / sum;
        for (auto& s : subjects_)
            s->setAllocatedHours(std::round(s->allocatedHours() * scale * 100.0) / 100.0);
    }
    void recordPerformance(const std::string& name, double score) {
        auto sp = findSubject(name);
        if (!sp) throw std::runtime_error("Subject not found: " + name);
        sp->updatePerformance(score);
    }
    void saveToFile(const std::string& filename) const {
        std::ofstream ofs(filename, std::ios::trunc);
        if (!ofs) throw std::runtime_error("Unable to open file for writing: " + filename);
        ofs << "name,difficulty,importance,perfScore,allocatedHours\n";
        for (const auto& s : subjects_) ofs << s->toCSV() << "\n";
    }
    void loadFromFile(const std::string& filename) {
        std::ifstream ifs(filename);
        if (!ifs) throw std::runtime_error("Unable to open file for reading: " + filename);
        std::string line;
        subjects_.clear();
        bool first = true;
        while (std::getline(ifs, line)) {
            if (line.empty()) continue;
            if (first && line.find("name,difficulty,importance") != std::string::npos) { first = false; continue; }
            first = false;
            Subject sub = Subject::fromCSV(line);
            subjects_.push_back(std::make_shared<Subject>(sub));
        }
    }
    void showSubjects() const {
        if (subjects_.empty()) { std::cout << "(No subjects available)\n"; return; }
        std::cout << "Subjects:\n";
        for (const auto& s : subjects_) std::cout << "  " << s->summary() << "\n";
    }
    Schedule showCurrentSchedule() const {
        Schedule sch;
        for (const auto& s : subjects_) sch.alloc[s->name()] = s->allocatedHours();
        return sch;
    }
};

void printHeader() { std::cout << "\n=== SMART STUDY PLANNER (AI Scheduling) ===\n"; }
void printMenu() {
    std::cout << "\nMenu:\n"
              << "1) Add Subject\n"
              << "2) Remove Subject\n"
              << "3) List Subjects\n"
              << "4) Set total daily hours\n"
              << "5) Generate Schedule (AI)\n"
              << "6) Show Current Schedule\n"
              << "7) Record Performance for Subject\n"
              << "8) Adaptive Adjustment\n"
              << "9) Save to file\n"
              << "10) Load from file\n"
              << "0) Exit\n"
              << "Enter choice: ";
}
int getInt(const std::string& prompt, int minv, int maxv) {
    while (true) {
        std::cout << prompt; int v;
        if (!(std::cin >> v)) { std::cin.clear(); std::cin.ignore(10000, '\n'); continue; }
        if (v < minv || v > maxv) continue; return v;
    }
}
double getDouble(const std::string& prompt, double minv, double maxv) {
    while (true) {
        std::cout << prompt; double v;
        if (!(std::cin >> v)) { std::cin.clear(); std::cin.ignore(10000, '\n'); continue; }
        if (v < minv || v > maxv) continue; return v;
    }
}
std::string getLineAfterPrompt(const std::string& prompt) {
    std::cout << prompt; std::string tmp; std::getline(std::cin, tmp); if (tmp.empty()) std::getline(std::cin, tmp); return tmp;
}
int main() {
    StudyPlanner planner; printHeader();
    try {
        planner.addSubject("Math", 9, 10, 80.0);
        planner.addSubject("Physics", 8, 9, 70.0);
        planner.addSubject("History", 4, 5, 90.0);
        planner.addSubject("English", 3, 4, 95.0);
        planner.setTotalDailyHours(4.0);
    } catch (...) {}
    while (true) {
        printMenu(); int choice; if (!(std::cin >> choice)) { std::cin.clear(); std::cin.ignore(10000, '\n'); continue; }
        try {
            if (choice == 0) break;
            else if (choice == 1) {
                std::string name = getLineAfterPrompt("Subject name: ");
                int diff = getInt("Difficulty (1-10): ", 1, 10);
                int imp = getInt("Importance (1-10): ", 1, 10);
                double perf = getDouble("Initial performance (0-100): ", 0.0, 100.0);
                planner.addSubject(name, diff, imp, perf);
            } else if (choice == 2) {
                std::string name = getLineAfterPrompt("Subject name to remove: ");
                planner.removeSubject(name);
            } else if (choice == 3) {
                planner.showSubjects();
            } else if (choice == 4) {
                double hrs = getDouble("Enter total study hours per day: ", 0.0, 24.0);
                planner.setTotalDailyHours(hrs);
            } else if (choice == 5) {
                Schedule sch = planner.generateSchedule();
                std::cout << sch.toString();
            } else if (choice == 6) {
                Schedule sch = planner.showCurrentSchedule();
                std::cout << sch.toString();
            } else if (choice == 7) {
                std::string name = getLineAfterPrompt("Subject name: ");
                double score = getDouble("Enter score (0-100): ", 0.0, 100.0);
                planner.recordPerformance(name, score);
            } else if (choice == 8) {
                planner.adaptiveAdjust();
                Schedule sch = planner.showCurrentSchedule();
                std::cout << sch.toString();
            } else if (choice == 9) {
                std::string fname = getLineAfterPrompt("Save filename: ");
                planner.saveToFile(fname);
            } else if (choice == 10) {
                std::string fname = getLineAfterPrompt("Load filename: ");
                planner.loadFromFile(fname);
            }
        } catch (const std::exception& ex) { std::cerr << "Error: " << ex.what() << "\n"; }
    }
    std::cout << "Goodbye!\n"; return 0;
}
