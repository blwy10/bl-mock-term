#pragma once

#include <string>
#include <vector>

#include "types.h"

// Minimal JSON serialisation for the bridge payloads. The renderer parses
// these with JSON.parse, so the field names here define the JS-visible shape
// declared in apps/src/types/mockterm.d.ts.
namespace mockterm::md::json {

std::string Quote(const std::string& s);

std::string ToJson(const Tick& t);
std::string ToJson(const std::vector<Tick>& ticks);
std::string ToJson(const Bar& b);
std::string ToJson(const std::vector<Bar>& bars);
std::string ToJson(const Headline& h);
std::string ToJson(const std::vector<Headline>& hs);
std::string ToJson(const ChatMessage& m);
std::string ToJson(const Instrument& i);
std::string ToJson(const std::vector<Instrument>& is);

}  // namespace mockterm::md::json
