#include "hstack.h"



#include "base/cereal/cereal.hpp"
#include "base/std_ext/string.h"
#include "base/Logger.h"

#include "tree/stream_TBuffer.h"

#include "base/cereal/types/string.hpp"
#include "base/cereal/types/vector.hpp"
#include "base/cereal/archives/binary.hpp"

#include "TPad.h"
#include "THStack.h"
#include "TBrowser.h"
#include "TDirectory.h"

using namespace ant;
using namespace std;

hstack::hstack(const string& name, const std::string& title,
               bool useIntelliLegend,
               bool ignoreEmptyHist,
               bool drawNoStack) :
    TNamed(name.c_str(), title.c_str()),
    UseIntelliLegend(useIntelliLegend),
    IgnoreEmptyHist(ignoreEmptyHist),
    DrawNoStack(drawNoStack)
{
    gDirectory->Append(this);
}

hstack::hstack() {}

hstack::~hstack()
{}

TH1* hstack::Hist_t::GetPtr(const string& path)
{
   auto ptr = dynamic_cast<TH1*>(gDirectory->Get(path.c_str()));
   if(ptr == nullptr)
       LOG(WARNING) << "Did not find TH1 for path " << path;
   return ptr;
}

string hstack::Hist_t::GetPath(const TH1* ptr)
{
    const std::string path = std_ext::formatter()
            << ptr->GetDirectory()->GetPath()
            << "/" << ptr->GetName();
    // strip the file of the full path
    const auto n = path.find_first_of(':');
    if(n == path.npos)
        return path;
    return path.substr(n+1);
}

hstack& hstack::operator<<(TH1* hist)
{

    hists.emplace_back(hist, current_option);

    xlabel = hist->GetXaxis()->GetTitle();
    ylabel = hist->GetYaxis()->GetTitle();
    return *this;
}

hstack& hstack::operator<<(const drawoption& c)
{
    current_option = c.Option();
    return *this;
}

void hstack::checkHists()
{
    auto it_hist = hists.begin();
    while(it_hist != hists.end()) {
        if(it_hist->Ptr == nullptr)
            it_hist = hists.erase(it_hist);
        else
            it_hist++;
    }
}

void hstack::Draw(const char* option)
{
    if(hists.empty())
        return;

    vector<string> orig_titles;
    if(UseIntelliLegend) {
        const auto& delim = ": ";
        vector<vector<string>> title_parts;
        map<string, unsigned>  token_counter;
        for(const auto& hist : hists) {
            const string title = hist.Ptr->GetTitle();
            orig_titles.emplace_back(title);
            title_parts.emplace_back(std_ext::tokenize_string(title, delim));
            for(const auto& token : title_parts.back())
                token_counter[token]++;
        }
        vector<vector<string>> unique_title_parts;
        for(size_t i=0; i< title_parts.size(); i++) {
            const auto& tokens = title_parts[i];
            unique_title_parts.emplace_back();
            auto& unique_tokens = unique_title_parts.back();
            for(const auto& token : tokens)
                if(token_counter[token] < hists.size())
                    unique_tokens.emplace_back(token);
            hists[i].Ptr->SetTitle(std_ext::concatenate_string(unique_tokens, delim).c_str());
        }
    }

    auto stack = new THStack((string(GetName())+"_").c_str(), GetTitle());

    unsigned nAdded = 0;
    for(const auto& hist : hists) {
        if(IgnoreEmptyHist && hist.Ptr->GetEntries()==0)
            continue;
        stack->Add(hist.Ptr, hist.Option.c_str());
        nAdded++;
    }

    if(nAdded>0) {
        string option_str(option);
        if(DrawNoStack)
            option_str += "nostack";
        stack->Draw(option_str.c_str());
        auto xaxis = stack->GetXaxis();
        if(xaxis)
            xaxis->SetTitle(xlabel.c_str());

        auto yaxis = stack->GetYaxis();
        if(yaxis)
            yaxis->SetTitle(ylabel.c_str());
    }
    else {
        LOG(WARNING) << "No histograms in ant::hstack to draw (maybe all empty)";
    }


    if(UseIntelliLegend) {
        if(nAdded>0)
            gPad->BuildLegend();
        for(size_t i=0;i<orig_titles.size();i++)
            hists[i].Ptr->SetTitle(orig_titles[i].c_str());
    }

}

void hstack::Browse(TBrowser* b)
{
    Draw(b ? b->GetDrawOption() : "");
    gPad->Update();
}

// cereal business

template<class Archive>
void save(Archive& archive,
          const TNamed& m)
{
    archive(string(m.GetName()), string(m.GetTitle()));
}

template<class Archive>
void load(Archive & archive,
          TNamed& m)
{
    string name;
    string title;
    archive(name, title);
    m.SetName(name.c_str());
    m.SetTitle(title.c_str());
}

namespace cereal
{
  template <class Archive>
  struct specialize<Archive, hstack, cereal::specialization::member_serialize> {};
}

void hstack::Streamer(TBuffer& R__b)
{
    stream_TBuffer buf(R__b);
    iostream inoutstream(addressof(buf));

    if (R__b.IsReading()) {
        cereal::BinaryInputArchive ar(inoutstream);
        ar(*this);
    }
    else {
        cereal::BinaryOutputArchive ar(inoutstream);
        ar(*this);
    }
}

