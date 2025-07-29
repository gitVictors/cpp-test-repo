#include "input_reader.h"
#include <cassert>
#include <iterator>
#include <algorithm>
#include <sstream>
#include "geo.h"

namespace input {


std::vector<std::pair<int, std::string>> ParseStopDistances(const std::string& input) {

    using namespace std;

    vector<pair<int, string>> result;

    // Находим начало списка расстояний (после координат)
    size_t dist_pos = input.find(',', input.find(',') + 1);
    if (dist_pos == string::npos) {
        return result; // Нет расстояний
    }

    string distances_str = input.substr(dist_pos);
    std::istringstream iss(distances_str);
    string token;

    while (getline(iss, token, ',')) {
        // Удаляем начальные и конечные пробелы
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);

        if (token.empty()) continue;

        // Парсим расстояние
        size_t m_pos = token.find('m');
        if (m_pos == string::npos) {
            throw invalid_argument("Invalid distance format - missing 'm'");
        }

        string dist_str = token.substr(0, m_pos);
        int distance;
        try {
            distance = stoi(dist_str);
        } catch (...) {
            throw invalid_argument("Invalid distance value");
        }

        if (distance <= 0) {
            throw invalid_argument("Distance must be positive");
        }

        // Парсим название остановки
        size_t to_pos = token.find("to ", m_pos);
        if (to_pos == string::npos) {
            throw invalid_argument("Invalid format - missing 'to'");
        }

        string stop_name = token.substr(to_pos + 3);
        stop_name.erase(0, stop_name.find_first_not_of(" \t"));
        stop_name.erase(stop_name.find_last_not_of(" \t") + 1);

        if (stop_name.empty()) {
            throw invalid_argument("Empty stop name");
        }

        result.emplace_back(distance, stop_name);
    }


    return result;
}

/**
 * Парсит строку вида "10.123,  -30.1837" и возвращает пару координат (широта, долгота)
 */
geo::Coordinates ParseCoordinates(std::string_view str) {
    static const double nan = std::nan("");

    auto not_space = str.find_first_not_of(' ');
    auto comma = str.find(',');

    if (comma == str.npos) {
        return {nan, nan};
    }

    auto not_space2 = str.find_first_not_of(' ', comma + 1);

    double lat = std::stod(std::string(str.substr(not_space, comma - not_space)));
    double lng = std::stod(std::string(str.substr(not_space2)));

    return {lat, lng};
}

/**
 * Удаляет пробелы в начале и конце строки
 */
std::string_view Trim(std::string_view string) {
    const auto start = string.find_first_not_of(' ');
    if (start == string.npos) {
        return {};
    }
    return string.substr(start, string.find_last_not_of(' ') + 1 - start);
}

/**
 * Разбивает строку string на n строк, с помощью указанного символа-разделителя delim
 */
std::vector<std::string_view> Split(std::string_view string, char delim) {
    std::vector<std::string_view> result;

    size_t pos = 0;
    while ((pos = string.find_first_not_of(' ', pos)) < string.length()) {
        auto delim_pos = string.find(delim, pos);
        if (delim_pos == string.npos) {
            delim_pos = string.size();
        }
        if (auto substr = Trim(string.substr(pos, delim_pos - pos)); !substr.empty()) {
            result.push_back(substr);
        }
        pos = delim_pos + 1;
    }

    return result;
}

/**
 * Парсит маршрут.
 * Для кольцевого маршрута (A>B>C>A) возвращает массив названий остановок [A,B,C,A]
 * Для некольцевого маршрута (A-B-C-D) возвращает массив названий остановок [A,B,C,D,C,B,A]
 */
std::vector<std::string_view> ParseRoute(std::string_view route) {
    if (route.find('>') != route.npos) {
        return Split(route, '>');
    }

    auto stops = Split(route, '-');
    std::vector<std::string_view> results(stops.begin(), stops.end());
    results.insert(results.end(), std::next(stops.rbegin()), stops.rend());

    return results;
}

input::CommandDescription ParseCommandDescription(std::string_view line) {
    auto colon_pos = line.find(':');
    if (colon_pos == line.npos) {
        return {};
    }

    auto space_pos = line.find(' ');
    if (space_pos >= colon_pos) {
        return {};
    }

    auto not_space = line.find_first_not_of(' ', space_pos);
    if (not_space >= colon_pos) {
        return {};
    }

    return {std::string(line.substr(0, space_pos)),
            std::string(line.substr(not_space, colon_pos - not_space)),
            std::string(line.substr(colon_pos + 1))};
}

void input::Reader::ParseLine(std::string_view line) {
    auto command_description = ParseCommandDescription(line);
    if (command_description) {
        commands_.push_back(std::move(command_description));
    }
}


void input::Reader::ApplyCommands(transport_catalogue::TransportCatalogue& catalogue)  {

    std::ranges::partition(commands_, [](CommandDescription const& cmd)
                    {
                            return cmd.command.starts_with("Stop");
                    });


    for (const auto& command : commands_) {
        if (command.command == "Stop") {
            auto coords = ParseCoordinates(command.description);
            catalogue.AddStop(command.id, coords);

        } else if (command.command == "Bus") {
            auto stops = ParseRoute(command.description);
            bool is_roundtrip = command.description.find('>') != std::string::npos;

            std::vector<std::string> stop_names;
            stop_names.reserve(stops.size());
            for (auto stop : stops) {
                stop_names.emplace_back(stop);
            }

            catalogue.AddBus(command.id, std::move(stop_names), is_roundtrip);
        }
    }

    for (const auto& command : commands_) {
        if (command.command == "Stop"){
            auto result = ParseStopDistances(command.description);
            // catalogue.AddDistance (command.id, result);

            const transport_catalogue::Stop* from_stop = catalogue.GetStop(command.id);

            if (!from_stop)
            {
                continue; // Остановка не найдена
            }

            // Добавляем все расстояния из вектора
            for (const auto& [distance, to_stop_name] : result) {

                const transport_catalogue::Stop* to_stop = catalogue.GetStop(to_stop_name);
                if (!to_stop)
                {
                    continue;
                }

                catalogue.SetDistance( from_stop ,  to_stop , distance );


            }
        }
    }
}

}// input
