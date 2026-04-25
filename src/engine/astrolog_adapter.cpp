#include "astrolog_adapter.h"
#include <filesystem>

// Astrolog headers — all vendor-specific types stay within this file.
// No Astrolog types, macros, or globals escape into Asteria headers.
extern "C++" {
#include "astrolog.h"
#include "extern.h"
#include "swephexp.h"
}

namespace asteria::engine {

std::mutex AstrologAdapter::s_mutex;
bool AstrologAdapter::s_initialized = false;
std::string AstrologAdapter::s_dataPath;

bool AstrologAdapter::initialize(const std::string& dataPath) {
  std::lock_guard<std::mutex> lock(s_mutex);
  if (s_initialized) return true;

  s_dataPath = dataPath;
  const std::string ephemPath = (std::filesystem::path(dataPath) / "ephem").string();

  // Set environment variable so Astrolog can find its data files
  // (astrolog.as, atlas.as, timezone.as, ephem/)
#ifdef _WIN32
  _putenv_s("ASTROLOG", dataPath.c_str());
  _putenv_s("SE_EPHE_PATH", ephemPath.c_str());
#else
  setenv("ASTROLOG", dataPath.c_str(), 1);
  setenv("SE_EPHE_PATH", ephemPath.c_str(), 1);
#endif

  // Initialize Astrolog's global state (tables, defaults, restrictions)
  InitProgram();

  if (std::filesystem::exists(ephemPath)) {
    swe_set_ephe_path(ephemPath.c_str());
  }

  // Configure for computation-only use (no display, no file output)
  us.fEphemFiles = fTrue;   // Use Swiss Ephemeris for accuracy
  us.fPlacalcPla = fFalse;  // Don't fall back to Placalc
  us.fMatrixPla = fFalse;   // Don't fall back to Matrix

  // Note: We do NOT load astrolog.as because it contains graphical commands
  // (_XJ, etc.) that are unavailable in our GRAPH-disabled library build.
  // All needed computation settings are configured programmatically above.

  s_initialized = true;
  return true;
}

bool AstrologAdapter::isInitialized() {
  return s_initialized;
}

std::string AstrologAdapter::engineVersion() {
  return std::string("Astrolog ") + szVersionCore;
}

int AstrologAdapter::mapHouseSystem(const std::string& name) {
  if (name == "Placidus")        return hsPlacidus;
  if (name == "Koch")            return hsKoch;
  if (name == "Equal")           return hsEqual;
  if (name == "Campanus")        return hsCampanus;
  if (name == "Meridian")        return hsMeridian;
  if (name == "Regiomontanus")   return hsRegiomontanus;
  if (name == "Porphyry")        return hsPorphyry;
  if (name == "Morinus")         return hsMorinus;
  if (name == "Topocentric")     return hsTopocentric;
  if (name == "Alcabitius")      return hsAlcabitius;
  if (name == "Whole")           return hsWhole;
  if (name == "Vedic")           return hsVedic;
  if (name == "Sripati")         return hsSripati;
  return hsPlacidus; // default
}

std::string AstrologAdapter::signName(int signIndex) {
  static const char* signs[] = {
    "", "Aries", "Taurus", "Gemini", "Cancer", "Leo", "Virgo",
    "Libra", "Scorpio", "Sagittarius", "Capricorn", "Aquarius", "Pisces"
  };
  if (signIndex >= 1 && signIndex <= 12) return signs[signIndex];
  return "Unknown";
}

void AstrologAdapter::setChartInfo(const ChartInput& input) {
  // Map Asteria's standard geographic convention to Astrolog's convention:
  // Astrolog: positive longitude = WEST, positive latitude = NORTH
  // Asteria:  positive longitude = EAST, positive latitude = NORTH
  double astrLon = -input.longitudeDeg;  // Negate: East+ → West+
  double astrLat = input.latitudeDeg;    // Same convention

  SetCI(ciCore,
        input.month, input.day, input.year,
        input.timeHours,
        input.dstHours,
        input.timezoneHours,
        astrLon,
        astrLat);

  // Configure house system
  us.nHouseSystem = input.houseSystem;

  // Configure zodiac mode
  us.fSidereal = input.sidereal ? fTrue : fFalse;

  // Copy to ciMain (Astrolog uses this for the primary chart)
  ciMain = ciCore;
}

void AstrologAdapter::readResults(ChartOutput& output) {
  output.engineVersion = engineVersion();
  output.engineMethod = us.fEphemFiles ? "SwissEphemeris" : "Matrix";

  // Read planet positions for the standard objects (Sun through East Point)
  // Objects 1-21 are the main astrological bodies
  static const int mainObjects[] = {
    oSun, oMoo, oMer, oVen, oMar, oJup, oSat, oUra, oNep, oPlu,
    oChi, oNod, oSou, oLil, oFor, oVtx, oEP
  };
  static const char* mainObjNames[] = {
    "Sun", "Moon", "Mercury", "Venus", "Mars", "Jupiter", "Saturn",
    "Uranus", "Neptune", "Pluto", "Chiron", "North Node", "South Node",
    "Lilith", "Fortune", "Vertex", "East Point"
  };

  for (int i = 0; i < 17; ++i) {
    int obj = mainObjects[i];
    double lon = cp0.obj[obj];
    double lat = cp0.alt[obj];
    double spd = cp0.dir[obj];
    int signIdx = static_cast<int>(lon) / 30 + 1;  // SFromZ macro
    int houseNum = cp0.house[obj];

    ChartOutput::PlanetData pd;
    pd.objectIndex = obj;
    pd.name = mainObjNames[i];
    pd.longitude = lon;
    pd.latitude = lat;
    pd.speed = spd;
    pd.sign = signName(signIdx);
    pd.signIndex = signIdx;
    pd.house = houseNum;
    pd.retrograde = (spd < 0.0);
    output.planets.push_back(pd);
  }

  // Read house cusps (1-12)
  for (int h = 1; h <= 12; ++h) {
    ChartOutput::HouseCuspData hc;
    hc.houseNumber = h;
    hc.longitude = cp0.cusp[h];
    output.houseCusps.push_back(hc);
  }
}

bool AstrologAdapter::computeChart(const ChartInput& input, ChartOutput& output) {
  std::lock_guard<std::mutex> lock(s_mutex);

  if (!s_initialized) return false;

  setChartInfo(input);

  // CastChart(0) = compute a standard natal chart
  // Returns the Julian Day, or 0 on error.
  CastChart(0);

  readResults(output);
  return true;
}

}  // namespace asteria::engine
