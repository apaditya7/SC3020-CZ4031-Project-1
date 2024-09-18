#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

struct NBARecord {
    std::string game_date_est;
    int team_id_home;
    int pts_home;
    float fg_pct_home;
    float ft_pct_home;
    float fg3_pct_home;
    int ast_home;
    int reb_home;
    int home_team_wins;

    NBARecord(const std::string& date, int team_id, int pts, float fg_pct, float ft_pct, float fg3_pct, int ast, int reb, int win)
        : game_date_est(date), team_id_home(team_id), pts_home(pts), fg_pct_home(fg_pct), ft_pct_home(ft_pct), fg3_pct_home(fg3_pct), ast_home(ast), reb_home(reb), home_team_wins(win) {}
};

std::vector<NBARecord> readRecordsFromFile(const std::string& filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open the file " << filename << std::endl;
        return {};
    }

    std::vector<NBARecord> records;
    std::string line;

    std::getline(file, line);

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string game_date_est;
        int team_id_home, pts_home, ast_home, reb_home, home_team_wins;
        float fg_pct_home, ft_pct_home, fg3_pct_home;
        char separator;

        try {
            std::getline(ss, game_date_est, '\t');
            ss >> team_id_home >> separator;
            ss >> pts_home >> separator;
            ss >> fg_pct_home >> separator;
            ss >> ft_pct_home >> separator;
            ss >> fg3_pct_home >> separator;
            ss >> ast_home >> separator;
            ss >> reb_home >> separator;
            ss >> home_team_wins;

            records.emplace_back(game_date_est, team_id_home, pts_home, fg_pct_home, ft_pct_home, fg3_pct_home, ast_home, reb_home, home_team_wins);
        } catch (...) {
            std::cerr << "Error parsing line: " << line << std::endl;
        }
    }

    return records;
}

void reportStatistics(const std::vector<NBARecord>& records, size_t blockSize) {
    size_t recordSize = sizeof(NBARecord); 
    size_t totalRecords = records.size(); 
    size_t recordsPerBlock = blockSize / recordSize; 
    size_t totalBlocks = (totalRecords + recordsPerBlock - 1) / recordsPerBlock; 

    std::cout << "Record size: " << recordSize << " bytes" << std::endl;
    std::cout << "Total number of records: " << totalRecords << std::endl;
    std::cout << "Records per block: " << recordsPerBlock << std::endl;
    std::cout << "Block size: " << blockSize << " bytes" << std::endl;
    std::cout << "Total number of blocks: " << totalBlocks << std::endl;
}

int main() {
    const std::string filename = "/Users/adityaap/task1_DBSP/games.txt";
    size_t blockSize = 4096; 

    std::vector<NBARecord> records = readRecordsFromFile(filename);

    reportStatistics(records, blockSize);

    return 0;
}
