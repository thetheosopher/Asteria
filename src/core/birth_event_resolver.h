#pragma once
#include <optional>
#include "domain/birth_event.h"
#include "domain/chart_request.h"
#include "domain/location_resolution.h"

namespace asteria::core {

/// Resolve a BirthEvent (+ optional Location) into a numeric ResolvedBirthInput
/// the engine can consume. Coordinates and timezone are taken from the
/// Location row when present; otherwise from the BirthEvent's direct fields.
///
/// Date is parsed from "YYYY-MM-DD"; time from "HH:MM[:SS]". Missing time
/// (i.e., the Unknown accuracy or empty birthTime) defaults to 12:00 (noon).
domain::ResolvedBirthInput resolveBirthInput(
    const domain::BirthEvent& event,
    const std::optional<domain::LocationResolution>& location = std::nullopt);

/// Apply the unknown-time policy from settings. Mutates the request:
///   - "unknown_time_no_houses": clears includeHouses if time is unknown.
///   - "noon_default_with_warning": forces noon time and sets the policy
///     string so the engine emits a "noon_default_applied" uncertainty flag.
void applyUnknownTimePolicy(domain::ChartRequest& request,
                             const domain::BirthEvent& event,
                             const std::string& policy);

/// Build a ResolvedBirthInput for an arbitrary moment (transit time).
/// `iso` is "YYYY-MM-DDTHH:MM[:SS]"; if parsing fails, returns nullopt.
/// `timezoneHours` and lat/lon are passed through unchanged.
std::optional<domain::ResolvedBirthInput> resolveTransitMoment(
    const std::string& iso,
    double latitudeDeg,
    double longitudeDeg,
    double timezoneHours);

}  // namespace asteria::core
