#include "CSVReader.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> result;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) result.push_back(field);
    return result;
}
}
std::vector<double> read_numeric_column(const std::string& path, const std::string& name) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open " + path);
    std::string line;
    if (!std::getline(file, line)) throw std::runtime_error("Empty CSV: " + path);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    auto header = fields(line);
    auto column = std::find(header.begin(), header.end(), name);
    if (column == header.end()) throw std::runtime_error("Missing column: " + name);
    const auto index = static_cast<std::size_t>(column - header.begin());
    std::vector<double> values;
    std::size_t line_number = 1;
    while (std::getline(file, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto row = fields(line);
        if (row.size() != header.size() || index >= row.size()) throw std::runtime_error("Malformed CSV row " + std::to_string(line_number));
        std::size_t used = 0;
        double value;
        try { value = std::stod(row[index], &used); }
        catch (const std::exception&) { throw std::runtime_error("Invalid value at CSV row " + std::to_string(line_number)); }
        if (used != row[index].size() || !std::isfinite(value)) throw std::runtime_error("Invalid value at CSV row " + std::to_string(line_number));
        values.push_back(value);
    }
    if (file.bad()) throw std::runtime_error("Error reading " + path);
    return values;
}
