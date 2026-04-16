// drafted with GPT-5.4

#pragma once

#include"nlohmann/json.hpp"
#include"StrongholdObservations.hpp"
#include"StructurePieces.hpp"
#include"Directions.hpp"
#include<fstream>
#include<string>
#include<algorithm>
#include<map>
#include<regex>
#include<limits>
#include<cmath>

namespace StrongholdObservations
{
    using json = nlohmann::json;
    using namespace utils;
    using namespace StructurePieces;

    inline Direction stringToDirection(const std::string& str)
    {
        for(uint32_t i = 0; i < DIRECTIONS_COUNT; i++)
            if(str == DirectionNames[i])
                return static_cast<Direction>(i);
        return EAST;
    }

    inline StructurePieceType stringToPieceType(const std::string& str)
    {
        for(uint32_t i = 0; i <= UNKNOWN; i++)
            if(str == StructurePieceTypeNames[i])
                return static_cast<StructurePieceType>(i);
        return UNKNOWN;
    }

    static double parseSpecialDouble(const nlohmann::json& j)
    {
        if (j.is_number()) {
            return j.get<double>();
        }
        if (j.is_string()) {
            const std::string s = j.get<std::string>();
            if (s == "inf" || s == "+inf" || s == "infinity" || s == "+infinity") {
                return std::numeric_limits<double>::infinity();
            }
            if (s == "-inf" || s == "-infinity") {
                return -std::numeric_limits<double>::infinity();
            }
        }
        throw std::runtime_error("invalid special double");
    }

    static nlohmann::json dumpSpecialDouble(double x)
    {
        if (std::isinf(x)) {
            return x > 0 ? "inf" : "-inf";
        }
        if (std::isnan(x)) {
            throw std::runtime_error("NaN is not allowed in JSON output");
        }
        return x;
    }

    inline void to_json(json& j, const StrongholdObservation::ObservedStrongholdTree::Node& node)
    {
        std::map<std::string, double> weightsMap;
        for(uint32_t i = 0; i < 15; i++)
            if(node.roomTypeLikelihoodWeights[i] > 0)
                weightsMap[StructurePieceTypeNames[i]] = node.roomTypeLikelihoodWeights[i];

        nlohmann::json arr = nlohmann::json::array();
        for(int i = 0; i < 5; ++i){
            arr.push_back(dumpSpecialDouble(node.branchGenerateWeights[i]));
        }

        j = json{
            {"ch", {node.ch[0], node.ch[1], node.ch[2], node.ch[3], node.ch[4]}},
            {"branchGenerateWeights", arr},
            {"customData", node.customData},
            {"determinedType", StructurePieceTypeNames[node.determinedType]},
            {"weights", weightsMap}
        };
    }

    inline void from_json(const json& j, StrongholdObservation::ObservedStrongholdTree::Node& node)
    {
        std::fill(node.ch, node.ch + 5, branchNotGenerate);
        std::fill(node.branchGenerateWeights, node.branchGenerateWeights + 5, 1.0);
        
        auto ch = j.at("ch").get<std::vector<int>>();
        for(uint32_t i = 0; i < 5; i++)
            node.ch[i] = (i < ch.size()) ? ch[i] : branchNotGenerate;

        std::fill(node.branchGenerateWeights, node.branchGenerateWeights + 5, 1.0);
        if(j.contains("branchGenerateWeights")){
            const auto& arr = j.at("branchGenerateWeights");
            for (size_t i = 0; i < std::min<size_t>(5, arr.size()); ++i) {
                node.branchGenerateWeights[i] = parseSpecialDouble(arr[i]);
            }
        }

        if(j.contains("customData"))
            j.at("customData").get_to(node.customData);
        else
            node.customData = 0;
        
        node.determinedType = stringToPieceType(j.at("determinedType").get<std::string>());

        std::fill(std::begin(node.roomTypeLikelihoodWeights), std::end(node.roomTypeLikelihoodWeights), 0.0);
        if(j.contains("weights"))
        {
            auto weightsMap = j.at("weights").get<std::map<std::string, double>>();
            for(auto const& [name, val] : weightsMap)
            {
                StructurePieceType type = stringToPieceType(name);
                if(type != UNKNOWN || name == "UNKNOWN")
                    node.roomTypeLikelihoodWeights[type] = val;
            }
        }
        else
        {
            if(node.determinedType == UNKNOWN)
            {
                for(uint32_t i = 0; i < 15; i++)
                    node.roomTypeLikelihoodWeights[i] = 1.0;
            }
            else
                node.roomTypeLikelihoodWeights[node.determinedType] = 1.0;
        }
    }

    inline void to_json(json& j, const StrongholdObservation& obs)
    {
        j = json{
            {"starterDirection", DirectionNames[obs.starterDirection]},
            {"tree", {
                {"totalNodes", obs.tree.totalNodes},
                {"nodes", json::array()}
            }}
        };

        for(uint32_t i = 0; i < obs.tree.totalNodes; ++i)
            j["tree"]["nodes"].push_back(obs.tree.nodes[i]);
    }

    inline void from_json(const json& j, StrongholdObservation& obs)
    {
        obs.starterDirection = stringToDirection(j.at("starterDirection").get<std::string>());
        
        const auto& treeJson = j.at("tree");
        obs.tree.totalNodes = treeJson.at("totalNodes").get<uint32_t>();
        
        const auto& nodesJson = treeJson.at("nodes");
        for(uint32_t i = 0; i < obs.tree.totalNodes; ++i)
            obs.tree.nodes[i] = nodesJson[i].get<StrongholdObservation::ObservedStrongholdTree::Node>();
    }

    inline void exportObservationToJson(const StrongholdObservation& obs, const std::string& filepath) 
    {
        json j = obs;
        std::string s = j.dump(4);

        std::regex ch_regex("\"ch\":\\s+\\[\\s+([\\s\\S]*?)\\s+\\]");
        std::regex branch_weights_regex("\"branchGenerateWeights\":\\s+\\[\\s+([\\s\\S]*?)\\s+\\]");
        
        std::smatch match;
        while(std::regex_search(s, match, ch_regex))
        {
            std::string content = match[1].str();
            content = std::regex_replace(content, std::regex("\\s+"), " ");
            if(!content.empty() && content.front() == ' ') content.erase(0, 1);
            if(!content.empty() && content.back() == ' ') content.pop_back();
            
            s.replace(match.position(), match.length(), "\"ch\": [" + content + "]");
        }

        while(std::regex_search(s, match, branch_weights_regex))
        {
            std::string content = match[1].str();
            content = std::regex_replace(content, std::regex("\\s+"), " ");
            if(!content.empty() && content.front() == ' ') content.erase(0, 1);
            if(!content.empty() && content.back() == ' ') content.pop_back();

            s.replace(match.position(), match.length(), "\"branchGenerateWeights\": [" + content + "]");
        }

        std::ofstream file(filepath);
        if(!file.is_open())
            return;

        file << s << std::endl;
    }

    inline StrongholdObservation importObservationFromJson(const std::string& filepath)
    {
        std::ifstream file(filepath);
        json j;
        file >> j;
        StrongholdObservation observation = j.get<StrongholdObservation>();
        Xoshiro256pp rng(0); // only affects door generation
        observation.build(rng);
        return observation;
    }
}