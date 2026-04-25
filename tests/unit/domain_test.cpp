#include <gtest/gtest.h>
#include "domain/person.h"
#include "domain/birth_event.h"
#include "domain/chart_request.h"
#include "domain/computed_chart.h"
#include "domain/types.h"

TEST(DomainTest, PersonDefaultConstruction) {
  asteria::domain::Person p;
  EXPECT_EQ(p.personId, 0);
  EXPECT_TRUE(p.fullName.empty());
  EXPECT_TRUE(p.tags.empty());
}

TEST(DomainTest, BirthEventUnknownTimeDefaults) {
  asteria::domain::BirthEvent event;
  EXPECT_EQ(event.timeAccuracy, asteria::domain::TimeAccuracy::Unknown);
  EXPECT_FALSE(event.noonDefaultApplied);
  EXPECT_TRUE(event.housesEnabled);
  EXPECT_FALSE(event.birthTime.has_value());
}

TEST(DomainTest, ChartRequestDefaults) {
  asteria::domain::ChartRequest req;
  EXPECT_EQ(req.chartType, asteria::domain::ChartType::Natal);
  EXPECT_EQ(req.defaultHouseSystem, "Placidus");
  EXPECT_EQ(req.zodiacMode, "Tropical");
  EXPECT_TRUE(req.includeHouses);
}

TEST(DomainTest, ComputedChartHoldsPositions) {
  asteria::domain::ComputedChart chart;
  chart.planets.push_back({"Sun", 15.0, 0.0, 0.98, "Aries", 1, false});
  chart.houseCusps.push_back({1, 0.0});
  chart.aspects.push_back({"Sun", "Moon", "Square", 3.0, std::string("applying")});
  EXPECT_EQ(chart.planets.size(), 1u);
  EXPECT_EQ(chart.planets[0].objectId, "Sun");
  EXPECT_TRUE(chart.planets[0].house.has_value());
  EXPECT_EQ(*chart.planets[0].house, 1);
  EXPECT_FALSE(chart.planets[0].retrograde);
}

TEST(DomainTest, PlanetPositionRetrograde) {
  asteria::domain::PlanetPosition p{"Mars", 223.0, -1.2, 0.6, "Scorpio", 8, true};
  EXPECT_TRUE(p.retrograde);
  EXPECT_EQ(p.sign, "Scorpio");
}
