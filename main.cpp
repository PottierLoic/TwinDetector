#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>
#include <cstdlib>
#include <array>
#include <mutex>
#include <regex>
#include <map>

std::mutex logMutex;
std::map<std::string, std::string> lastMacAddresses; // Key: IP address, Value: Last seen MAC address

void manageLogFiles(const std::string& directoryPath) {
  std::vector<std::filesystem::path> logFiles;

  for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
    if (entry.is_regular_file()) {
      logFiles.push_back(entry.path());
    }
  }

  std::sort(logFiles.begin(), logFiles.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
    return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
  });

  while (logFiles.size() > 50) {
    std::filesystem::remove(logFiles.front());
    logFiles.erase(logFiles.begin());
  }
}

std::string trim(const std::string& str) {
  size_t first = str.find_first_not_of(' ');
  if (std::string::npos == first) {
    return str;
  }
  size_t last = str.find_last_not_of(' ');
  return str.substr(first, (last - first + 1));
}

std::string executeCommand(const char* cmd) {
  std::array<char, 128> buffer;
  std::string result;
  #if defined(_WIN32)
  std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
  #else
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  #endif
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

std::string getMacAddress(const std::string& ip) {
  #if defined(_WIN32)
  std::string arpCommand = "arp -a " + ip;
  #else
  std::string arpCommand = "arp " + ip;
  #endif
  std::string arpOutput = executeCommand(arpCommand.c_str());
  std::regex macRegex("([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})");
  std::smatch macMatch;
  if (std::regex_search(arpOutput, macMatch, macRegex) && !macMatch.empty()) {
    return macMatch[0];
  }
  return "MAC Address not found";
}

bool isPingSuccess(const std::string& pingOutput) {
  return pingOutput.find("TTL=") != std::string::npos || pingOutput.find("ttl=") != std::string::npos;
}

void pingAndLog(const std::string& ip) {
  while (true) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_c);

    std::ostringstream dateStream;
    dateStream << std::put_time(now_tm, "%d_%m_%Y");
    int minute = now_tm->tm_min - (now_tm->tm_min % 10);
    dateStream << "_" << std::setfill('0') << std::setw(2) << now_tm->tm_hour << std::setw(2) << minute;
    std::string dateStr = dateStream.str();

      #if defined(_WIN32)
    std::string logDir = "logs\\" + ip;
    std::string globalLogFilePath = "logs\\warn.txt";
    #else
    std::string logDir = "logs/" + ip;
    std::string globalLogFilePath = "logs/warn.txt";
    #endif
    std::string logFileName = "log" + dateStr + ".txt";
    std::string logFilePath = logDir + "\\" + logFileName;

    std::filesystem::create_directories(logDir);
    manageLogFiles(logDir);

    #if defined(_WIN32)
    std::string pingCommand = "ping -n 1 " + ip;
    #else
    std::string pingCommand = "ping -c 1 " + ip;
    #endif
    std::string pingOutput = executeCommand(pingCommand.c_str());

    bool pingSuccess = isPingSuccess(pingOutput);
    std::string currentMac = "N/A";
    if (pingSuccess) {
      currentMac = getMacAddress(ip);
    }

    std::string warningMessage = "";
    {
      std::lock_guard<std::mutex> guard(logMutex);
      auto it = lastMacAddresses.find(ip);
      if (it != lastMacAddresses.end() && it->second != currentMac && currentMac != "MAC Address not found") {
        warningMessage = " Warning: MAC address changed from " + it->second + " to " + currentMac;
        std::ofstream globalLogFile(globalLogFilePath, std::ios_base::app);
        if (globalLogFile.is_open()) {
          globalLogFile << std::put_time(now_tm, "%c") << " - " << ip << " - " << warningMessage << ". Detailed log in: " << logFilePath << std::endl;
          globalLogFile.close();
        } else {
          std::cerr << "Unable to open global log file for writing." << std::endl;
        }
      }
      lastMacAddresses[ip] = currentMac;
    }

    std::ofstream logFile(logFilePath, std::ios_base::app);
    if (logFile.is_open()) {
      std::string successMessage = pingSuccess ? "Ping success" : "Ping failed";
      logFile << std::put_time(now_tm, "%c") << " - " << ip << " - " << successMessage << ". MAC: " << currentMac << warningMessage << std::endl;
      logFile.close();
    } else {
      std::cerr << "Unable to open log file for writing." << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}

int main() {
  std::vector<std::thread> threads;
  std::string line;
  std::ifstream ipsFile("list.txt");
  std::vector<std::string> observedIPs;


  if (ipsFile.is_open()) {
    while (getline(ipsFile, line)) {
      line = trim(line);
      if (!line.empty()) {
        observedIPs.push_back(line);
        threads.emplace_back(std::thread(pingAndLog, line));
      }
    }
    ipsFile.close();
  } else {
    std::cerr << "Unable to open list.txt" << std::endl;
    return 1;
  }

  std::cout << "Observing the following IP addresses:\n";
  for (const auto& ip : observedIPs) {
    std::cout << ip << " - Log files will be stored in: logs\\" << ip << "\\" << std::endl;
  }

  for (auto& t : threads) {
    t.join();
  }

  return 0;
}