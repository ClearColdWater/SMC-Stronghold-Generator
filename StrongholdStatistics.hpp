// drafted with GPT-5.4

#pragma once

#include "StrongholdSMC.hpp"
#include "StrongholdObservationJson.hpp"
#include "nlohmann/json.hpp"

#include <unordered_map>
#include <array>
#include <fstream>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <functional>

namespace StrongholdStatistics
{
    using json = nlohmann::json;

    using namespace StrongholdSMC;
    using namespace StrongholdObservations;
    using namespace StrongholdStrucures;
    using namespace StructurePieces;
    using namespace utils;

    static constexpr int PieceTypeCount = static_cast<int>(UNKNOWN) + 1; // = 15

    inline double safeDiv(double a, double b)
    {
        return b == 0.0 ? std::numeric_limits<double>::quiet_NaN() : a / b;
    }

    inline json jsonNumberOrNull(double x)
    {
        return std::isfinite(x) ? json(x) : json(nullptr);
    }

    struct PerNodeStatsAccumulator
    {
        double supportWeight = 0.0;
        double portalSubtreeWeight = 0.0;
        double exactPortalWeight = 0.0;

        double sumSubtreeRoomCount = 0.0;
        double sumNodeDepth = 0.0;
        double sumDirectChildCount = 0.0;

        double sumPortalDepthAbs = 0.0;
        double sumPortalDepthRel = 0.0;

        double sumExpansionOrder = 0.0;

        double sumSubtreeRoomCountSq = 0.0;
        double sumPortalDepthAbsSq = 0.0;
        double sumPortalDepthRelSq = 0.0;
        double sumExpansionOrderSq = 0.0;

        double pieceTypeWeight[16] = {0.0};
    };

    struct StrongholdPosteriorStats
    {
        bool initialized = false;

        uint64_t rawSampleCount = 0;
        uint64_t uniqueAncestors = 0;
        double totalWeight = 0.0;

        double sumTotalRooms = 0.0;
        double sumPortalExists = 0.0;
        double sumPortalDepthAbs = 0.0;
        double portalDepthWeight = 0.0;

        std::vector<PerNodeStatsAccumulator> nodeStats;

        void resetForObservation(const StrongholdObservation& observation)
        {
            initialized = true;
            rawSampleCount = 0;
            uniqueAncestors = 0;
            totalWeight = 0.0;
            sumTotalRooms = 0.0;
            sumPortalExists = 0.0;
            sumPortalDepthAbs = 0.0;
            portalDepthWeight = 0.0;
            nodeStats.assign(observation.tree.totalNodes, {});
        }
    };

    struct GeneratedNodeInfo
    {
        int child[5] = {-1, -1, -1, -1, -1};
        uint32_t subtreeSize = 0;
        bool hasPortalInSubtree = false;
        int portalDepthAbs = -1;
    };

    inline void computeObservationHashAndTreeDepth(
        const StrongholdObservation& observation,
        std::vector<uint64_t>& hashes,
        std::vector<int>& depths,
        uint32_t u = 0,
        uint64_t hash = 42,
        int depth = 0
    ){
        if(observation.tree.totalNodes == 0) return;

        if(hashes.empty())
        {
            hashes.assign(observation.tree.totalNodes, 0);
            depths.assign(observation.tree.totalNodes, -1);
        }

        hashes[u] = hash;
        depths[u] = depth;

        for(int slot = 0; slot < 5; ++slot)
        {
            int v = observation.tree.nodes[u].ch[slot];
            if(v >= 0)
                computeObservationHashAndTreeDepth(
                    observation, hashes, depths,
                    static_cast<uint32_t>(v),
                    updateHash(hash, slot),
                    depth + 1
                );
        }
    }

