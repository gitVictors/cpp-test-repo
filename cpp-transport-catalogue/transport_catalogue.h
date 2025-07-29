#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>
#include <deque>
#include <set>
#include "geo.h"



namespace transport_catalogue {

using namespace geo;
using namespace std;

struct Stop {
    std::string name;
    Coordinates coordinates;

};

struct Bus {
    std::string name;
    std::vector<const Stop*> stops;
    bool is_roundtrip;
};

struct RouteInfo {
    size_t stops_count;
    size_t unique_stops_count;
    double route_length; //длинна маршрута
    double curvature;  // отношение фактической длины маршрута к географическому расстоянию
};

// struct PairHasher {
//     size_t operator()(const std::pair<std::string_view, std::string_view> pr) const{
//         auto h1 = std::hash<std::string_view>{}(pr.first);
//         auto h2 = std::hash<std::string_view>{}(pr.second);
//         return h1 + h2 * 37;
//     }
// };

struct PairHasher {
    size_t operator()(const std::pair<const Stop*, const Stop*> pr) const {
        auto h1 = std::hash<const Stop*>{}(pr.first);
        auto h2 = std::hash<const Stop*>{}(pr.second);
        return h1 + h2 * 37;
    }
};

struct BusPtrCompare {
    bool operator()(const Bus* lhs, const Bus* rhs) const {
        return lhs->name < rhs->name;
    }
};


class TransportCatalogue {
public:


    void AddStop(const std::string& name, Coordinates& coordinates);
    void AddBus(const std::string& name, const std::vector<std::string>& stops, bool is_roundtrip);

    const Bus* GetBus( std::string_view name) const;
    const Stop* GetStop(std::string_view name) const;

    std::set<const Bus*, BusPtrCompare> GetBusesForStop(std::string_view stop_name) const;

    const RouteInfo RouteInformation(const std::string_view& number_name) const;

    //дистанция между остановками
    void AddDistance (const std::string& name, vector<pair<int, string>>& pvc );
    void SetDistance(const Stop* from, const Stop* to, int meters);
    int GetDistance(const Stop* from, const Stop* to) const;


private:
    void UpdateStopToBus (const std::string& name_number, const std::vector<std::string>& stops);

    std::deque<Bus> all_buses_;
    std::deque<Stop> all_stops_;

    std::unordered_map<std::string_view, const Stop*> stopname_to_stop_; //список остановок
    std::unordered_map<std::string_view, const Bus*> busname_to_bus_; //маршрут

    // Новое поле для хранения автобусов по остановкам
    std::unordered_map<std::string_view, std::set<const Bus*, BusPtrCompare>> stop_to_buses_;

    //дистанция между остановками
    std::unordered_map<std::pair<const Stop*, const Stop*>, int,  PairHasher> distances_;  //
};



} // namespace transport_catalogue
