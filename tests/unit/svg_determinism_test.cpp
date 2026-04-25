#include <gtest/gtest.h>
#include "render/chart_scene.h"
#include "render/svg_serializer.h"
#include <algorithm>
#include <locale>

using namespace asteria::render;

namespace {

ChartScene makeScene() {
  ChartScene s;
  s.width = 800;
  s.height = 800;
  s.canonicalHash = "abc123";
  s.engineMethod = "AstrologEmbedded";
  s.houseSystem = "Placidus";
  s.zodiacMode = "Tropical";

  CircleElement c{400.0, 400.0, 200.0, {10, 20, 30}, 2.0, "none"};
  c.layer = "rings"; c.id = "ring-outer";
  s.circles.push_back(c);

  LineElement l{100.0, 100.0, 200.0, 200.0, {200, 0, 0}, 1.0};
  l.layer = "aspects"; l.id = "asp-0";
  s.lines.push_back(l);

  TextElement t{400.0, 50.0, "Sun", 14, "middle", {0, 0, 0}};
  t.layer = "planets"; t.id = "planet-Sun";
  s.texts.push_back(t);

  TextElement t2{400.0, 70.0, "<title>", 12, "middle", {0, 0, 0}};
  t2.layer = "title"; t2.id = "title-main";
  s.texts.push_back(t2);

  return s;
}

}  // namespace

TEST(SvgSerializer, IsByteIdenticalForRepeatedCalls) {
  auto scene = makeScene();
  std::string a = serializeSvg(scene);
  std::string b = serializeSvg(scene);
  EXPECT_EQ(a, b);
}

TEST(SvgSerializer, GroupsByLayer) {
  auto svg = serializeSvg(makeScene());
  EXPECT_NE(svg.find(R"(<g class="aspects">)"), std::string::npos);
  EXPECT_NE(svg.find(R"(<g class="planets">)"), std::string::npos);
  EXPECT_NE(svg.find(R"(<g class="rings">)"), std::string::npos);
  EXPECT_NE(svg.find(R"(<g class="title">)"), std::string::npos);
}

TEST(SvgSerializer, EmitsArchivalMetadataComment) {
  auto svg = serializeSvg(makeScene());
  EXPECT_NE(svg.find("hash=abc123"), std::string::npos);
  EXPECT_NE(svg.find("engine=AstrologEmbedded"), std::string::npos);
  EXPECT_NE(svg.find("houses=Placidus"), std::string::npos);
}

TEST(SvgSerializer, EscapesXmlSpecialCharsInText) {
  auto scene = makeScene();
  auto svg = serializeSvg(scene);
  // Raw "<title>" must not appear; escaped form must.
  EXPECT_EQ(svg.find(">\n<title>"), std::string::npos);
  EXPECT_NE(svg.find("&lt;title&gt;"), std::string::npos);
}

TEST(SvgSerializer, NumericFormatIsLocaleIndependent) {
  // Swap the global locale to one that uses comma as decimal separator
  // (de_DE on Windows). The serializer must still emit '.' decimals.
  auto previous = std::locale();
  try {
    std::locale::global(std::locale("de-DE"));
  } catch (const std::runtime_error&) {
    GTEST_SKIP() << "de-DE locale unavailable on this system";
  }
  auto svg = serializeSvg(makeScene());
  std::locale::global(previous);

  EXPECT_NE(svg.find("400.000"), std::string::npos);
  EXPECT_EQ(svg.find("400,000"), std::string::npos);
}

TEST(SvgSerializer, IncludesElementIdsAsAttributes) {
  auto svg = serializeSvg(makeScene());
  EXPECT_NE(svg.find(R"(id="ring-outer")"), std::string::npos);
  EXPECT_NE(svg.find(R"(id="planet-Sun")"), std::string::npos);
  EXPECT_NE(svg.find(R"(id="asp-0")"), std::string::npos);
}

TEST(SvgSerializer, NamedLayersAppearInAlphabeticalOrder) {
  auto svg = serializeSvg(makeScene());
  auto pAsp   = svg.find(R"(<g class="aspects">)");
  auto pPlanets = svg.find(R"(<g class="planets">)");
  auto pRings = svg.find(R"(<g class="rings">)");
  auto pTitle = svg.find(R"(<g class="title">)");
  ASSERT_NE(pAsp, std::string::npos);
  ASSERT_NE(pPlanets, std::string::npos);
  ASSERT_NE(pRings, std::string::npos);
  ASSERT_NE(pTitle, std::string::npos);
  EXPECT_LT(pAsp, pPlanets);
  EXPECT_LT(pPlanets, pRings);
  EXPECT_LT(pRings, pTitle);
}