    template<nextPieceSelectionMethod method>
    inline void accumulateSingleStronghold(
        const Stronghold<method>& stronghold,
        const StrongholdObservation& observation,
        StrongholdPosteriorStats& out,
        double weight = 1.0
    ){
        if(!out.initialized)
            out.resetForObservation(observation);
        else if(out.nodeStats.size() != observation.tree.totalNodes)
            throw std::runtime_error("StrongholdPosteriorStats observation mismatch");

        if(weight <= 0.0) return;
        if(stronghold.builtPieces.list.empty()) return;

        if constexpr (method == observationGuided)
        {
            if(!stronghold.finished) return;
            if(stronghold.generationContext.logImportanceWeight <= -1e8) return;
        }

        const auto& list = stronghold.builtPieces.list;
        const uint32_t n = static_cast<uint32_t>(list.size());

        std::unordered_map<uint64_t, uint32_t> hashToIndex;
        hashToIndex.reserve(n * 2 + 1);

        std::vector<GeneratedNodeInfo> info(n);

        for(uint32_t i = 0; i < n; ++i)
            hashToIndex.emplace(getBase(list[i])->ancestryHash, i);

        for(uint32_t i = 0; i < n; ++i)
        {
            uint64_t h = getBase(list[i])->ancestryHash;
            for(int slot = 0; slot < 5; ++slot)
            {
                auto it = hashToIndex.find(updateHash(h, slot));
                if(it != hashToIndex.end())
                    info[i].child[slot] = static_cast<int>(it->second);
            }
        }

        std::function<void(uint32_t)> dfs = [&](uint32_t u)
        {
            const auto* piece = getBase(list[u]);

            info[u].subtreeSize = 1;
            info[u].hasPortalInSubtree = (piece->getPieceType() == PORTAL_ROOM);
            info[u].portalDepthAbs = info[u].hasPortalInSubtree ? static_cast<int>(piece->getRoomDepth()) : -1;

            for(int slot = 0; slot < 5; ++slot)
            {
                int v = info[u].child[slot];
                if(v == -1) continue;

                dfs(static_cast<uint32_t>(v));

                info[u].subtreeSize += info[v].subtreeSize;

                if(info[v].hasPortalInSubtree)
                {
                    info[u].hasPortalInSubtree = true;
                    if(info[u].portalDepthAbs == -1)
                        info[u].portalDepthAbs = info[v].portalDepthAbs;
                }
            }
        };

        dfs(0);

        out.rawSampleCount += 1;
        out.totalWeight += weight;
        out.sumTotalRooms += weight * n;

        if(info[0].hasPortalInSubtree)
        {
            out.sumPortalExists += weight;
            out.sumPortalDepthAbs += weight * info[0].portalDepthAbs;
            out.portalDepthWeight += weight;
        }

        for(uint32_t i = 0; i < n; ++i)
        {
            const auto* piece = getBase(list[i]);

            int nodeIndex = observation.branchHashToIndexMap.find(piece->ancestryHash);
            if(nodeIndex == -1) continue;

            auto& acc = out.nodeStats[nodeIndex];
            acc.supportWeight += weight;

            acc.sumSubtreeRoomCount += weight * info[i].subtreeSize;
            acc.sumSubtreeRoomCountSq += weight * info[i].subtreeSize * info[i].subtreeSize;

            acc.sumNodeDepth += weight * piece->getRoomDepth();

            int childCount = 0;
            for(int slot = 0; slot < 5; ++slot)
                childCount += (info[i].child[slot] != -1);
            acc.sumDirectChildCount += weight * childCount;

            acc.sumExpansionOrder += weight * piece->expansionOrder;
            acc.sumExpansionOrderSq += weight * piece->expansionOrder * piece->expansionOrder;

            acc.pieceTypeWeight[piece->getPieceType()] += weight;

            if(info[i].hasPortalInSubtree)
            {
                acc.portalSubtreeWeight += weight;

                acc.sumPortalDepthAbs += weight * info[i].portalDepthAbs;
                acc.sumPortalDepthAbsSq += weight * info[i].portalDepthAbs * info[i].portalDepthAbs;

                double relDepth = info[i].portalDepthAbs - static_cast<int>(piece->getRoomDepth());
                acc.sumPortalDepthRel += weight * relDepth;
                acc.sumPortalDepthRelSq += weight * relDepth * relDepth;
            }

            if(piece->getPieceType() == PORTAL_ROOM)
                acc.exactPortalWeight += weight;
        }
    }

