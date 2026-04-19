#include<iostream>
#include<random>
#include"StrongholdAPFManager.hpp"
#include"StrongholdObservationUtils.hpp"
#include"StrongholdStatistics.hpp"
#include"StrongholdVerifier.hpp"

using namespace std;
using namespace StrongholdSMC;
using namespace StrongholdObservations;
using namespace StrongholdStatistics;

int main()
{
    // generates approximately particleCount * runs strongholds in total. 
    // the actual stronghold count varies very slightly due to the nature of the chopthin algorithm.
    constexpr uint32_t particleCount = 20480; // memory requirement jumps because of the Plackett-Luce typePriorityProposal gamma model
    constexpr uint32_t runs = 4; 
    constexpr uint32_t nodeCount = 50; // total concrete rooms observed in the observation.
    constexpr bool excludePortalRoom = false; // generateObservation will explicitly exclude portal room from the observation generated.

    std::random_device rd;
    uint64_t seed = 42; //(static_cast<uint64_t>(rd()) << 32) | rd(); 10191517399343550686ULL
    StrongholdObservation observation = generateObservation(seed, nodeCount, excludePortalRoom);
    cout << "observation generated with seed " << seed << endl;
    ostringstream oss;
    oss << "observations/observation-" << seed << '-' << nodeCount << ".json";
    exportObservationToJson(observation, oss.str());/**/

    //StrongholdObservation observation = importObservationFromJson("observations/observation-42-40.json");

    StrongholdPosteriorStats mergedStats;
    StrongholdBatch batch;

    StrongholdAuxiliaryInfo info;
    bool hasInfo = false;

    for(uint32_t i = 0; i < runs; i++)
    {
        auto t1 = std::chrono::high_resolution_clock::now();
        bool success = batch.generateStrongholdsWithBootstrapInfo(
            &observation,
            (i == 0 ? 12 : 1), // more epochs to help the new gamma model get on track
            particleCount,
            0.5,
            42 + i,
            hasInfo ? &info : nullptr
        );

        if(!success)
        {
            cerr << "generation failed." << endl;
            return 1;
        }

        accumulateStrongholdSamples(batch.get(), batch.uniqueAncestors, observation, mergedStats);

        info = batch.getNextInfo();
        hasInfo = true;

        auto t2 = std::chrono::high_resolution_clock::now();

        cerr << '\r' << "run " << i
             << ", batch size = " << batch.get().size()
             << ", accumulated samples = " << mergedStats.rawSampleCount
             << ", time elapsed = " << std::chrono::duration<double, std::milli>(t2 - t1).count() << "ms"
             << endl;
    }

    exportStatsToJson(mergedStats, observation, "outputs/stronghold_stats_merged.json", &info);
    clog << "Exported merged posterior stats to outputs/stronghold_stats_merged.json" << endl;
    
    return 0;
}