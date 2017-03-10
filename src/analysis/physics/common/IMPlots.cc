#include "IMPlots.h"
#include "utils/combinatorics.h"
#include "utils/particle_tools.h"
#include "base/Logger.h"
#include "utils/combinatorics.h"
#include "TH1D.h"
#include "TTree.h"

using namespace ant;
using namespace ant::analysis;
using namespace ant::analysis::physics;
using namespace std;

IMPlots::IMPlots(const std::string& name, OptionsPtr opts):
    Physics(name, opts),
    m(8,{prs})
{
    prs.AddPromptRange({-2.5,2.5});
    prs.AddRandomRange({-15,-5});
    prs.AddRandomRange({  5,15});
    HistFac.SetTitlePrefix("aa");

    LOG(INFO) << "Promt Random Ratio = " << prs.Ratio();
    for(size_t i=0; i<m.size();++i) {
        m.at(i).MakeHistograms(HistFac,"IM_"+to_string(i+2),to_string(i+2)+" #gamma IM",BinSettings(1200),"IM [MeV]","");
    }
}

template <typename iter>
LorentzVec sumlv(iter start, iter end) {
    LorentzVec s;
    while(start != end ) {
        s += **(start);
        ++start;
    }
    return s;
}


void IMPlots::ProcessEvent(const TEvent& event, manager_t&)
{
    auto recon_particles = utils::ParticleTypeList::Make(event.Reconstructed().Candidates);
    const auto& photons = recon_particles.Get(ParticleTypeDatabase::Photon);

    for(unsigned n = MinNGamma(); n<MaxNGamma(); ++n) {
        for( auto comb = utils::makeCombination(photons,n); !comb.Done(); ++comb) {
            const LorentzVec sum = sumlv(comb.begin(), comb.end());
                for(const auto& h : event.Reconstructed().TaggerHits) {
                    prs.SetTaggerHit(h.Time);
                    m.at(n - MinNGamma()).Fill(sum.M());
                }
            }
       }
}

void IMPlots::ShowResult()
{
    canvas c(GetName());

    for(auto& h : m) {

        c << h.subtracted;
    }

    c<< endc;
}


Symmetric2Gamma::Symmetric2Gamma(const string& name, OptionsPtr opts):
    Physics(name, opts)
{
    const BinSettings im(1600);
    h_symIM = HistFac.makeTH1D("2 #gamma IM, symmectic #gamma energies ("+to_string(perc*100)+"% Window)", "IM [MeV]", "", im, "symmetric2gamma");

    tree    = HistFac.makeTTree("tree");

    tree->Branch("IM",     &b_IM);
    tree->Branch("E",      &b_E);
    tree->Branch("E1",     &b_E1);
    tree->Branch("E2",     &b_E2);
    tree->Branch("Theta1", &b_theta1);
    tree->Branch("Phi1",   &b_phi1);
    tree->Branch("Theta2", &b_theta2);
    tree->Branch("Phi2",   &b_phi2);

}

Symmetric2Gamma::~Symmetric2Gamma()
{}

void Symmetric2Gamma::ProcessEvent(const TEvent& event, manager_t&)
{
    auto recon_particles = utils::ParticleTypeList::Make(event.Reconstructed().Candidates);
    const auto& photons = recon_particles.Get(ParticleTypeDatabase::Photon);
    for( auto comb = utils::makeCombination(photons,2); !comb.Done(); ++comb) {
        const TParticlePtr& g1 = comb.at(0);
        const TParticlePtr& g2 = comb.at(1);

        const auto Eavg = (g1->Ek()+ g2->Ek()) / 2.0;
        if(fabs(g1->Ek() - Eavg) < perc * Eavg) {
            const LorentzVec sum = sumlv(comb.begin(), comb.end());

            b_IM = sum.M();
            b_E  = Eavg;

            b_E1     = g1->Ek();
            b_theta1 = g1->Theta();
            b_phi1   = g1->Phi();

            b_E2     = g2->Ek();
            b_theta2 = g2->Theta();
            b_phi2   = g2->Phi();

            tree->Fill();

            h_symIM->Fill(sum.M());
        }
    }

}

void Symmetric2Gamma::ShowResult()
{
    canvas(GetName())
            << h_symIM
            << endc;
}



IM_CB_TAPS_Plots::IM_CB_TAPS_Plots(const string& name, OptionsPtr opts)
    : Physics(name, opts)
{
    const auto& caloEnergyWindow = opts->Get<interval<double>>("CaloEnergyWindow", {-std_ext::inf, std_ext::inf});
    hists.emplace_back(hist_t{HistFac, {2,2}, hist_t::any, caloEnergyWindow});
//    hists.emplace_back(hist_t{HistFac, {3,3}, hist_t::any, caloEnergyWindow});
//    hists.emplace_back(hist_t{HistFac, {5,5}, hist_t::any, caloEnergyWindow});
//    hists.emplace_back(hist_t{HistFac, {6,6}, hist_t::any, caloEnergyWindow});

    const BinSettings bins_Mult(10);

    h_Mult_All    = HistFac.makeTH1D("Multiplicity: All",   "n Clusters/Event","", bins_Mult,"n_All");
    h_Mult_CB     = HistFac.makeTH1D("Multiplicity: CB",    "n Clusters/Event","", bins_Mult,"n_CB");
    h_Mult_TAPS   = HistFac.makeTH1D("Multiplicity: TAPS",  "n Clusters/Event","", bins_Mult,"n_TAPS");
}

