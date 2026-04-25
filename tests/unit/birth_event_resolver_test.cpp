#include <gtest/gtest.h>
#include "core/birth_event_resolver.h"
#include "domain/birth_event.h"
#include "domain/location_resolution.h"
#include "domain/chart_request.h"

using namespace asteria;

namespace {

domain::BirthEvent makeEvent(const std::string& date,
                             const std::optional<std::string>& time,
                             domain::TimeAccuracy acc = domain::TimeAccuracy::Exact) {
  domain::BirthEvent e;
  e.birthDate = date;
  e.birthTime = time;
  e.timeAccuracy = acc;
  return e;
}

domain::LocationResolution makeLoc(double lat, double lon, double tzHours) {
  domain::LocationResolution l;
  l.latitude = lat;
  l.longitude = lon;
  l.timezoneOffsetRuleRef = std::to_string(tzHours);
  return l;
}

}  // namespace

TEST(BirthEventResolver, ParsesDateAndTimeWithSeconds) {
  auto e = makeEvent("1985-07-04", std::string("13:45:30"));
  auto loc = makeLoc(47.6062, -122.3321, -8.0);
  auto in = core::resolveBirthInput(e, loc);
  EXPECT_EQ(in.year, 1985);
  EXPECT_EQ(in.month, 7);
  EXPECT_EQ(in.day, 4);
  EXPECT_NEAR(in.timeHours, 13.0 + 45.0/60.0 + 30.0/3600.0, 1e-9);
  EXPECT_NEAR(in.latitudeDeg, 47.6062, 1e-9);
  EXPECT_NEAR(in.longitudeDeg, -122.3321, 1e-9);
  EXPECT_NEAR(in.timezoneHours, -8.0, 1e-9);
}

TEST(BirthEventResolver, EventCoordinatesAndTimezoneUsedWhenNoLocation) {
  auto e = makeEvent("2000-01-01", std::string("12:00"));
  e.latitudeDeg = 40.0;
  e.longitudeDeg = -74.0;
  e.timezoneOffsetHours = -5.0;
  auto in = core::resolveBirthInput(e, std::nullopt);
  EXPECT_NEAR(in.latitudeDeg, 40.0, 1e-9);
  EXPECT_NEAR(in.longitudeDeg, -74.0, 1e-9);
  EXPECT_NEAR(in.timezoneHours, -5.0, 1e-9);
}

TEST(BirthEventResolver, EventTimezoneOverridesLocation) {
  auto e = makeEvent("2000-01-01", std::string("12:00"));
  e.timezoneOffsetHours = 2.5;
  auto in = core::resolveBirthInput(e, makeLoc(0.0, 0.0, -8.0));
  // BirthEvent's explicit override wins over Location's parsed offset.
  EXPECT_NEAR(in.timezoneHours, 2.5, 1e-9);
}

TEST(BirthEventResolver, UnknownAccuracyDefaultsToNoon) {
  auto e = makeEvent("1985-07-04", std::string("03:15"),
                     domain::TimeAccuracy::Unknown);
  auto in = core::resolveBirthInput(e, std::nullopt);
  EXPECT_NEAR(in.timeHours, 12.0, 1e-9);
}

TEST(BirthEventResolver, NoonPolicyForcesNoonAndKeepsHouses) {
  auto e = makeEvent("1985-07-04", std::string("03:15"),
                     domain::TimeAccuracy::Unknown);
  domain::ChartRequest req;
  req.primaryInput = core::resolveBirthInput(e, std::nullopt);
  req.includeHouses = false;  // some other code may have cleared this

  core::applyUnknownTimePolicy(req, e, "noon_default_with_warning");

  ASSERT_TRUE(req.primaryInput.has_value());
  EXPECT_NEAR(req.primaryInput->timeHours, 12.0, 1e-9);
  EXPECT_TRUE(req.includeHouses);
  EXPECT_EQ(req.unknownTimePolicy, "noon_default_with_warning");
}

TEST(BirthEventResolver, NoHousesPolicyClearsHouses) {
  auto e = makeEvent("1985-07-04", std::string("03:15"),
                     domain::TimeAccuracy::Unknown);
  domain::ChartRequest req;
  req.primaryInput = core::resolveBirthInput(e, std::nullopt);
  req.includeHouses = true;

  core::applyUnknownTimePolicy(req, e, "unknown_time_no_houses");

  EXPECT_FALSE(req.includeHouses);
  EXPECT_EQ(req.unknownTimePolicy, "unknown_time_no_houses");
}

TEST(BirthEventResolver, ExactTimeIsUntouched) {
  auto e = makeEvent("1985-07-04", std::string("13:45"),
                     domain::TimeAccuracy::Exact);
  domain::ChartRequest req;
  req.primaryInput = core::resolveBirthInput(e, std::nullopt);
  req.includeHouses = true;
  double t = req.primaryInput->timeHours;

  core::applyUnknownTimePolicy(req, e, "noon_default_with_warning");

  EXPECT_NEAR(req.primaryInput->timeHours, t, 1e-9);
  EXPECT_TRUE(req.includeHouses);
}

TEST(BirthEventResolver, TransitMomentParsesIso) {
  auto in = core::resolveTransitMoment("2024-06-15T18:30:00",
                                       47.0, -122.0, 0.0);
  ASSERT_TRUE(in.has_value());
  EXPECT_EQ(in->year, 2024);
  EXPECT_EQ(in->month, 6);
  EXPECT_EQ(in->day, 15);
  EXPECT_NEAR(in->timeHours, 18.5, 1e-9);
  EXPECT_NEAR(in->latitudeDeg, 47.0, 1e-9);
  EXPECT_NEAR(in->longitudeDeg, -122.0, 1e-9);
}

TEST(BirthEventResolver, TransitMomentRejectsMalformed) {
  auto in = core::resolveTransitMoment("not-a-date", 0, 0, 0);
  EXPECT_FALSE(in.has_value());
}
