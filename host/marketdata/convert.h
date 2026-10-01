#pragma once

#include <string>
#include <vector>

#include "types.h"

namespace mtcore {
class Element;
}

// Conversions from mtcomm wire elements (see docs/PROTOCOL.md) to the
// renderer-facing types in types.h.
namespace mockterm::md::convert {

// Market-data fields requested for every terminal subscription.
const std::vector<std::string>& TickFields();

// Overwrites the members of |tick| for which |fields| carries a value, so
// partial or conflated updates can be applied to a running image.
void ApplyTickFields(Tick* tick, const mtcore::Element& fields);

std::vector<Bar> BarsFromHistory(const mtcore::Element& response);
Headline HeadlineFrom(const mtcore::Element& element);
std::vector<Headline> HeadlinesFrom(const mtcore::Element& response);
ChatMessage ChatFrom(const mtcore::Element& element, const std::string& viewer);
Instrument InstrumentFrom(const mtcore::Element& element);
std::vector<Instrument> InstrumentsFrom(const mtcore::Element& response);

// Human-readable text of a REQUEST_FAILURE / SUBSCRIPTION_FAILURE body.
std::string ErrorText(const mtcore::Element& body);

}  // namespace mockterm::md::convert
