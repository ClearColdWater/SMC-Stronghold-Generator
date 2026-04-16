#pragma once

#include"AbstractStrongholdPieces.hpp"
#include"StrongholdPieceWeight.hpp"
#include"StrongholdObservations.hpp"
#include<cmath>
#include<cstring>
#include<limits>
#include<algorithm>

namespace StrongholdAuxiliary
{
    static constexpr double minSampleSize = 8, smoothDelta = -1.6094379124341003; // = std::log(0.2)
    using namespace StrongholdObservations;
    struct StrongholdAuxiliaryInfo
    {
        static inline double LogSumExp(double a, double b)
        {
            if(std::isinf(a) && a < 0) return b;
            if(std::isinf(b) && b < 0) return a;
            if(a < b)std::swap(a, b);
            return a + std::log1p(std::exp(b - a));
        }

        double nodeLogMeanImportanceWeightDelta[1024]; // keeps sum temporarily
        double nodeLogMeanImportanceWeightDeltaSq[1024]; // keeps squared sum temporarily
        double nodeImportanceWeightDeltaVariance[1024];
        double nodeTotalOmega[1024];

        double pairwisePendingWinRate[1024][1024]; // keeps sum temporarily
        double pairwiseTotalOmega[1024][1024];

        double totalLogImportanceDebt;

        void clear()
        {
            std::fill(nodeLogMeanImportanceWeightDelta, nodeLogMeanImportanceWeightDelta + 1024, -std::numeric_limits<double>::infinity());
            std::fill(nodeLogMeanImportanceWeightDeltaSq, nodeLogMeanImportanceWeightDeltaSq + 1024, -std::numeric_limits<double>::infinity());
            std::memset(nodeImportanceWeightDeltaVariance, 0, sizeof(nodeImportanceWeightDeltaVariance));
            std::memset(nodeTotalOmega, 0, sizeof(nodeTotalOmega));
            std::fill(&pairwisePendingWinRate[0][0], &pairwisePendingWinRate[0][0] + 1024 * 1024, 4.0);
            std::fill(&pairwiseTotalOmega[0][0], &pairwiseTotalOmega[0][0] + 1024 * 1024, 8.0);
            totalLogImportanceDebt = 0;
        }
        StrongholdAuxiliaryInfo(){ clear(); }

        void build(uint32_t nodeCount) // nodes use the same indexs of branchHashToIndexMap
        {
            totalLogImportanceDebt = 0;

            for(uint32_t i = 0; i < nodeCount; i++)
            {
                if(nodeTotalOmega[i] < minSampleSize)
                {
                    double logComplementOmega = std::log(minSampleSize - nodeTotalOmega[i]);
                    nodeLogMeanImportanceWeightDelta[i] = LogSumExp(logComplementOmega + smoothDelta, nodeLogMeanImportanceWeightDelta[i]);
                    nodeLogMeanImportanceWeightDeltaSq[i] = LogSumExp(logComplementOmega + 2.0 * smoothDelta, nodeLogMeanImportanceWeightDeltaSq[i]);
                    nodeTotalOmega[i] = minSampleSize;
                }
                double logOmega = std::log(nodeTotalOmega[i]);
                nodeLogMeanImportanceWeightDelta[i] -= logOmega;
                nodeLogMeanImportanceWeightDeltaSq[i] -= logOmega;

                double logMean = nodeLogMeanImportanceWeightDelta[i];
                double logMeanSq = nodeLogMeanImportanceWeightDeltaSq[i];
                double logMean2 = 2.0 * logMean;

                if(!std::isfinite(logMean) || !std::isfinite(logMeanSq) || logMeanSq <= logMean2)
                    nodeImportanceWeightDeltaVariance[i] = 0.0;
                else
                    nodeImportanceWeightDeltaVariance[i] =
                        std::exp(logMean2) * std::expm1(logMeanSq - logMean2);
            }

            for(uint32_t i = 0; i < nodeCount; i++)
                for(uint32_t j = 0; j < nodeCount; j++)
                    pairwisePendingWinRate[i][j] /= pairwiseTotalOmega[i][j];

            nodeLogMeanImportanceWeightDelta[0] = 0;
            nodeLogMeanImportanceWeightDelta[1] = 0;
            nodeLogMeanImportanceWeightDeltaSq[0] = 0;
            nodeLogMeanImportanceWeightDeltaSq[1] = 0;
            nodeImportanceWeightDeltaVariance[0] = 0;
            nodeImportanceWeightDeltaVariance[1] = 0;
            for(uint32_t i = 0; i < nodeCount; i++)
                totalLogImportanceDebt += nodeLogMeanImportanceWeightDelta[i];
        }

        void makeInfoFromObservation(const StrongholdObservation* observation)
        {
            clear();
            totalLogImportanceDebt = 0;
            std::memset(nodeTotalOmega, 0, sizeof(nodeTotalOmega));

            for(uint32_t i = 2; i < observation->tree.totalNodes; i++)
            {
                nodeLogMeanImportanceWeightDelta[i] = smoothDelta;
                nodeLogMeanImportanceWeightDeltaSq[i] = 2.0 * smoothDelta;
                nodeImportanceWeightDeltaVariance[i] = 0;
            }

            if(observation->tree.totalNodes > 0)
            {
                nodeLogMeanImportanceWeightDelta[0] = 0;
                nodeLogMeanImportanceWeightDeltaSq[0] = 0;
                nodeImportanceWeightDeltaVariance[0] = 0;
            }
            if(observation->tree.totalNodes > 1)
            {
                nodeLogMeanImportanceWeightDelta[1] = 0;
                nodeLogMeanImportanceWeightDeltaSq[1] = 0;
                nodeImportanceWeightDeltaVariance[1] = 0;
            }

            for(uint32_t i = 0; i < observation->tree.totalNodes; i++)
                totalLogImportanceDebt += nodeLogMeanImportanceWeightDelta[i];

            for(uint32_t i = 0; i < observation->tree.totalNodes; i++)
                for(uint32_t j = 0; j < observation->tree.totalNodes; j++)
                    pairwisePendingWinRate[i][j] /= pairwiseTotalOmega[i][j];
        }
    };
}