const IM_CB_TAPS_Plots::hist_t::range_t IM_CB_TAPS_Plots::hist_t::any = {0, numeric_limits<int>::max()};

IM_CB_TAPS_Plots::hist_t::hist_t(const HistogramFactory& HistFac,
                                 const range_t& cb, const range_t& taps,
                                 const interval<double>& caloEnergy_window) :
    n_CB(cb), n_TAPS(taps), CaloEnergy_Window(caloEnergy_window)
{
    auto to_string = [] (const range_t& r) {
        if(r == any)
            return string("any");
        if(r.Start() == r.Stop())
            return std::to_string(r.Start());
        return std::to_string(r.Start())+std::to_string(r.Stop());
    };

    prefix = std_ext::formatter()
                   << "h_"
                   << to_string(n_CB)   << "CB_"
                   << to_string(n_TAPS) << "TAPS";

    HistogramFactory histFac(prefix, HistFac, prefix);
    const BinSettings bins_IM   (400, 0, 1100); // MeV
    const BinSettings bins_angle(70, 0,    70); // degrees
    const BinSettings bins_timing(200,-100,100); // ns
    const BinSettings bins_energy(200, 0, 1000);

    h_IM_All   = histFac.makeTH1D("IM: All",  "IM / MeV","",bins_IM,"IM_All");
    h_IM_CB    = histFac.makeTH1D("IM: CB",   "IM / MeV","",bins_IM,"IM_CB");
    h_IM_CB_corr    = histFac.makeTH1D("IM: CB corr",   "IM / MeV","",bins_IM,"IM_CB_corr");
    h_IM_TAPS  = histFac.makeTH1D("IM: TAPS", "IM / MeV","",bins_IM,"IM_TAPS");

    h_Angle_CB   = histFac.makeTH1D("Angle: CB",   "angle [#circ]","",bins_angle,"Angle_CB");
    h_Angle_TAPS = histFac.makeTH1D("Angle: TAPS", "angle [#circ]","",bins_angle,"Angle_TAPS");



    h_ClusterHitTiming_CB   = histFac.makeTH2D("ClusterHitTiming: CB",   "Energy","t / ns",bins_energy,bins_timing,"ClusterHitTiming_CB");
    h_ClusterHitTiming_TAPS = histFac.makeTH2D("ClusterHitTiming: TAPS", "Energy","t / ns",bins_energy,bins_timing,"ClusterHitTiming_TAPS");

}

void IM_CB_TAPS_Plots::hist_t::Fill(const TCandidatePtrList& c_CB, const TCandidatePtrList& c_TAPS) const
{
    if(!n_CB.Contains(c_CB.size()))
        return;
    if(!n_TAPS.Contains(c_TAPS.size()))
        return;

    auto sum_as_photons = [this] (const TCandidatePtrList& cands) {
        LorentzVec sum;
        for(auto& cand : cands) {
            if(CaloEnergy_Window.Contains(cand->CaloEnergy))
            {
            sum += TParticle(ParticleTypeDatabase::Photon, cand);
            }

        }
        return sum;
    };

    const auto& sum_CB = sum_as_photons(c_CB);
//    const auto& sum_TAPS = sum_as_photons(c_TAPS);
//    h_IM_All->Fill((sum_CB+sum_TAPS).M());
    h_IM_CB->Fill(sum_CB.M());
//    h_IM_CB_corr->Fill(sum_as_corr_photons(c_CB).M());
//    h_IM_TAPS->Fill(sum_TAPS.M());

//    h_Angle_CB->Fill(min_angle(c_CB));
//    h_Angle_TAPS->Fill(min_angle(c_TAPS));

//    fill_timing(c_CB, h_ClusterHitTiming_CB);
//    fill_timing(c_TAPS, h_ClusterHitTiming_TAPS);
}

void IM_CB_TAPS_Plots::hist_t::ShowResult() const
{
    canvas(prefix)
//            << h_Angle_CB
//            << h_Angle_TAPS
//            << h_IM_All
            << h_IM_CB
//            << h_IM_CB_corr
//            << h_IM_TAPS
//            << drawoption("colz")
//            << h_ClusterHitTiming_CB
//            << h_ClusterHitTiming_TAPS
            << endc;
}


IM_CB_TAPS_Plots::~IM_CB_TAPS_Plots()
{

}

void IM_CB_TAPS_Plots::ProcessEvent(const TEvent& event, manager_t&)
{


    TCandidatePtrList c_CB;
    TCandidatePtrList c_TAPS;
    for(auto& c : event.Reconstructed().Candidates.get_iter()) {
        if(c->Detector & Detector_t::Type_t::CB)
            c_CB.emplace_back(c);
        else if(c->Detector & Detector_t::Type_t::TAPS)
            c_TAPS.emplace_back(c);
    }

    for(auto& h : hists)
        h.Fill(c_CB, c_TAPS);

    h_Mult_All->Fill(event.Reconstructed().Candidates.size());
    h_Mult_CB->Fill(c_CB.size());
    h_Mult_TAPS->Fill(c_TAPS.size());
}

void IM_CB_TAPS_Plots::ShowResult()
{
    for(const auto& h : hists) {
        h.ShowResult();
    }

//    canvas(GetName())
//            << h_Mult_All
//            << h_Mult_CB
//            << h_Mult_TAPS
//            << endc;
}


AUTO_REGISTER_PHYSICS(IMPlots)
AUTO_REGISTER_PHYSICS(Symmetric2Gamma)
AUTO_REGISTER_PHYSICS(IM_CB_TAPS_Plots)

