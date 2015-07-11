#include "ExpConfig.h"

#include "base/Logger.h"

#include "unpacker/tree/THeaderInfo.h"
#include "unpacker/UnpackerAcqu.h"

#include <type_traits>
#include <list>
#include <cxxabi.h>

using namespace std;
using namespace ant;



namespace ant { // templates need explicit namespace

namespace config {

namespace detector {

struct Detector {
  const Detector_t Type;

protected:
  Detector(const Detector_t& type) :
    Type(type) {}
  Detector(const Detector&) = delete; // disable copy
  virtual ~Detector() = default;
};

struct CB : public Detector {
  CB() : Detector(Detector_t::CB) {}
};

struct TAPS : public Detector {
  TAPS() : Detector(Detector_t::TAPS) {}
};

} // namespace detector

namespace setup {

} // namespace setup

namespace beamtime {

class EtaPrime :
    public ExpConfig::Module,
    public UnpackerAcquConfig
{
public:
  bool Matches(const THeaderInfo &header) const override {
    return true;
  }

  void BuildMappings(std::vector<hit_mapping_t>& hit_mappings,
                     std::vector<scaler_mapping_t>& scaler_mappings) override
  {
    hit_mapping_t map;
    map.LogicalChannel = {Detector_t::Trigger, ChannelType_t::Counter, 0};
    map.RawChannels.push_back(400);
//    map.LogicalElement = {Detector_t::Trigger, ChannelType_t::Counter, 1};
//    map.RawChannels.push_back(1853);
    hit_mappings.push_back(map);
  }


};


} // namespace beamtime

} // namespace config




template<typename T>
unique_ptr<T> Get_(const THeaderInfo& header) {
  VLOG(9) << "Searching for config of type "
          << abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);

  static_assert(is_base_of<ExpConfig::Base, T>::value, "T must be a base of ExpConfig::Base");

  // make a list of available configs
  std::list< std::unique_ptr<ExpConfig::Base> > modules;

  modules.emplace_back(new config::beamtime::EtaPrime());

  // remove the config if the config says it does not match
  modules.remove_if([&header] (const unique_ptr<ExpConfig::Base>& m) {
    return !m->Matches(header);
  });

  // check if something reasonable is left
  if(modules.empty()) {
    throw ExpConfig::Exception("No config found for header "+header.Description);
  }
  if(modules.size()>1) {
    throw ExpConfig::Exception("More than one config found for header "+header.Description);
  }

  // only one instance found, now try to cast it to the
  // requested type unique_ptr<T>
  // this only works if the found module is a derived class of the requested type

  T* ptr = dynamic_cast<T*>(modules.back().release());

  if(ptr==nullptr) {
    throw ExpConfig::Exception("Matched config does not fit to requested type");
  }

  // hand over the unique ptr
  return unique_ptr<T>(ptr);
}


unique_ptr<ExpConfig::Module> ExpConfig::Get(const THeaderInfo& header)
{
  return Get_<ExpConfig::Module>(header);
}

template<>
unique_ptr< UnpackerAcquConfig >
ExpConfig::Unpacker<UnpackerAcquConfig>::Get(const THeaderInfo& header) {
  return Get_< UnpackerAcquConfig >(header);
}


} // namespace ant