    template<nextPieceSelectionMethod method>
    inline void accumulateStrongholdSamples(
        const std::vector<Stronghold<method>>& samples,
        uint32_t uniqueAncestors,
        const StrongholdObservation& observation,
        StrongholdPosteriorStats& out,
        const std::vector<double>* weights = nullptr
    ){
        if(weights != nullptr && weights->size() != samples.size())
            throw std::runtime_error("weights size mismatch");

        for(size_t i = 0; i < samples.size(); ++i)
        {
            double w = (weights == nullptr ? 1.0 : (*weights)[i]);
            accumulateSingleStronghold(samples[i], observation, out, w);
        }

        out.uniqueAncestors += uniqueAncestors;
    }

    template<nextPieceSelectionMethod method>
    inline StrongholdPosteriorStats analyzeStrongholdSamples(
        const std::vector<Stronghold<method>>& samples,
        const StrongholdObservation& observation,
        const std::vector<double>* weights = nullptr
    ){
        StrongholdPosteriorStats out;
        out.resetForObservation(observation);
        accumulateStrongholdSamples(
            samples,
            static_cast<uint32_t>(samples.size()),
            observation,
            out,
            weights
        );
        return out;
    }

    inline json buildStatsJson(
        const StrongholdPosteriorStats& stats,
        const StrongholdObservation& observation,
        const StrongholdAuxiliaryInfo* debugInfo = nullptr
    ){
        std::vector<uint64_t> branchHashes;
        std::vector<int> treeDepths;

        if(observation.tree.totalNodes > 0)
            computeObservationHashAndTreeDepth(observation, branchHashes, treeDepths);

        json j;
        j["rawSampleCount"] = stats.rawSampleCount;
        j["uniqueAncestors"] = stats.uniqueAncestors;
        j["totalWeight"] = stats.totalWeight;
        j["observation"] = observation;

        if(debugInfo != nullptr)
        {
            json perNodeDebug = json::object();
            json deltaArr = json::array();
            for(uint32_t i = 0; i < observation.tree.totalNodes; i++)
                deltaArr.push_back(jsonNumberOrNull(debugInfo->nodeLogMeanImportanceWeightDelta[i]));
            perNodeDebug["nodeLogMeanImportanceWeightDelta"] = deltaArr;

            json varianceArr = json::array();
            for(uint32_t i = 0; i < observation.tree.totalNodes; i++)
                varianceArr.push_back(jsonNumberOrNull(debugInfo->nodeImportanceWeightDeltaVariance[i]));
            perNodeDebug["nodeImportanceWeightDeltaVariance"] = varianceArr;
            
            j["debugPerNode"] = perNodeDebug;
        }

        j["overall"] = {
            {"meanTotalRooms", jsonNumberOrNull(safeDiv(stats.sumTotalRooms, stats.totalWeight))},
            {"portalExistsProbability", jsonNumberOrNull(safeDiv(stats.sumPortalExists, stats.totalWeight))},
            {"meanPortalRoomDepthConditional", jsonNumberOrNull(safeDiv(stats.sumPortalDepthAbs, stats.portalDepthWeight))}
        };

        j["nodes"] = json::array();

        for(uint32_t i = 0; i < observation.tree.totalNodes; ++i)
        {
            const auto& obsNode = observation.tree.nodes[i];
            const auto& acc = stats.nodeStats[i];

            json pieceTypeProbability = json::object();
            double pieceTypeTotal = 0.0;
            for(int t = 0; t < PieceTypeCount; ++t)
                pieceTypeTotal += acc.pieceTypeWeight[t];

            if(pieceTypeTotal > 0)
            {
                for(int t = 0; t < PieceTypeCount; ++t)
                {
                    if(acc.pieceTypeWeight[t] > 0.0)
                        pieceTypeProbability[StructurePieceTypeNames[t]] = acc.pieceTypeWeight[t] / pieceTypeTotal;
                }
            }

            j["nodes"].push_back({
                {"index", i},
                {"branchHash", branchHashes.empty() ? std::string("0") : std::to_string(branchHashes[i])},
                {"treeDepth", treeDepths.empty() ? 0 : treeDepths[i]},
                {"determinedType", StructurePieceTypeNames[obsNode.determinedType]},
                {"customData", obsNode.customData},
                {"branchGenerateWeights", {
                    obsNode.branchGenerateWeights[0], obsNode.branchGenerateWeights[1], obsNode.branchGenerateWeights[2],
                    obsNode.branchGenerateWeights[3], obsNode.branchGenerateWeights[4]
                }},

                {"supportProbability", jsonNumberOrNull(safeDiv(acc.supportWeight, stats.totalWeight))},
                {"supportWeight", acc.supportWeight},

                {"portalSubtreeWeight", acc.portalSubtreeWeight},
                {"portalSubtreeProbabilityConditional", jsonNumberOrNull(safeDiv(acc.portalSubtreeWeight, acc.supportWeight))},
                {"portalSubtreeProbabilityUnconditional", jsonNumberOrNull(safeDiv(acc.portalSubtreeWeight, stats.totalWeight))},

                {"portalHereProbabilityConditional", jsonNumberOrNull(safeDiv(acc.exactPortalWeight, acc.supportWeight))},
                {"portalHereProbabilityUnconditional", jsonNumberOrNull(safeDiv(acc.exactPortalWeight, stats.totalWeight))},

                {"meanSubtreeRoomCountConditional", jsonNumberOrNull(safeDiv(acc.sumSubtreeRoomCount, acc.supportWeight))},
                {"meanSubtreeRoomCountUnconditional", jsonNumberOrNull(safeDiv(acc.sumSubtreeRoomCount, stats.totalWeight))},

                {"meanNodeDepthConditional", jsonNumberOrNull(safeDiv(acc.sumNodeDepth, acc.supportWeight))},
                {"meanDirectChildCountConditional", jsonNumberOrNull(safeDiv(acc.sumDirectChildCount, acc.supportWeight))},

                {"meanExpansionOrderConditional", jsonNumberOrNull(safeDiv(acc.sumExpansionOrder, acc.supportWeight))},

                {"meanPortalDepthAbsConditional", jsonNumberOrNull(safeDiv(acc.sumPortalDepthAbs, acc.portalSubtreeWeight))},
                {"meanPortalDepthRelConditional", jsonNumberOrNull(safeDiv(acc.sumPortalDepthRel, acc.portalSubtreeWeight))},

                {"sumSubtreeRoomCountSq", acc.sumSubtreeRoomCountSq},
                {"sumPortalDepthAbsSq", acc.sumPortalDepthAbsSq},
                {"sumPortalDepthRelSq", acc.sumPortalDepthRelSq},
                {"sumExpansionOrderSq", acc.sumExpansionOrderSq},

                {"pieceTypeProbability", pieceTypeProbability}
            });
        }

        return j;
    }

    inline void exportStatsToJson(
        const StrongholdPosteriorStats& stats,
        const StrongholdObservation& observation,
        const std::string& filepath,
        const StrongholdAuxiliaryInfo* debugInfo = nullptr
    ){
        std::ofstream file(filepath);
        if (!file.is_open()) return;
        file << buildStatsJson(stats, observation, debugInfo).dump(2) << std::endl;
    }
}