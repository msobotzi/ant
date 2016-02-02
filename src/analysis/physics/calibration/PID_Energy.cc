#include "PID_Energy.h"

#include "expconfig/ExpConfig.h"

using namespace std;
using namespace ant;
using namespace ant::analysis::physics;

PID_Energy::PID_Energy(const string& name, OptionsPtr opts) :
    Physics(name, opts)
{
    const auto detector = ExpConfig::Setup::GetDetector(Detector_t::Type_t::PID);
    const auto nChannels = detector->GetNChannels();

    const BinSettings pid_channels(nChannels);
    const BinSettings pid_rawvalues(300);


    h_pedestals = HistFac.makeTH2D(
                      "PID Pedestals",
                      "Raw ADC value",
                      "#",
                      pid_rawvalues,
                      pid_channels,
                      "Pedestals");

    h_bananas =
            HistFac.makeTH3D(
                "PID Bananas",
                "CB Energy / MeV",
                "PID Energy / MeV",
                "Channel",
                BinSettings(400,0,800),
                BinSettings(200,0,18),
                pid_channels,
                "Bananas"
                );

    for(unsigned ch=0;ch<nChannels;ch++) {
        stringstream ss;
        ss << "Ch" << ch;
        h_perChannel.push_back(
                    PerChannel_t(SmartHistFactory(ss.str(), HistFac, ss.str())));
    }
}

PID_Energy::PerChannel_t::PerChannel_t(SmartHistFactory HistFac)
{
    const BinSettings cb_energy(400,0,800);
    const BinSettings pid_timing(300,-300,700);
    const BinSettings pid_rawvalues(300);
    const BinSettings pid_energy(150,0,30);

    PedestalTiming = HistFac.makeTH2D(
                         "PID Pedestals Timing",
                         "Timing / ns",
                         "Raw ADC value",
                         pid_timing,
                         pid_rawvalues,
                         "PedestalTiming");

    PedestalNoTiming = HistFac.makeTH1D(
                           "PID Pedestals No Timing",
                           "Raw ADC value",
                           "#",
                           pid_rawvalues,
                           "PedestalNoTiming");

    Banana = HistFac.makeTH2D(
                 "PID Banana",
                 "CB Energy / MeV",
                 "PID Energy / MeV",
                 cb_energy,
                 pid_energy,
                 "Banana"
                 );

    BananaRaw = HistFac.makeTH2D(
                    "PID Banana Raw",
                    "CB Energy / MeV",
                    "PID ADC Value",
                    cb_energy,
                    BinSettings(300,0,2000),
                    "BananaRaw"
                    );

    //    BananaTiming = HistFac.makeTH3D(
    //                       "PID Banana Timing",
    //                       "CB Energy / MeV",
    //                       "PID Energy / MeV",
    //                       "PID Timing / ns",
    //                       cb_energy,
    //                       pid_energy,
    //                       pid_timing,
    //                       "BananaTiming"
    //                       );


    TDCMultiplicity = HistFac.makeTH1D("PID TDC Multiplicity", "nHits", "#", BinSettings(10), "TDCMultiplicity");
    QDCMultiplicity = HistFac.makeTH1D("PID QDC Multiplicity", "nHits", "#", BinSettings(10), "QDCMultiplicity");
}

void PID_Energy::ProcessEvent(const TEvent& event, manager_t&)
{
    // pedestals, best determined from clusters with energy information only

    std::map<unsigned, vector<double>> pedestals_ch;

    for(const TDetectorReadHit& readhit : event.Reconstructed->DetectorReadHits) {
        if(readhit.DetectorType != Detector_t::Type_t::PID)
            continue;
        if(readhit.ChannelType != Channel_t::Type_t::Integral)
            continue;
        pedestals_ch[readhit.Channel] = readhit.Converted;
    }


    for(const TClusterPtr& cluster : event.Reconstructed->Clusters) {
        if(cluster->DetectorType != Detector_t::Type_t::PID)
            continue;

        // only consider one cluster PID hits
        if(cluster->Hits.size() != 1)
            continue;
        const TClusterHit& pidhit = cluster->Hits.front();

        PerChannel_t& h = h_perChannel[pidhit.Channel];

        double timing = numeric_limits<double>::quiet_NaN();

        unsigned nIntegrals = 0;
        unsigned nTimings = 0;
        for(const TClusterHitDatum& datum : pidhit.Data) {
            if(datum.GetType() == Channel_t::Type_t::Integral) {
                nIntegrals++;
            }
            if(datum.GetType() == Channel_t::Type_t::Timing) {
                timing = datum.Value;
                nTimings++;
            }
        }

        h.QDCMultiplicity->Fill(nIntegrals);
        h.TDCMultiplicity->Fill(nTimings);

        const auto it_pedestals = pedestals_ch.find(pidhit.Channel);
        if(it_pedestals == pedestals_ch.end()) {
            assert(nIntegrals == 0);
            continue;
        }
        const auto& pedestals = it_pedestals->second;
        assert(nIntegrals == pedestals.size());

        if(nIntegrals != 1)
            continue;
        if(nTimings > 1)
            continue;

        const auto& pedestal = pedestals.front();

        h_pedestals->Fill(pedestal, pidhit.Channel);

        if(std::isfinite(timing))
            h.PedestalTiming->Fill(timing, pedestal);
        else
            h.PedestalNoTiming->Fill(pedestal);

    }


    // bananas per channel histograms
    for(const auto& candidate : event.Reconstructed->Candidates) {
        // only candidates with one cluster in CB and one cluster in PID
        if(candidate->Clusters.size() != 2)
            continue;
        const bool cb_and_pid = candidate->Detector & Detector_t::Type_t::CB &&
                                candidate->Detector & Detector_t::Type_t::PID;
        if(!cb_and_pid)
            continue;

        // search for PID cluster
        const TClusterPtr& pid_cluster = candidate->FindFirstCluster(Detector_t::Type_t::PID);

        h_bananas->Fill(candidate->CaloEnergy,
                        candidate->VetoEnergy,
                        pid_cluster->CentralElement);

        // per channel histograms
        PerChannel_t& h = h_perChannel[pid_cluster->CentralElement];

        // fill the banana
        h.Banana->Fill(candidate->CaloEnergy,
                       candidate->VetoEnergy);

        // is there an pedestal available?
        const auto it_pedestals = pedestals_ch.find(pid_cluster->CentralElement);
        if(it_pedestals == pedestals_ch.end()) {
            continue;
        }
        const auto& pedestals = it_pedestals->second;
        if(pedestals.size() != 1)
            continue;

        const auto& pedestal = pedestals.front();

        h.BananaRaw->Fill(candidate->CaloEnergy, pedestal);
        //h.BananaTiming->Fill(candidate->ClusterEnergy(), candidate->VetoEnergy(), timing);

    }
}

void PID_Energy::ShowResult()
{
    canvas(GetName())
            << drawoption("colz") << h_pedestals
            << endc;
}

AUTO_REGISTER_PHYSICS(PID_Energy)

