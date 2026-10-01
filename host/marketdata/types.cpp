#include "types.h"

namespace mockterm::md {

const char* ToString(AssetClass c) {
  switch (c) {
    case AssetClass::Equity: return "Equity";
    case AssetClass::Fx: return "Curncy";
    case AssetClass::Index: return "Index";
    case AssetClass::Rate: return "Rate";
  }
  return "Unknown";
}

bool ParseAssetClass(const std::string& s, AssetClass* out) {
  for (AssetClass c : {AssetClass::Equity, AssetClass::Fx, AssetClass::Index, AssetClass::Rate}) {
    if (s == ToString(c)) {
      *out = c;
      return true;
    }
  }
  return false;
}

bool ParseHistoryRange(const std::string& s, HistoryRange* out) {
  if (s == "1D") { *out = HistoryRange::OneDay; return true; }
  if (s == "1M") { *out = HistoryRange::OneMonth; return true; }
  if (s == "1Y") { *out = HistoryRange::OneYear; return true; }
  return false;
}

const char* ToString(HistoryRange r) {
  switch (r) {
    case HistoryRange::OneDay: return "1D";
    case HistoryRange::OneMonth: return "1M";
    case HistoryRange::OneYear: return "1Y";
  }
  return "1D";
}

const char* ToString(ConnectionState s) {
  switch (s) {
    case ConnectionState::Connecting: return "connecting";
    case ConnectionState::Live: return "live";
    case ConnectionState::Stale: return "stale";
    case ConnectionState::Down: return "down";
  }
  return "down";
}

}  // namespace mockterm::md
