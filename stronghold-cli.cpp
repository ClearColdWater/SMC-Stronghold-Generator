#include<iostream>
#include<random>
#include<filesystem>
#include"StrongholdAPFManager.hpp"
#include"StrongholdObservationUtils.hpp"
#include"StrongholdStatistics.hpp"
// #include"StrongholdVerifier.hpp"
#include"lyra/lyra.hpp"

using namespace std;
using namespace StrongholdSMC;
using namespace StrongholdObservations;
using namespace StrongholdStatistics;
namespace fs = std::filesystem;

static uint64_t makeRandomSeed()
{
    std::random_device rd;
    return (static_cast<uint64_t>(rd()) << 32) | rd();
}
static void ensureParentDir(const fs::path& path)
{
    if(!path.parent_path().empty())
        fs::create_directories(path.parent_path());
}
static string quoteArgument(const string& s)
{
    string out;
    out.push_back('"');
    for(char c : s)
    {
        if(c == '"')
            out.push_back('\\');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}
static fs::path findNearbyFile(const char* argv0, const vector<string>& names)
{
    fs::path exeDir = fs::absolute(fs::path(argv0)).parent_path();
    vector<fs::path> roots =
    {
        fs::current_path(),
        exeDir,
        exeDir.parent_path()
    };
    for(const fs::path& root : roots)
        for(const string& name : names)
        {
            fs::path p = root / name;
            if(fs::exists(p))
                return fs::absolute(p);
        }
    return fs::path();
}
static int runPlotter(const char* argv0, const fs::path& reportPath, const fs::path& plotDir)
{
    fs::create_directories(plotDir);
    fs::path plotterExe = findNearbyFile(
        argv0,
        {
#ifdef _WIN32
            "stronghold-plotter.exe",
            "StrongholdStatsPlotter.exe"
#else
            "stronghold-plotter",
            "StrongholdStatsPlotter"
#endif
        }
    );
    string cmd;
    if(!plotterExe.empty())
    {
        cmd =
            quoteArgument(plotterExe.string()) + ' ' +
            quoteArgument(fs::absolute(reportPath).string()) +
            " --output-dir " +
            quoteArgument(fs::absolute(plotDir).string());
#ifdef _WIN32
        cmd = "\"" + cmd + "\"";
#endif
    }
    else
    {
        fs::path plotterScript = findNearbyFile(argv0, {"StrongholdStatsPlotter.py"});
        if(plotterScript.empty())
            throw runtime_error("Could not find stronghold-plotter executable or StrongholdStatsPlotter.py.");
        cmd =
            string("python ") +
            quoteArgument(plotterScript.string()) + ' ' +
            quoteArgument(fs::absolute(reportPath).string()) +
            " --output-dir " +
            quoteArgument(fs::absolute(plotDir).string());
    }
    cout << "Running plotter..." << endl;
    return system(cmd.c_str());
}
static string getDefaultDemoObservationPath(uint64_t demoSeed, uint32_t nodeCount)
{
    return (fs::path("observations") /
        ("observation-" + to_string(demoSeed) + "-" + to_string(nodeCount) + ".json")).string();
}
static string getDefaultStatsPath(
    const string& observationPath,
    bool demoMode,
    uint64_t demoSeed,
    uint32_t nodeCount
){
    if(demoMode)
    {
        return (fs::path("outputs") /
            ("demo-" + to_string(demoSeed) + "-" + to_string(nodeCount) + "-stats.json")).string();
    }
    return (fs::path("outputs") /
        (fs::path(observationPath).stem().string() + "-stats.json")).string();
}
static string getDefaultPlotDir(const fs::path& reportPath){
    return (fs::path("plots") / reportPath.stem()).string();
}

int main(int argc, char** argv)
{
    try
    {
        uint32_t particleCount = 8192; // memory requirement jumps because of the Plackett-Luce typePriorityProposal gamma model
        uint32_t runs = 8; 
        uint64_t samplingSeed = makeRandomSeed();

        uint32_t bootstrapRuns = 8;
        double resampleThreshold = 0.5;

        bool demoMode = false;
        bool saveObservation = false;
        uint32_t nodeCount = 40; // total concrete rooms observed in the observation.
        uint64_t demoSeed = 42;
        bool excludePortalRoom = false; // generateObservation will explicitly exclude portal room from the observation generated.

        string observationPath;
        string outputPath;

        bool plotAfter = false;
        bool plotOnly = false;
        string plotsDir;

        bool showHelp = false;

        auto cli = 
            lyra::help(showHelp) |
            lyra::opt(demoMode)["--demo"].optional()
                ("demo mode: get an observation from a random stronghold, then sample for it.") |
            lyra::opt(plotAfter)["--plot"].optional()
                ("run sampling normally, then generate plots from the written stats report.") |
            lyra::opt(plotOnly)["--plot-only"].optional()
                ("skip sampling and only generate plots from an existing stats report. \n\
                          when specified, all sampling and demo mode arguments are ignored.") |
            lyra::opt(outputPath, "report_path")["-o"]["--out"].optional()
                ("output path for the JSON report file.\n\
                          Sampling mode: write JSON report here.\n\
                          Plot-only mode: read JSON report from here.") |
            lyra::opt(plotsDir, "plot_dir")["--plots"].optional()
                ("directory for plot output (default: plots/<report-file-stem>/).") |
            lyra::opt(particleCount, "particle_count")["-n"]["--particle-count"].optional()
                ("number of particles per run \n\
                          (number of stronghold instances present at the same time). \n\
                          the true number of particles at the end of each run varies \n\
                          very slightly due to the nature of the chopthin algorithm. \n\
                          If you hit out-of-memory errors, reduce this and increase --runs instead.\n\
                          ") |
            lyra::opt(runs, "runs")["-r"]["--runs"].optional()
                ("number of sampling runs. \n\
                          total stronghold generated is approximately particle count * runs. \n\
                          Prefer more particles each run for better performance.") |
            lyra::opt(samplingSeed, "sampling_seed")["-s"]["--seed"].optional()
                ("seed used for sampling. (default value generated from random_device) \n\
                          Note that even with the same seed, results may still differ \n\
                          due to thread scheduling.") |
            lyra::opt(demoSeed, "demo_seed")["--demo-seed"].optional()
                ("demo mode option, seed for demo observation. (use 0 for a random seed.)\n\
                          ") |
            lyra::opt(nodeCount, "node_count")["--nodes"].optional()
                ("demo mode option, number of nodes of concrete type (not UNKNOWN or NONE) \n\
                          in the generated observation.") |
            lyra::opt(excludePortalRoom)["--exclude-portal"].optional()
                ("demo mode option, exclude portal room from the generated observation.") |
            lyra::opt(saveObservation)["--save-observation"].optional()
                ("demo mode option, write the generated observation JSON under observations/.") |
            lyra::opt(bootstrapRuns, "bootstrap_runs")["--bootstrap-runs"].optional()
                ("number of pre-generation runs used to collect information \n\
                          for better sampling efficiency. Advanced option; \n\
                          leave it as default unless you know what you are doing.") |
            lyra::opt(resampleThreshold, "resample_threshold")["--threshold"].optional()
                ("effective sample size threshold for resampling. Advanced option; \n\
                          leave it as default unless you know what you are doing.") |
            lyra::arg(observationPath, "observation_path").optional()
                ("path to the JSON observation file.");
            
        if(argc == 1)
        {
            cout << cli << endl;
            return 0;
        }
        auto result = cli.parse({argc, argv});
        if(!result)
        {
            cerr << "Error: " << result.message() << endl;
            cerr << endl;
            cerr << cli << endl;
            return 1;
        }
        if(showHelp)
        {
            cout << cli << endl;
            return 0;
        }

        if(particleCount == 0)
            throw runtime_error("--particle-count must be greater than 0.");
        if(runs == 0)
            throw runtime_error("--runs must be greater than 0.");
        if(resampleThreshold < 0.0 || resampleThreshold > 1.0)
            throw runtime_error("--threshold must be in [0, 1].");
        if(plotAfter && plotOnly)
            throw runtime_error("--plot and --plot-only cannot be used together.");
        if(!plotsDir.empty() && !plotAfter && !plotOnly)
            throw runtime_error("--plots requires --plot or --plot-only.");
        if(plotOnly && (demoMode || !observationPath.empty()))
            throw runtime_error("--plot-only cannot be combined with --demo or observation_path.");
        if(plotOnly)
        {
            fs::path reportPath = outputPath.empty()
                ? fs::path("outputs") / "stronghold_stats_merged.json"
                : fs::path(outputPath);
            if(!fs::exists(reportPath))
                throw runtime_error("Stats report not found: " + reportPath.string());
            fs::path plotDir = plotsDir.empty()
                ? fs::path(getDefaultPlotDir(reportPath))
                : fs::path(plotsDir);
            int rc = runPlotter(argv[0], reportPath, plotDir);
            if(rc != 0)
                throw runtime_error("Plotting failed.");
            cout << "Wrote plots to " << fs::absolute(plotDir).string() << endl;
            return 0;
        }
        if(demoMode && !observationPath.empty())
            throw runtime_error("Use either --demo or observation_path, not both.");
        if(!demoMode && observationPath.empty())
            throw runtime_error("Missing observation_path. Use --demo to generate a demo observation.");
        if(!demoMode && saveObservation)
            throw runtime_error("--save-observation is only valid with --demo.");
        if(demoMode && demoSeed == 0)
            demoSeed = makeRandomSeed();
        
        StrongholdObservation observation;
        if(demoMode)
        {
            observation = generateObservation(demoSeed, nodeCount, excludePortalRoom);
            cout << "Generated demo observation with seed " << demoSeed << endl;
            if(saveObservation)
            {
                string observationOutputPath = getDefaultDemoObservationPath(demoSeed, nodeCount);
                ensureParentDir(observationOutputPath);
                exportObservationToJson(observation, observationOutputPath);
                cout << "Saved demo observation to " << observationOutputPath << endl;
            }
            if(outputPath.empty())
                outputPath = getDefaultStatsPath("", true, demoSeed, nodeCount);
        }
        else
        {
            if(!fs::exists(fs::path(observationPath)))
                throw runtime_error("Observation file not found: " + observationPath);
            observation = importObservationFromJson(observationPath);
            cout << "Loaded observation from " << observationPath << endl;
            if(outputPath.empty())
                outputPath = getDefaultStatsPath(observationPath, false, 0, 0);
        }

        cout << "Sampling seed: " << samplingSeed << endl;

        StrongholdPosteriorStats mergedStats;
        StrongholdBatch batch;
        StrongholdAuxiliaryInfo info;
        bool hasInfo = false;
        for(uint32_t i = 0; i < runs; i++)
        {
            auto t1 = std::chrono::high_resolution_clock::now();
            uint32_t epochs = hasInfo ? 1 : (bootstrapRuns + 1);
            bool success = batch.generateStrongholdsWithBootstrapInfo(
                &observation,
                epochs,
                particleCount,
                resampleThreshold,
                samplingSeed + i,
                hasInfo ? &info : nullptr
            );
            if(!success)
                throw runtime_error("Generation failed on run " + to_string(i + 1) + ".");
            accumulateStrongholdSamples(batch.get(), batch.uniqueAncestors, observation, mergedStats);

            info = batch.getNextInfo();
            hasInfo = true;
            auto t2 = std::chrono::high_resolution_clock::now();
            cout << "run " << (i + 1) << "/" << runs
                    << " | batch size = " << batch.get().size()
                    << " | unique ancestors = " << batch.uniqueAncestors
                    << " | accumulated samples = " << mergedStats.rawSampleCount
                    << " | time elapsed = "
                    << std::chrono::duration<double, std::milli>(t2 - t1).count()
                    << "ms"
                    << endl;
        }
        ensureParentDir(outputPath);
        exportStatsToJson(mergedStats, observation, outputPath, hasInfo ? &info : nullptr);
        cout << "Exported merged posterior stats to " << outputPath << endl;
        fs::path plotDir;
        if(plotAfter)
        {
            plotDir = plotsDir.empty()
                ? fs::path(getDefaultPlotDir(outputPath))
                : fs::path(plotsDir);
            int rc = runPlotter(argv[0], fs::path(outputPath), plotDir); 
            if(rc != 0)
                throw runtime_error("Sampling finished and the stats report was written, but plotting failed.");
            cout << "Wrote plots to " << fs::absolute(plotDir).string() << endl;
        }
        return 0;
    }
    catch(const exception& e)
    {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}